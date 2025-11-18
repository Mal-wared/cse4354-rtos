#include "util.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

int atoi(const char *num)
{
    int result = 0;
    int sign = 1;
    int i = 0;

    // Handle initial whitespace
    while (num[i] == 32)
    {
        i++;
    }

    // Handle signed
    if (num[i] == '-')
    {
        sign = -1;
        i++;
    }
    else if (num[i] == '+')
    {
        i++;
    }

    // Create integer
    while (num[i] >= 48 && num[i] <= 57)
    {
        result = result * 10 + (num[i] - '0');
        i++;
    }

    return sign * result;
}

char tolower(char c)
{
    if (c >= 'A' && c <= 'Z')
    {
        return c + 32;
    }
    return c;
}

int strcmp(const char *s1, const char *s2)
{
    while (*s1 == *s2)
    {
        if (*s1 == '\0')
        {
            return 0;
        }
        s1++;
        s2++;
    }
    return *s1 - *s2;
}

// case insenstive
int stricmp(const char *s1, const char *s2)
{
    while (tolower(*s1) == tolower(*s2))
    {
        if (*s1 == '\0')
        {
            return 0;
        }
        s1++;
        s2++;
    }
    return tolower(*s1) - tolower(*s2);
}

void reverseStr(char str[], int length)
{
    int start = 0;
    int end = length - 1;
    while (start < end)
    {
        char temp = str[start];
        str[start] = str[end];
        str[end] = temp;
        start++;
        end--;
    }
}

char* strcpy(char *dest, const char *src)
{
    // Save the original destination address
    char *original_dest = dest;

    while (*src != '\0')
    {
        *dest = *src;
        dest++;
        src++;
    }

    *dest = '\0';

    // Return the original starting address of the destination
    return original_dest;
}

char* strncpy(char *dest, const char *src, unsigned int char_limit)
{
    // Save the original destination address
    char *original_dest = dest;

    unsigned int i = 0;
    while (*src != '\0' && i < char_limit - 1)
    {
        *dest = *src;
        dest++;
        src++;
        i++;
    }

    *dest = '\0';

    // Return the original starting address of the destination
    return original_dest;
}

void itoa(int32_t num, char *str)
{
    int i = 0;
    bool isNegative = false;

    if (num == 0)
    {
        str[i++] = '0';
        str[i] = '\0';
        return;
    }

    int64_t tempNum = num;

    if (tempNum < 0)
    {
        isNegative = true;
        tempNum = -tempNum;
    }

    while (tempNum != 0)
    {
        int rem = tempNum % 10;
        str[i++] = rem + '0';
        tempNum = tempNum / 10;
    }

    if (isNegative)
    {
        str[i++] = '-';
    }

    str[i] = '\0';
    int start = 0;
    int end = i - 1;
    while (start < end)
    {
        char temp = str[start];
        str[start] = str[end];
        str[end] = temp;
        start++;
        end--;
    }
}

void itoh(uint32_t num, char *str)
{
    char temp_str[9]; // Temp buffer for 8 hex digits + null terminator
    int i = 0;

    // Hex-character look-up table
    char hex_chars[] = "0123456789ABCDEF";

    // Zero-case
    if (num == 0)
    {
        strcpy(str, "0x0000.0000");
        return;
    }

    // Create the initial hex string (in reverse)
    while (num != 0)
    {
        temp_str[i++] = hex_chars[num % 16];
        num /= 16;
    }

    // Pad with leading zeros (in reverse) until we have 8 digits
    while (i < 8)
    {
        temp_str[i++] = '0';
    }
    temp_str[i] = '\0';

    // Reverse the padded string to get the correct order
    reverseStr(temp_str, i);

    // Build the final formatted string
    str[0] = '0';
    str[1] = 'x';
    strncpy(&str[2], &temp_str[0], 4); // Copy the first 4 hex digits
    str[6] = '.';
    strncpy(&str[7], &temp_str[4], 4); // Copy the last 4 hex digits
    str[11] = '\0'; // Null-terminate the final string
}

void itoh_be(uint32_t num, char *str)
{
    const char hex_chars[] = "0123456789ABCDEF";
    int i;
    char *p = &str[2];

    str[0] = '0';
    str[1] = 'x';
    str[6] = '.';
    str[11] = '\0';

    // This loop ensures correct big-endian order by starting with
    // the most significant byte (i=3) and counting down.
    for (i = 3; i >= 0; i--)
    {
        uint8_t byte = (num >> (i * 8)) & 0xFF;

        *p++ = hex_chars[byte >> 4];
        *p++ = hex_chars[byte & 0x0F];

        // Place the period after the second byte (i=2) is processed
        if (i == 2)
        {
            p++;
        }
    }
}

