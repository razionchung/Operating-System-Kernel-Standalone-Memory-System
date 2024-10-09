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
#include "kernel.h"
#include "scheduler.h"
#include "linkedlist-job.h"
#include "user.h"
#include "logger.h"
#include "shell.h"

#define S_SIGSTOP 0
#define S_SIGCONT 1
#define S_SIGTERM 2
static int thread_id = 1;

static struct Process *to_exit;
struct Process *k_process_create(struct Process *parent)
{
    struct Process *child = malloc(sizeof(struct Process));
    child->context = parent->context;
    child->recorded = -1;
    child->parent = parent;
    child->childrens = malloc(sizeof(struct LinkedList));
    child->childrens->head = NULL;
    child->childrens->tail = NULL;
    child->parent_process_id = parent->thread_process_id;
    child->thread_process_id = k_get_next_pid();
    child->group_id = parent->group_id;
    child->num_children = 0;
    child->is_orphan = false;
    parent->num_children++;
    child->priority = 0;
    child->status = ACTIVE_STAT;
    child->signal_terminated = false;
    child->awake_time = -1;
    child->to_wait = -2;
    s_insert(0, child);
    insert_end(parent->childrens, child);
    return child;
}

struct Process *k_process_create_with_priority(struct Process *parent, int priority)
{
    struct Process *child = malloc(sizeof(struct Process));
    child->context = parent->context;
    child->recorded = -1;
    child->parent = parent;
    child->childrens = malloc(sizeof(struct LinkedList));
    child->childrens->head = NULL;
    child->childrens->tail = NULL;
    child->parent_process_id = parent->thread_process_id;
    child->thread_process_id = k_get_next_pid();
    child->group_id = parent->group_id;
    child->num_children = 0;
    child->is_orphan = false;
    parent->num_children++;
    child->priority = priority;
    child->status = ACTIVE_STAT;
    child->signal_terminated = false;
    child->awake_time = -1;
    child->to_wait = -2;
    s_insert(priority, child);
    insert_end(parent->childrens, child);
    return child;
}

int k_get_next_pid()
{
    thread_id++;
    return thread_id - 1;
}
// not done
int k_process_kill(struct Process *process, int signal)
{
    if (signal == S_SIGCONT)
    {
        p_remove_stop_job(process->thread_process_id);
        process->stop_time = -1;
        process->status = ACTIVE_STAT;
        log_event("CONTINUED", process);
        // bool exist = false;
        // if (process->parent) {

        // } else {
        //     exist = true;
        // }
        // if (!exist) {
        //     process->parent->status = PAUSED_STAT;
        // }
        if (!p_search_bg(process->thread_process_id))
        {
            struct Process *shell = k_lookup_process(1);
            shell->status = PAUSED_STAT;
            shell->fg_cont = true;
            shell->to_wait = process->thread_process_id;
            set_fg_pid(process->thread_process_id);
            log_event("BLOCKED", shell);
        }
    }
    else if (signal == S_SIGSTOP)
    {
        p_add_stop_job(process->thread_process_id);
        process->stop_time = s_get_time();
        process->status = STOP_STAT;
        log_event("STOPPED", process);
        struct Process *shell = k_lookup_process(1);
        if (shell->status == PAUSED_STAT && process->thread_process_id == get_fg_pid())
        {
            shell->status = ACTIVE_STAT;
            log_event("CONTINUED", shell);
        }
    }
    else if (signal == S_SIGTERM)
    {
        struct LinkedList *to_remove = s_get_priority(process->priority);
        delete_node(to_remove, process->thread_process_id);
        p_search_and_remove(process->thread_process_id);
        process->signal_terminated = true;
        to_exit = process;
        p_exit_process();
        struct Process *shell = k_lookup_process(1);
        if (shell->status == PAUSED_STAT && process->thread_process_id == get_fg_pid())
        {
            shell->status = ACTIVE_STAT;
            log_event("CONTINUED", shell);
        }
    }
    return 0;
}

