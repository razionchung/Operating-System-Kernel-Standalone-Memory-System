#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include "./pennfat.h"
#include "./descriptors.h"

struct PennFAT *pf = NULL;  // Definition

void mkfs(char *fs_name, int blocks_in_fat, int block_size_config) {
    // Validate block_size_config
    if (block_size_config < 0 || block_size_config > 4) {
        block_size_config = 0;
    }

    int total_fs_size = 0;

    // Calculate the block size
    int block_size = BLOCK_SIZES[block_size_config];

    // Calculate the FAT size
    int fat_size = block_size * blocks_in_fat;

    // Calculate the number of data blocks (FAT entries - 1)
    int num_data_blocks = (fat_size / 2) - 1;

    // Calculate the total file system size
    if (blocks_in_fat == 32 && block_size_config == 4) {
        total_fs_size = block_size * (blocks_in_fat - 1) + block_size * num_data_blocks;
    } else {
        total_fs_size = fat_size + (block_size * num_data_blocks);
    }

    // Open the filesystem file
    int fs_fd = open(fs_name, O_CREAT | O_RDWR, 0666);
    if (fs_fd == -1) {
        perror("Error opening file system file");
        return;
    }

    // Set the file size
    if (ftruncate(fs_fd, total_fs_size) == -1) {
        perror("Error setting file system size");
        close(fs_fd);
        return;
    }

    // Initialize the first bytes of the FAT
    unsigned char init_bytes[] = {block_size_config, (unsigned char)blocks_in_fat, 0xff, 0xff};
    if (write(fs_fd, init_bytes, sizeof(init_bytes)) == -1) {
        perror("Error writing initial bytes");
        close(fs_fd);
        return;
    }

    close(fs_fd);
}


void mount_fs(char *fs_name) {
    pf = malloc(sizeof(struct PennFAT));


    if (pf == NULL) {
        perror("Failed to allocate memory for PennFAT");
        return;
    }

    pf->fs_fd = open(fs_name, O_RDWR);
    if (pf->fs_fd == -1) {
        perror("Error opening file system file");
        free(pf);
        return;
    }

    uint16_t first_fat_entry;
    if (read(pf->fs_fd, &first_fat_entry, sizeof(first_fat_entry)) != sizeof(first_fat_entry)) {
        perror("Error reading first FAT entry");
        close(pf->fs_fd);
        free(pf);
        return;
    }

    int block_size_config = first_fat_entry & 0x00FF;
    int blocks_in_fat = (first_fat_entry & 0xFF00) >> 8;
    if (block_size_config < 0 || block_size_config > 4) {
        block_size_config = 0;
    }
    pf->block_size = BLOCK_SIZES[block_size_config];
    pf->fat_size = pf->block_size * blocks_in_fat;

    pf->fat = mmap(NULL, pf->fat_size, PROT_READ | PROT_WRITE, MAP_SHARED, pf->fs_fd, 0);
    if (pf->fat == MAP_FAILED) {
        fprintf(stderr, "Error mapping FAT: %s\n", strerror(errno));
        close(pf->fs_fd);
        free(pf);
        return;
    }

}


void unmount_fs() {
    if (pf == NULL) {
        fprintf(stderr, "No file system is currently mounted.\n");
        return;
    }

    // Unmap the FAT from memory
    if (pf->fat != NULL) {
        if (munmap(pf->fat, pf->fat_size) == -1) {
            perror("Error unmapping FAT");
        }
        pf->fat = NULL; // Reset the pointer after successful unmapping
    }

    // Close the file descriptor
    if (pf->fs_fd != -1) {
        close(pf->fs_fd);
        pf->fs_fd = -1; // Reset the file descriptor
    }

    // Free the PennFAT structure
    free(pf);
    pf = NULL; // Reset the global pointer to indicate the file system is unmounted
}


