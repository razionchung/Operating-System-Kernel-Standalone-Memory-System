#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <sys/mman.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <stdint.h>
#include "pennfat.h"
#include "directory.h"
#include <stdbool.h>

unsigned int find_free_block() {
    // Allocate memory to read the FAT region
    unsigned short* fat_table = malloc(pf->fat_size);

    if (!fat_table) {
        fprintf(stderr, "Failed to allocate memory for FAT table\n");
        return -1;
    }

    // Read the FAT table from the file system
    // FAT is at the beginning of the filesystem
    lseek(pf->fs_fd, 0, SEEK_SET);
    ssize_t bytes_read = read(pf->fs_fd, fat_table, pf->fat_size);

    if (bytes_read < 0) {
        perror("Failed to read FAT table");
        free(fat_table);
        return -1;
    }

    // Search for a free block
    unsigned int free_block_index = -1;
    for (unsigned int i = 1; i < (pf->fat_size / sizeof(unsigned short)); ++i) {
        if (fat_table[i] == 0x0000) {
            free_block_index = i;
            break;
        }
    }

    free(fat_table);
    
    if (free_block_index == -1) {
        fprintf(stderr, "No free blocks available in the filesystem\n");
    }

    return free_block_index;
}


unsigned int fetch_block_number(uint16_t start_block, unsigned int block_offset) {
    uint16_t current_block = start_block;

    for (unsigned int i = 0; i < block_offset; ++i) {
        printf("Iteration %u: current_block: %u\n", i, current_block);

        if (current_block == 0xFFFF) {
            printf("End of the file reached at block: %u\n", current_block);
            return 0xFFFF;
        }

        current_block = pf->fat[current_block];
    }

    return current_block;
}


unsigned int allocate_new_block() {
    for (unsigned int i = 2; i < pf->block_size * 8; ++i) {

        if (pf->fat[i] == 0) {
            pf->fat[i] = 0xFFFF; // Mark as end of file
            return i;
        }

    }
    return 0; // No free block available
}


void update_fat_entry(int fs_fd, uint16_t current_block, uint16_t new_block) {
    pf->fat[current_block] = new_block;
    // Each FAT entry is 2 bytes
    lseek(fs_fd, current_block * 2, SEEK_SET);
    write(fs_fd, &pf->fat[current_block], 2);
    fsync(fs_fd);
}


void update_directory_entry(int fs_fd, DirectoryEntry *entry) {
    if (!entry) {
        fprintf(stderr, "update_directory_entry called with NULL entry\n");
        return;
    }

    size_t directory_entries_count = pf->block_size / sizeof(DirectoryEntry);
    DirectoryEntry* entries = malloc(directory_entries_count * sizeof(DirectoryEntry));

    if (entries == NULL) {
        perror("Failed to allocate memory for directory entries");
        return;
    }

    unsigned int directory_block_index = 1;
    off_t directory_offset = (directory_block_index - 1) * pf->block_size + pf->fat_size;
    bool entry_updated = false;

    while (pf->fat[directory_block_index] != 0xFFFF && !entry_updated) {
        lseek(pf->fs_fd, directory_offset, SEEK_SET);

        if (read(pf->fs_fd, entries, directory_entries_count * sizeof(DirectoryEntry)) != directory_entries_count * sizeof(DirectoryEntry)) {
            perror("Failed to read directory entries");
            free(entries);
            return;
        }

        for (size_t i = 0; i < directory_entries_count; ++i) {
            if (strncmp(entries[i].name, entry->name, sizeof(entries[i].name)) == 0) {
                memcpy(&entries[i], entry, sizeof(DirectoryEntry));
                lseek(pf->fs_fd, directory_offset + i * sizeof(DirectoryEntry), SEEK_SET);
                write(pf->fs_fd, &entries[i], sizeof(DirectoryEntry));
                entry_updated = true;
                break;
            }
        }

        if (!entry_updated) {
            directory_block_index = pf->fat[directory_block_index];
            directory_offset = directory_block_index * pf->block_size;
        }
    }

    lseek(pf->fs_fd, directory_offset, SEEK_SET);

    if (read(pf->fs_fd, entries, directory_entries_count * sizeof(DirectoryEntry)) != directory_entries_count * sizeof(DirectoryEntry)) {
        perror("Failed to read directory entries");
        free(entries);
        return;
    }

    for (size_t i = 0; i < directory_entries_count; ++i) {
        if (strncmp(entries[i].name, entry->name, sizeof(entries[i].name)) == 0) {
            memcpy(&entries[i], entry, sizeof(DirectoryEntry));
            lseek(pf->fs_fd, directory_offset + i * sizeof(DirectoryEntry), SEEK_SET);
            write(pf->fs_fd, &entries[i], sizeof(DirectoryEntry));
            entry_updated = true;
            break;
        }
    }

    free(entries);

    if (!entry_updated) {
        fprintf(stderr, "Failed to find directory entry for update\n");
    }
}