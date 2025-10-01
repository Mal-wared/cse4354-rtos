#ifndef _UTIL_H
#define _UTIL_H

#include <stdint.h>

// printFlags Macros
#define PRINT_STACK_POINTERS        (1 << 0)
#define PRINT_MFAULT_FLAGS          (1 << 1)
#define PRINT_OFFENDING_INSTRUCTION (1 << 2)
#define PRINT_DATA_ADDRESSES        (1 << 3)
#define PRINT_STACK_DUMP            (1 << 4)


int atoi(const char *num);
int strcmp(const char *s1, const char *s2);
char tolower(char c);
void itoa(int32_t num, char str[]);
void printPid(uint32_t pid);
void printFlags(uint32_t flags);
#endif