uint16_t touch_fs(char* filename) {
    if (pf == NULL || pf->fs_fd == -1) {
        fprintf(stderr, "File system is not mounted. PF: %p, FD: %d\n", (void*)pf, pf ? pf->fs_fd : -1);
        return -1;
    }

    filename[strcspn(filename, "\n")] = 0;

    // Calculate the number of directory entries
    size_t directory_entries_count = pf->block_size / sizeof(DirectoryEntry);

    // Read all directory entries into an array
    DirectoryEntry* entries = malloc(directory_entries_count * sizeof(DirectoryEntry));
    if (entries == NULL) {
        perror("Failed to allocate memory for directory entries");
        return -1;
    }

    off_t directory_offset = pf->fat_size; // Assuming directory entries start right after the FAT
    unsigned int directory_block_index = 1;
    while (pf->fat[directory_block_index] != 0xFFFF) {
        if (directory_block_index != 1) {
            directory_offset = (directory_block_index - 1) * pf->block_size + pf->fat_size;
        }
        lseek(pf->fs_fd, directory_offset, SEEK_SET);
        read(pf->fs_fd, entries, directory_entries_count * sizeof(DirectoryEntry));

        // Check if the file already exists
        DirectoryEntry* entry = NULL;
        for (size_t i = 0; i < directory_entries_count; i++) {
            if (strcmp(entries[i].name, filename) == 0) {
                entry = &entries[i];
                entry->mtime = time(NULL);
                break;
            }
        }

        if (entry == NULL) {
            // File does not exist, find an empty slot for a new directory entry
            for (size_t i = 0; i < directory_entries_count; i++) {
                if (entries[i].name[0] == '\0' || entries[i].name[0] == 1) { // Empty or deleted entry
                    entry = &entries[i];
                    strncpy(entry->name, filename, sizeof(entry->name));
                    entry->name[sizeof(entry->name) - 1] = '\0';
                    entry->size = 0;
                    entry->firstBlock = 0; // Update as needed
                    entry->type = 1;       // Assuming regular file
                    entry->perm = 6;       // Assuming read-write
                    entry->mtime = time(NULL);
                    break;
                }
            }
        }
        if (entry != NULL) {
            break;
        }
        directory_block_index = pf->fat[directory_block_index];
    }

    if (directory_block_index != 1) {
        directory_offset = (directory_block_index - 1) * pf->block_size + pf->fat_size;
    }
    lseek(pf->fs_fd, directory_offset, SEEK_SET);
    read(pf->fs_fd, entries, directory_entries_count * sizeof(DirectoryEntry));

    // Check if the file already exists
    DirectoryEntry* entry = NULL;
    for (size_t i = 0; i < directory_entries_count; i++) {
        if (strcmp(entries[i].name, filename) == 0) {
            entry = &entries[i];
            entry->mtime = time(NULL);
            break;
        }
    }
    if (entry == NULL) {
        // File does not exist, find an empty slot for a new directory entry
        for (size_t i = 0; i < directory_entries_count; i++) {
            if (entries[i].name[0] == '\0' || entries[i].name[0] == 1) { // Empty or deleted entry
                entry = &entries[i];
                strncpy(entry->name, filename, sizeof(entry->name));
                entry->name[sizeof(entry->name) - 1] = '\0';
                entry->size = 0;
                entry->firstBlock = 0; // Update as needed
                entry->type = 1;       // Assuming regular file
                entry->perm = 6;       // Assuming read-write
                entry->mtime = time(NULL);
                break;
            }
        }
    }
    if (entry == NULL) {
        uint16_t new_block_index = find_free_block();
        // printf("New block for directory found %d\n", new_block_index);
        if (new_block_index == (uint16_t)-1) {
            fprintf(stderr, "No free block available.\n");
            free(entries);
            return -1;
        }
        pf->fat[directory_block_index] = new_block_index;
        off_t new_directory_offset = (new_block_index - 1) * pf->block_size + pf->fat_size;
        DirectoryEntry* new_entries = calloc(directory_entries_count, sizeof(DirectoryEntry));
        if (new_entries == NULL) {
            perror("Failed to allocate memory for new directory block");
            free(entries);
            return -1;
        }

        // Update the FAT to indicate the block is in use
        pf->fat[new_block_index] = 0xFFFF;  // Assuming this marks the block as used

        // Add the new directory entry to the new block
        strncpy(new_entries[0].name, filename, sizeof(new_entries[0].name));
        new_entries[0].name[sizeof(new_entries[0].name) - 1] = '\0';
        new_entries[0].size = 0;
        new_entries[0].firstBlock = 0;  // Update as needed
        new_entries[0].type = 1;        // Assuming regular file
        new_entries[0].perm = 6;        // Assuming read-write
        new_entries[0].mtime = time(NULL);

        // Write the new block to the file system
        lseek(pf->fs_fd, new_directory_offset, SEEK_SET);
        if (write(pf->fs_fd, new_entries, directory_entries_count * sizeof(DirectoryEntry)) < 0) {
            perror("Failed to write the new directory block");
        }
        // DirectoryEntry* new_entry = &new_entries[0];
        free(new_entries);
        return new_block_index;
    }

    // Write back the updated entries to the file system
    lseek(pf->fs_fd, directory_offset, SEEK_SET);
    if (write(pf->fs_fd, entries, directory_entries_count * sizeof(DirectoryEntry)) < 0) {
        perror("Failed to write the updated directory block");
    }

    free(entries);
    return directory_block_index;
}


void ls_fs() {
    if (pf == NULL || pf->fs_fd == -1) {
        fprintf(stderr, "File system is not mounted.\n");
        return;
    }

    // Read directory entries into an array
    size_t directory_entries_count = pf->block_size / sizeof(DirectoryEntry);
    DirectoryEntry* entries = malloc(directory_entries_count * sizeof(DirectoryEntry));
    if (entries == NULL) {
        perror("Failed to allocate memory for directory entries");
        return;
    }

    off_t directory_offset = pf->fat_size; // Assuming directory entries start right after the FAT
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

            printf("%d %s %d %s %s\n", entries[i].firstBlock, permString, entries[i].size, timeString, entries[i].name);
        }
        directory_block_index = pf->fat[directory_block_index];
    }


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

        printf("%d %s %d %s %s\n", entries[i].firstBlock, permString, entries[i].size, timeString, entries[i].name);
    }


    free(entries);
}


