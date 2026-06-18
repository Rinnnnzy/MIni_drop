#pragma once

namespace dropd {

// Daemonize forks the process into the background, detaches it from the
// controlling terminal, resets umask/cwd, and redirects stdio to /dev/null.
// Call this at the very beginning of main() before any threads are created.
void Daemonize();

} // namespace dropd
