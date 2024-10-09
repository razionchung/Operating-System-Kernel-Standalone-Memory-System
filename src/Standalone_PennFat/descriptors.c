#include "descriptors.h"
#include "directory.h"
#include <time.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int curr_stdin;
int curr_stdout;

OpenFileDescriptor openFileDescriptors[MAX_OPEN_FILES];  // Array of open file descriptors


int initialize_file_descriptor(DirectoryEntry* entry, int mode) {
    static OpenFileDescriptor openFileDescriptors[MAX_OPEN_FILES];
    static int isInitialized = 0;

    if (!isInitialized) {
        for (int i = 0; i < MAX_OPEN_FILES; i++) {
            openFileDescriptors[i].used = 0;
        }
        isInitialized = 1;
    }

    int fd = -1;
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (!openFileDescriptors[i].used) {
            fd = i;
            openFileDescriptors[i].used = 1;
            openFileDescriptors[i].entry = entry;
            openFileDescriptors[i].mode = mode;
            openFileDescriptors[i].cursor = (mode == F_APPEND) ? entry->size : 0;
            break;
        }
    }

    if (fd == -1) {
        fprintf(stderr, "No available file descriptors.\n");
    }

    return fd;
}


int f_open(const char *fname, int mode) {
    if (pf == NULL || pf->fs_fd == -1) {
        fprintf(stderr, "File system is not mounted.\n");
        return -1;
    }

    int stored = 0;
    int temp = 0;
    DirectoryEntry *entry = find_file(&stored, &temp, fname);
    if (!entry) {
        fprintf(stderr, "Unable to find or create directory entry for '%s'.\n", fname);
        int length = strlen(fname);
        char *filename = (char *)malloc(length + 1);
        strcpy(filename, fname);
        filename[length] = '\0';
        if (mode == F_WRITE && entry == NULL) {
            int directory_block_offset = -1;
            int directory_entry_offset = -1;
            touch_fs(filename);
            entry = find_file(&directory_block_offset, &directory_entry_offset, filename);
        } else {
            return -1;
        }
    }

    if (mode == F_READ && !(entry->perm & 4)) {
        fprintf(stderr, "Read permission denied for '%s'.\n", fname);
        return -1;
    } else if ((mode == F_WRITE || mode == F_APPEND) && !(entry->perm & 2)) {
        fprintf(stderr, "Write permission denied for '%s'.\n", fname);
        return -1;
    }

    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (openFileDescriptors[i].used && strcmp(openFileDescriptors[i].entry->name, fname) == 0) {
            fprintf(stderr, "File '%s' is already opened.\n", fname);
            return -1;
        }
    }

    int fd = -1;
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (!openFileDescriptors[i].used) {
            fd = i;
            openFileDescriptors[i].used = 1;
            openFileDescriptors[i].entry = entry;
            openFileDescriptors[i].mode = mode;

            if (mode == F_APPEND) {
                openFileDescriptors[i].cursor = entry->size;
            } else if (mode == F_WRITE) {
                // Truncate the file
                entry->size = 0;
                openFileDescriptors[i].cursor = 0;
                // Clear the content of the file
                clear_file_content(entry);
            } else {
                openFileDescriptors[i].cursor = 0;
            }

            printf("File '%s' opened successfully with fd %d.\n", fname, fd);
            return fd;
        }
    }

    fprintf(stderr, "No available file descriptors to open '%s'.\n", fname);
    return -1;
}