DirectoryEntry* find_file(int *directory_block_offset, int *directory_entry_offset, const char* filename) {
    if (pf == NULL || pf->fs_fd == -1) {
        fprintf(stderr, "File system is not mounted.\n");
        return NULL;
    }
    size_t directory_entries_count = pf->block_size / sizeof(DirectoryEntry);
    DirectoryEntry* entries = malloc(directory_entries_count * sizeof(DirectoryEntry));
    if (entries == NULL) {
        perror("Failed to allocate memory for directory entries");
        return NULL;
    }


    unsigned int directory_block_index = 1;
    off_t directory_offset = (directory_block_index - 1) * pf->block_size + pf->fat_size;


    while (pf->fat[directory_block_index] != 0xFFFF) {
        directory_offset = (directory_block_index - 1) * pf->block_size + pf->fat_size;
        lseek(pf->fs_fd, directory_offset, SEEK_SET);
        if (read(pf->fs_fd, entries, directory_entries_count * sizeof(DirectoryEntry)) != directory_entries_count * sizeof(DirectoryEntry)) {
            perror("Failed to read directory entries");
            free(entries);
            return NULL;
        }


        for (size_t i = 0; i < directory_entries_count; i++) {
            if (strcmp(entries[i].name, filename) == 0) {
                // Caller is responsible for freeing this memory
                // printf("FOUND\n");
                *directory_block_offset = directory_offset;
                *directory_entry_offset = i;
                return &entries[i];
            }
        }


        directory_block_index = pf->fat[directory_block_index];
    }
    directory_offset = (directory_block_index - 1) * pf->block_size + pf->fat_size;
    lseek(pf->fs_fd, directory_offset, SEEK_SET);
    read(pf->fs_fd, entries, directory_entries_count * sizeof(DirectoryEntry));
    DirectoryEntry* entry = NULL;
    for (size_t i = 0; i < directory_entries_count; i++) {
        if (strcmp(entries[i].name, filename) == 0) {
            entry = &entries[i];
            entry->mtime = time(NULL);
            *directory_block_offset = directory_offset;
            *directory_entry_offset = i;
            // printf("FOUND\n");
            return entry;
        }
    }
    free(entries);
    // printf("NOT FOUND\n");
    return NULL;
}


void mv_fs(const char* source, const char* dest) {

    if (pf == NULL || pf->fs_fd == -1) {
        fprintf(stderr, "mv_fs: File system is not mounted.\n");
        return;
    }
    int dest_directory_block_offset = 0;
    int dest_directory_entry_offset = 0;
    int source_directory_block_offset = 0;
    int source_directory_entry_offset = 0;
    DirectoryEntry* source_file = find_file(&source_directory_block_offset, &source_directory_entry_offset, source);
    if (source_file == NULL) {
        fprintf(stderr, "mv_fs: Source does not exist\n");
        return;
    }
    DirectoryEntry* dest_file = find_file(&dest_directory_block_offset, &dest_directory_entry_offset, dest);
    if (dest_file != NULL) {
        rm_fs(dest);
    }
    strncpy(source_file->name, dest, sizeof(source_file->name) - 1);
    source_file->name[sizeof(source_file->name) - 1] = '\0';
    source_file->mtime = time(NULL);

    // Write the updated directory entries back to the filesystem
    lseek(pf->fs_fd, source_directory_block_offset + source_directory_entry_offset * sizeof(DirectoryEntry), SEEK_SET);
    write(pf->fs_fd, source_file, sizeof(DirectoryEntry));
    
    // free(entries);
}


void trim_newline(char *str) {
    size_t len = strlen(str);
    if (len > 0 && str[len - 1] == '\n') {
        str[len - 1] = '\0';
    }
}


