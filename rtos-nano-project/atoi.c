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
