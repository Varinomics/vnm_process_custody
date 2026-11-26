#pragma once

#include <windows.h>

#include <span>
#include <vector>

namespace vnm::process_custody {

class Process_creation_attributes
{
public:
    Process_creation_attributes() = default;
    Process_creation_attributes(const Process_creation_attributes&) = delete;
    Process_creation_attributes& operator=(const Process_creation_attributes&) = delete;

    ~Process_creation_attributes()
    {
        if (m_list) {
            DeleteProcThreadAttributeList(m_list);
        }
    }

    // Win32 borrows both lists through CreateProcess. Jobs live here; the
    // caller owns the handle array, which may only become available at launch.
    const char* prepare(HANDLE family_job, HANDLE child_job, bool list_handles)
    {
        size_t count = 0;
        if (family_job) {
            m_jobs[count++] = family_job;
        }
        if (child_job && child_job != family_job) {
            m_jobs[count++] = child_job;
        }
        DWORD attributes = DWORD(count != 0) + DWORD(list_handles);
        if (!attributes) {
            return nullptr;
        }
        SIZE_T bytes = 0;
        InitializeProcThreadAttributeList(nullptr, attributes, 0, &bytes);
        if (!bytes) {
            return "InitializeProcThreadAttributeList(size)";
        }
        m_storage.resize(bytes);
        auto* list = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(m_storage.data());
        if (!InitializeProcThreadAttributeList(list, attributes, 0, &bytes)) {
            return "InitializeProcThreadAttributeList";
        }
        m_list = list;
        if (count && !UpdateProcThreadAttribute(list, 0, PROC_THREAD_ATTRIBUTE_JOB_LIST,
                m_jobs, count * sizeof(HANDLE), nullptr, nullptr))
        {
            return "UpdateProcThreadAttribute(JOB_LIST)";
        }
        return nullptr;
    }

    bool set_handles(std::span<HANDLE> handles)
    {
        return UpdateProcThreadAttribute(m_list, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
            handles.data(), handles.size_bytes(), nullptr, nullptr);
    }

    LPPROC_THREAD_ATTRIBUTE_LIST list() const { return m_list; }

private:
    HANDLE                       m_jobs[2]{};
    std::vector<unsigned char>   m_storage;
    LPPROC_THREAD_ATTRIBUTE_LIST m_list = nullptr;
};

} // namespace vnm::process_custody