void cp_fs(char* source, char* dest, int from_host) {
    // printf("from_host is %d\n", from_host);
    source[strcspn(source, "\n")] = 0;
    dest[strcspn(dest, "\n")] = 0;
    if (from_host == 1) {
        // cp -h SOURCE DEST
        FILE* file = fopen(source, "r");
        if (file == NULL) {
            perror("Error opening file");
            return;
        }
        // printf("source is %s, dest is %s\n", source, dest);
        int directory_block_offset = 0;
        int directory_entry_offset = 0;
        DirectoryEntry* dest_entry = find_file(&directory_block_offset, &directory_entry_offset, dest);
        // fprintf(stderr, "\n1\n");
        if (dest_entry == NULL) {
            touch_fs(dest);
            dest_entry = find_file(&directory_block_offset, &directory_entry_offset, dest);
            if (dest_entry == NULL) {
                perror("Failed creating a new output file");
                return;
            }
        }
        if (dest_entry->firstBlock == 0) {
            // fprintf(stderr, "\n2\n");
            uint16_t new_block_index = find_free_block();
            dest_entry->firstBlock = new_block_index;
            pf->fat[new_block_index] = 0xFFFF;
        }
        uint16_t output_index = dest_entry->firstBlock;
        uint16_t output_block_offset = output_index * pf->block_size;
        char buffer[pf->block_size];
        size_t bytes_read;
        size_t total_bytes_read = 0;
        uint16_t temp_index = 0;
        int i = 0;
        int total_fs_size = pf->fat_size + (pf->block_size * (pf->fat_size / 2 - 1));
        // printf("total_fs_size is %d\n", total_fs_size);
        while ((bytes_read = fread(buffer, sizeof(char), pf->block_size, file)) > 0) {
            // printf("READING\n");
            i++;
            total_bytes_read += bytes_read;
            // printf("bytes_read is %zu, i is %d\n", bytes_read, i);
            lseek(pf->fs_fd, output_block_offset, SEEK_SET);
            write(pf->fs_fd, buffer, bytes_read);
            // printf("total_bytes_read is %zu\n", total_bytes_read);
            // printf("output_index is %d\n", output_index);


            if (output_index >= pf->block_size / 2 - 1) {
                printf("almost out of fat space!\n");
                break;
            }
            if (pf->fat[output_index] == 0xFFFF) {
                uint16_t new_block_index = find_free_block();
                pf->fat[output_index] = new_block_index;
                pf->fat[new_block_index] = 0xFFFF;
            }
            // index incorrect
            temp_index = output_index;
            output_index = pf->fat[output_index];
            output_block_offset = output_index * pf->block_size;
        }
        if (temp_index >= pf->block_size / 2 - 2) {
            pf->fat[temp_index + 1] = 0xFFFF;
            unsigned short num = total_fs_size - pf->block_size;
            pf->fat[temp_index] = ((num >> 8) & 0x00FF) | ((num << 8) & 0xFF00);
        } else {
            pf->fat[pf->fat[temp_index]] = 0;
            pf->fat[temp_index] = 0xFFFF;
        }
        dest_entry->size = total_bytes_read;
        dest_entry->mtime = time(NULL);
        // printf("directory_block_offset is %d\n", directory_block_offset);
        // printf("directory_entry_offset is %d\n", directory_entry_offset);
        lseek(pf->fs_fd, directory_block_offset + directory_entry_offset * 64, SEEK_SET);
        write(pf->fs_fd, dest_entry, sizeof(DirectoryEntry));
        fclose(file);
    } else if (from_host == 2) {
        // cp SOURCE -h DEST
        FILE* file = fopen(dest, "w");
        if (file == NULL) {
            perror("Error opening file");
            return;
        }
        // printf("source is %s, dest is %s\n", source, dest);
        int directory_block_offset = 0;
        int directory_entry_offset = 0;
        DirectoryEntry* source_entry = find_file(&directory_block_offset, &directory_entry_offset, source);
        if (source_entry == NULL) {
            perror("Source file does not exist");
            return;
        }
        if (source_entry->firstBlock == 0) {
            perror("Source file is empty");
            return;
        }
        uint16_t input_index = source_entry->firstBlock;
        uint16_t input_block_offset = input_index * pf->block_size;
        char buffer[pf->block_size];
        while (pf->fat[input_index] != 0xFFFF) {
            lseek(pf->fs_fd, input_block_offset, SEEK_SET);
            read(pf->fs_fd, buffer, pf->block_size);
            fwrite(buffer, sizeof(char), pf->block_size, file);
            input_index = pf->fat[input_index];
            input_block_offset = input_index * pf->block_size;
        }
        lseek(pf->fs_fd, input_block_offset, SEEK_SET);
        read(pf->fs_fd, buffer, pf->block_size);
        fwrite(buffer, sizeof(char), pf->block_size, file);
        fclose(file);
    } else {
        // cp SOURCE DEST
        char* mode = "-w";
        trim_newline(dest);
        char *argv[3];
        argv[0] = source;
        argv[1] = mode;
        argv[2] = dest;
        int temp1 = 0;
        int temp2 = 0;
        if (find_file(&temp1, &temp2, source) == NULL) {
            perror("Source input does not exist.\n");
            return;
        }
        cat_fs(3, argv);
    }
}


