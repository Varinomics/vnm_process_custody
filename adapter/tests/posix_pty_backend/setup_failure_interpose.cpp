// Darwin dyld binds these replacements only in the injected test image.
#define DYLD_INTERPOSE(replacement, original) \
    __attribute__((used, section("__DATA,__interpose"))) \
    static const struct { const void* replacement; const void* original; } \
        interpose_##replacement = {reinterpret_cast<const void*>(&replacement), \
            reinterpret_cast<const void*>(&original)};
#include <fcntl.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <util.h>

#include <cerrno>
#include <cstdarg>
#include <cstdlib>
#include <cstring>

namespace {
int master_fd = -1;
int slave_fd  = -1;

bool fail(const char* operation)
{
    const char* selected = std::getenv("VNM_TEST_PTY_FAILURE");
    if (selected && std::strcmp(selected, operation) == 0) {
        errno = EACCES;
        return true;
    }
    return false;
}

int failing_openpty(int* master, int* slave, char* name, termios* term, winsize* size)
{
    if (fail("openpty")) {
        return -1;
    }
    const int result = openpty(master, slave, name, term, size);
    if (result == 0) {
        master_fd = *master;
        slave_fd  = *slave;
    }
    return result;
}

int failing_fcntl(int fd, int command, ...)
{
    if (command == F_GETFD || command == F_GETFL) {
        return fcntl(fd, command);
    }
    va_list args;
    va_start(args, command);
    const int value = va_arg(args, int);
    va_end(args);
    if (command == F_SETFD &&
        ((fd == master_fd && fail("fcntl(master)")) || (fd == slave_fd && fail("fcntl(slave)"))))
    {
        return -1;
    }
    return fcntl(fd, command, value);
}

int failing_ioctl(int fd, unsigned long command, ...)
{
    if ((fd == master_fd && command == TIOCSWINSZ && fail("ioctl(TIOCSWINSZ)")) ||
        (fd == slave_fd && command == TIOCSCTTY && fail("ioctl(TIOCSCTTY)")))
    {
        return -1;
    }
    va_list args;
    va_start(args, command);
    void* value = va_arg(args, void*);
    va_end(args);
    return ioctl(fd, command, value);
}

int failing_sigaction(int signal, const struct sigaction* action, struct sigaction* previous)
{
    if (slave_fd >= 0 &&
        ((signal == SIGHUP && fail("sigaction(SIGHUP)")) ||
         (signal == SIGTTOU && fail("sigaction(SIGTTOU)"))))
    {
        return -1;
    }
    return sigaction(signal, action, previous);
}

int failing_fstat(int fd, struct stat* info)
{
    if (fd == slave_fd && fail("fstat")) {
        return -1;
    }
    return fstat(fd, info);
}

pid_t failing_getsid(pid_t pid)
{
    if (slave_fd >= 0 && fail("getsid")) {
        return -1;
    }
    if (slave_fd >= 0 && fail("setsid")) {
        return getpid() + 1;
    }
    return getsid(pid);
}

pid_t failing_setsid()
{
    if (slave_fd >= 0 && fail("setsid")) {
        return -1;
    }
    return setsid();
}

int clobbering_close(int fd)
{
    const int result = close(fd);
    errno = EBADF;
    return result;
}

DYLD_INTERPOSE(failing_openpty, openpty)
DYLD_INTERPOSE(failing_fcntl, fcntl)
DYLD_INTERPOSE(failing_ioctl, ioctl)
DYLD_INTERPOSE(failing_sigaction, sigaction)
DYLD_INTERPOSE(failing_fstat, fstat)
DYLD_INTERPOSE(failing_getsid, getsid)
DYLD_INTERPOSE(failing_setsid, setsid)
DYLD_INTERPOSE(clobbering_close, close)
}
