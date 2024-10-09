#include <signal.h> // sigaction, sigemptyset, sigfillset, signal
#include <stdbool.h>
#include <stdio.h>    // dprintf, fputs, perror
#include <stdlib.h>   // malloc, free
#include <sys/time.h> // setitimer
#include <ucontext.h> // getcontext, makecontext, setcontext, swapcontext
#include <unistd.h>   // read, usleep, write
#include "linkedlist-job.h"
#include "shell.h"
#include "logger.h"
#include "kernel.h"
#include "user.h"

#define THREAD_COUNT 4
#define NOT_WAITING -2

static ucontext_t mainContext;
static ucontext_t idleContext;
static ucontext_t schedulerContext;
static ucontext_t zombieContext;
static ucontext_t shellContext;
static struct LinkedList *priority_1;
static struct LinkedList *priority_0;
static struct LinkedList *priority_n1;
static ucontext_t *activeContext;
static struct Process *currentPCB;
static bool idle;
static const int centisecond = 10000; // 10 milliseconds
static int currTicks = 0;

void s_set()
{
    setcontext(&schedulerContext);
}

void s_swap()
{
    swapcontext(activeContext, &schedulerContext);
}

bool s_check_active(struct LinkedList *queue)
{
    if (!queue)
    {
        return false;
    }
    struct Entry *temp = queue->head;
    bool to_return = false;
    while (temp)
    {
        struct Entry *next = temp->next;
        if (temp->process->status == ACTIVE_STAT)
        {
            to_return = true;
        }
        else if (temp->process->status == PAUSED_STAT && temp->process->awake_time != -1 && currTicks > temp->process->awake_time)
        {
            struct Process *curr = temp->process;
            log_event("EXITED", curr);
            if (curr->parent)
            {
                k_set_to_exit(temp->process);

                temp->process->awake_time = -1;
                if (curr->parent->status == PAUSED_STAT)
                {
                    log_event("UNBLOCKED", curr->parent);
                    curr->parent->status = ACTIVE_STAT;
                }

                to_return = true;
                p_exit_process();
            }
            else
            {
                fprintf(stderr, "pid: %d, awake: %d, time: %d done", curr->thread_process_id, curr->awake_time, currTicks);
                k_set_to_exit(temp->process);

                temp->process->awake_time = -1;
                p_exit_process();
            }
        }
        else if (temp->process->status == DONE_STAT)
        {
            struct Process *curr = temp->process;
            if (curr->parent)
            {
                if (curr->parent->status == PAUSED_STAT)
                {
                    log_event("UNBLOCKED", curr->parent);
                    curr->parent->status = ACTIVE_STAT;
                }
                to_return = true;
            }
        }
        temp = next;
    }
    return to_return;
}

bool check_actual(struct LinkedList *queue)
{
    if (!queue)
    {
        return false;
    }
    struct Entry *temp = queue->head;
    while (temp)
    {
        if (temp->process->status == ACTIVE_STAT)
        {
            return true;
        }
        temp = temp->next;
    }
    return false;
}

