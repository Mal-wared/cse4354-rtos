#ifndef _UTIL_H
#define _UTIL_H

#include <stdint.h>

int atoi(const char *num);
int strcmp(const char *s1, const char *s2);
char tolower(char c);
void itoa(int32_t num, char str[]);
void printPid(uint32_t pid);
void printFlags(uint32_t flags);
#endif