struct Process *k_lookup_process(int thread_id)
{
    struct LinkedList *priority = s_get_priority(0);
    struct Entry *temp = priority->head;
    while (temp)
    {
        if (temp->process->thread_process_id == thread_id)
        {
            return temp->process;
        }
        temp = temp->next;
    }
    priority = s_get_priority(1);
    temp = priority->head;
    while (temp)
    {
        if (temp->process->thread_process_id == thread_id)
        {
            return temp->process;
        }
        temp = temp->next;
    }
    priority = s_get_priority(-1);
    temp = priority->head;
    while (temp)
    {
        if (temp->process->thread_process_id == thread_id)
        {
            return temp->process;
        }
        temp = temp->next;
    }
    return NULL;
}

// bool check_idle()
// {
//     return idle;
// }

bool k_set_idle()
{
    s_set_idle();
    return true;
}

void k_kill_all(int signal)
{
    bool zero_ended = false;
    bool one_ended = false;
    struct Entry *temp = s_get_priority(0)->head;
    while (temp)
    {
        if (signal == S_SIGCONT)
        {
            p_remove_stop_job(temp->process->thread_process_id);
            temp->process->stop_time = -1;
            temp->process->status = ACTIVE_STAT;
            log_event("CONTINUED", temp->process);
        }
        else if (signal == S_SIGSTOP)
        {
            p_add_stop_job(temp->process->thread_process_id);
            temp->process->stop_time = s_get_time();
            temp->process->status = STOP_STAT;
            log_event("STOPPED", temp->process);
        }
        else if (signal == S_SIGTERM)
        {
            struct LinkedList *to_remove = NULL;
            if (temp->process->priority == 1)
            {
                to_remove = s_get_priority(1);
            }
            else if (temp->process->priority == 0)
            {
                to_remove = s_get_priority(0);
            }
            else
            {
                to_remove = s_get_priority(-1);
            }
            to_exit = temp->process;
            p_exit_process();
            delete_node(to_remove, temp->process->thread_process_id);
            p_search_and_remove(temp->process->thread_process_id);
        }
        temp = temp->next;
        if (!temp)
        {
            if (!zero_ended)
            {
                zero_ended = true;
                temp = s_get_priority(1)->head;
            }
            if (!temp && !one_ended)
            {
                one_ended = true;
                temp = s_get_priority(-1)->head;
            }
        }
    }
}

void k_process_cleanup(struct Process *process)
{
    free_process(process);
}

// struct LinkedList *get_priority(int priority)
// {
//     if (priority == 0)
//     {
//         return priority_0;
//     }
//     else if (priority == 1)
//     {
//         return priority_1;
//     }
//     else
//     {
//         return priority_n1;
//     }
// }

void k_reap_zombie(int pid)
{
    sigset_t mask;
    sigset_t old_mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGALRM);
    sigprocmask(SIG_BLOCK, &mask, &old_mask);
    struct Process *process = k_lookup_process(pid);
    struct LinkedList *children = process->childrens;
    struct Entry *temp = children->head;
    while (temp)
    {
        set_orphan(temp, true);
        temp->process->parent = NULL;
        temp = temp->next;
    }
    struct LinkedList *to_remove = s_get_priority(process->priority);
    delete_node(to_remove, process->thread_process_id);
    delete_node(process->parent->childrens, process->thread_process_id);

    free_process(process);
    sigprocmask(SIG_SETMASK, &old_mask, NULL);
}

int k_get_terminal_normal_status()
{
    return TERMINATE_NORMAL;
}
int k_get_stop_signal_status()
{
    return STOP_SIGNAL;
}

int k_get_terminal_signal_status()
{
    return TERMINATE_SIGNAL;
}

int k_get_running_status()
{
    return RUNNING;
}

int k_get_sigstop_signal()
{
    return S_SIGSTOP;
}

int k_get_sigcont_signal()
{
    return S_SIGCONT;
}

int k_get_sigterm_signal()
{
    return S_SIGTERM;
}

void k_initiate_to_exit()
{
    to_exit = NULL;
}

void k_set_to_exit(struct Process *process)
{
    to_exit = process;
}

struct Process *k_get_to_exit()
{
    return to_exit;
}

char *k_get_sigstop_str()
{
    return S_SIGSTOP_STR;
}

char *k_get_sigcont_str()
{
    return S_SIGCONT_STR;
}

char *k_get_sigterm_str()
{
    return S_SIGTERM_STR;
}