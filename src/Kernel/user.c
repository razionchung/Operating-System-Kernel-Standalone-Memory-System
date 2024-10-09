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
#include "user.h"
#include "scheduler.h"
#include "linkedlist-job.h"
#include "logger.h"
#include "kernel.h"
#include "../Standalone_PennFat/pennfat.h"
#include "../Standalone_PennFat/descriptors.h"

static struct LinkedList bg_job = {.head = NULL, .tail = NULL};
static struct LinkedList stop_job = {.head = NULL, .tail = NULL};

int getSignal(char *sig)
{
    if (strcmp(sig, "S_SIGTERM") == 0)
        return k_get_sigterm_signal();
    else if (strcmp(sig, "S_SIGSTOP") == 0)
        return k_get_sigstop_signal();
    else if (strcmp(sig, "S_SIGCONT") == 0)
        return k_get_sigcont_signal();
    return -1;
}

pid_t p_spawn(void (*func)(), char *argv[], int fd0, int fd1)
{
    // bool check = check_idle();
    struct Process *curr = s_get_current();
    struct Process *child = k_process_create(curr);
    child->input_descriptor = fd0;
    child->output_descriptor = fd1;
    child->cmd = argv[0];
    child->bg_time = -1;
    child->stop_time = -1;
    child->argv = NULL;
    int count = 0;
    while (argv[count] != NULL)
    {
        count++;
    }
    ucontext_t *pointer = malloc(sizeof(ucontext_t));
    if (count == 1)
    {
        getcontext(pointer);
        sigemptyset(&pointer->uc_sigmask);
        setStack(&pointer->uc_stack);
        pointer->uc_link = s_get_zombie_context();
        makecontext(pointer, func, 0);
    }
    else if (count == 2)
    {
        if (strcmp(argv[0], "sleep") == 0)
        {
            getcontext(pointer);
            sigemptyset(&pointer->uc_sigmask);
            setStack(&pointer->uc_stack);
            pointer->uc_link = s_get_zombie_context();
            unsigned int t = atoi(argv[1]);
            makecontext(pointer, func, count, t);
        }
        else if (strcmp(argv[0], "kill") == 0)
        {
            getcontext(pointer);
            sigemptyset(&pointer->uc_sigmask);
            setStack(&pointer->uc_stack);
            pointer->uc_link = s_get_zombie_context();
            makecontext(pointer, func, count, getSignal(argv[2]));
        }
        else
        {
            getcontext(pointer);
            sigemptyset(&pointer->uc_sigmask);
            setStack(&pointer->uc_stack);
            pointer->uc_link = s_get_zombie_context();
            makecontext(pointer, func, count, argv[1]);
        }
    }
    else if (count == 3)
    {
        getcontext(pointer);
        sigemptyset(&pointer->uc_sigmask);
        setStack(&pointer->uc_stack);
        pointer->uc_link = s_get_zombie_context();
        makecontext(pointer, func, count, argv[1], argv[2]);
    }
    else if (count == 4)
    {
        if (strcmp(argv[0], "cp") == 0)
        {
            getcontext(pointer);
            sigemptyset(&pointer->uc_sigmask);
            setStack(&pointer->uc_stack);
            pointer->uc_link = s_get_zombie_context();
            makecontext(pointer, func, count, argv[1], argv[2], atoi(argv[3]));
        }
    }
    else
    {
        s_makeContext(pointer, func, 0);
        pointer->uc_link = s_get_zombie_context();
        makecontext(pointer, func, 0);
    }
    child->context = pointer;
    k_set_idle();
    log_event("CREATE", child);
    return child->thread_process_id;
}

