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
#include "../../pennos.h"
#include "./parser.h"
#include "./linkedlist-job.h"
#include <sys/stat.h>
#include "./stress.h"

/**
 * The helper function we used to get the current foreground job pid.
 * @return The current foreground job pid.
 */
int get_fg_pid();

/**
 * The function we used to create an interactive shell. With this we can spawn a process with shell.
 */
void shell();

/**
 * The helper function we used to set the foreground job pid.
 * @param pid The pid we set as the foreground job pid.
 */
void set_fg_pid(int pid);

bool handle(struct parsed_command *result, char *output);