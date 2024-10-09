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
#include <sys/stat.h>
#include "../../pennos.h"
#include "./parser.h"
#include "./linkedlist-job.h"
#include "./shell.h"
#include "./user.h"
#include "stress.h"
#include "../Standalone_PennFat/descriptors.h"

static int fg_pid = 1;

int get_fg_pid()
{
    return fg_pid;
}

void set_fg_pid(int pid)
{
    fg_pid = pid;
}

void sigint_handler()
{
    fprintf(stderr, "\n");
    if (fg_pid != 1)
        p_kill(fg_pid, p_get_sigterm_signal());
    else
    {
        int x = write(STDERR_FILENO, PROMPT, strlen(PROMPT));
        if (x == -1)
        {
            perror("writing errors\n");
        }
    }
}

void sigstp_handler()
{
    fprintf(stderr, "\n");
    if (fg_pid != 1)
        p_kill(fg_pid, p_get_sigstop_signal());
    else
    {
        int x = write(STDERR_FILENO, PROMPT, strlen(PROMPT));
        if (x == -1)
        {
            perror("writing errors\n");
        }
    }
}

bool is_shell_cmd(char *str)
{
    char *shell_func[] = {"cat", "sleep", "busy", "echo", "ls", "touch", "mv", "cp", "rm", "chmod", "ps", "kill", "zombify", "orphanify", "nice", "nice_pid", "man", "bg", "fg", "jobs", "logout"};
    for (int i = 0; i < 21; i++)
    {
        if (strcmp(str, shell_func[i]) == 0)
        {
            return true;
        }
    }
    return false;
}

void shell()
{
    while (true)
    {
        int parse = 0;
        // handles Ctrl-C behavior
        if (signal(SIGINT, sigint_handler) == SIG_ERR)
        {
            // To-do: free everything and exit
            perror("error handling signal\n");
            exit(EXIT_FAILURE);
        }
        // handles Ctrl-Z behavior
        if (signal(SIGTSTP, sigstp_handler) == SIG_ERR)
        {
            // To-do: free everything and exit
            perror("error handling signal\n");
            exit(EXIT_FAILURE);
        }
        // initialize var result to store parsed_command later
        struct parsed_command *result = NULL;
        // for noninteractive shell, we would automatically parse in the next command given that
        // it is not the end of file
        // otherwise, we need to actually read in the PROMPT that user enters
        // Read Input
        if (p_get_current()->status != ACTIVE_STAT)
        {
            continue;
        }
        int x = write(STDERR_FILENO, PROMPT, strlen(PROMPT));
        if (x == -1)
        {
            perror("writing errors\n");
        }
        const size_t maxLineLength = 4096;
        char cmd[maxLineLength];
        ssize_t numBytes = read(STDIN_FILENO, cmd, maxLineLength);
        // End string with terminator, same as Milestone
        if (numBytes <= 0)
        {
            perror("error reading\n");
            exit(EXIT_FAILURE);
        }
        if (numBytes > 0)
        {
            if (cmd[0] == '\n')
            {
                continue;
            }
        }
        // end with terminator
        cmd[numBytes] = '\0';
        parse = parse_command(cmd, &result);
        // handle parser error
        if (parse == -1)
        {
            // free up memory before exit
            if (result)
            {
                free(result);
            }
            perror("Error when parsing command");
            continue;
        }
        if (parse > 0)
        {
            char *s = "syntax error\n";
            write(STDERR_FILENO, s, 13 * sizeof(char));
            continue;
        }
        int i = 0;
        int dum1 = 0, dum2 = 0;
        if (!is_shell_cmd(result->commands[0][0]) && find_file(&dum1, &dum2, result->commands[0][0]))
        {
            int fd = f_open(result->commands[0][0], F_READ);
            char *buffer = malloc(1024 * sizeof(char));
            char temp = '\0';

            while (f_read(fd, 1, &temp) != 0)
            {
                buffer[i++] = temp;
                if (temp == '\n')
                {
                    buffer[i] = '\0';
                    struct parsed_command *read_cmd = NULL;
                    parse = parse_command(buffer, &read_cmd);
                    int char_count = 0;
                    while (result->stdout_file[char_count] && result->stdout_file[char_count] != '\0')
                    {
                        char_count++;
                    }
                    char *output = malloc(char_count + 1);
                    for (int i = 0; i < char_count; i++)
                    {
                        output[i] = result->stdout_file[i];
                    }
                    output[char_count] = '\0';
                    bool logged_out = handle(read_cmd, output);
                    if (logged_out)
                        return;
                    i = 0;
                }
            }
        }
        else
        {
            bool logged_out = handle(result, NULL);
            if (logged_out)
                return;
        }
    }
}