pid_t p_spawn_with_priority(void (*func)(), char *argv[], int fd0, int fd1, int priority)
{
    struct Process *curr = s_get_current();
    struct Process *child = k_process_create_with_priority(curr, priority);
    child->input_descriptor = fd0;
    child->output_descriptor = fd1;
    child->cmd = argv[0];
    child->bg_time = -1;
    child->stop_time = -1;
    child->to_wait = -2;
    child->argv = NULL;
    int count = 0;
    while (argv[count] != NULL)
    {
        count++;
    }
    ucontext_t *pointer = malloc(sizeof(ucontext_t));
    if (count == 1)
    {
        getcontext(pointer);
        sigemptyset(&pointer->uc_sigmask);
        setStack(&pointer->uc_stack);
        pointer->uc_link = s_get_zombie_context();
        makecontext(pointer, func, 0);
    }
    else if (count == 2)
    {
        if (strcmp(argv[0], "sleep") == 0)
        {
            getcontext(pointer);
            sigemptyset(&pointer->uc_sigmask);
            setStack(&pointer->uc_stack);
            pointer->uc_link = s_get_zombie_context();
            unsigned int t = atoi(argv[1]);
            makecontext(pointer, func, count, t);
        }
        else
        {
            getcontext(pointer);
            sigemptyset(&pointer->uc_sigmask);
            setStack(&pointer->uc_stack);
            pointer->uc_link = s_get_zombie_context();
            makecontext(pointer, func, count, argv[1]);
        }
    }
    else if (count == 3)
    {
        if (strcmp(argv[0], "kill") == 0)
        {
            getcontext(pointer);
            sigemptyset(&pointer->uc_sigmask);
            setStack(&pointer->uc_stack);
            pointer->uc_link = s_get_zombie_context();
            makecontext(pointer, func, count, atoi(argv[1]), getSignal(argv[2]));
        }
        else
        {
            getcontext(pointer);
            sigemptyset(&pointer->uc_sigmask);
            setStack(&pointer->uc_stack);
            pointer->uc_link = s_get_zombie_context();
            makecontext(pointer, func, count, argv[1], argv[2]);
        }
    }
    else if (count == 4)
    {
        if (strcmp(argv[0], "cp") == 0)
        {
            getcontext(pointer);
            sigemptyset(&pointer->uc_sigmask);
            setStack(&pointer->uc_stack);
            pointer->uc_link = s_get_zombie_context();
            makecontext(pointer, func, count, argv[1], argv[2], atoi(argv[3]));
        }
    }
    else
    {
        s_makeContext(pointer, func, 0);
        pointer->uc_link = s_get_zombie_context();
        makecontext(pointer, func, 0);
    }
    child->context = pointer;
    k_set_idle();
    log_event("CREATE", child);
    return child->thread_process_id;
}

pid_t p_spawn_with_input(void (*func)(), char *argv[], int fd0, int fd1, char **actual_input)
{
    struct Process *curr = s_get_current();
    struct Process *child = k_process_create(curr);
    child->input_descriptor = fd0;
    child->output_descriptor = fd1;
    child->cmd = argv[0];
    child->bg_time = -1;
    child->stop_time = -1;
    child->argv = actual_input;
    int count = 0;
    while (argv[count] != NULL)
    {
        count++;
    }
    int count_2 = 0;
    while (actual_input[count_2] != NULL)
    {
        count_2++;
    }
    ucontext_t *pointer = malloc(sizeof(ucontext_t));
    if (strcmp(child->cmd, "echo") == 0)
    {
        getcontext(pointer);
        sigemptyset(&pointer->uc_sigmask);
        setStack(&pointer->uc_stack);
        pointer->uc_link = s_get_zombie_context();
        makecontext(pointer, func, 0);
    }
    else if (count == 1)
    {
        getcontext(pointer);
        sigemptyset(&pointer->uc_sigmask);
        setStack(&pointer->uc_stack);
        pointer->uc_link = s_get_zombie_context();
        makecontext(pointer, func, 0);
    }
    else if (count == 2)
    {
        if (strcmp(argv[0], "sleep") == 0)
        {
            getcontext(pointer);
            sigemptyset(&pointer->uc_sigmask);
            setStack(&pointer->uc_stack);
            pointer->uc_link = s_get_zombie_context();
            unsigned int t = atoi(argv[1]);
            makecontext(pointer, func, count, t);
        }
        else if (strcmp(argv[0], "kill") == 0)
        {
            getcontext(pointer);
            sigemptyset(&pointer->uc_sigmask);
            setStack(&pointer->uc_stack);
            pointer->uc_link = s_get_zombie_context();
            makecontext(pointer, func, 2, getSignal(argv[1]), curr->thread_process_id);
        }
        else
        {
            getcontext(pointer);
            sigemptyset(&pointer->uc_sigmask);
            setStack(&pointer->uc_stack);
            pointer->uc_link = s_get_zombie_context();
            makecontext(pointer, func, count, argv[1]);
        }
    }
    else if (count == 3)
    {
        getcontext(pointer);
        sigemptyset(&pointer->uc_sigmask);
        setStack(&pointer->uc_stack);
        pointer->uc_link = s_get_zombie_context();
        makecontext(pointer, func, count, argv[1], argv[2]);
    }
    else if (count == 4)
    {
        if (strcmp(argv[0], "cp") == 0)
        {
            getcontext(pointer);
            sigemptyset(&pointer->uc_sigmask);
            setStack(&pointer->uc_stack);
            pointer->uc_link = s_get_zombie_context();
            makecontext(pointer, func, count, argv[1], argv[2], atoi(argv[3]));
        }
    }
    else
    {
        s_makeContext(pointer, func, 0);
        pointer->uc_link = s_get_zombie_context();
        makecontext(pointer, func, 0);
    }
    child->context = pointer;
    k_set_idle();
    log_event("CREATE", child);
    return child->thread_process_id;
}

