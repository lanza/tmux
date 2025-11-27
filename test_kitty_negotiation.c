#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <string.h>

int main() {
    struct termios old_tio, new_tio;
    unsigned char buf[256];
    int n, i;

    // Get current terminal settings
    tcgetattr(STDIN_FILENO, &old_tio);
    new_tio = old_tio;
    new_tio.c_lflag &= ~(ICANON | ECHO);
    new_tio.c_cc[VMIN] = 0;
    new_tio.c_cc[VTIME] = 1;
    tcsetattr(STDIN_FILENO, TCSANOW, &new_tio);

    printf("=== Kitty Protocol Test in tmux ===\n\n");

    // Step 1: Query current flags
    printf("1. Querying kitty flags: ESC[?u\n");
    printf("\033[?u");
    fflush(stdout);
    sleep(1);

    // Read response
    n = read(STDIN_FILENO, buf, sizeof(buf));
    if (n > 0) {
        printf("   Response (%d bytes): ", n);
        for (i = 0; i < n; i++) {
            if (buf[i] == 0x1b) printf("ESC");
            else if (buf[i] == '[') printf("[");
            else if (buf[i] >= 32 && buf[i] < 127) printf("%c", buf[i]);
            else printf("<%02x>", buf[i]);
        }
        printf("\n");
    } else {
        printf("   No response\n");
    }

    // Step 2: Enable kitty protocol with flag 1
    printf("\n2. Enabling kitty protocol: ESC[>1u\n");
    printf("\033[>1u");
    fflush(stdout);
    sleep(1);

    // Step 3: Query again to verify
    printf("\n3. Querying flags again: ESC[?u\n");
    printf("\033[?u");
    fflush(stdout);
    sleep(1);

    n = read(STDIN_FILENO, buf, sizeof(buf));
    if (n > 0) {
        printf("   Response (%d bytes): ", n);
        for (i = 0; i < n; i++) {
            if (buf[i] == 0x1b) printf("ESC");
            else if (buf[i] == '[') printf("[");
            else if (buf[i] >= 32 && buf[i] < 127) printf("%c", buf[i]);
            else printf("<%02x>", buf[i]);
        }
        printf("\n");
    } else {
        printf("   No response\n");
    }

    printf("\n4. Now press Ctrl-Shift-I (then 'q' to quit)\n\n");

    // Read keys
    while (1) {
        n = read(STDIN_FILENO, buf, sizeof(buf));
        if (n > 0) {
            if (n == 1 && buf[0] == 'q') break;

            printf("Key pressed (%d bytes): ", n);
            for (i = 0; i < n; i++) {
                printf("%02x ", buf[i]);
            }
            printf("= ");
            for (i = 0; i < n; i++) {
                if (buf[i] == 0x1b) printf("ESC");
                else if (buf[i] == '[') printf("[");
                else if (buf[i] >= 32 && buf[i] < 127) printf("%c", buf[i]);
                else printf("<%02x>", buf[i]);
            }
            printf("\n");
        }
    }

    // Cleanup - disable kitty protocol
    printf("\n5. Disabling kitty protocol: ESC[<u\n");
    printf("\033[<u");
    fflush(stdout);

    tcsetattr(STDIN_FILENO, TCSANOW, &old_tio);
    return 0;
}