bool handle(struct parsed_command *result, char *output)
{
    if (result == NULL)
    {
        return false;
    }
    int ppid = -1;
    bool is_nohang = false;
    if (result->is_background)
        is_nohang = true;
    if (strcmp(result->commands[0][0], "fg") == 0)
    {
        int job_id;
        if (result->commands[0][1])
            job_id = atoi(result->commands[0][1]);
        else
            job_id = p_search_most_recent();
        if (job_id != -1)
        {
            fg_pid = job_id;
            p_search_and_remove(job_id);
            p_kill(job_id, SIGCONT);
            ppid = job_id;
        }
        else
        {
            perror("invalid pid");
        }
    }
    // bg command
    else if (strcmp(result->commands[0][0], "bg") == 0)
    {
        int job_id;
        if (result->commands[0][1])
            job_id = atoi(result->commands[0][1]);
        else
            job_id = p_search_most_recent_stop();
        if (job_id != -1)
        {
            p_kill(job_id, p_get_sigcont_signal());
        }
        else
        {
            perror("Invalid pid");
        }
        return false;
    }
    // if jobs command
    else if (strcmp(result->commands[0][0], "jobs") == 0)
    {
        char *argv[] = {"jobs", NULL};
        ppid = p_spawn(p_print_all_jobs, argv, STDIN_FILENO, STDOUT_FILENO);
    }
    else if (strcmp(result->commands[0][0], "cat") == 0)
    {
        if (!output)
        {
            int argc = 0;
            while (result->commands[0][argc])
            {
                argc++;
            }
            char **actual = malloc(sizeof(char *) * argc);
            for (int i = 1; i < argc; i++)
            {
                actual[i - 1] = result->commands[0][i];
            }
            actual[argc - 1] = NULL;
            if (result->stdout_file)
            {
                char *argv[] = {"cat", NULL};
                ppid = p_spawn_with_input(p_run_cat_fs, argv, STDIN_FILENO, STDOUT_FILENO, actual);
            }
            else
            {
                char *argv[] = {"cat", NULL};
                ppid = p_spawn_with_input(p_run_cat_fs, argv, STDIN_FILENO, STDOUT_FILENO, actual);
            }
        }
        else
        {
            int argc = 0;
            while (result->commands[0][argc])
            {
                argc++;
            }
            char **actual = malloc(sizeof(char *) * (argc + 2));
            for (int i = 1; i < argc; i++)
            {
                actual[i - 1] = result->commands[0][i];
            }
            actual[argc - 1] = "-a";
            if (result->stdout_file)
            {
                int char_count = 0;
                while (result->stdout_file[char_count] && result->stdout_file[char_count] != '\0')
                {
                    char_count++;
                }
                char *output_2 = malloc((char_count + 1) * sizeof(char));
                for (int i = 0; i < char_count; i++)
                {
                    output_2[i] = result->stdout_file[i];
                }
                output_2[char_count] = '\0';
                actual[argc] = output_2;
            }
            else
            {
                actual[argc] = output;
            }
            actual[argc + 1] = NULL;
            char *argv[] = {"cat", NULL};
            ppid = p_spawn_with_input(p_run_cat_fs, argv, STDIN_FILENO, STDOUT_FILENO, actual);
        }
    }
    else if (strcmp(result->commands[0][0], "logout") == 0)
    {
        // need change
        unmount_fs();
        return true;
    }
    else if (strcmp(result->commands[0][0], "man") == 0)
    {
        fprintf(stderr, "-- cat (S*) The usual cat from bash, etc.\n");
        fprintf(stderr, "-- sleep n (S*) sleep for n seconds.\n");
        fprintf(stderr, "-- busy (S*) busy wait indefinitely.\n");
        fprintf(stderr, "-- echo (S*) similar to echo(1) in the VM.\n");
        fprintf(stderr, "-- ls (S*) list all files in the working directory (similar to ls -il in bash), same formatting as ls in the standalone PennFAT.\n");
        fprintf(stderr, "-- touch file ... (S*) create an empty file if it does not exist, or update its timestamp otherwise.\n");
        fprintf(stderr, "-- mv src dest (S*) rename src to dest.\n");
        fprintf(stderr, "-- cp src dest (S*) copy src to dest\n");
        fprintf(stderr, "-- rm file ... (S*) remove files\n");
        fprintf(stderr, "-- chmod (S*) similar to chmod(1) in the VM\n");
        fprintf(stderr, "-- ps (S*) list all processes on PennOS. Display pid, ppid, and priority.\n");
        fprintf(stderr, "-- kill [ -SIGNAL_NAME ] pid ... (S*) send the specified signal to the specified processes, where -SIGNAL_NAME is either term (the default), stop, or cont, corresponding to S_SIGTERM, S_SIGSTOP, and S_SIGCONT, respectively. Similar to /bin/kill in the VM.\n");
        fprintf(stderr, "-- zombify (S*) execute the following code or its equivalent in your API using p_spawn.\n");
        fprintf(stderr, "-- orphanify (S*) execute the following code or its equivalent in your API using p_spawn.\n");
        fprintf(stderr, "-- nice priority command [arg] (S) set the priority of the command to priority and execute the command.\n");
        fprintf(stderr, "-- nice_pid priority pid (S) adjust the nice level of process pid to priority priority.\n");
        fprintf(stderr, "-- man (S) list all available commands.\n");
        fprintf(stderr, "-- bg [job_id] (S) continue the last stopped job, or the job specified by job_id. Note that this does mean you will need to implement the & operator in your shell.\n");
        fprintf(stderr, "-- fg [job_id] (S) bring the last stopped or backgrounded job to the foreground, or the job specified by job_id.\n");
        fprintf(stderr, "-- jobs (S) list all jobs.\n");
        fprintf(stderr, "-- logout (S) exit the shell and shutdown PennOS.\n");
        return false;
    }
    else if (strcmp(result->commands[0][0], "orphanify") == 0)
    {
        char *argv[2] = {"orphanify", NULL};
        ppid = p_spawn(p_orphanify, argv, STDIN_FILENO, STDOUT_FILENO);
    }
    else if (strcmp(result->commands[0][0], "zombify") == 0)
    {
        char *argv[2] = {"zombify", NULL};
        ppid = p_spawn(p_zombify, argv, STDIN_FILENO, STDOUT_FILENO);
    }
    else if (strcmp(result->commands[0][0], "sleep") == 0)
    {
        char *argv[] = {"sleep", result->commands[0][1], NULL};
        ppid = p_spawn(p_sleep, argv, STDIN_FILENO, STDOUT_FILENO);
    }
    else if (strcmp(result->commands[0][0], "busy") == 0)
    {
        char *argv[] = {"busy", NULL};
        ppid = p_spawn(p_busy_wait, argv, STDIN_FILENO, STDOUT_FILENO);
    }
    else if (strcmp(result->commands[0][0], "echo") == 0)
    {
        int argc = 0;
        while (result->commands[0][argc])
        {
            argc++;
        }
        char **actual = malloc(sizeof(char *) * (argc + 1));
        for (int i = 0; i < argc; i++)
        {
            actual[i] = result->commands[0][i];
        }
        actual[argc] = NULL;
        int fd = -1;

        if (result->stdout_file)
        {
            if (result->is_file_append)
            {
                fd = f_open(result->stdout_file, F_APPEND);
            }
            else
            {
                fd = f_open(result->stdout_file, F_WRITE);
            }
        }
        else
        {
            if (output)
            {

                fd = f_open(output, F_APPEND);
                if (fd == -1)
                {
                    touch_fs(output);
                    fd = f_open(output, F_APPEND);
                }

                fprintf(stderr, "%d", fd);
            }
        }
        char *argv[] = {"echo", NULL};
        ppid = p_spawn_with_input(p_run_echo, argv, STDIN_FILENO, fd, actual);
    }
    else if (strcmp(result->commands[0][0], "ls") == 0)
    {
        int argc = 0;
        while (result->commands[0][argc])
        {
            argc++;
        }

        if (argc == 1)
        {
            char *argv[] = {"ls", NULL};
            ppid = p_spawn(p_run_f_ls_null, argv, STDIN_FILENO, STDOUT_FILENO);
        }
        else
        {
            char **actual = malloc(sizeof(char *) * (argc + 1));
            for (int i = 0; i < argc; i++)
            {
                actual[i] = result->commands[0][i];
            }
            actual[argc] = NULL;
            char *argv[] = {"ls", NULL};
            ppid = p_spawn_with_input(p_run_f_ls_list, argv, STDIN_FILENO, STDOUT_FILENO, actual);
        }
    }
    else if (strcmp(result->commands[0][0], "touch") == 0)
    {
        int argc = 0;
        while (result->commands[0][argc])
        {
            argc++;
        }
        if (argc == 1)
        {
            fprintf(stderr, "No filename passed in\n");
        }
        else
        {
            char **actual = malloc(sizeof(char *) * (argc + 1));
            for (int i = 0; i < argc; i++)
            {
                actual[i] = result->commands[0][i];
            }
            actual[argc] = NULL;
            char *argv[] = {"touch", NULL};
            ppid = p_spawn_with_input(p_run_touch_fs, argv, STDIN_FILENO, STDOUT_FILENO, actual);
        }
    }
    else if (strcmp(result->commands[0][0], "mv") == 0)
    {
        int argc = 0;
        while (result->commands[0][argc])
        {
            argc++;
        }
        if (argc != 3)
        {
            fprintf(stderr, "Invalid number of input\n");
        }
        else
        {
            char *argv[] = {"mv", result->commands[0][1], result->commands[0][2], NULL};
            ppid = p_spawn(p_run_mv_fs, argv, STDIN_FILENO, STDOUT_FILENO);
        }
    }
    else if (strcmp(result->commands[0][0], "cp") == 0)
    {
        int argc = 0;
        while (result->commands[0][argc])
        {
            argc++;
        }
        if (argc != 3)
        {
            fprintf(stderr, "Invalid number of input\n");
        }
        else
        {
            char *argv[] = {"cp", result->commands[0][1], result->commands[0][2], "0", NULL};
            ppid = p_spawn(p_run_cp_fs, argv, STDIN_FILENO, STDOUT_FILENO);
        }
    }
    else if (strcmp(result->commands[0][0], "rm") == 0)
    {
        int argc = 0;
        while (result->commands[0][argc])
        {
            argc++;
        }
        if (argc < 2)
        {
            fprintf(stderr, "Invalid number of input\n");
        }
        else
        {
            char **actual = malloc(sizeof(char *) * (argc + 1));
            for (int i = 0; i < argc; i++)
            {
                actual[i] = result->commands[0][i];
            }
            actual[argc] = NULL;
            char *argv[] = {"rm", NULL};
            ppid = p_spawn_with_input(p_run_rm_fs, argv, STDIN_FILENO, STDOUT_FILENO, actual);
        }
    }
    else if (strcmp(result->commands[0][0], "chmod") == 0)
    {
        int argc = 0;
        while (result->commands[0][argc])
        {
            argc++;
        }
        if (argc != 3)
        {
            fprintf(stderr, "Invalid number of input\n");
        }
        else
        {
            char *stat = result->commands[0][1];
            char *filename = result->commands[0][2];
            char *argv[] = {"chmod", filename, stat, NULL};
            ppid = p_spawn(p_run_chmod_fs, argv, STDIN_FILENO, STDOUT_FILENO);
        }
    }
    else if (strcmp(result->commands[0][0], "ps") == 0)
    {

        char *argv[] = {"ps", NULL};
        ppid = p_spawn(p_print_all_jobs, argv, STDIN_FILENO, STDOUT_FILENO);
    }
    else if (strcmp(result->commands[0][0], "kill") == 0)
    {
        if (result->commands[0][1][0] == '-')
        {
            int argc = 0;
            while (result->commands[0][argc])
            {
                argc++;
            }
            char **temp_argv = malloc((argc + 1) * sizeof(char *));
            int index = 0;
            while (result->commands[0][index])
            {
                if (index == 1)
                {
                    if (strcmp(result->commands[0][1], "-term") == 0)
                    {
                        temp_argv[index] = p_get_sigterm_str();
                    }
                    else if (strcmp(result->commands[0][1], "-stop") == 0)
                    {
                        temp_argv[index] = p_get_sigstop_str();
                    }
                    else if (strcmp(result->commands[0][1], "-cont") == 0)
                    {
                        temp_argv[index] = p_get_sigcont_str();
                    }
                }
                else
                {
                    temp_argv[index] = result->commands[0][index];
                }
                index++;
            }
            temp_argv[index] = NULL;
            if (strcmp(result->commands[0][1], "-term") == 0)
            {
                char *argv[] = {"kill", "S_SIGTERM", NULL};
                fprintf(stderr, "debug");
                ppid = p_spawn_with_input(p_run_kill, argv, STDIN_FILENO, STDOUT_FILENO, temp_argv);
            }
            else if (strcmp(result->commands[0][1], "-stop") == 0)
            {
                char *argv[] = {"kill", "S_SIGSTOP", NULL};
                ppid = p_spawn_with_input(p_run_kill, argv, STDIN_FILENO, STDOUT_FILENO, temp_argv);
            }
            else if (strcmp(result->commands[0][1], "-cont") == 0)
            {
                char *argv[] = {"kill", "S_SIGCONT", NULL};
                ppid = p_spawn_with_input(p_run_kill, argv, STDIN_FILENO, STDOUT_FILENO, temp_argv);
            }
        }
        else
        {
            int argc = 0;
            while (result->commands[0][argc])
            {
                argc++;
            }
            fprintf(stderr, "%d", argc);
            char **temp_argv = malloc((argc + 2) * sizeof(char *));
            int index = 0;
            int index_2 = 0;
            while (result->commands[0][index_2])
            {
                if (index == 1)
                {
                    temp_argv[index] = "2";
                }
                else
                {
                    if (index == 0)
                    {
                        temp_argv[index] = result->commands[0][index];
                    }
                    else
                    {
                        temp_argv[index] = result->commands[0][index - 1];
                    }
                    index_2++;
                }
                index++;
            }
            temp_argv[index] = NULL;
            char *argv[] = {"kill", "S_SIGTERM", NULL};
            ppid = p_spawn_with_input(p_run_kill, argv, STDIN_FILENO, STDOUT_FILENO, temp_argv);
        }
    }
    else if (strcmp(result->commands[0][0], "nice") == 0)
    {
        int priority = atoi(result->commands[0][1]);
        int curr = 2;
        int length = 0;
        while (result->commands[0][curr] != NULL)
        {
            length++;
            curr++;
        }
        int pointer = 0;
        curr = 2;
        char *argv[length + 1];
        while (pointer < length)
        {
            argv[pointer] = result->commands[0][curr];
            curr++;
            pointer++;
        }
        argv[pointer] = NULL;
        if ((strcmp(argv[0], "busy") == 0))
        {
            ppid = p_spawn_with_priority(p_busy_wait, argv, STDIN_FILENO, STDOUT_FILENO, priority);
        }
        if ((strcmp(argv[0], "sleep") == 0))
        {
            ppid = p_spawn_with_priority(p_sleep, argv, STDIN_FILENO, STDOUT_FILENO, priority);
        }
    }
    else if (strcmp(result->commands[0][0], "nice_pid") == 0)
    {
        int ret = p_nice(atoi(result->commands[0][2]), atoi(result->commands[0][1]));
        if (ret == -1)
        {
            perror("Pid does not exist\n");
        }
        return false;
    }
    else if (strcmp(result->commands[0][0], "hang") == 0)
    {
        char *argv[] = {"hang", NULL};
        ppid = p_spawn(hang, argv, STDIN_FILENO, STDOUT_FILENO);
    }
    else if (strcmp(result->commands[0][0], "nohang") == 0)
    {
        char *argv[] = {"nohang", NULL};
        ppid = p_spawn(nohang, argv, STDIN_FILENO, STDOUT_FILENO);
    }
    else if (strcmp(result->commands[0][0], "recur") == 0)
    {
        char *argv[] = {"recur", NULL};
        ppid = p_spawn(recur, argv, STDIN_FILENO, STDOUT_FILENO);
    }
    else
    {
        fprintf(stderr, "Invalid command\n");
        return false;
    }
    if (result->is_background)
        p_add_background_job(ppid);
    else
        fg_pid = ppid;
    int wstatus;
    p_waitpid(ppid, &wstatus, is_nohang);
    return false;
}