pid_t p_initiate_shell(void (*func)(), int argc, char *argv[])
{
    struct Process *child = malloc(sizeof(struct Process));
    s_initiate_shell_context(argc, argv, child);
    child->recorded = -1;
    child->parent = NULL;
    child->context->uc_link = NULL;
    child->childrens = malloc(sizeof(struct LinkedList));
    child->childrens->head = NULL;
    child->childrens->tail = NULL;
    child->parent_process_id = 0;
    child->fg_cont = false;
    child->thread_process_id = k_get_next_pid();
    child->num_children = 0;
    child->priority = -1;
    child->status = ACTIVE_STAT;
    child->signal_terminated = false;
    child->awake_time = -1;
    child->cmd = "shell";
    child->is_orphan = true;
    s_insert(-1, child);
    child->input_descriptor = STDIN_FILENO;
    child->output_descriptor = STDOUT_FILENO;
    s_set_current(child);
    log_event("CREATE", child);
    return child->thread_process_id;
}

pid_t p_waitpid(pid_t pid, int *wstatus, bool nohang)
{
    if (nohang)
    {

        if (pid >= 0)
        {
            struct Process *child = k_lookup_process(pid);
            if (child)
            {
                if (child->status == DONE_STAT)
                {
                    if (child->signal_terminated)
                    {
                        if (wstatus)
                        {
                            *wstatus = TERMINATE_SIGNAL;
                        }
                    }
                    else
                    {
                        if (wstatus)
                        {
                            *wstatus = TERMINATE_NORMAL;
                        }
                    }
                    k_reap_zombie(child->thread_process_id);
                    struct Process *c = s_get_current();
                    c->to_wait = -2;
                    log_event("WAITED", c);
                    return pid;
                }
                else if (child->status == STOP_STAT)
                {
                    if (wstatus)
                    {
                        *wstatus = STOP_SIGNAL;
                    }

                    struct Process *c = s_get_current();
                    c->to_wait = -2;
                    log_event("WAITED", c);
                    return pid;
                }
            }
            struct Process *c = s_get_current();
            c->to_wait = -2;
            return -1;
        }
        else if (pid == -1)
        {
            int to_return = -1;
            struct LinkedList *children = s_get_current()->childrens;
            if (children->head)
            {
                struct Entry *temp = children->head;
                while (temp)
                {
                    if (temp->process->status == DONE_STAT)
                    {
                        if (temp->process->signal_terminated)
                        {
                            if (wstatus)
                            {
                                *wstatus = TERMINATE_SIGNAL;
                            }
                        }
                        else
                        {
                            if (wstatus)
                            {
                                *wstatus = TERMINATE_NORMAL;
                            }
                        }
                        to_return = temp->process->thread_process_id;
                        k_reap_zombie(temp->process->thread_process_id);
                        break;
                    }
                    temp = temp->next;
                }
            }
            else
            {
                struct Process *c = s_get_current();
                c->to_wait = -2;
                return -1;
            }
            if (to_return == -1)
            {
                struct Process *c = s_get_current();
                c->to_wait = -2;
                return 0;
            }
            struct Process *c = s_get_current();
            c->to_wait = -2;
            log_event("WAITED", s_get_current());
            return to_return;
        }
    }
    else
    {
        while (1)
        {
            if (pid >= 0)
            {
                struct Process *child = k_lookup_process(pid);
                if (!child)
                {
                    struct Process *c = s_get_current();
                    c->to_wait = -2;
                    return -1;
                }
                if (child->status == DONE_STAT)
                {
                    if (child->signal_terminated)
                    {
                        if (wstatus)
                        {
                            *wstatus = TERMINATE_SIGNAL;
                        }
                    }
                    else
                    {
                        if (wstatus)
                        {
                            *wstatus = TERMINATE_NORMAL;
                        }
                    }
                    k_reap_zombie(child->thread_process_id);
                    struct Process *c = s_get_current();
                    c->to_wait = -2;
                    return pid;
                }
                if (child->recorded == -1)
                {
                    child->recorded = child->status;
                }
                else if (child->status != child->recorded)
                {
                    struct Process *c = s_get_current();
                    c->to_wait = -2;
                    return child->thread_process_id;
                }
                s_get_current()->to_wait = pid;
            }
            else if (pid == -1)
            {
                struct Process *curr = s_get_current();
                struct Entry *pointer = curr->childrens->head;
                if (pointer)
                {
                    while (pointer)
                    {
                        pid_t to_return = -1;
                        if (pointer->process->status == DONE_STAT)
                        {
                            if (pointer->process->signal_terminated)
                            {
                                if (wstatus)
                                {
                                    *wstatus = TERMINATE_SIGNAL;
                                }
                            }
                            else
                            {
                                if (wstatus)
                                {
                                    *wstatus = TERMINATE_NORMAL;
                                }
                            }
                            to_return = pointer->process->thread_process_id;
                            k_reap_zombie(pointer->process->thread_process_id);
                            struct Process *c = s_get_current();
                            c->to_wait = -2;
                            return to_return;
                        }
                        else if (pointer->process->status == STOP_STAT)
                        {
                            if (wstatus)
                            {
                                *wstatus = STOP_SIGNAL;
                            }

                            to_return = pointer->process->thread_process_id;
                            struct Process *c = s_get_current();
                            c->to_wait = -2;
                            return to_return;
                        }
                        else
                        {
                            if (pointer->process->recorded == -1)
                            {
                                pointer->process->recorded = pointer->process->status;
                            }
                            else if (pointer->process->status != pointer->process->recorded)
                            {
                                to_return = pointer->process->thread_process_id;
                                struct Process *c = s_get_current();
                                c->to_wait = -2;
                                return to_return;
                            }
                        }
                    }
                    curr->to_wait = -1;
                }
                else
                {
                    struct Process *c = s_get_current();
                    c->to_wait = -2;
                    return -1;
                }
            }
            log_event("BLOCKED", s_get_current());
            s_set_status(PAUSED_STAT);
            swapcontext(s_get_current()->context, s_get_scheduler_context());
        }
    }
    struct Process *c = s_get_current();
    c->to_wait = -2;
    return -1;
}

