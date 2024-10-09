#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdbool.h>
#include <ucontext.h>
#include <signal.h>
#include "./linkedlist-job.h"

#define TERMINATE_NORMAL 0 /**< The wstatus standing for process terminated normally. */
#define STOP_SIGNAL 1      /**< The wstatus standing for process stopped by a signal. */
#define TERMINATE_SIGNAL 2 /**< The wstatus standing for process terminated by a signal. */
#define RUNNING 3          /**< The wstatus standing for process still running (for nohang). */

#define S_SIGSTOP_STR "0" /**< The string of the signal sent to stop a process. (used for shell) */
#define S_SIGCONT_STR "1" /**< The string of the signal sent to continue a previously stopped process. (used for shell) */
#define S_SIGTERM_STR "2" /**< The string of the signal sent to terminate a process. (used for shell) */

#define S_SIGSTOP 0 /**< The signal sent to stop a process. */
#define S_SIGCONT 1 /**< The signal sent to continue a previously stopped process. */
#define S_SIGTERM 2 /**< The signal sent to terminate a process. */

/**
 * The next pid can be assigned to a new process.
 */
static int thread_id;

/**
 * Create a process with a given process as its parent.
 *
 * @param parent The pointer to the parent process.
 * @return The pointer to the Process we created.
 */
struct Process *k_process_create(struct Process *parent);

/**
 * Kill a Process with the given signal.
 *
 * @param process The process we want to kill with the signal.
 * @param signal The signal we are sending to the process.
 * @return The status of whether the killing is successful.
 */
int k_process_kill(struct Process *process, int signal);

/**
 * Kill all processes with a given signal.
 *
 * @param signal The signal we are sending to all processes.
 */
void k_kill_all(int signal);

/**
 * Set the idle param to false, i.e. we are leaving the idle context.
 */
bool k_set_idle();

/**
 * Get the current available pid and increment it by 1.
 * @return The next pid we can assign to a new process.
 */
int k_get_next_pid();

/**
 * Look up a process from our job queue.
 * @param thread_id The pid we are looking for.
 * @return The process we found with this pid, NULL on failed.
 */
struct Process *k_lookup_process(int thread_id);

/**
 * Create a process with a certain priority (nice value) and a process as its parent.
 * @param parent The process we need to set as this process's parent.
 * @param priority The nice value for this process.
 * @return The process created.
 */
struct Process *k_process_create_with_priority(struct Process *parent, int priority);

/**
 * Clean up a process from our job queue and free it.
 * @param process The process to clean up.
 */
void k_process_cleanup(struct Process *process);

/**
 * Reap a zombie process from our job queue with its pid.
 * @param pid The pid of the process to reap.
 */
void k_reap_zombie(int pid);

/**
 * For abstraction sake, get the TERMINAL_NORMAL wstatus from outside.
 * @return The TERMINAL_NORMAL defined.
 */
int k_get_terminal_normal_status();

/**
 * For abstraction sake, get the STOP_SIGNAL wstatus from outside.
 * @return The STOP_SIGNAL defined.
 */
int k_get_stop_signal_status();

/**
 * For abstraction sake, get the TERMINAL_SIGNAL wstatus from outside.
 * @return The TERMINAL_SIGNAL defined.
 */
int k_get_terminal_signal_status();

/**
 * For abstraction sake, get the RUNNING wstatus from outside.
 * @return The RUNNING defined.
 */
int k_get_running_status();

/**
 * For abstraction sake, get the S_SIGSTOP signal from outside.
 * @return The S_SIGSTOP signal defined.
 */
int k_get_sigstop_signal();

/**
 * For abstraction sake, get the S_SIGCONT signal from outside.
 * @return The S_SIGCONT signal defined.
 */
int k_get_sigcont_signal();

/**
 * For abstraction sake, get the S_SIGTERM signal from outside.
 * @return The S_SIGTERM signal defined.
 */
int k_get_sigterm_signal();

/**
 * Initiate the to_exit stored for p_exit_process().
 */
void k_initiate_to_exit();

/**
 * Set to_exit as a process for p_exit_process().
 * @param process The process to set as to_exit.
 */
void k_set_to_exit(struct Process *process);

/**
 * Get the process stored in to_exit.
 * @return The process stored inside to_exit.
 */
struct Process *k_get_to_exit();

/**
 * The helper function we used to get S_SIGSTOP_STR for shell use.
 * @return The S_SIGSTOP_STR we defined.
 */
char *k_get_sigstop_str();

/**
 * The helper function we used to get S_SIGCONT_STR for shell use.
 * @return The S_SIGCONT_STR we defined.
 */
char *k_get_sigcont_str();

/**
 * The helper function we used to get S_SIGTERM_STR for shell use.
 * @return The S_SIGTERM_STR we defined.
 */
char *k_get_sigterm_str();