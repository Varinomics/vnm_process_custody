#pragma once

#include <QtCore/qglobal.h>

#include <cstdint>
#include <memory>

QT_BEGIN_NAMESPACE
class QProcess;
QT_END_NAMESPACE

namespace vnm::process_custody {

#ifdef Q_OS_WIN
/// Adds borrowed jobs and, when requested, Qt's stdio handles at child creation.
/// Keep the guard through a single QProcess::start(). It restores the previous
/// argument modifier afterwards. An existing modifier may adjust ordinary
/// STARTUPINFO fields or creation flags; the guard owns the extended attributes.
class Qprocess_creation_admission
{
public:
    Qprocess_creation_admission(
        QProcess* process, std::uintptr_t family_job, std::uintptr_t task_job, bool restrict_standard_handles);
    ~Qprocess_creation_admission();

    /// Start the process only if the requested creation policy was prepared.
    bool established() const;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};
#endif

/// Start the configured process with its Qt channels and ordinary Qt wait
/// ownership. A Windows job and stdio restriction apply at creation. Pass
/// zero and false on other systems. Qt reports asynchronous startup errors.
bool start_qprocess(QProcess* process, std::uintptr_t task_job, bool restrict_standard_handles);

} // namespace vnm::process_custody