int p_kill(pid_t pid, int sig)
{
    if (pid >= 0)
    {
        struct Process *result = k_lookup_process(pid);
        if (result)
        {
            k_process_kill(result, sig);
            return 0;
        }
        else
        {
            return -1;
        }
    }
    else if (pid == -1)
    {
        k_kill_all(sig);
        return 0;
    }
    return -1;
}

void p_exit(void)
{
    struct Process *curr = s_get_current();
    if (curr->parent)
    {
        if (curr->parent->to_wait == -1 | curr->parent->to_wait == curr->thread_process_id)
        {
            if (curr->signal_terminated)
            {
                curr->signal_terminated = false;
                log_event("SIGNALED", curr);
            }
            else
            {
                log_event("EXITED", curr);
            }
            log_event("ZOMBIE", curr);
        }
        else
        {
            log_event("ZOMBIE", curr);
        }
    }
    else
    {
        if (curr->signal_terminated)
        {
            curr->signal_terminated = false;
            log_event("SIGNALED", curr);
        }
        else
        {
            log_event("EXITED", curr);
        }
    }
    if (curr->childrens)
    {
        struct Entry *pointer = curr->childrens->head;
        while (pointer)
        {
            log_event("ORPHAN", pointer->process);
            pointer = pointer->next;
        }
    }
    curr->status = DONE_STAT;
    if (!curr->parent)
    {
        k_reap_zombie(curr->thread_process_id);
    }
}

