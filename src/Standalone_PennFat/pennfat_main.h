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

static const int BLOCK_SIZES[] = {256, 512, 1024, 2048, 4096};

static struct PennFAT *pf;

/**
 * The main function of our standalone PennFaT, used to initiate everything and allow
 * PennFat to execute the commands required.
 * @param argc The number of arguments passed in from the terminal.
 * @param argv The arguments from terminal.
 * @return Anything on exit.
 */
int main(int argc, char *argv[]);

#endif