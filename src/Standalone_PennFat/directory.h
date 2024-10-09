#ifndef _DIRECTORY_H
#define _DIRECTORY_H

#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <stdint.h>
#include "./pennfat.h"

#define F_WRITE         1   ///< Writing mode.
#define F_READ          2   ///< Read-only mode.
#define F_APPEND        3   ///< Append mode.

/**
 * Finds a free block in the file system.
 * 
 * @return The index of the free block, or -1 if no block is available or an error occurs.
 */
unsigned int find_free_block();

/**
 * Fetches the block number at a given offset from the start block.
 * 
 * @param start_block The starting block number.
 * @param block_offset The block offset from the start block.
 * @return The block number at the offset, or 0xFFFF if the end of the file is reached or an error occurs.
 */
unsigned int fetch_block_number(uint16_t start_block, unsigned int block_offset);

/**
 * Allocates a new block in the file system.
 * 
 * @return The block number of the newly allocated block, or 0 if no block is available.
 */
unsigned int allocate_new_block();

/**
 * Updates a FAT entry in the file system.
 * 
 * @param fs_fd File descriptor for the file system.
 * @param current_block The current block to be updated.
 * @param new_block The new block number to update to.
 */
void update_fat_entry(int fs_fd, uint16_t current_block, uint16_t new_block);

/**
 * Updates a directory entry in the file system.
 * 
 * @param fs_fd File descriptor for the file system.
 * @param entry Pointer to the directory entry to update.
 */
void update_directory_entry(int fs_fd, DirectoryEntry *entry);

#endif /* _DIRECTORY_H */