static void scheduler(void)
{
    idle = false;
    bool wake_0 = s_check_active(priority_0);
    bool wake_n1 = s_check_active(priority_n1);
    bool wake_1 = s_check_active(priority_1);
    bool found = wake_1 | wake_0 | wake_n1;
    if (!found)
    {
        idle = true;
        fprintf(stderr, "Current Time: %d, ", currTicks);
        setcontext(&idleContext);
    }
    bool found_0 = check_actual(priority_0);
    bool found_n1 = check_actual(priority_n1);
    bool found_1 = check_actual(priority_1);
    // generate a number within range [0, 18]
    int thread = -1;
    if (found_0 && found_1 && found_n1)
    {
        int indicator = rand() % 19;
        // if the number is in [0, 8] we choose the first priority queue
        if (indicator <= 8)
        {
            thread = 3;
        }
        // if the number is in [9, 14] we choose the second priority queue
        else if (indicator <= 14)
        {
            thread = 2;
        }
        // if the number is in [15, 18] we choose the third priority queue
        else
        {
            thread = 1;
        }
    }
    else if (!found_0 && found_1 && found_n1)
    {
        int indicator = rand() % 13;
        if (indicator <= 8)
        {
            thread = 3;
        }
        else
        {
            thread = 1;
        }
    }
    else if (found_0 && !found_1 && found_n1)
    {
        int indicator = rand() % 15;
        if (indicator <= 5)
        {
            thread = 2;
        }
        else
        {
            thread = 3;
        }
    }
    else if (found_0 && found_1 && !found_n1)
    {
        int indicator = rand() % 10;
        if (indicator <= 5)
        {
            thread = 2;
        }
        else
        {
            thread = 1;
        }
    }
    else if (!found_0 && !found_1 && found_n1)
    {
        thread = 3;
    }
    else if (!found_0 && found_1 && !found_n1)
    {
        thread = 1;
    }
    else if (found_0 && !found_1 && !found_n1)
    {
        thread = 2;
    }
    if (thread == 1)
    {

        currentPCB = poll(priority_1);

        while (currentPCB->status != ACTIVE_STAT)
        {
            insert_end(priority_1, currentPCB);
            currentPCB = poll(priority_1);
        }

        insert_end(priority_1, currentPCB);

        activeContext = currentPCB->context;
    }
    else if (thread == 2)
    {
        currentPCB = poll(priority_0);
        while (currentPCB->status != ACTIVE_STAT)
        {
            insert_end(priority_0, currentPCB);
            currentPCB = poll(priority_0);
        }
        insert_end(priority_0, currentPCB);
        activeContext = currentPCB->context;
    }
    else if (thread == 3)
    {
        currentPCB = poll(priority_n1);
        while (currentPCB->status != ACTIVE_STAT)
        {
            insert_end(priority_n1, currentPCB);
            currentPCB = poll(priority_n1);
        }
        insert_end(priority_n1, currentPCB);
        activeContext = currentPCB->context;
    }
    log_event("SCHEDULE", currentPCB);
    setcontext(activeContext);
    perror("setcontext");
}

// #include <valgrind/valgrind.h>
void setStack(stack_t *stack)
{
    void *sp = malloc(819200);
    // VALGRIND_STACK_REGISTER(sp, sp + 819200);
    *stack = (stack_t){.ss_sp = sp, .ss_size = 819200};
}

void s_makeContext(ucontext_t *ucp, void (*func)(), int thread)
{
    getcontext(ucp);
    sigemptyset(&ucp->uc_sigmask);
    setStack(&ucp->uc_stack);
}

static void makeContexts(void)
{
    s_makeContext(&schedulerContext, scheduler, 0);
    makecontext(&schedulerContext, scheduler, 0);
}

static void alarmHandler(int signum)
{ // SIGALARM
    bool initiated = currTicks != 0;
    currTicks++;
    if (!initiated)
    {
        initiated = true;
        swapcontext(activeContext, &schedulerContext);
    }
    else
    {
        if (idle)
        {
            s_set();
        }
        else
        {
            s_swap();
        }
    }
}

static void setAlarmHandler(void)
{
    struct sigaction act;
    act.sa_handler = alarmHandler;
    act.sa_flags = SA_RESTART;
    sigfillset(&act.sa_mask);
    sigaction(SIGALRM, &act, NULL);
}

static void setTimer(void)
{
    struct itimerval it;
    it.it_interval = (struct timeval){.tv_usec = centisecond * 20};
    it.it_value = it.it_interval;
    setitimer(ITIMER_REAL, &it, NULL);
}

void freeStacks()
{
    free(schedulerContext.uc_stack.ss_sp);
}
static void idleFunction(void)
{
    printf("Idle context Running.\n");
    idle = true;
    sigemptyset(&idleContext.uc_sigmask);
    sigsuspend(&idleContext.uc_sigmask);
}

static void zombiefy(void)
{
    k_set_to_exit(currentPCB);
    p_exit_process();
    activeContext = &schedulerContext;
}

