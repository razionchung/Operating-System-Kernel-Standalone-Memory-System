#ifndef DESCRIPTORS_H
#define DESCRIPTORS_H
#include "./directory.h"
#include "./pennfat.h"

#define MAX_OPEN_FILES 100  
#define F_WRITE         1   ///< Writing mode, truncates file if exists, creates if not. Only one instance can be open.
#define F_READ          2   ///< Read-only mode, error if file does not exist.
#define F_APPEND        3   ///< Append mode, for reading and writing. Does not truncate, file pointer at end.

#define F_SEEK_SET 0        ///< Seek from the beginning of the file.
#define F_SEEK_CUR 1        ///< Seek from the current file position.
#define F_SEEK_END 2        ///< Seek from the end of the file.

/**
 * Structure representing an open file descriptor.
 */
typedef struct {
    int used;               ///< Flag indicating if this descriptor is in use.
    DirectoryEntry *entry;  ///< Pointer to the associated directory entry.
    int mode;               ///< Mode in which the file was opened.
    unsigned int cursor;    ///< Current position in the file.
} OpenFileDescriptor;

// Function declarations with updated Doxygen comments

/**
 * Opens a file named fname in the specified mode.
 * 
 * Modes:
 * F_WRITE - Opens for writing and reading; truncates if exists, creates if not.
 *            Only one instance can be open in F_WRITE mode.
 * F_READ - Opens for reading only; errors if file does not exist.
 * F_APPEND - Opens for reading and writing; does not truncate, sets pointer at end.
 * 
 * @param fname Name of the file to open. Filename should follow POSIX standards.
 * @param mode Mode to open the file.
 * @return File descriptor on success, negative value on error.
 */
int f_open(const char *fname, int mode);

/**
 * Reads n bytes from the file referenced by fd into buf.
 * 
 * @param fd File descriptor to read from.
 * @param n Number of bytes to read.
 * @param buf Buffer to store the read data.
 * @return Number of bytes read, 0 if EOF, negative number on error.
 */
int f_read(int fd, int n, char *buf);

/**
 * Writes n bytes from the string str to the file referenced by fd.
 * 
 * @param fd File descriptor to write to.
 * @param str String containing the data to write.
 * @param n Number of bytes to write.
 * @return Number of bytes written, negative value on error.
 */
int f_write(int fd, const char *str, int n);

/**
 * Clears the content of a file associated with a directory entry.
 * Used in F_WRITE mode to overwrite the current directory entry.
 * 
 * @param entry Directory entry of the file to clear.
 */
void clear_file_content(DirectoryEntry *entry);

/**
 * Deletes a file with the specified name.
 * 
 * @param fname Name of the file to delete.
 * @return 0 on success, -1 on error.
 */
int f_unlink(const char *fname);

/**
 * Sets the file cursor of an open file descriptor.
 * 
 * Modes:
 * F_SEEK_SET - The file offset is set to offset bytes.
 * F_SEEK_CUR - The file offset is set to its current location plus offset bytes.
 * F_SEEK_END - The file offset is set to the size of the file plus offset bytes.
 * 
 * @param fd File descriptor to seek.
 * @param offset Offset to set the cursor.
 * @param whence Position from where offset is applied.
 * @return New cursor position on success, -1 on error.
 */
int f_lseek(int fd, int offset, int whence);

/**
 * Closes an open file descriptor.
 * 
 * @param fd File descriptor to close.
 * @return 0 on success, -1 on error.
 */
int f_close(int fd);

/**
 * Lists files in a directory.
 * 
 * @param filename Name of the file or directory to list.
 */
void f_ls(const char *filename);

/**
 * Duplicates a file descriptor.
 * 
 * @param fd_curr Current file descriptor.
 * @param fd_new New file descriptor.
 * @return New file descriptor on success, -1 on error.
 */
int f_dup2(int fd_curr, int fd_new);

/**
 * @brief Finds the file descriptor for a given file name.
 * 
 * This function searches through the open file descriptors to find
 * one that is associated with the specified file name. If a matching
 * file descriptor is found, it is returned. If the file is not currently
 * open or if the file name is not found, the function returns -1.
 *
 * @param fname The name of the file for which the file descriptor is sought.
 *              It is a null-terminated string. The function performs a 
 *              case-sensitive search for this file name.
 * 
 * @return int  Returns the file descriptor (an integer) if a matching file
 *              is found. If the file is not open or the file name is not
 *              found, the function returns -1.
 */
int find_descriptor_by_name(const char *fname);

#endif // DESCRIPTORS_H