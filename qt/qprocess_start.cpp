#include <vnm_process_custody/qprocess_start.h>

#include <QProcess>

#ifdef Q_OS_WIN
#  include "process_creation_attributes_win.h"
#  include <QPointer>
#endif

namespace vnm::process_custody {

#ifdef Q_OS_WIN
class Qprocess_creation_admission::Impl
{
public:
    Impl(QProcess* process, HANDLE family_job, HANDLE task_job, bool restrict_handles)
    {
        if (m_attributes.prepare(family_job, task_job, restrict_handles)) {
            return;
        }
        m_established = true;
        if (!m_attributes.list()) {
            return;
        }
        m_process  = process;
        m_previous = process->createProcessArgumentsModifier();
        process->setCreateProcessArgumentsModifier([this, restrict_handles](QProcess::CreateProcessArguments* args) {
            if (m_previous) {
                m_previous(args);
            }
            if (args->flags & EXTENDED_STARTUPINFO_PRESENT) {
                // A pre-existing attribute list cannot be merged or dropped.
                args->applicationName = L"";
                return;
            }
            m_startup.StartupInfo = *args->startupInfo;
            if (restrict_handles) {
                const auto& info = m_startup.StartupInfo;
                m_handles[0] = info.hStdInput;
                m_handles[1] = info.hStdOutput;
                m_handles[2] = info.hStdError;
                if (!(info.dwFlags & STARTF_USESTDHANDLES) || !m_attributes.set_handles(m_handles)) {
                    // Qt has opened its pipes at this point. Refuse creation
                    // if their inheritance policy cannot be applied.
                    args->applicationName = L"";
                    return;
                }
            }
            m_startup.StartupInfo.cb = sizeof(m_startup);
            m_startup.lpAttributeList = m_attributes.list();
            args->startupInfo = &m_startup.StartupInfo;
            args->flags |= EXTENDED_STARTUPINFO_PRESENT;
        });
    }

    ~Impl()
    {
        if (m_process) {
            m_process->setCreateProcessArgumentsModifier(m_previous);
        }
    }

    bool established() const { return m_established; }

private:
    bool                                    m_established = false;
    QPointer<QProcess>                      m_process;
    QProcess::CreateProcessArgumentModifier m_previous;
    Process_creation_attributes             m_attributes;
    STARTUPINFOEXW                          m_startup{};
    HANDLE                                  m_handles[3]{};
};

Qprocess_creation_admission::Qprocess_creation_admission(
    QProcess* process, std::uintptr_t family_job, std::uintptr_t task_job, bool restrict_standard_handles)
:
    m_impl(std::make_unique<Impl>(process,
        reinterpret_cast<HANDLE>(family_job), reinterpret_cast<HANDLE>(task_job), restrict_standard_handles))
{}

Qprocess_creation_admission::~Qprocess_creation_admission() = default;

bool Qprocess_creation_admission::established() const { return m_impl->established(); }
#endif

bool start_qprocess(QProcess* process, std::uintptr_t task_job, bool restrict_standard_handles)
{
    if (process->state() != QProcess::NotRunning) {
        return false;
    }
#ifdef Q_OS_WIN
    Qprocess_creation_admission admission(process, 0, task_job, restrict_standard_handles);
    if (!admission.established()) {
        return false;
    }
#else
    if (task_job || restrict_standard_handles) {
        return false;
    }
#endif
    process->start();
    return process->state() != QProcess::NotRunning;
}

} // namespace vnm::process_custody
