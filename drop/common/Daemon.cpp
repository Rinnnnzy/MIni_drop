#include "Daemon.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstdlib>

namespace dropd {

void Daemonize() {
    // First fork: parent exits so the child is not a process group leader.
    pid_t pid = ::fork();
    if (pid < 0) ::exit(1);
    if (pid > 0) ::exit(0);

    // Create a new session and detach from the controlling terminal.
    ::setsid();

    // Second fork: prevent the daemon from reacquiring a controlling terminal.
    pid = ::fork();
    if (pid < 0) ::exit(1);
    if (pid > 0) ::exit(0);

    ::umask(0);
    ::chdir("/");

    // Redirect stdin/stdout/stderr to /dev/null.
    int fd = ::open("/dev/null", O_RDWR);
    if (fd >= 0) {
        ::dup2(fd, STDIN_FILENO);
        ::dup2(fd, STDOUT_FILENO);
        ::dup2(fd, STDERR_FILENO);
        if (fd > STDERR_FILENO) ::close(fd);
    }
}

} // namespace dropd
