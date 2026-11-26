#include <vnm_process_custody/qprocess_start.h>

#include <QCoreApplication>
#include <QProcess>
#include <qt_windows.h>

#include <cstdio>

namespace custody = vnm::process_custody;

namespace {

bool exercise(HANDLE family, HANDLE task, HANDLE event)
{
    const auto event_value = reinterpret_cast<quintptr>(event);
    QProcess child;
    child.setProgram(QCoreApplication::applicationFilePath());
    child.setArguments({QStringLiteral("--child"), QString::number(event_value)});
    int modifications = 0;
    child.setCreateProcessArgumentsModifier([&modifications](QProcess::CreateProcessArguments* args) {
        ++modifications;
        args->flags |= CREATE_NO_WINDOW;
    });
    {
        custody::Qprocess_creation_admission guard(&child,
            reinterpret_cast<quintptr>(family), reinterpret_cast<quintptr>(task), true);
        if (!guard.established()) {
            std::fputs("Could not prepare the two jobs and Qt pipes.\n", stderr);
            return false;
        }
        child.start();
    }
    if (!child.waitForStarted(5000)) {
        std::fprintf(stderr, "Qt child failed to start: %s\n", qPrintable(child.errorString()));
        return false;
    }
    if (custody::start_qprocess(&child, reinterpret_cast<quintptr>(task), true)) {
        std::fputs("Reported a new launch for an already running child.\n", stderr);
        return false;
    }
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, DWORD(child.processId()));
    BOOL in_family = FALSE;
    BOOL in_task   = FALSE;
    const bool admitted = process &&
        IsProcessInJob(process, family, &in_family) && IsProcessInJob(process, task, &in_task);
    if (process) {
        CloseHandle(process);
    }
    child.write("read\n");
    child.closeWriteChannel();
    if (!admitted || !in_family || !in_task || !child.waitForFinished(5000) ||
        child.exitCode() != 0 || child.readAllStandardOutput().trimmed() != "read" ||
        child.readAllStandardError().trimmed() != "child" ||
        WaitForSingleObject(event, 0) != WAIT_TIMEOUT || modifications != 1)
    {
        std::fputs("Qt channel, handle inheritance or job membership mismatch.\n", stderr);
        return false;
    }

    // An event is not a job. Admission must fail before any child can run.
    if (custody::start_qprocess(&child, event_value, true) || child.state() != QProcess::NotRunning) {
        std::fputs("Invalid job accepted.\n", stderr);
        return false;
    }

    if (!custody::start_qprocess(&child, reinterpret_cast<quintptr>(task), true)) {
        std::fputs("Could not reuse the settled process for a new job.\n", stderr);
        return false;
    }
    child.closeWriteChannel();
    if (!child.waitForFinished(5000) || WaitForSingleObject(event, 0) != WAIT_TIMEOUT) {
        std::fputs("Job admission failed to preserve the standard handle list.\n", stderr);
        return false;
    }

    // The caller's modifier remains installed when the guard leaves scope.
    const int before = modifications;
    child.start();
    child.closeWriteChannel();
    if (!child.waitForFinished(5000) || modifications != before + 1 ||
        WaitForSingleObject(event, 0) != WAIT_OBJECT_0)
    {
        std::fputs("Previous modifier or unrestricted Qt inheritance was not restored.\n", stderr);
        return false;
    }
    return true;
}

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    const auto args = app.arguments();
    if (args.size() == 3 && args[1] == QStringLiteral("--child")) {
        SetEvent(reinterpret_cast<HANDLE>(quintptr(args[2].toULongLong())));
        char bytes[16]{};
        if (std::fgets(bytes, sizeof(bytes), stdin)) {
            std::fputs(bytes, stdout);
        }
        std::fputs("child\n", stderr);
        return 0;
    }
    SECURITY_ATTRIBUTES inheritable{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    HANDLE event  = CreateEventW(&inheritable, TRUE, FALSE, nullptr);
    HANDLE family = CreateJobObjectW(nullptr, nullptr);
    HANDLE task   = CreateJobObjectW(nullptr, nullptr);
    const bool ok = family && task && event && exercise(family, task, event);
    if (task)   { CloseHandle(task);   }
    if (family) { CloseHandle(family); }
    if (event)  { CloseHandle(event);  }
    return ok ? 0 : 1;
}
