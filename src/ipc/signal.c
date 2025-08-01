#include "../include/signal.h"
#include "../include/process.h"
#include "../include/terminal.h"
#include "../include/scheduler.h"

// Initialize signal handling
void signal_init(void) {
    // Nothing to do for now
}

// Send a signal to a process
void signal_send(int pid, int sig) {
    process_t* proc = process_find_by_pid(pid);
    if (!proc) return;
    
    // Handle special signals
    switch (sig) {
        case SIGKILL:
            // Force terminate
            terminal_writestring("[SIGNAL] Killing process ");
            terminal_writestring(proc->name);
            terminal_writestring("\n");
            proc->state = PROCESS_STATE_TERMINATED;
            break;
            
        case SIGTERM:
            // Request termination
            terminal_writestring("[SIGNAL] Terminating process ");
            terminal_writestring(proc->name);
            terminal_writestring("\n");
            proc->state = PROCESS_STATE_TERMINATED;
            break;
            
        case SIGSTOP:
            // Stop process
            if (proc->state == PROCESS_STATE_RUNNING || 
                proc->state == PROCESS_STATE_READY) {
                terminal_writestring("[SIGNAL] Stopping process ");
                terminal_writestring(proc->name);
                terminal_writestring("\n");
                proc->state = PROCESS_STATE_BLOCKED;
            }
            break;
            
        case SIGCONT:
            // Continue process
            if (proc->state == PROCESS_STATE_BLOCKED) {
                terminal_writestring("[SIGNAL] Continuing process ");
                terminal_writestring(proc->name);
                terminal_writestring("\n");
                proc->state = PROCESS_STATE_READY;
            }
            break;
            
        case SIGINT:
            // Interrupt - for now just terminate
            terminal_writestring("[SIGNAL] Interrupting process ");
            terminal_writestring(proc->name);
            terminal_writestring("\n");
            proc->state = PROCESS_STATE_TERMINATED;
            break;
            
        default:
            terminal_writestring("[SIGNAL] Unknown signal\n");
            break;
    }
}

// Handle pending signals for current process
void signal_handle(void) {
    // Simple implementation for now
    // In real OS, would check signal mask and pending signals
}