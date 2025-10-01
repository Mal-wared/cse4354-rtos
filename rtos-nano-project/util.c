#include <stdint.h>
#include <stdbool.h>

int atoi(const char *num) {
    int result = 0;
    int sign = 1;
    int i = 0;

    // Handle initial whitespace
    while (num[i] == 32) {
        i++;
    }

    // Handle signed
    if (num[i] == '-') {
        sign = -1;
        i++;
    } else if (num[i] == '+') {
        i++;
    }

    // Create integer
    while (num[i] >= 48 && num[i] <= 57) {
        result = result * 10 + (num[i] - '0');
        i++;
    }

    return sign * result;
}

char tolower(char c) {
    if (c >= 'A' && c <= 'Z') {
        return c + 32;
    }
    return c;
}

int strcmp(const char *s1, const char *s2) {
    while (tolower(*s1) == tolower(*s2)) {
        if(*s1 == '\0') {
            return 0;
        }
        s1++;
        s2++;
    }
    uint8_t result = (uint8_t)tolower(*s1) - (uint8_t)tolower(*s2);
    return result;
}


void itoa(int32_t num, char* str) {
    int i = 0;
    bool isNegative = false;

    if (num == 0) {
        str[i++] = '0';
        str[i] = '\0';
        return;
    }

    int64_t tempNum = num;

    if (tempNum < 0) {
        isNegative = true;
        tempNum = -tempNum;
    }

    while (tempNum != 0) {
        int rem = tempNum % 10;
        str[i++] = rem + '0';
        tempNum = tempNum / 10;
    }

    if (isNegative) {
        str[i++] = '-';
    }

    str[i] = '\0';
    int start = 0;
    int end = i - 1;
    while (start < end) {
        char temp = str[start];
        str[start] = str[end];
        str[end] = temp;
        start++;
        end--;
    }
}

void printPid(uint32_t pid) {
    char pidStr[10];
    itoa(pid, pidStr);
    putsUart0(pidStr);
    putsUart0("\n");
}

void printFlags(uint32_t flags) {
    if (flags = )
}
