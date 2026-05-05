/*
 * practice_bomb.c — A mini Bomb Lab for learning GDB/LLDB
 *
 * Compile (macOS M1):
 *   gcc -g -O0 -o practice_bomb practice_bomb.c
 *
 * Run:
 *   ./practice_bomb
 *
 * Debug (macOS):
 *   lldb ./practice_bomb
 *
 * Debug (Linux / class server):
 *   gdb ./practice_bomb
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ──────────────────────────────────────────────
 * Helper functions
 * ────────────────────────────────────────────── */

void explode_bomb(void) {
    printf("\n💥 BOOM!!! The bomb has exploded.\n");
    printf("Next time, set a breakpoint on explode_bomb!\n\n");
    exit(1);
}

/* Read one line from stdin, strip newline */
char *read_line(void) {
    static char buf[256];
    printf("Enter input: ");
    if (fgets(buf, sizeof(buf), stdin) == NULL) {
        printf("Unexpected EOF. Bye.\n");
        exit(0);
    }
    /* strip trailing newline */
    size_t len = strlen(buf);
    if (len > 0 && buf[len - 1] == '\n')
        buf[len - 1] = '\0';
    return buf;
}

/* ──────────────────────────────────────────────
 * Phase 1: String comparison
 *
 * Pattern: The phase stores a hidden string and
 * calls strcmp() against your input.
 *
 * How to solve with a debugger:
 *   - Disassemble phase_1
 *   - Find the address loaded into the register
 *     that gets passed to strcmp
 *   - Examine that address as a string
 * ────────────────────────────────────────────── */

void phase_1(const char *input) {
    const char *expected = "science is organized knowledge";
    if (strcmp(input, expected) != 0)
        explode_bomb();
}

/* ──────────────────────────────────────────────
 * Phase 2: Numeric sequence with a loop
 *
 * Pattern: The phase reads N integers from your
 * input via sscanf, then uses a loop to verify
 * a mathematical relationship between them.
 *
 * How to solve with a debugger:
 *   - Disassemble phase_2
 *   - Identify the sscanf format string → tells
 *     you how many numbers are expected
 *   - Step through the loop, watch how each
 *     element is computed from the previous one
 * ────────────────────────────────────────────── */

void phase_2(const char *input) {
    int nums[5];
    int count = sscanf(input, "%d %d %d %d %d",
                       &nums[0], &nums[1], &nums[2],
                       &nums[3], &nums[4]);
    if (count != 5)
        explode_bomb();

    /* Rule: first number must be 3,
     * each subsequent number = previous * 2 */
    if (nums[0] != 3)
        explode_bomb();

    for (int i = 1; i < 5; i++) {
        if (nums[i] != nums[i - 1] * 2)
            explode_bomb();
    }
}

/* ──────────────────────────────────────────────
 * Phase 3: Switch statement
 *
 * Pattern: The phase reads an int (and possibly
 * more values), then uses a switch on the first
 * int to select an expected second value.
 *
 * How to solve with a debugger:
 *   - Identify the sscanf format → two ints
 *   - Find the jump table or cmp chain
 *   - For each case, read what value is compared
 *     against the second input
 * ────────────────────────────────────────────── */

void phase_3(const char *input) {
    int a, b;
    if (sscanf(input, "%d %d", &a, &b) != 2)
        explode_bomb();

    int expected;
    switch (a) {
        case 0: expected = 531; break;
        case 1: expected = 196; break;
        case 2: expected = 818; break;
        case 3: expected = 107; break;
        default:
            explode_bomb();
            return; /* unreachable, silences warning */
    }

    if (b != expected)
        explode_bomb();
}

/* ──────────────────────────────────────────────
 * Phase 4: Recursive function
 *
 * Pattern: The phase reads an integer, passes it
 * to a recursive function (often Fibonacci-like
 * or factorial-like), and checks the return value.
 *
 * How to solve with a debugger:
 *   - Disassemble func4 — identify the recursion
 *   - Determine what func4(n) returns for small n
 *   - Find which return value the phase expects
 *   - Work backwards to find the input n
 * ────────────────────────────────────────────── */

int func4(int n) {
    if (n <= 0) return 1;
    if (n == 1) return 1;
    return func4(n - 1) + func4(n - 2);
}

void phase_4(const char *input) {
    int n;
    if (sscanf(input, "%d", &n) != 1)
        explode_bomb();
    if (n < 0 || n > 15)
        explode_bomb();

    /* func4 is Fibonacci: func4(8) = 34 */
    if (func4(n) != 34)
        explode_bomb();
}

/* ──────────────────────────────────────────────
 * main
 * ────────────────────────────────────────────── */

int main(void) {
    char *input;

    printf("╔════════════════════════════════════╗\n");
    printf("║   Practice Bomb — 4 Phases         ║\n");
    printf("║   Set breakpoints before running!  ║\n");
    printf("╚════════════════════════════════════╝\n\n");

    printf("── Phase 1 ──\n");
    input = read_line();
    phase_1(input);
    printf("✅ Phase 1 defused!\n\n");

    printf("── Phase 2 ──\n");
    input = read_line();
    phase_2(input);
    printf("✅ Phase 2 defused!\n\n");

    printf("── Phase 3 ──\n");
    input = read_line();
    phase_3(input);
    printf("✅ Phase 3 defused!\n\n");

    printf("── Phase 4 ──\n");
    input = read_line();
    phase_4(input);
    printf("✅ Phase 4 defused!\n\n");

    printf("🎉 All phases defused! Nice work.\n");
    return 0;
}