int f_read(int fd, int n, char *buf) {
    if (pf == NULL || pf->fs_fd == -1) {
        fprintf(stderr, "File system is not mounted.\n");
        return -1;
    }

    if (fd < 0 || fd >= MAX_OPEN_FILES || !openFileDescriptors[fd].used) {
        fprintf(stderr, "Invalid file descriptor.\n");
        return -1;
    }

    OpenFileDescriptor *opened = &openFileDescriptors[fd];
    
    if (!(opened->mode & F_READ)) {
        fprintf(stderr, "File not opened in read mode.\n");
        return -1;
    }

    int total_bytes_read = 0;
    while (n > total_bytes_read && opened->cursor < opened->entry->size) {
        unsigned int block_offset = opened->cursor % pf->block_size;
        unsigned int block_number = fetch_block_number(opened->entry->firstBlock, opened->cursor / pf->block_size);
        unsigned int offset = block_number * pf->block_size + block_offset;

        lseek(pf->fs_fd, offset, SEEK_SET);
        int bytes_to_read = (n - total_bytes_read < pf->block_size - block_offset) 
                            ? (n - total_bytes_read) : (pf->block_size - block_offset);

        int bytes_read = read(pf->fs_fd, buf + total_bytes_read, bytes_to_read);
        if (bytes_read <= 0) {
            perror("End of file / read error");
            return -1;
        }

        total_bytes_read += bytes_read;
        opened->cursor += bytes_read;
    }

    return total_bytes_read;
}


int f_write(int fd, const char *str, int n) {
    if (pf == NULL || pf->fs_fd == -1) {
        fprintf(stderr, "File system is not mounted.\n");
        return -1;
    }

    if (fd < 0 || fd >= MAX_OPEN_FILES || !openFileDescriptors[fd].used) {
        fprintf(stderr, "Invalid file descriptor.\n");
        return -1;
    }

    DirectoryEntry* curr_output_file = NULL;
    size_t directory_entries_count = pf->block_size / sizeof(DirectoryEntry);
    DirectoryEntry* entries = malloc(directory_entries_count * sizeof(DirectoryEntry));

    if (entries == NULL) {
        perror("Failed to allocate memory for directory entries");
        return -1;
    }

    unsigned int directory_block_index = 1;
    off_t directory_offset = (directory_block_index - 1) * pf->block_size + pf->fat_size;

    while (pf->fat[directory_block_index] != 0xFFFF) {
        directory_offset = (directory_block_index - 1) * pf->block_size + pf->fat_size;
        lseek(pf->fs_fd, directory_offset, SEEK_SET);
        if (read(pf->fs_fd, entries, directory_entries_count * sizeof(DirectoryEntry)) != directory_entries_count * sizeof(DirectoryEntry)) {
            perror("Failed to read directory entries");
            free(entries);
            return -1;
        }
        if (curr_output_file != NULL) {
            break;
        }
        directory_block_index = pf->fat[directory_block_index];
    }

    if (curr_output_file == NULL) {
        directory_offset = (directory_block_index - 1) * pf->block_size + pf->fat_size;
        lseek(pf->fs_fd, directory_offset, SEEK_SET);
        read(pf->fs_fd, entries, directory_entries_count * sizeof(DirectoryEntry));
    }

    OpenFileDescriptor *fileDesc = &openFileDescriptors[fd];
    DirectoryEntry *entry = fileDesc->entry;

    // Handling F_WRITE: Overwrite the file
    if (fileDesc->mode == F_WRITE) {
        // Clear existing content
        clear_file_content(entry);
        // Reset cursor to start
        fileDesc->cursor = 0;
        // Reset file size
        entry->size = 0;
    } else if (fileDesc->mode == F_APPEND) {
        // In F_APPEND mode, set cursor to the end of the file
        fileDesc->cursor = entry->size;
    }

    int bytesWritten = 0;
    unsigned int block_number = entry->firstBlock;
    unsigned int last_block_number = 0;

    while (n > bytesWritten) {
        unsigned int block_offset = fileDesc->cursor % pf->block_size;

        if (block_number == 0) {
            // Allocate new block if none exists or if current block is full
            unsigned int new_block = allocate_new_block();
            if (new_block == 0) {
                fprintf(stderr, "No more blocks available.\n");
                return -1;
            }
            if (block_number == 0) {
                entry->firstBlock = new_block; // Update the first block for the file
            } else {
                update_fat_entry(pf->fs_fd, last_block_number, new_block);
            }
            entry->size = n;
            last_block_number = new_block;
            block_number = new_block;
            block_offset = 0; // Reset block offset for new block
        }

        unsigned int offset = block_number * pf->block_size + block_offset;
        lseek(pf->fs_fd, offset, SEEK_SET);
        int bytes_to_write = (n - bytesWritten < pf->block_size - block_offset) ? 
                     (n - bytesWritten) : (pf->block_size - block_offset);
        int written = write(pf->fs_fd, str + bytesWritten, bytes_to_write);

        if (written < 0) {
            perror("Write error");
            return -1;
        }

        bytesWritten += written;
        fileDesc->cursor += written;
        entry->size = (entry->size < fileDesc->cursor) ? fileDesc->cursor : entry->size;
    }

    // Update the directory entry
    update_directory_entry(pf->fs_fd, entry);
    return bytesWritten;
}