void p_exit_process()
{
    struct Process *process = k_get_to_exit();
    bool need_reap = false;
    if (process->parent)
    {
        if (process->parent->to_wait == -1 | process->parent->to_wait == process->thread_process_id)
        {
            if (process->signal_terminated)
            {
                process->signal_terminated = false;
                log_event("SIGNALED", process);
            }
            else
            {
                log_event("EXITED", process);
            }
            log_event("ZOMBIE", process);
        }
        else
        {
            log_event("ZOMBIE", process);
        }
        if (process->parent->to_wait == -2)
        {
            need_reap = true;
        }
    }
    else
    {
        if (process->signal_terminated)
        {
            process->signal_terminated = false;
            log_event("SIGNALED", process);
        }
        else
        {
            log_event("EXITED", process);
        }
    }
    if (process->childrens)
    {
        struct Entry *pointer = process->childrens->head;
        while (pointer)
        {
            log_event("ORPHAN", pointer->process);
            pointer = pointer->next;
        }
    }
    process->status = DONE_STAT;
    if (!process->parent)
    {
        k_reap_zombie(process->thread_process_id);
    }
    if (strcmp(process->cmd, "sleep") == 0 && need_reap)
    {
        k_reap_zombie(process->thread_process_id);
    }
}

bool W_WIFEXITED(int *status)
{
    return *status == TERMINATE_NORMAL;
}

bool W_WIFSTOPPED(int *status)
{
    return *status == STOP_SIGNAL;
}

bool W_WIFSIGNALED(int *status)
{
    return *status == TERMINATE_SIGNAL;
}

int p_nice(pid_t pid, int priority)
{
    struct Process *process = k_lookup_process(pid);
    if (!process)
    {
        return -1;
    }
    int old_nice = process->priority;
    struct LinkedList *to_remove = s_get_priority(process->priority);
    struct LinkedList *to_add = s_get_priority(priority);
    delete_node(to_remove, process->thread_process_id);
    insert_end(to_add, process);
    process->priority = priority;
    log_nice_event(old_nice, process);
    return 0;
}

// might need encap
void p_sleep(unsigned int ticks)
{
    struct Process *curr = s_get_current();
    curr->awake_time = s_get_time() + (int)ticks;
    log_event("BLOCKED", curr);
    curr->status = PAUSED_STAT;
    s_swap();
    p_exit();
}

void p_busy_wait()
{
    while (true)
        ;
}

void p_zombie_child()
{
}

void p_orphan_child()
{
    while (1)
        ;
}

void p_print_all_jobs()
{
    s_print_all_jobs();
}

void p_add_background_job(int pid)
{
    struct Process *p = k_lookup_process(pid);
    if (p)
    {
        p->bg_time = s_get_time();
        insert_end(&bg_job, p);
    }
    else
        perror("Pid does not exist\n");
}

void p_add_stop_job(int pid)
{
    struct Process *p = k_lookup_process(pid);
    if (p)
    {
        p->stop_time = s_get_time();
        insert_end(&stop_job, p);
    }
    else
        perror("Pid does not exist\n");
}

void p_remove_background_job(int pid)
{
    struct Process *p = k_lookup_process(pid);
    if (p)
    {
        p->bg_time = -1;
        delete_node(&bg_job, pid);
    }
}

void p_remove_stop_job(int pid)
{
    struct Process *p = k_lookup_process(pid);
    if (p)
    {
        p->stop_time = -1;
        delete_node(&stop_job, pid);
    }
}

void p_search_and_remove(int pid)
{
    struct Entry *stopEntry = search_list(&stop_job, pid);
    struct Entry *bgEntry = search_list(&bg_job, pid);
    if (stopEntry)
    {
        p_remove_stop_job(pid);
    }
    if (bgEntry)
    {
        p_remove_background_job(pid);
    }
}

int p_search_most_recent()
{
    struct Entry *stopEntry = stop_job.tail;
    struct Entry *bgEntry = bg_job.tail;
    if (stopEntry && bgEntry)
    {
        if (stopEntry->process->stop_time > bgEntry->process->bg_time)
        {
            return stopEntry->process->thread_process_id;
        }
        else
        {
            return bgEntry->process->thread_process_id;
        }
    }
    else if (stopEntry)
    {
        return stopEntry->process->thread_process_id;
    }
    else if (bgEntry)
    {
        return bgEntry->process->thread_process_id;
    }
    else
    {
        return -1;
    }
}

int p_search_most_recent_stop()
{
    struct Entry *stopEntry = stop_job.tail;
    if (stopEntry)
    {
        return stopEntry->process->thread_process_id;
    }
    else
    {
        return -1;
    }
}

