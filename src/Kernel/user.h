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

/**
 * The int we use to store the pid the current Process is waiting on.
 */
static int waiting_on;

/**
 * The pointer we used to store the waiting status.
 */
static int *waiting_wstatus;

/**
 * Forks a new thread that retains most of the attributes of the parent thread.
 * @param func The function to execute inside this PCB.
 * @param argv The arguments passed in when executing func.
 * @param fd0 The file descriptor for the input file.
 * @param fd1 The file descriptor for the output file.
 * @return The pid of the child thread on success, or -1 on error.
 */
pid_t p_spawn(void (*func)(), char *argv[], int fd0, int fd1);

/**
 * Forks a new thread that retains most of the attributes of the parent thread with a certain priority (nice value).
 * @param func The function to execute inside this PCB.
 * @param argv The arguments passed in when executing func.
 * @param fd0 The file descriptor for the input file.
 * @param fd1 The file descriptor for the output file.
 * @param priority The priority of the thread created.
 * @return The pid of the child thread on success, or -1 on error.
 */
pid_t p_spawn_with_priority(void (*func)(), char *argv[], int fd0, int fd1, int priority);

/**
 * Set the calling thread as blocked (if nohang is false) until a child of the calling thread changes state.
 * @param pid The pid the calling thread is trying to wait on.
 * @param wstatus The status pointer to store the wstatus.
 * @param nohang Indicates if the calling thread should be block-waiting on the child.
 * @return The pid of the child which has changed state on success, or -1 on error.
 */
pid_t p_waitpid(pid_t pid, int *wstatus, bool nohang);

/**
 * The helper function we used to invoke the kernel shell initiation and initiate the shell.
 * @param func The function to run (which is just shell).
 * @param argc The number of arguments passed in.
 * @param argv The arguments passed in.
 * @return The pid of the thread created.
 */
pid_t p_initiate_shell(void (*func)(), int argc, char *argv[]);

/**
 * Function used to send a thread to a running thread.
 * @param pid The pid of the thread we are trying to send a signal.
 * @param sig The signal we are trying to send.
 * @return 0 on success, -1 on error.
 */
int p_kill(pid_t pid, int sig);

/**
 * The function we used to exit the current thread unconditionally and check/log for zombie/orphan status.
 */
void p_exit(void);

/**
 * The function we used to exit a certain thread (pid stored in to_exit) unconditionally and check/log for zombie/orphan status.
 */
void p_exit_process();

/**
 * A helper function to check if the child terminates normally(calling p_exit).
 * @param status The status pointer we are looking at.
 * @return True if the child terminates normally, False otherwise.
 */
bool W_WIFEXITED(int *status);

/**
 * A helper function to check if the child is stopped by a signal.
 * @param status The status pointer we are looking at.
 * @return True if the child is stopped by a signal, False otherwise.
 */
bool W_WIFSTOPPED(int *status);

/**
 * A helper function to check if the child is terminated by a signal.
 * @param status The status pointer we are looking at.
 * @return True if the child is terminated by a signal, False otherwise.
 */
bool W_WIFSIGNALED(int *status);

/**
 * The function used to set the priority of the thread with pid to some priority.
 * @param pid The pid of the thread we want to set its priority.
 * @param priority The new priority for this thread.
 * @return 0 on success, -1 on error.
 */
int p_nice(pid_t pid, int priority);

/**
 * The function used to set the calling process to blocked until ticks of the system clock elapse, and then sets the thread to running.
 * @param ticks The number of ticks to sleep.
 */
void p_sleep(unsigned int ticks);

/**
 * The function to busy wait (used for 'busy') command.
 */
void p_busy_wait();

/**
 * The function used to spawn a Zombie child.
 */
void p_zombie_child();

/**
 * The function used to spawn an orphan child.
 */
void p_orphan_child();

/**
 * The function to print out all the jobs by the order of priority.
 */
void p_print_all_jobs();

/**
 * The helper function to add a thread into the background.
 * @param pid The pid for the thread to add into the background.
 */
void p_add_background_job(int pid);

/**
 * The helper function to add a thread into the stopped queue.
 * @param pid The pid for the thread to add into the stopped queue.
 */
void p_add_stop_job(int pid);

/**
 * The helper function to remove a thread from the background.
 * @param pid The pid for the thread to remove from the background queue.
 */
void p_remove_background_job(int pid);

/**
 * The helper function to remove a thread from the stopped queue.
 * @param pid The pid for the thread to remove from the stopped queue.
 */
void p_remove_stop_job(int pid);

/**
 * The helper function to search for a thread and remove this thread.
 * @param pid The pid for the thread to search and remove.
 */
void p_search_and_remove(int pid);

/**
 * The helper function to search for the most recent background/stopped job.
 * @return The pid of the most recent background/stopped job.
 */
int p_search_most_recent();

/**
 * The helper function to search for the most recent stopped job.
 * @return The pid of the most recent stopped job.
 */
int p_search_most_recent_stop();

/**
 * The helper function to search if the thread with pid is in the background.
 * @param pid The pid to search in the background.
 * @return The PCB of the thread if this pid exists in the background queue, NULL otherwise.
 */
struct Process *p_search_bg(int pid);