void clear_file_content(DirectoryEntry *entry) {
    unsigned int block_number = entry->firstBlock;

    while (block_number != 0 && block_number != 0xFFFF) {
        unsigned int offset = block_number * pf->block_size;
        char clearBuffer[pf->block_size];
        memset(clearBuffer, 0, sizeof(clearBuffer));
        lseek(pf->fs_fd, offset, SEEK_SET);
        write(pf->fs_fd, clearBuffer, sizeof(clearBuffer));

        unsigned int next_block = pf->fat[block_number];
        pf->fat[block_number] = 0;
        block_number = next_block;
    }

    entry->firstBlock = 0;
}


int f_close(int fd) {
    if (fd < 0 || fd >= MAX_OPEN_FILES || !openFileDescriptors[fd].used) {
        fprintf(stderr, "Invalid file descriptor.\n");
        return -1;
    }

    openFileDescriptors[fd].used = 0;
    openFileDescriptors[fd].entry = NULL;
    openFileDescriptors[fd].cursor = 0;
    openFileDescriptors[fd].mode = 0;

    return 0; // Success
}

int f_unlink(const char *fname) {
    size_t directory_entries_count = pf->block_size / sizeof(DirectoryEntry);
    DirectoryEntry* entries = malloc(directory_entries_count * sizeof(DirectoryEntry));

    if (entries == NULL) {
        perror("Failed to allocate memory for directory entries");
        return -1;
    }

    off_t directory_offset = pf->fat_size;
    unsigned int directory_block_index = 1;
    int found = 0;

    while (pf->fat[directory_block_index] != 0xFFFF) {
        if (directory_block_index != 1) {
            directory_offset = (directory_block_index - 1) * pf->block_size + pf->fat_size;
        }

        lseek(pf->fs_fd, directory_offset, SEEK_SET);
        read(pf->fs_fd, entries, directory_entries_count * sizeof(DirectoryEntry));

        for (size_t i = 0; i < directory_entries_count; i++) {
            if (strcmp(entries[i].name, fname) == 0) {
                entries[i].name[0] = 1; // Mark as deleted
                found = 1;
                break;
            }
        }

        if (!found) {
            directory_block_index = pf->fat[directory_block_index];
        } else {
            break;
        }
    }

    if (!found) {
        if (directory_block_index != 1) {
            directory_offset = (directory_block_index - 1) * pf->block_size + pf->fat_size;
        }

        lseek(pf->fs_fd, directory_offset, SEEK_SET);
        read(pf->fs_fd, entries, directory_entries_count * sizeof(DirectoryEntry));

        for (size_t i = 0; i < directory_entries_count; i++) {
            if (strcmp(entries[i].name, fname) == 0) {
                entries[i].name[0] = 1; // Mark as deleted
                found = 1;
                break;
            }
        }

        if (!found) {
            fprintf(stderr, "File not found.\n");
            free(entries);
            return -1;
        }
    }

    // Write the updated directory entries back to the filesystem
    lseek(pf->fs_fd, directory_offset, SEEK_SET);

    if (write(pf->fs_fd, entries, directory_entries_count * sizeof(DirectoryEntry)) < 0) {
        perror("Failed to write the updated directory block");
    }

    free(entries);
    return 0;
}