void s_setup()
{
    signal(SIGINT, SIG_IGN);  // Ctrl-C
    signal(SIGQUIT, SIG_IGN); /* Ctrl-\ */
    signal(SIGTSTP, SIG_IGN); // Ctrl-Z
    makeContexts();
    getcontext(&idleContext);
    sigemptyset(&idleContext.uc_sigmask);
    idleContext.uc_link = NULL;
    setStack(&idleContext.uc_stack);

    makecontext(&idleContext, idleFunction, 0);
    getcontext(&zombieContext);
    sigemptyset(&zombieContext.uc_sigmask);
    zombieContext.uc_link = &schedulerContext;
    setStack(&zombieContext.uc_stack);
    makecontext(&zombieContext, zombiefy, 0);

    setAlarmHandler();
    setTimer();
}

void s_initiate_priorities()
{
    priority_0 = malloc(sizeof(struct LinkedList));
    if (!priority_0)
    {
        perror("malloc failed");
        exit(EXIT_FAILURE);
    }
    priority_0->head = NULL;
    priority_0->tail = NULL;
    priority_1 = malloc(sizeof(struct LinkedList));
    if (!priority_1)
    {
        perror("malloc failed");
        exit(EXIT_FAILURE);
    }
    priority_1->head = NULL;
    priority_1->tail = NULL;
    priority_n1 = malloc(sizeof(struct LinkedList));
    if (!priority_n1)
    {
        perror("malloc failed");
        exit(EXIT_FAILURE);
    }
    priority_n1->head = NULL;
    priority_n1->tail = NULL;
}
void s_initiate()
{
    activeContext = &shellContext;
    swapcontext(&mainContext, activeContext);
}

void s_insert(int priority, struct Process *pcb)
{
    if (priority == 0)
    {
        insert_end(priority_0, pcb);
    }
    else if (priority == 1)
    {
        insert_end(priority_1, pcb);
    }
    else if (priority == -1)
    {
        insert_end(priority_n1, pcb);
    }
}
void s_initiate_shell_context(int argc, char *argv[], struct Process *process)
{
    getcontext(&shellContext);
    sigemptyset(&shellContext.uc_sigmask);
    setStack(&shellContext.uc_stack);
    shellContext.uc_link = &mainContext;
    makecontext(&shellContext, shell, 0);
    process->context = &shellContext;
}
void s_set_current(struct Process *process)
{
    currentPCB = process;
}

struct Process *s_get_current()
{
    return currentPCB;
}

int s_get_time()
{
    return currTicks;
}

struct LinkedList *s_get_priority(int priority)
{
    if (priority == 0)
    {
        return priority_0;
    }
    else if (priority == 1)
    {
        return priority_1;
    }
    else if (priority == -1)
    {
        return priority_n1;
    }
    return NULL;
}

ucontext_t *s_get_zombie_context()
{
    return &zombieContext;
}
ucontext_t *s_get_scheduler_context()
{
    return &schedulerContext;
}
void s_set_status(int status)
{
    currentPCB->status = status;
}

void s_print_all_jobs()
{
    fprintf(stdout, "%-5s%-5s%5s%5s%15s\n\n", "PID", "PPID", "PRI", "STAT", "CMD");
    struct Entry *temp = priority_0->head;
    char *dict[] = {"R", "B", "Z", "S"};
    while (temp)
    {
        struct Process *p = temp->process;
        fprintf(stderr, "%-5d%-5d%5d%5s%15s\n\n", p->thread_process_id, p->parent_process_id, p->priority, dict[p->status], p->cmd);
        temp = temp->next;
    }
    temp = priority_1->head;
    while (temp)
    {
        struct Process *p = temp->process;
        fprintf(stderr, "%-5d%-5d%5d%5s%15s\n\n", p->thread_process_id, p->parent_process_id, p->priority, dict[p->status], p->cmd);
        temp = temp->next;
    }
    temp = priority_n1->head;
    while (temp)
    {
        struct Process *p = temp->process;
        fprintf(stderr, "%-5d%-5d%5d%5s%15s\n\n", p->thread_process_id, p->parent_process_id, p->priority, dict[p->status], p->cmd);
        temp = temp->next;
    }
    p_exit();
}

void s_set_idle()
{
    idle = false;
}