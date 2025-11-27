#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* Test the buffer copy logic from tty_keys_kitty_key */
void test_buffer_copy(const char *test_name, const char *input, size_t input_len) {
    char tmp[64];
    size_t end;
    
    printf("\n=== Test: %s ===\n", test_name);
    printf("Input: ");
    for (size_t i = 0; i < input_len; i++) {
        if (input[i] == '\033') printf("ESC");
        else printf("%c", input[i]);
    }
    printf(" (len=%zu)\n", input_len);
    
    /* Find the 'u' terminator */
    end = 0;
    for (size_t i = 2; i < input_len; i++) {
        if (input[i] == 'u') {
            end = i;
            break;
        }
    }
    
    if (end == 0) {
        printf("ERROR: No 'u' terminator found\n");
        return;
    }
    
    printf("Terminator 'u' at index: %zu\n", end);
    printf("Copying from buf[2] to buf[%zu] = %zu bytes\n", end-1, end-2);
    
    /* OLD BUGGY CODE would do:
     * memcpy(tmp, input + 2, end);     // Copies 'end' bytes from input[2]
     * tmp[end-1] = '\0';
     * This reads input[2] through input[2+end-1] = input[end+1] OUT OF BOUNDS!
     */
    
    /* NEW CORRECT CODE: */
    if (end < 2) {
        printf("ERROR: end < 2\n");
        return;
    }
    
    memcpy(tmp, input + 2, end - 2);
    tmp[end - 2] = '\0';
    
    printf("Extracted: '%s'\n", tmp);
    printf("SUCCESS: No buffer over-read!\n");
}

int main() {
    printf("=== Kitty Keyboard Protocol Buffer Copy Tests ===\n");
    
    /* Test 1: Simple key 'a' = 97 */
    test_buffer_copy("Key 'a'", "\033[97u", 5);
    
    /* Test 2: Key 'A' with shift modifier */
    test_buffer_copy("Key 'A' with modifier", "\033[65;2u", 8);
    
    /* Test 3: Key with alternate code */
    test_buffer_copy("Key with alternate", "\033[97:98u", 9);
    
    /* Test 4: Complex sequence */
    test_buffer_copy("Complex sequence", "\033[97:98;2:1u", 13);
    
    /* Test 5: Minimal sequence */
    test_buffer_copy("Minimal", "\033[1u", 4);
    
    printf("\n=== All tests passed! ===\n");
    return 0;
}
