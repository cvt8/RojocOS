# rojocOS
*A tiny x86‑64 teaching operating system extended with an encrypted filesystem, secure heap allocator, and user‑supplied entropy pool.*

**Authors** · [Romain de Coudenhove](mailto:romain.de.coudenhove@ens.psl.eu) · [Johan Utterström](mailto:johan.utterstrom@ens.psl.eu) · [Constantin Vaillant‑Tenzer](mailto:constantin.tenzer@ens.psl.eu)


We used, as a base code, Eddie Kohler's C version of WeensyOS forked from [Harvard CS61 2017 
pset4](https://github.com/cs61/cs61-psets-f17) with updates merged
from [Harvard CS61 2023 pset3](https://github.com/cs61/cs61-f23-psets).


## Features

| Area              | What we added                                                                                        | Why it matters                                                        |
| ----------------- | ---------------------------------------------------------------------------------------------------- | --------------------------------------------------------------------- |
| **Filesystem**    | *Tree‑based layout* (32 children/node), **AES‑256 per‑file encryption**, constant‑time secure‑delete | Instant disc encryption demo; safe removal of secrets                 |
| **Randomness**    | 128‑bit entropy pool mixed from keystroke timing + TSC; exposed via `sys_getrandom`                  | Deterministic builds become *optional*; keys seeded with user entropy |
| **Kernel heap**   | Tiny first‑fit allocator with 16‑byte alignment and zero‑on‑free                                     | Prevents after‑free data leaks; ready for SIMD/AES buffers            |
| **Shell**         | Minimal Bourne‑like shell with familiar commands (`ls`, `cat`, `mkdir`, …)                           | No need to rebuild images to test the OS                              |
| **Teaching size** | < 4 kLoC kernel, < 1 kLoC libc & progs                                                               | Fits in a semester project                                            |

---

## Requirements

Requires `qemu-system-x86_64`. Type `make run` to run the OS in the 
emulator. If you have problems, check out Troubleshooting below.

On macOS with brew, install `qemu`, `x86_64-elf-binutils`, and 
`x86_64-elf-gcc`, and compile with `make GCCPREFIX=x86_64-elf- run`.

## Running the OS

There are several ways to run the OS.

*   `make run`

    Build the OS and pop up a QEMU window to run it. Close the QEMU
    window to exit the OS.

*   `make run-console`

    Build the OS and run QEMU in the current terminal window. Press
    Control-C in the terminal to exit the OS.

*   `make STOP=1 run-console-gdb`

    Build the OS, run QEMU in the current terminal window, and wait for the 
    debugger to connect. Start `gdb` and type `source weensyos.gdb` to 
    connect.

In all of these run modes, QEMU also creates a file named `log.txt`.
The code we hand out doesn't actually log anything yet, but you may
find it useful to add your own calls to `log_printf` from the kernel.

Finally, run `make clean` to clean up your directory.

## Code contents

### Directory Overview

| Path                | Purpose                                                                                              |
| ------------------- | ---------------------------------------------------------------------------------------------------- |
| **boot/**           | 16‑/32‑bit bootloader that transitions the CPU from real mode to long mode and loads the kernel ELF. |
| **kernel/**         | All privileged‑mode code: scheduler, syscalls, memory management, drivers, and core subsystems.      |
| **lib/**            | Tiny libc subset shared by the kernel *and* user programs (string, stdint, malloc shim, etc.).       |
| **lib‑aes/**        | Self‑contained AES‑256 implementation used by the encrypted filesystem.                              |
| **lib‑elf/**        | Minimal ELF parsing helpers for the kernel loader.                                                   |
| **lib‑filesystem/** | In‑memory tree filesystem with per‑inode encryption glue.                                            |
| **processes/**      | Independent user‑space programs (each becomes its own ELF image on the disk).                        |
| **link/**           | ld scripts that place code/data at the right physical/virtual addresses.                             |
| **build/**          | Makefile fragments plus auto‑generated artefacts; safe to delete with `make clean`.                  |

---

### boot/

| File          | Description                                                                                           |
| ------------- | ----------------------------------------------------------------------------------------------------- |
| `bootentry.S` | Real‑mode stub (16‑bit). Enables A20, sets up a protected‑mode GDT, then jumps to `boot.c`.           |
| `boot.c`      | 32‑bit loader: switches to 64‑bit long mode, copies the kernel to `0x100000`, and jumps to its entry. |

---

### kernel/

| File                 | Description                                                                                                      |
| -------------------- | ---------------------------------------------------------------------------------------------------------------- |
| `kernel.c`           | Main entry, trap handler stubs, scheduler loop, and syscall dispatcher.                                          |
| `kernel.h`           | Global kernel declarations (process table, constants, syscall numbers). **Should not be included by user code.** |
| `k‑exception.S`      | Assembly for interrupt/trap/fault vectors; saves CPU state and calls C handlers.                                 |
| `k‑hardware.c`       | Low‑level port‑IO & MSR helpers; PIT/keyboard/serial drivers; page‑table primitives.                             |
| `k‑hardware.h`       | Declarations for the above.                                                                                      |
| `k‑loader.c`         | Loads ELF images from the disk image into a newly forked process’s address space.                                |
| `k‑malloc.c`         | 16‑byte‑aligned, zero‑on‑free heap allocator used by kernel subsystems.                                          |
| `k‑malloc.h`         | Allocator API (`kmalloc`, `kfree`, stats).                                                                       |
| `k‑entropy.c`        | Collects keyboard‑timing entropy on boot and implements `sys_getrandom`.                                         |
| `k‑entropy.h`        | Entropy pool API.                                                                                                |
| `k‑filedescriptor.c` | Per‑process FD table management (`open`, dup/fork logic).                                                        |
| `k‑filedescriptor.h` | FD data structures.                                                                                              |
| `README.md`          | Internal design notes for the allocator (kept here to avoid cluttering the main README).                         |

---

### lib/

| File                              | Description                                                                       |
| --------------------------------- | --------------------------------------------------------------------------------- |
| `lib.c` / `lib.h`                 | Misc helpers used by both kernel and user code (`console_printf`, `panic`, etc.). |
| `string.c` / `string.h`           | `memcpy`, `strcmp`, `strlen`, etc.—no libc dependency.                            |
| `lib‑malloc.c` / `lib‑malloc.h`   | *User‑space* bump‑pointer allocator (separate from kernel allocator).             |
| `process.c` / `process.h`         | Thin wrapper used by user programs for `fork`, `execv`, `wait`, and exit status.  |
| `x86‑64.h`                        | Inline assembly wrappers for key x86‑64 instructions and control registers.       |
| `errno.h`, `stddef.h`, `stdint.h` | Tiny POSIX‑style typedef and error code headers.                                  |

---

### lib‑aes/

| File    | Description                                                                       |
| ------- | --------------------------------------------------------------------------------- |
| `aes.c` | 700‑line AES‑256 implementation in C (encrypt/decrypt, key schedule, CBC helper). |
| `aes.h` | API header (block + CBC encrypt/decrypt functions).                               |

---

### lib‑elf/

| File    | Description                                                     |
| ------- | --------------------------------------------------------------- |
| `elf.h` | ELF structs/constants (32‑ & 64‑bit) used by the kernel loader. |

---

### lib‑filesystem/

| File           | Description                                                                    |
| -------------- | ------------------------------------------------------------------------------ |
| `filesystem.c` | In‑RAM tree FS: directory traversal, block allocation table, encryption hooks. |
| `filesystem.h` | FS API (`fs_open`, `fs_write`, inode structs).                                 |

---

### link/

| File         | Description                                                                  |
| ------------ | ---------------------------------------------------------------------------- |
| `boot.ld`    | Places `bootentry.S` at physical `0x7C00` with no relocations.               |
| `kernel.ld`  | Virtual layout for kernel: text at `0xFFFFFFFF80000000`; phys at `0x100000`. |
| `process.ld` | User ELF layout: text at `0x00400000`; stack at top of user address space.   |
| `shared.ld`  | Common symbols shared by kernel & user link scripts.                         |

---

### processes/

| Program          | Description                                                                          |
| ---------------- | ------------------------------------------------------------------------------------ |
| `p‑shell.c`      | Bourne‑like interactive shell with built‑in `cd`, I/O redirection, and simple pipes. |
| `p‑ls.c`         | Lists directory entries (`readdir` → `console_printf`).                              |
| `p‑cat.c`        | Streams a file to stdout.                                                            |
| `p‑touch.c`      | Creates an empty file (allocates encrypted inode).                                   |
| `p‑mkdir.c`      | Makes a directory.                                                                   |
| `p‑rm.c`         | Securely deletes a file (`unlink` + zero blocks).                                    |
| `p‑plane.c`      | Fullscreen text editor used to demonstrate read/write loops.                         |
| `p‑allocator.c`  | Stress‑tests user allocator by randomly allocating & freeing blocks.                 |
| `p‑testmalloc.c` | Verifies kernel allocator invariants via syscall fuzzing.                            |
| `p‑entropy.c`    | Prints 32 random bytes from `sys_getrandom` to stdout.                               |
| `p‑fork.c`       | Simple fork bomb demo to test scheduler fairness.                                    |
| `p‑echo.c`       | Echoes stdin to stdout; handy for redirection tests.                                 |
| `p‑rand.c`       | Generates pseudorandom numbers in user space (LCG) for comparison with kernel RNG.   |

---


## Build files

The main output of the build process is a disk image, `weensyos.img`.
QEMU "boots" off this disk image, but it could also boot on real
hardware! The build process also produces other files that you can
look at and puts them in the `obj/` directory.

*   `obj/kernel.asm`

    This file is the output of `objdump -S` on the kernel. Use it to see
    the kernel's assembly code.

*   `obj/kernel.sym`

    This smaller file just lists all the kernel's symbols (i.e.,
    variable names).

*   `obj/p-allocator.asm`, `obj/p-allocator.sym`, ...

    Similar files are generated for process code.

## Available commands on the OS

Here is a list of the commands that you can use on the OS.


| Program        | Description                                             |
| -------------- | ------------------------------------------------------- |
| **shell**      | Minimal bourne‑like shell (built‑in `cd`, `exit`).      |
| **ls**         | List directory entries.                                 |
| **cat**        | Dump file to console.                                   |
| **touch**      | Create empty file (allocates inode + key).              |
| **mkdir**      | Create directory.                                       |
| **rm**         | Securely delete file (`unlink`).                        |
| **plane**      | Tiny fullscreen text editor (demo of read/write loop).  |
| **testmalloc** | Runs allocator stress tests.                            |
| **entropy**    | Prints 32 random bytes, optionally forces pool refresh. |
| **hide**       | Hide the hardware allocation                            |
| **show**       | Show the hardware allocation                            |


## Troubleshooting

If Control-C doesn't work on your QEMU, make sure you are using an
actual Control key. On some machines QEMU ignores key remappings (such
as swapping Control and Caps Lock).

If Control-C still doesn't work on your QEMU, forcibly close it by
running `make kill`.

## References

* M. Sabt, M. Achemlal, A. Bouabdallah, “Trusted Execution Environment: What it is, and what it is not”, *IEEE TrustCom*, 2015.
* [Harvard CS61 WeensyOS](https://github.com/cs61/cs61-psets-f17)
* [Trusted Execution Environment ‑ Wikipedia](https://en.wikipedia.org/wiki/Trusted_execution_environment)
* [OS dev wiki](https://wiki.osdev.org)