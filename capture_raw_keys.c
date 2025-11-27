#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>

void print_byte(unsigned char c, int index) {
  printf("[%3d] 0x%02x (%3d) ", index, c, c);

  if (c == 0x1b)
    printf("ESC");
  else if (c == 0x0d)
    printf("CR");
  else if (c == 0x0a)
    printf("LF");
  else if (c == '[')
    printf("'['");
  else if (c < 32)
    printf("^%c", c + 64);
  else if (c == 127)
    printf("DEL");
  else if (c >= 32 && c < 127)
    printf("'%c'", c);
  else
    printf("(0x%02x)", c);

  printf("\n");
}

int main() {
  struct termios old_tio, new_tio;
  unsigned char buf[256];
  int n, i;
  fd_set readfds;
  struct timeval tv;

  // Get current terminal settings
  tcgetattr(STDIN_FILENO, &old_tio);
  new_tio = old_tio;

  // Disable canonical mode and echo
  new_tio.c_lflag &= ~(ICANON | ECHO);
  new_tio.c_cc[VMIN] = 0;
  new_tio.c_cc[VTIME] = 0;

  tcsetattr(STDIN_FILENO, TCSANOW, &new_tio);

  printf("=== Kitty Protocol Test ===\n\n");

  // Query kitty support
  printf("1. Querying kitty protocol support...\n");
  printf("   Sending: ESC [ ? u (bytes: 0x1b 0x5b 0x3f 0x75)\n");
  write(STDOUT_FILENO, "\033[?u", 4);
  fflush(stdout);

  // Wait for response
  printf("   Waiting for response...\n");
  sleep(1);

  FD_ZERO(&readfds);
  FD_SET(STDIN_FILENO, &readfds);
  tv.tv_sec = 0;
  tv.tv_usec = 100000; // 100ms

  if (select(STDIN_FILENO + 1, &readfds, NULL, NULL, &tv) > 0) {
    n = read(STDIN_FILENO, buf, sizeof(buf));
    if (n > 0) {
      printf("   Response (%d bytes):\n", n);
      for (i = 0; i < n; i++) {
        print_byte(buf[i], i);
      }
    } else {
      printf("   No response\n");
    }
  } else {
    printf("   No response (timeout)\n");
  }

  printf("\n2. Enabling kitty protocol with flags=1...\n");
  printf("   Sending: ESC [ > 1 u (bytes: 0x1b 0x5b 0x3e 0x31 0x75)\n");
  write(STDOUT_FILENO, "\033[>1u", 5);
  fflush(stdout);

  printf("\n--- Protocol enabled ---\n");
  printf("Now press Ctrl-Shift-I (or Ctrl-Shift-L)\n");
  printf("Press lowercase 'q' to quit\n\n");

  int seq_num = 0;
  while (1) {
    FD_ZERO(&readfds);
    FD_SET(STDIN_FILENO, &readfds);
    tv.tv_sec = 10;
    tv.tv_usec = 0;

    if (select(STDIN_FILENO + 1, &readfds, NULL, NULL, &tv) > 0) {
      n = read(STDIN_FILENO, buf, sizeof(buf));
      if (n > 0) {
        // Check for quit
        if (n == 1 && buf[0] == 'q') {
          break;
        }

        printf("=== Key sequence #%d (%d bytes) ===\n", seq_num++, n);
        for (i = 0; i < n; i++) {
          print_byte(buf[i], i);
        }
        printf("\n");
      }
    }
  }

  // Disable kitty protocol
  printf("\nDisabling kitty protocol...\n");
  printf("  Sending: ESC [ < u (bytes: 0x1b 0x5b 0x3c 0x75)\n");
  write(STDOUT_FILENO, "\033[<u", 4);
  fflush(stdout);

  // Restore old terminal settings
  tcsetattr(STDIN_FILENO, TCSANOW, &old_tio);
  printf("\nExiting...\n");

  return 0;
}
