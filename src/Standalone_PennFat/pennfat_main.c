#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "./pennfat.h"
#include "descriptors.h"

int main(int argc, char *argv[]) {
    char line[1024];
    
    printf("pennfat> ");
    while (fgets(line, sizeof(line), stdin)) {
        char *cmd = strtok(line, " \n");
        if (!cmd) {
            printf("pennfat> ");
            continue;
        }

        if (strcmp(cmd, "mkfs") == 0) {
            char *fs_name = strtok(NULL, " ");
            char *blocks_in_fat_str = strtok(NULL, " ");
            char *block_size_config_str = strtok(NULL, " ");

            if (!fs_name || !blocks_in_fat_str || !block_size_config_str) {
                fprintf(stderr, "Usage: mkfs FS_NAME BLOCKS_IN_FAT BLOCK_SIZE_CONFIG\n");
            } else {
                int blocks_in_fat = atoi(blocks_in_fat_str);
                int block_size_config = atoi(block_size_config_str);
                mkfs(fs_name, blocks_in_fat, block_size_config);
            }
        } else if (strcmp(cmd, "mount") == 0) {
            char *fs_name = strtok(NULL, " ");
            // Remove newline character from fs_name if present
            fs_name[strcspn(fs_name, "\n")] = 0;
            if (!fs_name) {
                fprintf(stderr, "Usage: mount FS_NAME\n");
            } else {
                mount_fs(fs_name);
            }
        } else if (strcmp(cmd, "umount") == 0) {
            unmount_fs();
        } else if (strcmp(cmd, "touch") == 0) {
            char *filename = strtok(NULL, " ");
            while (filename) {
                if (filename[strlen(filename) - 1] == '\n') {
                    filename[strlen(filename) - 1] = '\0'; // Remove newline character
                }
                touch_fs(filename);
                filename = strtok(NULL, " "); // Get the next file name
            }
        } else if (strcmp(cmd, "mv") == 0) {
            char* source = strtok(NULL, " ");
            char* dest = strtok(NULL, " ");
            if (!source || !dest) {
                fprintf(stderr, "Usage: mv SOURCE DEST\n");
            } else {
                dest[strcspn(dest, "\n")] = '\0';
                mv_fs(source, dest);
            }
        } else if (strcmp(cmd, "ls") == 0) {
            ls_fs();
        } else if (strcmp(cmd, "rm") == 0) {
            char *filename = strtok(NULL, " ");
            if (!filename) {
                fprintf(stderr, "Usage: rm FILE1 [FILE2]...\n");
            } else {
                do {
                    filename[strcspn(filename, "\n")] = '\0'; // Remove newline character
                    rm_fs(filename);
                    filename = strtok(NULL, " ");
                } while (filename != NULL);
            }
        } else if (strcmp(cmd, "cp") == 0) {
            char *arg1 = strtok(NULL, " ");
            char *arg2 = strtok(NULL, " ");
            char *arg3 = strtok(NULL, " ");

            if (!arg1 || !arg2) {
                fprintf(stderr, "Usage: cp [ -h ] SOURCE DEST\n       cp SOURCE -h DEST\n");
            } else {
                int from_host = 0;
                char *source;
                char *dest;

                if (strcmp(arg1, "-h") == 0) {
                    // Handle case: cp -h SOURCE DEST
                    from_host = 1;
                    source = arg2;
                    dest = arg3;
                } else if (arg3 != NULL && strcmp(arg2, "-h") == 0) {
                    // Handle case: cp SOURCE -h DEST
                    from_host = 2;
                    source = arg1;
                    dest = arg3;
                } else {
                    // Handle case: cp SOURCE DEST (within filesystem)
                    source = arg1;
                    dest = arg2;
                }

                if (dest == NULL) {
                    fprintf(stderr, "Invalid usage. Please check the command format.\n");
                } else {
                    cp_fs(source, dest, from_host);
                }
            }
        } else if (strcmp(cmd, "cat") == 0) {
            char *args[4]; // Adjust the size as needed
            int argc = 0;
            char *arg = strtok(NULL, " ");

            while (arg != NULL) {
                args[argc++] = arg;
                arg = strtok(NULL, " ");
            }

            for (int i = 0; i < argc; i++) {
                args[i][strcspn(args[i], "\n")] = '\0'; // Remove newline characters
            }

            cat_fs(argc, args);
        } else if (strcmp(cmd, "chmod") == 0) {
            char* filename = strtok(NULL, " ");
            char* perm = strtok(NULL, " ");
            chmod_fs(filename, perm);
        } else {
            fprintf(stderr, "Unknown command: %s\n", cmd);
        }

        printf("pennfat> ");
    }

    return 0;
}