#ifndef SIGNAL_H
#define SIGNAL_H

#include <stdint.h>

// Signal numbers
#define SIGINT  2   // Interrupt (Ctrl+C)
#define SIGKILL 9   // Kill (cannot be caught)
#define SIGTERM 15  // Terminate
#define SIGSTOP 19  // Stop process
#define SIGCONT 18  // Continue process

// Signal handler type
typedef void (*sighandler_t)(int);

// Special signal handlers
#define SIG_DFL ((sighandler_t)0)  // Default handler
#define SIG_IGN ((sighandler_t)1)  // Ignore signal

// Signal functions
int sys_kill(int pid, int sig);
sighandler_t sys_signal(int signum, sighandler_t handler);

// Internal signal handling
void signal_init(void);
void signal_send(int pid, int sig);
void signal_handle(void);

#endif