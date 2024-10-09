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
#include "./parser.h"
#include "parser.h"

/**
 * The current PCB pointer.
 */
static struct Process *currentPCB;

#define ACTIVE_STAT 0 /**< the process is actively running and can be scheduled. */
#define PAUSED_STAT 1 /**< the process is paused for waiting on children and cannot be scheduled. */
#define DONE_STAT 2   /**< the process is already terminated and didn't get reaped as a zombie. */
#define STOP_STAT 3   /**< the process is stopped by signal and can be continued by SIGCONT. */

#ifndef PROCESS
#define PROCESS
/**
 * The PCB struct used for our OS.
 */
struct Process
{
    ucontext_t *context;          /**< the ucontext of the PCB */
    int thread_process_id;        /**< the pid of the PCB */
    int parent_process_id;        /**< the pid of this PCB's parent */
    struct Process *parent;       /**< PCB pointer to its parent */
    struct LinkedList *childrens; /**< the Linkedlist of the PCB's children */
    int priority;                 /**< the priority (nice value) of this PCB */
    int input_descriptor;         /**< the input descriptor for this Process */
    int output_descriptor;        /**< the output descriptor for this Process */
    int num_children;             /**< the number of children for this Process */
    int status;                   /**< the status for this PCB */
    bool signal_terminated;       /**< record if this process is terminated by signal */
    int group_id;                 /**< Process group id (unused) */
    int awake_time;               /**< the time this process should awake (only for sleep) */
    char *cmd;                    /**< the command for this Process (for logging) */
    pid_t to_wait;                /**< the pid this Process is waiting on */
    int recorded;                 /**< the recorded status (modified when it's parent called waitpid with hang) */
    bool is_orphan;               /**< record whether it's an orphan */
    int bg_time;                  /**< record when was this Process stored in background */
    int stop_time;                /**< record when was this Process stopped */
    bool fg_cont;                 /**< record if it should be continued at foreground */
    char **argv;                  /**< The modified arguments we are taking into specific functions */
};

#endif

#ifndef ENTRY
#define ENTRY
/**
 * The Node struct used for our Linkedlist.
 */
struct Entry
{
    struct Process *process; /**< the Process stored inside this node*/
    struct Entry *prev;      /**< the pointer to the previous node */
    struct Entry *next;      /**< the pointer to the next node */
};
#endif

#ifndef LIST
#define LIST
/**
 * The Linkedlist struct.
 */
struct LinkedList
{
    struct Entry *head; /**< the pointer to the head node of this linkedlist */
    struct Entry *tail; /**< the pointer to the tail node of this linkedlist */
};
#endif

/**
 * Insert a process into the end of the linkedlist.
 *
 * @param list The pointer to the linkedlist to insert into.
 * @param process The pointer to process we want to insert.
 */
void insert_end(struct LinkedList *list, struct Process *process);

/**
 * Free a process completely from our memory.
 *
 * @param process The pointer to process we want to insert.
 */
void free_process(struct Process *process);

/**
 * Search a linkedlist for the node containing the process we want.
 *
 * @param list The pointer to the list we want to search in.
 * @param id The pid of the Process we are searching for.
 * @return The node entry containing the process we are looking for.
 */
struct Entry *search_list(struct LinkedList *list, int id);

/**
 * Delete a node containing the process we want from a LinkedList.
 *
 * @param list The pointer to the list we want to delete the node from.
 * @param id The pid of the Process we are deleting.
 * @return The node entry containing the process we are looking for.
 */
struct Process *delete_node(struct LinkedList *list, int id);

/**
 * Print out a LinkedList.
 *
 * @param list The pointer to the list we want to print.
 */
void print_list(struct LinkedList *list);

/**
 * Retrieve the process contained in the last node from a LinkedList.
 *
 * @param list The pointer to the list we want to retrieve the node from.
 * @return The process contained in the last node from this LinkedList.
 */
struct Process *retrieve_latest(struct LinkedList *list);

/**
 * Retrieve the process contained in the first node from a LinkedList.
 *
 * @param list The pointer to the list we want to retrieve the node from.
 * @return The process contained in the first node from this LinkedList.
 */
struct Process *poll(struct LinkedList *list);

/**
 * Completely free all entries inside a list and the Linkedlist itself.
 *
 * @param list The pointer to the list we want to free.
 */
void free_list(struct LinkedList *list);

/**
 * Set the process containing in a node entry as some orphan status.
 *
 * @param e The node containing the process we are setting to some orphan status.
 * @param orphan The orphan status we are setting.
 */
void set_orphan(struct Entry *e, bool orphan);
