#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <string.h>

int main() {
    struct termios old_tio, new_tio;
    unsigned char buf[256];
    int n, total = 0;

    tcgetattr(STDIN_FILENO, &old_tio);
    new_tio = old_tio;
    new_tio.c_lflag &= ~(ICANON | ECHO);
    new_tio.c_cc[VMIN] = 0;
    new_tio.c_cc[VTIME] = 2;  // 200ms timeout
    tcsetattr(STDIN_FILENO, TCSANOW, &new_tio);

    printf("=== iTerm Kitty Protocol Test ===\n\n");

    printf("1. Enabling kitty protocol: ESC[>1u\n");
    write(STDOUT_FILENO, "\033[>1u", 5);
    fflush(stdout);
    sleep(1);

    printf("2. Press Ctrl-Shift-I then press 'q' to quit\n\n");

    while (1) {
        n = read(STDIN_FILENO, buf, sizeof(buf));
        if (n > 0) {
            if (n == 1 && buf[0] == 'q') break;

            printf("Received %d bytes: ", n);
            for (int i = 0; i < n; i++) {
                printf("%02x ", buf[i]);
            }
            printf("\n  As text: ");
            for (int i = 0; i < n; i++) {
                if (buf[i] == 0x1b) printf("ESC");
                else if (buf[i] == '[') printf("[");
                else if (buf[i] >= 32 && buf[i] < 127) printf("%c", buf[i]);
                else printf("<%02x>", buf[i]);
            }
            printf("\n\n");
        }
    }

    printf("\n3. Disabling: ESC[<u\n");
    write(STDOUT_FILENO, "\033[<u", 4);
    fflush(stdout);

    tcsetattr(STDIN_FILENO, TCSANOW, &old_tio);
    return 0;
}