int f_lseek(int fd, int offset, int whence) {
    if (fd < 0 || fd >= MAX_OPEN_FILES || !openFileDescriptors[fd].used) {
        fprintf(stderr, "Invalid file descriptor.\n");
        return -1;
    }

    OpenFileDescriptor *fileDesc = &openFileDescriptors[fd];
    int new_position;

    switch (whence) {
        case F_SEEK_SET:
            new_position = offset;
            break;
        case F_SEEK_CUR:
            new_position = fileDesc->cursor + offset;
            break;
        case F_SEEK_END:
            new_position = fileDesc->entry->size + offset;
            break;
        default:
            fprintf(stderr, "Invalid seek option.\n");
            return -1;
    }

    if (new_position < 0) {
        fprintf(stderr, "Seek position is out of bounds.\n");
        return -1;
    }

    fileDesc->cursor = new_position;
    return new_position;
}


void f_ls(const char *filename) {
    // Read directory entries into an array
    size_t directory_entries_count = pf->block_size / sizeof(DirectoryEntry);
    DirectoryEntry* entries = malloc(directory_entries_count * sizeof(DirectoryEntry));

    if (entries == NULL) {
        perror("Failed to allocate memory for directory entries");
        return;
    }

    off_t directory_offset = pf->fat_size;
    unsigned int directory_block_index = 1;
    while (pf->fat[directory_block_index] != 0xFFFF) {
        if (directory_block_index != 1) {
            directory_offset = (directory_block_index - 1) * pf->block_size + pf->fat_size;
        }

        lseek(pf->fs_fd, directory_offset, SEEK_SET);
        read(pf->fs_fd, entries, directory_entries_count * sizeof(DirectoryEntry));

        for (size_t i = 0; i < directory_entries_count; i++) {
            // Skip if the entry is empty or deleted
            if (entries[i].name[0] == '\0' || entries[i].name[0] == 1) {
                continue;
            }

            // Convert time to human-readable format
            struct tm *tm = localtime(&entries[i].mtime);
            char timeString[100];
            strftime(timeString, sizeof(timeString), "%b %d %H:%M", tm);

            // Convert permissions to a string
            char permString[4];
            sprintf(permString, "%c%c%c",
                    (entries[i].perm & 4) ? 'r' : '-',
                    (entries[i].perm & 2) ? 'w' : '-',
                    (entries[i].perm & 1) ? 'x' : '-');

            if (filename == NULL || strcmp(filename, entries[i].name) == 0) {
                    printf("%d %s %d %s %s\n", entries[i].firstBlock, permString, entries[i].size, timeString, entries[i].name);
                    if (filename != NULL) break;
                }
        }

        directory_block_index = pf->fat[directory_block_index];
    }

    if (directory_block_index != 1) {
        directory_offset = (directory_block_index - 1) * pf->block_size + pf->fat_size;
    }

    lseek(pf->fs_fd, directory_offset, SEEK_SET);
    read(pf->fs_fd, entries, directory_entries_count * sizeof(DirectoryEntry));

    for (size_t i = 0; i < directory_entries_count; i++) {
        if (entries[i].name[0] == '\0' || entries[i].name[0] == 1) {
            continue;
        }
        
        struct tm *tm = localtime(&entries[i].mtime);
        char timeString[100];
        strftime(timeString, sizeof(timeString), "%b %d %H:%M", tm);

        char permString[4];
        sprintf(permString, "%c%c%c", 
            (entries[i].perm & 4) ? 'r' : '-', 
            (entries[i].perm & 2) ? 'w' : '-', 
            (entries[i].perm & 1) ? 'x' : '-');

        if (filename == NULL || strcmp(filename, entries[i].name) == 0) {
            printf("%d %s %d %s %s\n", entries[i].firstBlock, permString, entries[i].size, timeString, entries[i].name);
            if (filename != NULL) break;
        }

    }

    free(entries);
}

int f_dup2(int fd_curr, int fd_new) {
    if (fd_curr == 0) {
        curr_stdin = fd_new;
        return fd_new;
    } else if (fd_curr == 1) {
        curr_stdout = fd_new;
        return fd_new;
    } else {
        return -1;
    }
}

int find_descriptor_by_name(const char *fname) {
    if (fname == NULL) {
        fprintf(stderr, "File name is NULL.\n");
        return -1;
    }

    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (openFileDescriptors[i].used && strcmp(openFileDescriptors[i].entry->name, fname) == 0) {
            return i;
        }
    }

    fprintf(stderr, "No open file with name '%s' found.\n", fname);
    return -1;
}