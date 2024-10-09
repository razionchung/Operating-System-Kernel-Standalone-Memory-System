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

/**
 * Helper function used to open the log file and print logs into the log file.
 * @param event_type The event status to print as shown in demo.
 * @param process The process we need to print log for.
 */
void log_event(const char *event_type, struct Process *process);

/**
 * Special helper function used to log nice-related processes.
 * @param old_nice The old nice value.
 * @param process The process we need to print log for.
 */
void log_nice_event(int old_nice, struct Process *process);