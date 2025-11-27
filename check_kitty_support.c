#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <string.h>

int main() {
    struct termios old, new;
    char response[256] = {0};
    int i = 0;
    
    // Get current terminal settings
    tcgetattr(STDIN_FILENO, &old);
    new = old;
    
    // Set raw mode
    new.c_lflag &= ~(ICANON | ECHO);
    new.c_cc[VMIN] = 0;
    new.c_cc[VTIME] = 10;  // 1 second timeout
    tcsetattr(STDIN_FILENO, TCSANOW, &new);
    
    // Send kitty protocol query
    printf("\033[?u");
    fflush(stdout);
    
    // Read response
    usleep(100000);  // Wait 100ms for response
    while (i < sizeof(response) - 1) {
        if (read(STDIN_FILENO, &response[i], 1) == 1) {
            if (response[i] == 'u' && i > 0 && response[i-1] >= '0' && response[i-1] <= '9') {
                i++;
                break;
            }
            i++;
        } else {
            break;
        }
    }
    
    // Restore terminal settings
    tcsetattr(STDIN_FILENO, TCSANOW, &old);
    
    if (i > 0) {
        printf("Terminal response: ");
        for (int j = 0; j < i; j++) {
            if (response[j] == '\033') printf("ESC");
            else if (response[j] < 32) printf("^%c", response[j] + 64);
            else printf("%c", response[j]);
        }
        printf("\n");
        
        // Parse the response
        if (strstr(response, "[?0u")) {
            printf("Result: Kitty protocol NOT supported (flags = 0)\n");
        } else if (strstr(response, "[?")) {
            printf("Result: Kitty protocol SUPPORTED!\n");
        } else {
            printf("Result: Unexpected response\n");
        }
    } else {
        printf("No response received - terminal doesn't support kitty protocol\n");
    }
    
    return 0;
}
