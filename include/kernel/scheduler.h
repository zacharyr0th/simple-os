#ifndef SCHEDULER_H
#define SCHEDULER_H

// Scheduler functions
void scheduler_init(void);
void scheduler_enable(void);
void scheduler_disable(void);
void schedule(void);
void scheduler_tick(void);
void scheduler_stats(void);

#endif // SCHEDULER_H