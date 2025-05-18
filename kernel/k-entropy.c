// kernel/entropy.c — keyboard‑timing entropy collector
#include "k-entropy.h"
#include "x86-64.h"
#include "lib.h"

static uint8_t entropy_buffer[ENTROPY_NEEDED_BYTES];
static bool    entropy_initialized   = false;
static unsigned entropy_usage_counter = 0;

// on‑boot collection (16 keystrokes)
void request_user_entropy(void) {
    console_clear();
    console_printf(CPOS(0,0),0x0F00,"ENTROPY COLLECTION\n");
    console_printf(CPOS(2,0),0x0700,
        "Please type %d random characters...", ENTROPY_NEEDED_BYTES);

    for (int i = 0; i < ENTROPY_NEEDED_BYTES; ++i) {
        console_printf(CPOS(4,0),0x0700,"Progress: %d/%d ",
                       i, ENTROPY_NEEDED_BYTES);

        int ch; volatile uint64_t timing = 0;
        do { ch = keyboard_readc(); timing++; } while (ch <= 0);

        uint64_t tsc = read_cycle_counter();
        entropy_buffer[i] = (uint8_t)(ch ^ tsc ^ (tsc>>8)
                               ^ (tsc>>16) ^ timing);
        console_printf(CPOS(6,i),0x0700,"*");
    }

    for (int i = 0; i < ENTROPY_NEEDED_BYTES; ++i)
        entropy_buffer[i] ^= entropy_buffer[(i+7)&(ENTROPY_NEEDED_BYTES-1)];

    entropy_initialized   = true;
    entropy_usage_counter = 0;

    console_printf(CPOS(8,0),0x0F00,"Done! Continuing...");
    uint64_t until = read_cycle_counter() + 2ULL*200000000; // ≈2 s
    while (read_cycle_counter() < until) { /* spin */ }

    console_clear();
}

// collect a single keystroke during refresh
static void collect_single_keystroke(int index, int have, int need) {
    console_printf(CPOS(23,0),0x0700,
        "Refresh entropy (%d/%d)... press a key ", have, need);

    int ch; volatile uint64_t timing = 0;
    do { ch = keyboard_readc(); timing++; } while (ch <= 0);

    uint64_t tsc = read_cycle_counter();
    entropy_buffer[index] = (uint8_t)(ch ^ tsc ^ (tsc>>8)
                               ^ (tsc>>16) ^ timing);

    /* blank the prompt (80 spaces) */
    console_printf(CPOS(23,0),0x0700,
        "                                                                                ");
}

// periodic refresh after ENTROPY_REFRESH_THRESHOLD uses
static void refresh_entropy(void) {
    int saved_cursor = cursorpos;
    console_printf(CPOS(0,0),0x0F00,"Refreshing entropy pool...");

    for (int i = 0; i < ENTROPY_NEEDED_BYTES; ++i)
        collect_single_keystroke(i, i , ENTROPY_NEEDED_BYTES);

    for (int i = 0; i < ENTROPY_NEEDED_BYTES; ++i)
        entropy_buffer[i] ^= entropy_buffer[(i+7)&(ENTROPY_NEEDED_BYTES-1)];

    entropy_usage_counter = 0;
    console_printf(CPOS(0,0),0x0700,
        "                                                                                ");
    cursorpos = saved_cursor;
}

// public interface
unsigned get_entropy_value(void) {
    if (!entropy_initialized)
        request_user_entropy();
    else if (entropy_usage_counter >= ENTROPY_REFRESH_THRESHOLD)
        refresh_entropy();

    ++entropy_usage_counter;

    unsigned v = 0;
    for (int i = 0; i < 4; ++i)
        v = (v << 8)
          | entropy_buffer[(i + (read_cycle_counter() & 0xF))
                           % ENTROPY_NEEDED_BYTES];

    return v ^ (unsigned)read_cycle_counter();
}
