#include "x86-64.h"
#include "elf.h"
#include "lib.h"
#include "kernel.h"

// k-loader.c
//
//    Load a weensy application into memory from a RAM image.

#define SECTORSIZE              512

extern uint8_t _binary_obj_p_allocator_start[];
extern uint8_t _binary_obj_p_allocator_end[];
extern uint8_t _binary_obj_p_fork_start[];
extern uint8_t _binary_obj_p_fork_end[];
extern uint8_t _binary_obj_p_shell_start[];
extern uint8_t _binary_obj_p_shell_end[];
extern uint8_t _binary_obj_p_cat_start[];
extern uint8_t _binary_obj_p_cat_end[];
extern uint8_t _binary_obj_p_echo_start[];
extern uint8_t _binary_obj_p_echo_end[];
extern uint8_t _binary_obj_p_ls_start[];
extern uint8_t _binary_obj_p_ls_end[];
extern uint8_t _binary_obj_p_mkdir_start[];
extern uint8_t _binary_obj_p_mkdir_end[];
//extern uint8_t _binary_obj_p_rand_start[];
//extern uint8_t _binary_obj_p_rand_end[];
extern uint8_t _binary_obj_p_entropy_start[];
extern uint8_t _binary_obj_p_entropy_end[];
extern uint8_t _binary_obj_p_plane_start[];
extern uint8_t _binary_obj_p_plane_end[];
extern uint8_t _binary_obj_p_touch_start[];
extern uint8_t _binary_obj_p_touch_end[];
extern uint8_t _binary_obj_p_rm_start[];
extern uint8_t _binary_obj_p_rm_end[];
// new for below

struct ramimage {
    void* begin;
    void* end;
} ramimages[] = {
    { _binary_obj_p_allocator_start, _binary_obj_p_allocator_end },
    { _binary_obj_p_fork_start, _binary_obj_p_fork_end },
    { _binary_obj_p_shell_start, _binary_obj_p_shell_end },
    { _binary_obj_p_cat_start, _binary_obj_p_cat_end },
    { _binary_obj_p_echo_start, _binary_obj_p_echo_end },
    { _binary_obj_p_ls_start, _binary_obj_p_ls_end },
    { _binary_obj_p_mkdir_start, _binary_obj_p_mkdir_end },
    { _binary_obj_p_rm_start, _binary_obj_p_rm_end },
    { _binary_obj_p_entropy_start, _binary_obj_p_entropy_end },
    { _binary_obj_p_plane_start, _binary_obj_p_plane_end },
    { _binary_obj_p_touch_start, _binary_obj_p_touch_end }
};

static int program_load_segment(proc* p, const elf_program* ph,
                                const uint8_t* src,
                                x86_64_pagetable* (*allocator)(void));

// program_load(p, programnumber)
//    Load the code corresponding to program `programnumber` into the process
//    `p` and set `p->p_registers.reg_rip` to its entry point. Calls
//    `assign_physical_page` to as required. Returns 0 on success and
//    -1 on failure (e.g. out-of-memory). `allocator` is passed to
//    `virtual_memory_map`.

int program_load(proc* p, int programnumber,
                 x86_64_pagetable* (*allocator)(void)) {
    // is this a valid program?
    int nprograms = sizeof(ramimages) / sizeof(ramimages[0]);
    assert(programnumber >= 0 && programnumber < nprograms);
    elf_header* eh = (elf_header*) ramimages[programnumber].begin;
    assert(eh->e_magic == ELF_MAGIC);

    // load each loadable program segment into memory
    elf_program* ph = (elf_program*) ((const uint8_t*) eh + eh->e_phoff);
    for (int i = 0; i < eh->e_phnum; ++i) {
        if (ph[i].p_type == ELF_PTYPE_LOAD) {
            const uint8_t* pdata = (const uint8_t*) eh + ph[i].p_offset;
            if (program_load_segment(p, &ph[i], pdata, allocator) < 0) {
                return -1;
            }
        }
    }

    // set the entry point from the ELF header
    p->p_registers.reg_rip = eh->e_entry;
    return 0;
}


// program_load_segment(p, ph, src, allocator)
//    Load an ELF segment at virtual address `ph->p_va` in process `p`. Copies
//    `[src, src + ph->p_filesz)` to `dst`, then clears
//    `[ph->p_va + ph->p_filesz, ph->p_va + ph->p_memsz)` to 0.
//    Calls `assign_physical_page` to allocate pages and `virtual_memory_map`
//    to map them in `p->p_pagetable`. Returns 0 on success and -1 on failure.

static int program_load_segment(proc* p, const elf_program* ph,
        const uint8_t* src,
        x86_64_pagetable* (*allocator)(void)) {
    uintptr_t va = (uintptr_t) ph->p_va;
    uintptr_t end_file = va + ph->p_filesz, end_mem = va + ph->p_memsz;
    va &= ~(PAGESIZE - 1);                // round to page boundary

    // allocate memory
    for (uintptr_t addr = va; addr < end_mem; addr += PAGESIZE) {
    uintptr_t paddr = page_alloc(p->p_pid);

    if (paddr > 0) {
    if (virtual_memory_map(p->p_pagetable, addr, paddr, PAGESIZE,
            PTE_P | PTE_W | PTE_U, allocator) < 0) {
    paddr = 0;
    }
    }

    if (paddr <= 0) {
    console_printf(CPOS(22, 0), 0xC000,
    "program_load_segment(pid %d): can't assign address %p\n",
    p->p_pid, addr);
    return -1;
    }
    }

    // ensure new memory mappings are active
    set_pagetable(p->p_pagetable);

    // copy data from executable image into process memory
    memcpy((uint8_t*) va, src, end_file - va);
    memset((uint8_t*) end_file, 0, end_mem - end_file);

    // restore kernel pagetable
    set_pagetable(kernel_pagetable);
    return 0;
}
