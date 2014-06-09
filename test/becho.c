#include <stdio.h>
#include <string.h>

int main(int argc, char** argv) {
    if (argc < 2) return 1;

    int len = strlen(argv[1]);
    char buf[len + sizeof(len)];

    memcpy(buf, &len, sizeof(len));
    memcpy(buf + sizeof(len), argv[1], len);

    write(1, buf, sizeof(buf));
}