struct Process *p_search_bg(int pid)
{
    struct Entry *temp = bg_job.head;
    while (temp)
    {
        if (temp->process->thread_process_id == pid)
        {
            return temp->process;
        }
        temp = temp->next;
    }
    return NULL;
}

int p_get_sigstop_signal()
{
    return k_get_sigstop_signal();
}

int p_get_sigcont_signal()
{
    return k_get_sigcont_signal();
}

int p_get_sigterm_signal()
{
    return k_get_sigterm_signal();
}

void p_initiate_to_exit()
{
    k_initiate_to_exit();
}

struct Process *p_get_current()
{
    return s_get_current();
}

void p_initiate_priorities()
{
    s_initiate_priorities();
}

void p_setup()
{
    s_setup();
}

void p_initiate()
{
    s_initiate();
}

struct Process *p_lookup_process(pid_t pid)
{
    return k_lookup_process(pid);
}

void p_zombify()
{
    char *argv[2] = {"zombie_child", NULL};
    p_spawn(p_zombie_child, argv, STDIN_FILENO, STDOUT_FILENO);
    while (1)
        ;
    p_exit();
}

void p_orphanify()
{
    char *argv[2] = {"orphan_child", NULL};
    p_spawn(p_orphan_child, argv, STDIN_FILENO, STDOUT_FILENO);
    p_exit();
}

void p_run_kill(int sig, pid_t pid)
{
    char **argv = p_get_current()->argv;
    int count = 0;
    while (argv[count] != NULL)
    {
        count++;
    }
    for (int i = 2; i < count; i++)
    {
        p_kill(atoi(argv[i]), sig);
    }
    p_exit();
}

void p_run_mv_fs(char *source, char *dest)
{
    mv_fs(source, dest);
    p_exit();
}

void p_run_chmod_fs(char *filename, char *perm)
{
    chmod_fs(filename, perm);
    p_exit();
}

void p_run_cp_fs(char *source, char *des, int from_host)
{
    cp_fs(source, des, from_host);
    p_exit();
}

void p_run_touch_fs()
{
    char **argv = p_get_current()->argv;
    int count = 0;
    while (argv[count] != NULL)
    {
        count++;
    }
    for (int i = 1; i < count; i++)
    {
        touch_fs(argv[i]);
    }
    p_exit();
}

void p_run_f_ls_list()
{
    char **argv = p_get_current()->argv;
    int count = 0;
    while (argv[count] != NULL)
    {
        count++;
    }
    for (int i = 1; i < count; i++)
    {
        f_ls(argv[i]);
    }
    p_exit();
}

void p_run_f_ls_null()
{
    f_ls(NULL);
    p_exit();
}

void p_run_rm_fs()
{
    char **argv = p_get_current()->argv;
    int count = 0;
    while (argv[count] != NULL)
    {
        count++;
    }
    for (int i = 1; i < count; i++)
    {
        rm_fs(argv[i]);
    }
    p_exit();
}

void p_run_echo()
{
    char **argv = p_get_current()->argv;
    int count = 0, len = 0;
    while (argv[count] != NULL)
    {
        len += strlen(argv[count]);
        len++;
        count++;
    }
    char *combined = malloc(len * sizeof(char));
    strcpy(combined, argv[1]);
    if (count == 2)
        strcat(combined, "\n");
    else
        strcat(combined, " ");
    for (int i = 2; i < count; i++)
    {
        strcat(combined, argv[i]);
        if (i != count - 1)
        {
            strcat(combined, " ");
        }
        else
        {
            strcat(combined, "\n");
        }
    }
    if (p_get_current()->output_descriptor != -1)
    {
        f_write(p_get_current()->output_descriptor, combined, strlen(combined));
        f_close(p_get_current()->output_descriptor);
    }
    else
    {

        fprintf(stderr, "%s", combined);
    }
    free(combined);
    p_exit();
}

void p_run_cat_fs()
{
    char **argv = p_get_current()->argv;
    int count = 0, len = 0;
    while (argv[count] != NULL)
    {
        len += strlen(argv[count]);
        len++;
        count++;
    }
    cat_fs(count, argv);
    p_exit();
}

char *p_get_sigstop_str()
{
    return k_get_sigstop_str();
}

char *p_get_sigcont_str()
{
    return k_get_sigcont_str();
}

char *p_get_sigterm_str()
{
    return k_get_sigterm_str();
}