/**
 * A helper function to get the S_SIGSTOP we defined for abstraction.
 * @return The S_SIGSTOP we defined.
 */
int p_get_sigstop_signal();

/**
 * A helper function to get the S_SIGCONT we defined for abstraction.
 * @return The S_SIGCONT we defined.
 */
int p_get_sigcont_signal();

/**
 * A helper function to get the S_SIGTERM we defined for abstraction.
 * @return The S_SIGTERM we defined.
 */
int p_get_sigterm_signal();

/**
 * A helper function to initiate the to_exit PCB stored for p_exit_process() as NULL.
 */
void p_initiate_to_exit();

/**
 * A helper function to get the current PCB for abstraction sake.
 * @return The PCB for the currently running thread.
 */
struct Process *p_get_current();

/**
 * A helper function to invoke scheduler initialization inside kernel/scheduler for abstraction.
 */
void p_initiate_priorities();

/**
 * A helper function to invoke setup inside kernel/scheduler for abstraction.
 */
void p_setup();

/**
 * A helper function to invoke initialization inside kernel/scheduler for abstraction.
 */
void p_initiate();

/**
 * A modified version of p_spawn to take in input from the terminal. Forks a new thread that retains most of the attributes of the parent thread.
 * @param func The function to execute inside this PCB.
 * @param argv The arguments passed in when executing func.
 * @param fd0 The file descriptor for the input file.
 * @param fd1 The file descriptor for the output file. (-1 if not specified)
 * @param priority The priority of the thread created.
 * @param actual_input The pointer to the modified input from the terminal.
 * @return The pid of the child thread on success, or -1 on error.
 */
pid_t p_spawn_with_input(void (*func)(), char *argv[], int fd0, int fd1, char **actual_input);

/**
 * A helper function to redirect k_lookup_process for abstraction sake.
 * @param pid The pid of the thread we are searching for.
 * @return The pointer to the PCB of the thread we want, -1 on error.
 */
struct Process *p_lookup_process(pid_t pid);

/**
 * The function we used to deal with 'zombify' command, which spawns a zombie child.
 */
void p_zombify();

/**
 * The function we used to deal with 'orphanify' command, which spawns an orphan child.
 */
void p_orphanify();

/**
 * The function used to p_spawn processes for 'kill' command. In which we kill processes with some signal.
 * @param sig The signal we are sending.
 * @param pid The process we are sending the signal from.
 */
void p_run_kill(int sig, pid_t pid);

/**
 * The function used to p_spawn processes for 'mv' command. In which we rename a file in our filesystem.
 * @param source The original name of the file descriptor.
 * @param dest The target name of the file descriptor.
 */
void p_run_mv_fs(char *source, char *dest);

/**
 * The function used to p_spawn processes for 'chmod' command. In which we modify the access permission to a file in our filesystem.
 * @param filename The name of the file descriptor.
 * @param perm The target permission of the file descriptor.
 */
void p_run_chmod_fs(char *filename, char *perm);

/**
 * The function used to p_spawn processes for 'cp' command. In which we copy a file into a new file.
 * @param source The name of the file descriptor to copy from.
 * @param des The name of the file descriptor to copy into.
 * @param from_host Indicator for whether the file is in the host or our own filesystem.
 */
void p_run_cp_fs(char *source, char *des, int from_host);

/**
 * The function used to p_spawn processes for 'touch' command. In which we touch a file in our filesystem.
 * It creates the file if it does not exist, and it doesn't do anything otherwise.
 */
void p_run_touch_fs();

/**
 * The function used to p_spawn processes for 'ls' command. In which we list out details of all files in our filesystem.
 * This is the special version when we take in inputs for 'ls', and we only list out the details for the files specified.
 */
void p_run_f_ls_list();

/**
 * The function used to p_spawn processes for 'ls' command. In which we list out details of all files in our filesystem.
 * This is the special version when we don't take in input, and we just list out the details for all the files.
 */
void p_run_f_ls_null();

/**
 * The function used to p_spawn processes for 'rm' command. In which we remove a file from our filesystem.
 * The inputs will be passed from our struct.
 */
void p_run_rm_fs();

/**
 * The function used to p_spawn processes for 'echo' command. In which we does basically the echo(1) behavior as in VM.
 * The inputs will be passed from our struct.
 */
void p_run_echo();

/**
 * The function used to p_spawn processes for 'cat' command. In which we does basically the 'cat' behavior as in bash.
 */
void p_run_cat_fs();

/**
 * The helper function we used to redirect into kernel and get S_SIGSTOP_STR we defined inside of kernel for shell use.
 * @return The S_SIGSTOP_STR we defined inside of kernel.
 */
char *p_get_sigstop_str();

/**
 * The helper function we used to redirect into kernel and get S_SIGCONT_STR we defined inside of kernel for shell use.
 * @return The S_SIGSTOP_STR we defined inside of kernel.
 */
char *p_get_sigcont_str();

/**
 * The helper function we used to redirect into kernel and get S_SIGTERM_STR we defined inside of kernel for shell use.
 * @return The S_SIGSTOP_STR we defined inside of kernel.
 */
char *p_get_sigterm_str();

void p_print(int fd, char *string, int length);