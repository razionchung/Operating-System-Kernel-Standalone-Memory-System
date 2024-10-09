#include <signal.h>   // sigaction, sigemptyset, sigfillset, signal
#include <stdio.h>    // dprintf, fputs, perror
#include <stdlib.h>   // malloc, free
#include <sys/time.h> // setitimer
#include <ucontext.h> // getcontext, makecontext, setcontext, swapcontext
#include <unistd.h>   // read, usleep, write
#include "linkedlist-job.h"
// #include <valgrind/valgrind.h>
#include <stdbool.h>

#define NOT_WAITING -2 /**< the indicator we used to indicate the parent is not waiting for it's child (stored inside to_wait). */

/**
 * The helper function we used to set the current stack.
 * @param The pointer to the stack we are setting.
 */
void setStack(stack_t *stack);

/**
 * The helper function we used to set the relavent properties of a context.
 * @param ucp The pointer to the context we are setting.
 * @param func The function run on the context ucp.
 * @param thread indicating whether this is shell make context.
 */
void s_makeContext(ucontext_t *ucp, void (*func)(), int thread);

/**
 * The helper function we used to free the current stack.
 */
void freeStacks(void);

/**
 * The function we used to check if there is active job in the priority queue while checking if any of sleep jobs should be awaken.
 * @param queue The pointer to the priority queue we are looking at.
 * @return True if the current priority queue has an active job or we wake up some process in any of the priority queues.
 */
bool s_check_active(struct LinkedList *queue);

/**
 * The helper function we used to set up signal handler for shell and zombieContext (run after a process finished) and idleContext (for sleep).
 */
void s_setup();

/**
 * The helper function we used to officially swap to the shell context.
 */
void s_initiate();

/**
 * The helper function we used to initialize our three priority job queues.
 */
void s_initiate_priorities();

/**
 * The function we used to insert a job into a certain priority queue.
 * @param priority The priority (nice value) we are trying to insert.
 * @param pcb The pointer to the PCB of the process we are trying to insert.
 */
void s_insert(int priority, struct Process *pcb);

/**
 * The helper function to initiate the shell context and stored inside its PCB.
 * @param argc The number of arguments passed in.
 * @param argv The arguments passed in.
 * @param process The process PCB for the shell.
 */
void s_initiate_shell_context(int argc, char *argv[], struct Process *process);

/**
 * The helper function used to set the current PCB. (for Abstraction)
 * @param process The process to set as currentPCB.
 */
void s_set_current(struct Process *process);

/**
 * The helper function used to get the current PCB. (for Abstraction)
 * @return The pointer to the currentPCB.
 */
struct Process *s_get_current();

/**
 * The helper function used to get a certain priority job queue.
 * @param priority The priority (nice value) we are trying to get.
 * @return The pointer to the priority job LinkedList we want.
 */
struct LinkedList *s_get_priority(int priority);

/**
 * The helper function we used to swap the current context with scheduler.
 */
void s_swap();

/**
 * The helper function we used to set the current context as scheduler.
 */
void s_set();

/**
 * The helper function we used to get the Zombie Context (context after a process finished) for abstraction.
 * @return The zombie context.
 */
ucontext_t *s_get_zombie_context();

/**
 * The helper function used to set the status of the current running Process.
 * @param status The status we need to set for the current PCB.
 */
void s_set_status(int status);

/**
 * The helper function we used to print all the jobs in the order of nice value. (used for 'ps' and'jobs')
 */
void s_print_all_jobs();

/**
 * The helper function we used to get the Scheduler Context for abstraction.
 * @return The scheduler context.
 */
ucontext_t *s_get_scheduler_context();

/**
 * The helper function we used to get the current time(ticks) for abstraction.
 * @return The current time.
 */
int s_get_time();

/**
 * The helper function we used to set the current status of if we are in the idleContext.
 */
void s_set_idle();