// kernel/entropy.h — keyboard‑timing entropy collector
#ifndef WEENSYOS_ENTROPY_H
#define WEENSYOS_ENTROPY_H
#include "kernel.h"

#define ENTROPY_NEEDED_BYTES      16   // keystrokes to gather
#define ENTROPY_REFRESH_THRESHOLD 10000 // re‑prompt user for randomness after this many uses. Chosen for demo convenience. Lower a lot for higher stakes applciations (e.g. 64 or even less)

void request_user_entropy(void); // interactive collector
unsigned get_entropy_value(void); // 32‑bit mixed value

#endif