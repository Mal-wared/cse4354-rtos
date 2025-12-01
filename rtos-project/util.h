// Nicholas Nhat Tran
// 1002027150

#ifndef _UTIL_H
#define _UTIL_H

#include <stdint.h>
#include <stddef.h>

// printFlags Macros
#define PRINT_STACK_POINTERS        (1 << 0)
#define PRINT_MFAULT_FLAGS          (1 << 1)
#define PRINT_OFFENDING_INSTRUCTION (1 << 2)
#define PRINT_DATA_ADDRESSES        (1 << 3)
#define PRINT_STACK_DUMP            (1 << 4)


int atoi(const char *num);
int strcmp(const char *s1, const char *s2);
int stricmp(const char *s1, const char *s2);
char tolower(char c);
void reverseStr(char str[], int length);
char* strcpy(char* dest, const char* src);
char* strncpy(char *dest, const char *src, unsigned int char_limit);
void itoa(int32_t num, char str[]);
void itoh(uint32_t num, char* str);
void itoh_be(uint32_t num, char* str);
uint32_t strlen(const char *str);
#endif
