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
#include "logger.h"
#include "linkedlist-job.h"
#include "scheduler.h"

FILE *open_log_file()
{
    FILE *file = fopen("../log/log", "a");
    if (file == NULL)
    {
        perror("Error opening log file");
        return NULL;
    }
    return file;
}

void log_event(const char *event_type, struct Process *process)
{
    // va_list args;
    FILE *log_file = open_log_file();
    if (log_file == NULL)
    {
        return;
    }
    fprintf(log_file, "[%d] %s %d %d %s\n", s_get_time(), event_type, process->thread_process_id, process->priority, process->cmd);
    fclose(log_file);
}

void log_nice_event(int old_nice, struct Process *process)
{
    // va_list args;
    FILE *log_file = open_log_file();
    if (log_file == NULL)
    {
        return;
    }
    fprintf(log_file, "[%d] NICE %d %d %d %s\n", s_get_time(), process->thread_process_id, old_nice, process->priority, process->cmd);
    fclose(log_file);
}
