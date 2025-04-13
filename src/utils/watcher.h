#pragma once

// RunWatcher is the main entry for a watcher process.
// Usage: ChickenJockey.exe --watchdog <A|B> [peerPID]
// If peerPID is provided, it will be used to monitor the peer; otherwise the watcher will use its role
// to relaunch the peer process if necessary.
int RunWatcher(int argc, char* argv[]);
