#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <stdint.h>

#ifndef PENNFAT_H
#define PENNFAT_H

#define CAT_BUFFER_SIZE 4096 /**< buffer for writing */


/**
 * Array of block sizes, whose index corresponds to the block_size_config
 * variable that is fed into mkfs()
*/
static const int BLOCK_SIZES[] = {256, 512, 1024, 2048, 4096};


/**
 * Pointer to the PennFAT struct pf, which is accessible all throughout
 * the program after the file system is mounted. 
*/
extern struct PennFAT *pf;  // Declaration

/**
 * The DirectoryEntry struct we use to store the information for each file. 
*/
typedef struct DirectoryEntry {
    char name[32];          /**< filename */
    uint32_t size;          /**< size of bytes written in the file */
    uint16_t firstBlock;    /**< index of the first block of the file, 0 if no memory allocated */
    uint8_t type;           /**< type of file: 0, 1, 2, 4 */
    uint8_t perm;           /**< permission of file: 0, 2, 4, 5, 6, 7 */
    time_t mtime;           /**< time of last update for this file */
    char reserved[16];      /**< reserved bits for this file, unused */
} DirectoryEntry;

/**
 * The PennFAT struct we use to store metadata for the file system, and a
 * pointer to the file allocation table (FAT)
*/
typedef struct PennFAT {
    uint16_t *fat;          /**< pointer to the file allocation table (fat) */
    size_t fat_size;        /**< size of the fat entry */
    size_t block_size;      /**< size of one data block */
    int fs_fd;              /**< file descriptor of the file system */
    int num_directories;    /**< number of directories in the file system, unused */
} PennFAT;


/**
* Creates a PennFAT filesystem in the file named FS_NAME.
* The number of blocks in the FAT region is BLOCKS_IN_FAT (ranging from 1 through 32),
* and the block size is 256, 512, 1024, 2048, or 4096 bytes corresponding to the value
* (0 through 4) of BLOCK_SIZE_CONFIG fed into BLOCK_SIZES[].
*
* @param fs_name filename
* @param blocks_in_fat number of blocks in the FAT region (1-32)
* @param block_size_config a number 0-4 that corresponds to block size
*/
void mkfs(char *fs_name, int blocks_in_fat, int block_size_config);


/**
* Mounts the filesystem named FS_NAME by loading the FAT into memory.
*
* @param fs_name name of the file we are mounting
*/
void mount_fs(char *fs_name);


/**
* Unmount the current filesystem in memory.
*/
void unmount_fs();

/**
 * Creates a DirectoryEntry for a new file called filename and stores
 * the DirectoryEntry in a directory block. 
 * 
 * @param filename name of the file we want to create/touch
*/
uint16_t touch_fs(char *filename);

/**
 * Reads in the `cat` command from the command line, parse argv according
 * to the different formats of `cat`. Specifically, a `cat` command can:
 * 1. `cat FILE... -w/a OUTPUT_FILE`: concatenates the content in FILE and
 * write/append it to OUTPUT_FILE.
 * 2. `cat -w/a OUTPUT_FILE`: reads content from stdin and writes/appends
 * it to OUTPUT_FILE.
 * 3. `cat FILE...`: concatenates all content in FILE and write to stdout.
 * 
 * @param argc number of parameters read from the `cat` command
 * @param argv array of parameters
*/
void cat_fs(int argc, char **argv);

/**
 * Lists all files in the directory.
*/
void ls_fs();

/**
 * Renames the file named source to dest
 * 
 * @param source name of the file wanting to rename
 * @param dest name of file renamed to
*/
void mv_fs(const char* source, const char* dest);

/**
 * Removes the file named filename from the directory
 * 
 * @param filename name of the file to remove
*/
void rm_fs(const char* filename);

/**
 * Helper function to trim the last '\n' character from str.
 * 
 * @param str string to trim '\n' from
*/
void trim_newline(char *str);

/**
 * Copies the content from a file named source to the file
 * named dest. If source doesn't exist, an error is given.
 * If dest doesn't exist, creates the new file named dest.
 * 
 * @param source name of source file
 * @param dest name of dest file
 * @param from_host 0: both files are in directory, 1: source is from host, 2: dest is from host
*/
void cp_fs(char* source, char* dest, int from_host);

/**
 * Changes the permission for the file named filename
 * according to the specifications in perm string.
 * 
 * @param filename name of the file whose permission we are changing
 * @param perm string that specifies modification for permission
*/
void chmod_fs(char* filename, char* perm);

/**
 * Iterates through all the directory blocks and attempts to find the
 * file named filename. If found, directory_block_offset is the index
 * of the block that this directory entry is in, and directory_entry_offset
 * is the number of entry this specific file is in this block.
 * 
 * @param directory_block_offset index of data block that stores this directory entry
 * @param directory_entry_offset index of directory entry within this data block
 * @param filename name of file we are attempting to find. 
*/
DirectoryEntry* find_file(int *directory_block_offset, int *directory_entry_offset, const char* filename);


#endif