void rm_fs(const char* filename) {
    if (pf == NULL || pf->fs_fd == -1) {
        fprintf(stderr, "File system is not mounted.\n");
        return;
    }

    size_t directory_entries_count = pf->block_size / sizeof(DirectoryEntry);
    DirectoryEntry* entries = malloc(directory_entries_count * sizeof(DirectoryEntry));
    if (entries == NULL) {
        perror("Failed to allocate memory for directory entries");
        return;
    }

    off_t directory_offset = pf->fat_size;
    unsigned int directory_block_index = 1;
    int found = 0;
    uint16_t data_block_index = -1;
    while (pf->fat[directory_block_index] != 0xFFFF) {
        if (directory_block_index != 1) {
            directory_offset = (directory_block_index - 1) * pf->block_size + pf->fat_size;
        }
        lseek(pf->fs_fd, directory_offset, SEEK_SET);
        read(pf->fs_fd, entries, directory_entries_count * sizeof(DirectoryEntry));
        for (size_t i = 0; i < directory_entries_count; i++) {
            if (strcmp(entries[i].name, filename) == 0) {
                entries[i].name[0] = 1; // Mark as deleted
                found = 1;
                data_block_index = entries[i].firstBlock;
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
            if (strcmp(entries[i].name, filename) == 0) {
                entries[i].name[0] = 1; // Mark as deleted
                found = 1;
                data_block_index = entries[i].firstBlock;
                break;
            }
        }

        if (!found) {
            fprintf(stderr, "File not found.\n");
            free(entries);
            return;
        }
    }
    if (data_block_index != 0) {
        uint16_t later_index = pf->fat[data_block_index];
        pf->fat[data_block_index] = 0;
        while (later_index != 0xFFFF) {
            uint16_t temp = later_index;
            later_index = pf->fat[later_index];
            pf->fat[temp] = 0;
        }
    }

    // Write the updated directory entries back to the filesystem
    lseek(pf->fs_fd, directory_offset, SEEK_SET);
    if (write(pf->fs_fd, entries, directory_entries_count * sizeof(DirectoryEntry)) < 0) {
        perror("Failed to write the updated directory block");
    }

    free(entries);
}


void cat_fs(int argc, char **argv) {
    if (argc < 1) {
        fprintf(stderr, "Usage: cat FILE... [-w OUTPUT_FILE] [-a OUTPUT_FILE]\n");
        return;
    }

    if (pf == NULL || pf->fs_fd == -1) {
        fprintf(stderr, "File system is not mounted.\n");
        return;
    }

    int mode = F_WRITE; // Default mode
    char *output_file = NULL;
    int fileArgsStart = 0;

    if (argc >= 3 && (strcmp(argv[argc - 2], "-w") == 0 || strcmp(argv[argc - 2], "-a") == 0)) {
        mode = (strcmp(argv[argc - 2], "-w") == 0) ? F_WRITE : F_APPEND;
        output_file = argv[argc - 1];
        fileArgsStart = 0;
    } else if (argc == 2 && (strcmp(argv[0], "-w") == 0 || strcmp(argv[0], "-a") == 0)) {
        mode = (strcmp(argv[0], "-w") == 0) ? F_WRITE : F_APPEND;
        output_file = argv[1];
        fileArgsStart = argc; // Set to argc so that loop for file concatenation doesn't execute
    } else {
        fileArgsStart = 0;
    }

    if (output_file) {
        DirectoryEntry* curr_output_file = NULL;
        size_t directory_entries_count = pf->block_size / sizeof(struct DirectoryEntry);
        DirectoryEntry* entries = malloc(directory_entries_count * sizeof(struct DirectoryEntry));
        if (entries == NULL) {
            perror("Failed to allocate memory for directory entries");
            return;
        }

        unsigned int directory_block_index = 1;
        off_t directory_offset = (directory_block_index - 1) * pf->block_size + pf->fat_size;

        while (pf->fat[directory_block_index] != 0xFFFF) {
            directory_offset = (directory_block_index - 1) * pf->block_size + pf->fat_size;
            lseek(pf->fs_fd, directory_offset, SEEK_SET);
            if (read(pf->fs_fd, entries, directory_entries_count * sizeof(DirectoryEntry)) != directory_entries_count * sizeof(DirectoryEntry)) {
                perror("Failed to read directory entries");
                free(entries);
                return;
            }

            for (size_t i = 0; i < directory_entries_count; i++) {
                if (strcmp(entries[i].name, output_file) == 0) {
                    // Caller is responsible for freeing this memory
                    curr_output_file = &entries[i];
                    break;
                }
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
            for (size_t i = 0; i < directory_entries_count; i++) {
                if (strcmp(entries[i].name, output_file) == 0) {
                    curr_output_file = &entries[i];
                    curr_output_file->mtime = time(NULL);
                    // printf("File named %s exists\n", output_file);
                    break;
                }
            }
        }
        int directory_block_offset = -1;
        int directory_entry_offset = -1;
        // curr_output_file is NULL if it doesn't already exist
        if (curr_output_file != NULL) {
            // check read/write permissions
            switch (mode) {
                case F_READ:
                    if (curr_output_file->perm < 4) {
                        fprintf(stderr, "Permission denied: cannot open '%s' for reading.\n", output_file);
                        return;
                    }
                    break;
                case F_WRITE:
                    if (curr_output_file->perm != 2 && curr_output_file->perm != 6 && curr_output_file->perm != 7) {
                        fprintf(stderr, "Permission denied: cannot open '%s' for writing.\n", output_file);
                        return;
                    }
                case F_APPEND:
                    if (curr_output_file->perm != 2 && curr_output_file->perm != 6 && curr_output_file->perm != 7) { // Check write permission
                        fprintf(stderr, "Permission denied: cannot open '%s' for writing.\n", output_file);
                        return;
                    }
                    break;
            }
        } else {
            if (output_file[strlen(output_file) - 1] == '\n') {
                output_file[strlen(output_file) - 1] = '\0'; // Remove newline character
            }
            touch_fs(output_file);
            curr_output_file = find_file(&directory_block_offset, &directory_entry_offset, output_file);
            if (curr_output_file == NULL) {
                perror("Failed creating a new output file");
                return;
            }
        }
        // If the file doesn't contain any content (touch'd), we need to identify
        // its firstBlock location
        if (curr_output_file->firstBlock == 0) {
            uint16_t new_block_index = find_free_block();
            // printf("new_block_index is %d\n", new_block_index);;
            curr_output_file->firstBlock = new_block_index;
            pf->fat[new_block_index] = 0xFFFF;
        }
        // printf("First block of output file is %d\n", curr_output_file->firstBlock);
        uint16_t output_index = curr_output_file->firstBlock;
        uint16_t output_block_offset = output_index * pf->block_size;
        // printf("output_block_offset is %d\n", output_block_offset);
        ssize_t block_write_offset = curr_output_file->size % pf->block_size;
        uint16_t remaining_free_bytes = pf->block_size - block_write_offset;


        // WRITE -w
        if (mode == F_WRITE) {
            char zeroBuffer[pf->block_size];
            memset(zeroBuffer, 0, pf->block_size);
            lseek(pf->fs_fd, curr_output_file->firstBlock * pf->block_size, SEEK_SET);
            if (write(pf->fs_fd, zeroBuffer, pf->block_size) == -1) {
                perror("Error clearing the block");
                return;
            }
            uint16_t later_index = pf->fat[output_index];
            pf->fat[output_index] = 0xFFFF;
            while (later_index != 0xFFFF) {
                lseek(pf->fs_fd, later_index * pf->block_size, SEEK_SET);
                if (write(pf->fs_fd, zeroBuffer, pf->block_size) == -1) {
                    perror("Error clearing the block");
                    return;
                }
                uint16_t temp = later_index;
                later_index = pf->fat[later_index];
                pf->fat[temp] = 0;
            }
            
        // APPEND -a
        } else if (mode == F_APPEND) {
            while (pf->fat[output_index] != 0xFFFF) {
                output_index = pf->fat[output_index];
            }
            output_block_offset = output_index * pf->block_size;
        }


        // cat FILE ... [ -w OUTPUT_FILE ]
        // cat FILE ... [ -a OUTPUT_FILE ]
        uint16_t total_bytes = 0;
        if (fileArgsStart != argc) {
            for (int i = fileArgsStart; i < argc - 2; i++) {
                char* input_file = argv[i];
                // printf("input_file is %s", input_file);
                char input_buffer[pf->block_size];
                int directory_entry_offset = -1;
                int directory_block_offset = -1;
                DirectoryEntry* input_entry = find_file(&directory_block_offset, &directory_entry_offset, input_file);
                if (input_entry == NULL) {
                    // printf("Input file does not exist in directory\n");
                    return;
                }
                uint16_t input_index = input_entry->firstBlock;
                // printf("Remaining block, input_index is %d\n", input_index);
                if (input_index == 0) {
                    continue;
                }
                uint32_t bytes_to_read = input_entry->size;
                total_bytes += bytes_to_read;
                lseek(pf->fs_fd, directory_offset, SEEK_SET);
                if (write(pf->fs_fd, entries, directory_entries_count * sizeof(DirectoryEntry)) < 0) {
                    perror("Failed to write the updated directory block");
                }
                uint16_t input_block_offset = input_index * pf->block_size;
                if (mode == F_WRITE) {
                    uint16_t num_full_blocks = bytes_to_read / pf->block_size;
                    for (int j = 0; j < num_full_blocks; j++) {
                        // printf("Full block %d, input_index is %d\n", j, input_index);
                        lseek(pf->fs_fd, input_block_offset, SEEK_SET);
                        read(pf->fs_fd, input_buffer, pf->block_size);
                        lseek(pf->fs_fd, output_block_offset, SEEK_SET);
                        write(pf->fs_fd, input_buffer, pf->block_size);
                        input_index = pf->fat[input_index];
                        input_block_offset = input_index * pf->block_size;
                        uint16_t new_output_index = find_free_block();
                        pf->fat[output_index] = new_output_index;
                        output_index = new_output_index;
                        output_block_offset = output_index * pf->block_size;
                    }
                    if (bytes_to_read % pf->block_size != 0) {
                        //printf("Remaining block, input_index is %d\n", input_index);
                        char remaining_buffer[bytes_to_read - num_full_blocks * pf->block_size];
                        lseek(pf->fs_fd, input_block_offset, SEEK_SET);
                        read(pf->fs_fd, remaining_buffer, bytes_to_read - num_full_blocks * pf->block_size);
                        // char zeroBuffer[pf->block_size];
                        // memset(zeroBuffer, 0, pf->block_size);
                        // lseek(pf->fs_fd, output_block_offset, SEEK_SET);
                        // if (write(pf->fs_fd, zeroBuffer, pf->block_size) == -1) {
                        //     perror("Error clearing the block");
                        //     return;
                        // }
                        lseek(pf->fs_fd, output_block_offset, SEEK_SET);
                        write(pf->fs_fd, remaining_buffer, bytes_to_read - num_full_blocks * pf->block_size);
                    }
                    pf->fat[output_index] = 0xFFFF;
                    output_block_offset += bytes_to_read;
                } else if (mode == F_APPEND) {
                    while (bytes_to_read > 0) {
                        ssize_t read_size = bytes_to_read < pf->block_size ? bytes_to_read : pf->block_size;
                        char input_buffer[read_size];
                        lseek(pf->fs_fd, input_block_offset, SEEK_SET);
                        read(pf->fs_fd, input_buffer, read_size);
                        if (remaining_free_bytes > 0) {
                            ssize_t write_size = remaining_free_bytes < read_size ? remaining_free_bytes : read_size;
                            lseek(pf->fs_fd, output_block_offset + block_write_offset, SEEK_SET);
                            write(pf->fs_fd, input_buffer, write_size);
                            bytes_to_read -= write_size;
                            block_write_offset += write_size;
                            remaining_free_bytes -= write_size;
                            if (remaining_free_bytes == 0) {
                                // Find a new block if we've run out of space in the current block
                                uint16_t new_block_index = find_free_block();
                                pf->fat[output_index] = new_block_index;
                                output_index = new_block_index;
                                pf->fat[output_index] = 0xFFFF;


                                output_block_offset = new_block_index * pf->block_size;
                                block_write_offset = 0;
                                remaining_free_bytes = pf->block_size;
                            }
                        } else {
                            lseek(pf->fs_fd, output_block_offset, SEEK_SET);
                            write(pf->fs_fd, input_buffer + (read_size - bytes_to_read), bytes_to_read);
                            block_write_offset += bytes_to_read;
                            bytes_to_read = 0;
                        }
                        // printf("Remaining block, input_index is %d\n", input_index);
                        if (input_index == 0xFFFF) {
                            break;
                        }
                        input_index = pf->fat[input_index];
                    }
                }
            }
            if (mode == F_WRITE) {
                curr_output_file->size = total_bytes;
            } else if (mode == F_APPEND) {
                curr_output_file->size += total_bytes;
            }
            
            curr_output_file->mtime = time(NULL);
            // printf("curr_output_file->firstBlock is %d\n", curr_output_file->firstBlock);
            // printf("curr_output_file->size is %d\n", curr_output_file->size);
            if (directory_entry_offset != -1) {
                entries[directory_entry_offset] = *curr_output_file;
            }
            lseek(pf->fs_fd, directory_offset, SEEK_SET);
            if (write(pf->fs_fd, entries, directory_entries_count * sizeof(DirectoryEntry)) < 0) {
                perror("Failed to write the updated directory block");
            }
            // printf("curr_output_file->size is %d\n", curr_output_file->size);
        }


        // cat -w OUTPUT_FILE
        // cat -a OUTPUT_FILE
        if (fileArgsStart == argc) { // Handle reading from terminal
            char buffer[CAT_BUFFER_SIZE];
            memset(buffer, 0, CAT_BUFFER_SIZE);
            ssize_t bytes_read;
            ssize_t total_bytes_written;
            int new_block_needed = 0;

            if (mode == F_WRITE) {
                block_write_offset = 0;
                total_bytes_written = 0;
            } else if (mode == F_APPEND) {
                block_write_offset = curr_output_file->size % pf->block_size;
                if (curr_output_file->size != 0 && block_write_offset == 0) {
                    new_block_needed = 1;
                }
                total_bytes_written = 0;
                // printf("block_write_offset is %zu\n", block_write_offset);
            }
            
            while ((bytes_read = read(STDIN_FILENO, buffer, CAT_BUFFER_SIZE) - 1) > 0) {
                trim_newline(buffer);
                total_bytes_written += bytes_read;
                ssize_t curr_bytes_left = 0;
                if (new_block_needed) {
                    uint16_t new_block_index = find_free_block();
                    pf->fat[output_index] = new_block_index;
                    output_index = new_block_index;
                    pf->fat[output_index] = 0xFFFF;
                    output_block_offset = output_index * pf->block_size;
                    new_block_needed = 0;
                }
                if (block_write_offset + bytes_read > pf->block_size) {
                    curr_bytes_left = pf->block_size - block_write_offset;
                    lseek(pf->fs_fd, output_block_offset + block_write_offset, SEEK_SET);
                    if (write(pf->fs_fd, buffer, curr_bytes_left) == -1) {
                        perror("Error writing to file");
                        return;
                    }
                    ssize_t bytes_to_write = bytes_read - curr_bytes_left;
                    ssize_t bytes_written = curr_bytes_left;
                    uint16_t curr_block_index = output_index;
                    block_write_offset = 0;
                    while (bytes_to_write > 0) {
                        uint16_t new_block_index = find_free_block();
                        pf->fat[curr_block_index] = new_block_index;
                        curr_block_index = new_block_index;
                        pf->fat[curr_block_index] = 0xFFFF;
                        ssize_t write_size = 0;
                        if (bytes_to_write > pf->block_size) {
                            write_size = pf->block_size;
                            block_write_offset = 0;
                        } else {
                            write_size = bytes_to_write;
                            block_write_offset = bytes_to_write;
                        }
                        lseek(pf->fs_fd, new_block_index * pf->block_size, SEEK_SET);
                        if (write(pf->fs_fd, buffer + bytes_written, write_size) == -1) {
                            perror("Error writing to file");
                            return;
                        }
                        bytes_to_write -= write_size;
                        bytes_written += write_size;
                    }


                } else {
                    if (mode == F_WRITE) {
                        char zeroBuffer[pf->block_size];
                        memset(zeroBuffer, 0, pf->block_size);
                        lseek(pf->fs_fd, output_block_offset, SEEK_SET);
                        write(pf->fs_fd, zeroBuffer, pf->block_size);
                    }
                    lseek(pf->fs_fd, output_block_offset + block_write_offset, SEEK_SET);
                    if (write(pf->fs_fd, buffer, bytes_read) == -1) {
                        perror("Error writing to file");
                        return;
                    }
                    output_block_offset += bytes_read;
                    block_write_offset += bytes_read;
                }
                // printf("Bytes read is %zd\n", bytes_read);
            }

            // update curr_output_file variable
            if (mode == F_WRITE) {
                curr_output_file->size = total_bytes_written;
            } else if (mode == F_APPEND) {
                curr_output_file->size += total_bytes_written;
            }
            curr_output_file->mtime = time(NULL);
            if (directory_entry_offset != -1) {
                entries[directory_entry_offset] = *curr_output_file;
            }
            // printf("curr_output_file->size is %d\n", entries[stored].size);
            // printf("curr_output_file->firstBlock is %d\n", entries[stored].firstBlock);
            lseek(pf->fs_fd, directory_offset, SEEK_SET);
            if (write(pf->fs_fd, entries, directory_entries_count * sizeof(DirectoryEntry)) < 0) {
                perror("Failed to write the updated directory block");
            }
        }
    } else {
        // cat FILE...
        for (int i = fileArgsStart; i < argc; i++) {
            char* input_file = argv[i];
            // printf("input_file is %s", input_file);
            char input_buffer[pf->block_size];
            int directory_block_offset = 0;
            int directory_entry_offset = 0;
            DirectoryEntry* input_entry = find_file(&directory_block_offset, &directory_entry_offset, input_file);
            if (input_entry == NULL) {
                perror("Input file does not exist in directory\n");
                return;
            }
            uint16_t input_index = input_entry->firstBlock;
            if (input_index == 0) {
                continue;
            }
            uint16_t input_block_offset = input_index * pf->block_size;
            uint32_t bytes_to_read = input_entry->size;
            uint16_t num_full_blocks = bytes_to_read / pf->block_size;
            for (int j = 0; j < num_full_blocks; j++) {
                // printf("Full block %d, input_index is %d\n", j, input_index);
                lseek(pf->fs_fd, input_block_offset, SEEK_SET);
                read(pf->fs_fd, input_buffer, pf->block_size);
                write(STDOUT_FILENO, input_buffer, pf->block_size);
                input_index = pf->fat[input_index];
                input_block_offset = input_index * pf->block_size;
            }
            if (bytes_to_read % pf->block_size != 0) {
                // printf("Remaining block, input_index is %d\n", input_index);
                char remaining_buffer[bytes_to_read - num_full_blocks * pf->block_size];
                lseek(pf->fs_fd, input_block_offset, SEEK_SET);
                read(pf->fs_fd, remaining_buffer, bytes_to_read - num_full_blocks * pf->block_size);
                write(STDOUT_FILENO, remaining_buffer, bytes_to_read - num_full_blocks * pf->block_size);
            }
        }
    }
}


void chmod_fs(char* filename, char* perm) {
    fprintf(stderr, "inside: %s, %s\n", filename, perm);
    if (pf == NULL || pf->fs_fd == -1) {
        fprintf(stderr, "chmod_fs: File system is not mounted.\n");
        return;
    }
    
    // Calculate the number of directory entries
    size_t directory_entries_count = pf->block_size / sizeof(DirectoryEntry);

    // Allocate space for the directory entries
    DirectoryEntry* entries = malloc(directory_entries_count * sizeof(DirectoryEntry));
    if (entries == NULL) {
        perror("mv_fs: Failed to allocate memory for directory entries");
        return;
    }

    // Read the directory entries from the filesystem
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
            if (strcmp(entries[i].name, filename) == 0) {
                found = 1;
                int mask = 0;
                for (int i=0;i<strlen(perm);i++) {
                    if (perm[i] == 'r') mask |= 4;
                    if (perm[i] == 'w') mask |= 2;
                    if (perm[i] == 'x') mask |= 1;
                }
                if (perm[0] == '+') {
                    mask |= entries[i].perm;
                } else if (perm[0] == '-') {
                    mask ^= entries[i].perm;
                }
                if (mask == 1 || mask == 3) {
                    perror("Invalid command");
                } else {
                    entries[i].perm = mask;
                    entries[i].mtime = time(NULL);
                }
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


        // Find the source file and rename it
        for (size_t i = 0; i < directory_entries_count; i++) {
            if (strcmp(entries[i].name, filename) == 0) {
                found = 1;
                int mask = 0;
                for (int i=0;i<strlen(perm);i++) {
                    if (perm[i] == 'r') mask |= 4;
                    if (perm[i] == 'w') mask |= 2;
                    if (perm[i] == 'x') mask |= 1;
                }
                if (perm[0] == '+') {
                    mask |= entries[i].perm;
                } else if (perm[0] == '-') {
                    mask ^= entries[i].perm;
                }
                if (mask == 1 || mask == 3) {
                    perror("Invalid command");
                } else {
                    entries[i].perm = mask;
                    entries[i].mtime = time(NULL);
                }
                break;
            }
        }
        if (!found) {
            fprintf(stderr, "chmod_fs: Filename '%s' not found.\n", filename);
            free(entries);
            return;
        }
    }

    // Write the updated directory entries back to the filesystem
    lseek(pf->fs_fd, directory_offset, SEEK_SET);
    write(pf->fs_fd, entries, directory_entries_count * sizeof(DirectoryEntry));
    free(entries);
}