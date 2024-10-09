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
#include <signal.h>
#include "./linkedlist-job.h"
#include "./parser.h"

// Insert a job at the end of the linkedlist
void insert_end(struct LinkedList *list, struct Process *process)
{
    // allocate memory
    struct Entry *ne = malloc(sizeof(struct Entry));
    if (!ne)
    {
        // allocate failure
        perror("malloc failed");
        exit(EXIT_FAILURE);
    }
    // set job
    ne->process = process;
    // if list is empty
    if (!list->head)
    {
        list->head = ne;
        list->tail = ne;
        ne->prev = NULL;
        ne->next = NULL;
        return;
    }
    else
    {
        ne->prev = list->tail;
        ne->next = NULL;
        list->tail->next = ne;
        list->tail = ne;
    }
}

// delete the entry with thread process id
struct Process *delete_node(struct LinkedList *list, int id)
{
    // search for the id
    struct Entry *temp = list->head;
    while (temp != NULL)
    {
        if (temp->process->thread_process_id == id)
        {
            break;
        }
        temp = temp->next;
    }
    // if search failure
    if (temp == NULL)
    {
        return NULL;
    }
    // if the list is empty
    if (list->head == list->tail)
    {
        list->head = NULL;
        list->tail = NULL;
    }
    // if to delete is at tail
    else if (temp == list->tail)
    {
        list->tail = temp->prev;
        list->tail->next = NULL;
    }
    // if to delete is at head
    else if (temp == list->head)
    {
        list->head = temp->next;
        list->head->prev = NULL;
    }
    // delete from middle
    else
    {
        temp->prev->next = temp->next;
        temp->next->prev = temp->prev;
    }
    temp->prev = NULL;
    temp->next = NULL;
    struct Process *to_return = temp->process;
    // free up memory
    free(temp);
    return to_return;
}

void free_process(struct Process *process)
{
    if (process->childrens)
    {
        free(process->childrens);
    }
    if (process->argv) 
    {
        free(process->argv);
    }
    free(process);
}

// search the linkedlist for thread job id, and return the entry
struct Entry *search_list(struct LinkedList *list, int id)
{
    // set to head
    struct Entry *temp = list->head;
    // search for id
    while (temp != NULL)
    {
        if (temp->process->thread_process_id == id)
        {
            return temp;
        }
        temp = temp->next;
    }
    return temp;
}

// retrive the last job in the list
struct Process *retrieve_latest(struct LinkedList *list)
{
    // if empty return NULL
    if (!list->tail)
    {
        return NULL;
    }
    struct Entry *entry = list->tail;
    // if list is of length 1
    if (entry == list->head)
    {
        list->head = NULL;
        list->tail = NULL;
    }
    else
    {
        entry->prev->next = NULL;
        list->tail = entry->prev;
    }
    struct Process *process = entry->process;
    free(entry);
    entry = NULL;
    return process;
}

// same to poll for linkedlist
struct Process *poll(struct LinkedList *list)
{
    // if empty return NULL
    if (!list->head)
    {
        return NULL;
    }
    struct Entry *entry = list->head;
    // if list is of length 1
    if (entry == list->tail)
    {
        list->head = NULL;
        list->tail = NULL;
    }
    else
    {
        entry->next->prev = NULL;
        list->head = entry->next;
    }
    struct Process *process = entry->process;
    free(entry);
    return process;
}

// free out all entries and jobs in the list (helper)
void free_list(struct LinkedList *list)
{
    // start from the first job
    struct Process *process = poll(list);
    while (process)
    {
        // keep polling and freeing
        if (process)
        {
            free_process(process);
            process = NULL;
        }
        process = poll(list);
    }
}

void set_orphan(struct Entry *e, bool orphan)
{
    e->process->is_orphan = orphan;
}
