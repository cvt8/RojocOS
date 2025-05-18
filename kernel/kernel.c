#include "filesystem.h"
#include "kernel.h"
#include "k-hardware.h"
#include "lib.h"
#include "errno.h"
#include "k-entropy.h"
#include "string.h"
#include "k-malloc.h"
#include "k-filedescriptor.h"

// kernel.c
//
//    This is the kernel.


// INITIAL PHYSICAL MEMORY LAYOUT
//
//  +-------------- Base Memory --------------+
//  v                                         v
// +-----+--------------------+----------------+--------------------+---------/
// |     | Kernel      Kernel |       :    I/O | App 1        App 1 | App 2
// |     | Code + Data  Stack |  ...  : Memory | Code + Data  Stack | Code ...
// +-----+--------------------+----------------+--------------------+---------/
// 0  0x40000              0x80000 0xA0000 0x100000             0x140000
//                                             ^
//                                             | \___ PROC_SIZE ___/
//                                      PROC_START_ADDR

#define PROC_SIZE 0x40000       // initial state only

static proc processes[NPROC];   // array of process descriptors
                                // Note that `processes[0]` is never used.
proc* current;                  // pointer to currently executing proc

#define HZ 100                  // timer interrupt frequency (interrupts/sec)
static unsigned ticks;          // # timer interrupts so far

void schedule(void);
void run(proc* p) __attribute__((noreturn));

static int memshow_enabled = 0;


// PAGEINFO
//
//    The pageinfo[] array keeps track of information about each physical page.
//    There is one entry per physical page.
//    `pageinfo[pn]` holds the information for physical page number `pn`.
//    You can get a physical page number from a physical address `pa` using
//    `PAGENUMBER(pa)`. (This also works for page table entries.)
//    To change a physical page number `pn` into a physical address, use
//    `PAGEADDRESS(pn)`.
//
//    pageinfo[pn].refcount is the number of times physical page `pn` is
//      currently referenced. 0 means it's free.
//    pageinfo[pn].owner is a constant indicating who owns the page.
//      PO_KERNEL means the kernel, PO_RESERVED means reserved memory (such
//      as the console), and a number >=0 means that process ID.
//
//    pageinfo_init() sets up the initial pageinfo[] state.

typedef struct physical_pageinfo {
    int8_t owner;
    int8_t refcount;
} physical_pageinfo;

static physical_pageinfo pageinfo[PAGENUMBER(MEMSIZE_PHYSICAL)];

static void pageinfo_init(void);


// Memory functions

void check_virtual_memory(void);
void memshow_physical(void);
void memshow_virtual(x86_64_pagetable* pagetable, const char* name);
void memshow_virtual_animate(void);


// Filesystem

#define FILESYSTEM_DISK_OFFSET 1024*512

static fs_descriptor fsdesc;

static int fs_read_disk(uintptr_t ptr, uint64_t start, size_t size) {
    int r = readdisk(ptr, start + FILESYSTEM_DISK_OFFSET, size);
    if (r < 0) return -EIO;
    return 0;
}

static int fs_write_disk(uintptr_t ptr, uint64_t start, size_t size) {
    int r = writedisk(ptr, start + FILESYSTEM_DISK_OFFSET, size);
    if (r < 0) return -EIO;
    return 0;
}

static void fs_generate_random(uint8_t *buffer, size_t size) {
    for (size_t i = 0; i < size; i++) {
        buffer[i] = (uint8_t) rand();
    }
}

static normpath resolve_path(const char *path) {
    log_printf("resolve_path / current->p_cwd : %s\n", current->p_cwd);
    log_printf("resolve_path / path : %s\n", path);

    char *buffer = (char *) kernel_malloc(256);
    join_path(current->p_cwd, path, buffer);
    normpath resolved_normpath = {
        .str = buffer,
        .len = strlen(buffer)
    };
    return resolved_normpath;
}


// kernel(command)
//    Initialize the hardware and processes and start running. The `command`
//    string is an optional string passed from the boot loader.

static void process_setup(pid_t pid, int program_number, pid_t parent);

void kernel(void) {
    hardware_init();
    log_printf("Starting WeensyOS\n");

    pageinfo_init();
    console_clear();
    timer_init(HZ);

    request_user_entropy();   // collect user generated entropy at boot

    // nullptr is inaccessible even to the kernel
    virtual_memory_map(kernel_pagetable, (uintptr_t) 0, (uintptr_t) 0,
		       PAGESIZE, PTE_P, NULL); // | PTE_W | PTE_U

    // Init filesystem


    fs_init(&fsdesc, fs_read_disk, fs_write_disk, fs_generate_random);

    log_printf("block_count : %d\n", fsdesc.metadata.block_count);
    log_printf("inode_count : %d\n", fsdesc.metadata.inode_count);
    log_printf("node_count : %d\n", fsdesc.metadata.node_count);

    // Set up process descriptors
    memset(processes, 0, sizeof(processes));
    for (pid_t i = 0; i < NPROC; i++) {
        processes[i].p_pid = i;
        processes[i].p_state = P_FREE;
    }

    strcpy(processes[0].p_cwd, "/");

    //process_setup(6, 0); // p-allocator
    process_setup(5, 2, 0); // hello
    process_setup(1, 1, 0); // fork

    run(&processes[1]);
}

uintptr_t page_alloc(pageowner_t owner)
{
    static int pageno = 0;
    uintptr_t pt = (uintptr_t)NULL;

    for (int tries = 0; tries < PAGENUMBER(MEMSIZE_PHYSICAL); ++tries)
    {
	if (pageinfo[pageno].owner == PO_FREE) {
	    pageinfo[pageno].owner = owner;
	    pageinfo[pageno].refcount++;
	    pt = (uintptr_t)(PAGEADDRESS(pageno));

	    memset((void*) PAGEADDRESS(pageno), 0, PAGESIZE);

	    //log_printf("proc %d: page_alloc = %d (%p)\n", current->p_pid, pageno, PAGEADDRESS(pageno));
	    break;
	}

	pageno = (pageno + 1) % PAGENUMBER(MEMSIZE_PHYSICAL);
    }

    return pt;
}

x86_64_pagetable* pagetable_alloc(void)
{
    return (x86_64_pagetable*)page_alloc(current->p_pid);
}

// process_setup(pid, program_number, parent)
//    Load application program `program_number` as process number `pid`.
//    This loads the application's code and data into memory, sets its
//    %rip and %rsp, gives it a stack page, and marks it as runnable.

void process_setup(pid_t pid, int program_number, pid_t parent) {
    extern char end[];

    process_init(&processes[pid], 0);

    current = &processes[pid];
    x86_64_pagetable* p_pagetable = pagetable_alloc();
    assert(p_pagetable != NULL);

    virtual_memory_map(p_pagetable,
		       KERNEL_START_ADDR,
		       KERNEL_START_ADDR,
		       PAGEADDRESS(PAGENUMBER((uintptr_t)end) + 1)
			    - KERNEL_START_ADDR,
		       PTE_P | PTE_W, pagetable_alloc);
    virtual_memory_map(p_pagetable,
		       KERNEL_STACK_TOP - PAGESIZE,
		       KERNEL_STACK_TOP - PAGESIZE,
		       PAGESIZE,
		       PTE_P | PTE_W, pagetable_alloc);
    virtual_memory_map(p_pagetable,
		       (uintptr_t) console,
		       (uintptr_t) console,
		       PAGESIZE, PTE_P | PTE_W | PTE_U, pagetable_alloc);

    processes[pid].p_pagetable = p_pagetable;
    int r = program_load(&processes[pid], program_number, pagetable_alloc);
    assert(r >= 0);

    processes[pid].p_registers.reg_rsp = MEMSIZE_VIRTUAL;
    uintptr_t stack_page = processes[pid].p_registers.reg_rsp - PAGESIZE;
    uintptr_t pstack_page = page_alloc(pid);
    virtual_memory_map(p_pagetable, stack_page, pstack_page,
                       PAGESIZE, PTE_P | PTE_W | PTE_U, pagetable_alloc);

    processes[pid].p_parent = parent;
    processes[pid].p_state = P_RUNNABLE;
    processes[pid].p_wait_pid = -1;
    processes[pid].fd_max = 0;
    processes[pid].fd_list = NULL;
    strcpy(processes[pid].p_cwd, processes[parent].p_cwd);
}

void process_kill(pid_t pid) {
    processes[pid].p_state = P_BROKEN;

    for (int pn = 0; pn < NPAGETABLEENTRIES; pn++) {
        if (pageinfo[pn].owner == pid) {
            assert(pageinfo[pn].refcount == 1);
            pageinfo[pn].owner = PO_FREE;
            pageinfo[pn].refcount = 0;
        }
    }

    if (processes[pid].p_parent >= 1) {
        assert(processes[pid].p_parent < NPROC);
        proc* parent = &processes[processes[pid].p_parent];

        if (parent->p_wait_pid == pid) {
            *parent->p_wait_exit_code = processes[pid].p_exit_code;
            parent->p_state = P_RUNNABLE;
        }
    }
}


// assign_physical_page(addr, owner)
//    Allocates the page with physical address `addr` to the given owner.
//    Fails if physical page `addr` was already allocated. Returns 0 on
//    success and -1 on failure. Used by the program loader.

int assign_physical_page(uintptr_t addr, int8_t owner) {
    if ((addr & 0xFFF) != 0
        || addr >= MEMSIZE_PHYSICAL
        || pageinfo[PAGENUMBER(addr)].refcount != 0) {
        return -1;
    } else {
        pageinfo[PAGENUMBER(addr)].refcount = 1;
        pageinfo[PAGENUMBER(addr)].owner = owner;
	memset((void*) PAGEADDRESS(PAGENUMBER(addr)), 0, PAGESIZE);
        return 0;
    }
}

#define STDIN_LENGTH 2024
static int stdin_buffer[STDIN_LENGTH];
static int stdin_next = 0;
static int stdin_end = 0;

// returns nothing; if there is a keyboard input, puts it into the buffer.
void check_keyboard_push(void) {
    int c = check_keyboard();

    if (c != 0 && c != -1) {
        log_printf("key %i pushed, char : %c\n", c, c);
        stdin_buffer[stdin_end] = c;
        stdin_end = (stdin_end + 1) % STDIN_LENGTH;
        assert(stdin_next != stdin_end);
    }
}

// returns -1 if there is not innput character; else returns the character (similar to the other function check_keyboard_push right above; except there is a buffer with push now)
int check_keyboard_pop(void) {
    int c = check_keyboard();

    if (c !=0 && c == -1) {
        if (stdin_next == stdin_end) {
            return -1;
        }

        c = stdin_buffer[stdin_next];
        stdin_next = (stdin_next + 1) % STDIN_LENGTH;
    }

    return c;
}


// exception(reg)
//    Exception handler (for interrupts, traps, and faults).
//
//    The register values from exception time are stored in `reg`.
//    The processor responds to an exception by saving application state on
//    the kernel's stack, then jumping to kernel assembly code (in
//    k-exception.S). That code saves more registers on the kernel's stack,
//    then calls exception().
//
//    Note that hardware interrupts are disabled whenever the kernel is running.

void exception(x86_64_registers* reg) {
    // Copy the saved registers into the `current` process descriptor
    // and always use the kernel's page table.
    current->p_registers = *reg;
    set_pagetable(kernel_pagetable);

    // It can be useful to log events using `log_printf`.
    // Events logged this way are stored in the host's `log.txt` file.
    //log_printf("proc %d: exception %d\n", current->p_pid, reg->reg_intno);

    // Show the current cursor location and memory state
    // (unless this is a kernel fault).
    console_show_cursor(cursorpos);
    if (reg->reg_intno != INT_PAGEFAULT || (reg->reg_err & PFERR_USER)) {
        check_virtual_memory();
        if (memshow_enabled) {
            memshow_physical();
            memshow_virtual_animate();
        }
    }

    // If Control-C was typed, exit the virtual machine.
    check_keyboard_push();


    // Actually handle the exception.
    switch (reg->reg_intno) {

    case INT_SYS_PANIC:
        log_printf("proc %d: exception INT_SYS_PANIC (%d)\n", current->p_pid, reg->reg_intno);

        panic(NULL);
        break;                  // will not be reached

    case INT_SYS_GETPID:
        log_printf("proc %d: exception INT_SYS_GETPID (%d)\n", current->p_pid, reg->reg_intno);

        current->p_registers.reg_rax = current->p_pid;
        break;

    case INT_SYS_EXIT:
        log_printf("proc %d: exception INT_SYS_EXIT (%d)\n", current->p_pid, reg->reg_intno);
        current->p_exit_code = current->p_registers.reg_rdi;
        process_kill(current->p_pid);
        break;

    case INT_SYS_HELLO: {
        console_printf(CPOS(10, 10), 0x0C00, "Hello, from Kernel");

        char buffer[17];
        fs_read_disk((uintptr_t) buffer, 0, 16);
        buffer[16] = '\0';
        log_printf("buffer : %s\n", buffer);

        
        buffer[0] = 'M';
        fs_write_disk((uintptr_t) buffer, 0, 16);
        
        fs_read_disk((uintptr_t) buffer, 0, 16);
        log_printf("buffer : %s\n", buffer);

        break;
    }

    case INT_SYS_KEYBORD:
        current->p_registers.reg_rax = check_keyboard_pop();
        break;

    case INT_SYS_OPEN: {
        log_printf("proc %d: exception INT_SYS_OPEN (%d)\n", current->p_pid, reg->reg_intno);

        uintptr_t va = current->p_registers.reg_rdi;
        vamapping vam = virtual_memory_lookup(current->p_pagetable, va);
        normpath path = resolve_path((char *) vam.pa);

        log_printf("path : %s\n", path);

        int64_t r = fs_getattr(&fsdesc, path);
        if (r < 0) {
            log_printf("getattr failed %d\n", r);
            current->p_registers.reg_rax = r;
            break;
        }
        uint32_t inode = (uint32_t) r;
        log_printf("inode : %d\n", inode);

        current->fd_max++;
        r = fdlist_add_entry(&current->fd_list, current->fd_max, inode);
        if (r < 0) {
            log_printf("fdlist_add_entry failed %d\n", inode);
            current->p_registers.reg_rax = r;
            break;
        };


        // TODO: test
        //proc_fdentry_t *entry = fdlist_search_entry(&current->fd_list, current->fd_max);
        //log_printf("entry : %d\n", entry->inode);

        current->p_registers.reg_rax = inode;
        current->p_registers.reg_rax = current->fd_max;
        break;
    }

    case INT_SYS_REMOVE: {
        log_printf("proc %d: exception INT_SYS_REMOVE (%d)\n", current->p_pid, reg->reg_intno);

        uintptr_t va = current->p_registers.reg_rdi;
        vamapping vam = virtual_memory_lookup(current->p_pagetable, va);
        normpath path = resolve_path((char *) vam.pa);

        int r = fs_remove(&fsdesc, path);
        if (r < 0) {
            log_printf("remove failed %d\n", r);
            current->p_registers.reg_rax = -1;
            break;
        }

        current->p_registers.reg_rax = 0;
        break;
    }

    case INT_SYS_GETRANDOM:
        current->p_registers.reg_rax = get_entropy_value();
        break;

    case INT_SYS_PAGE_ALLOC: {
        uintptr_t vaddr = current->p_registers.reg_rdi;
        uintptr_t paddr = page_alloc(current->p_pid);
        
        if (paddr == (uintptr_t)NULL) {
            current->p_registers.reg_rax = -1;
            console_printf(CPOS(24, 0), 0x0C00, "Out of physical memory!");
        } else {
            virtual_memory_map(current->p_pagetable, vaddr, paddr,
                                PAGESIZE, PTE_P | PTE_W | PTE_U, NULL);
            current->p_registers.reg_rax = vaddr;
        }
        
        break;
    }

    case INT_SYS_READ: {
        log_printf("proc %d: exception INT_SYS_READ (%d)\n", current->p_pid, reg->reg_intno);

        int fd = current->p_registers.reg_rdi;
        log_printf("fd : %d\n", fd);

        uintptr_t va = current->p_registers.reg_rsi;
        vamapping vam = virtual_memory_lookup(current->p_pagetable, va);
        uintptr_t buf = vam.pa;

        size_t size = current->p_registers.reg_rdx; // TODO: Max ssize_t / size_t
        log_printf("size : %d\n", size);

        proc_fdentry_t *entry = fdlist_search_entry(&current->fd_list, fd);
        log_printf("entry : %d\n", entry->inode);
        log_printf("offset : %d\n", entry->offset);

        // TODO: check if size available

        int r = fs_read(&fsdesc, entry->inode, (void *) buf, size, entry->offset);
        if (r < 0) {
            current->p_registers.reg_rax = r;
            break;
        }

        log_printf("read %d bytes\n", r);

        entry->offset += r;
        current->p_registers.reg_rax = r;
        break;
    }

    case INT_SYS_WRITE: {
        log_printf("proc %d: exception INT_SYS_WRITE (%d)\n", current->p_pid, reg->reg_intno);

        int fd = current->p_registers.reg_rdi;

        uintptr_t va = current->p_registers.reg_rsi;
        vamapping vam = virtual_memory_lookup(current->p_pagetable, va);
        uintptr_t buf = vam.pa;

        size_t size = current->p_registers.reg_rdx; // TODO: Max ssize_t / size_t
        //log_printf("buf : %p\n", (void *) buf);
        log_printf("size : %d\n", size);

        proc_fdentry_t *entry = fdlist_search_entry(&current->fd_list, fd);
        if (entry == NULL) {
            log_printf("fd %d not found\n", fd);
            current->p_registers.reg_rax = -1;
            break;
        }
        log_printf("fd : %d, inode : %d, offset : %d\n", fd, entry->inode, entry->offset);

        int r = fs_write(&fsdesc, entry->inode, (void *) buf, size, entry->offset);
        if (r < 0) {
            log_printf("write failed %d\n", r);
            current->p_registers.reg_rax = r;
            break;
        }

        entry->offset += size;

        current->p_registers.reg_rax = size;
        break;
    }

    case INT_SYS_MKDIR: {
        log_printf("proc %d: exception INT_SYS_MKDIR (%d)\n", current->p_pid, reg->reg_intno);
        
        uintptr_t va = current->p_registers.reg_rdi;
        vamapping vam = virtual_memory_lookup(current->p_pagetable, va);
        normpath path = resolve_path((char *) vam.pa);

        log_printf("mkdir path : %.*s\n", (int)path.len, path.str);
        int r = fs_touch(&fsdesc, path, 0);

        if (r < 0) {
            log_printf("mkdir failed %d\n", r);
            current->p_registers.reg_rax = r;
            break;
        }

        current->p_registers.reg_rax = 0;
        break;
    }

    case INT_SYS_TOUCH: {
        log_printf("proc %d: exception INT_SYS_TOUCH (%d)\n", current->p_pid, reg->reg_intno);
        
        uintptr_t va = current->p_registers.reg_rdi;
        vamapping vam = virtual_memory_lookup(current->p_pagetable, va);
        normpath path = resolve_path((char *) vam.pa);

        int64_t r = fs_alloc_inode(&fsdesc);
        if (r < 0) {
            log_printf("alloc inode failed %d\n", r);
            current->p_registers.reg_rax = r;
            break;
        }
        uint32_t inode = (uint32_t) r;

        log_printf("inode : %d\n", inode);

        r = fs_touch(&fsdesc, path, inode);
        if (r < 0) {
            log_printf("touch failed %d\n", r);
            current->p_registers.reg_rax = r;
            break;
        }

        current->p_registers.reg_rax = 0;
        break;
    }

    case INT_SYS_LISTDIR: {
        log_printf("proc %d: exception INT_SYS_LISTDIR (%d)\n", current->p_pid, reg->reg_intno);
        
        uintptr_t va = current->p_registers.reg_rdi;
        vamapping vam = virtual_memory_lookup(current->p_pagetable, va);
        normpath path = resolve_path((char *) vam.pa);

        log_printf("listdir path : %.*s\n", (int)path.len, path.str);

        va = current->p_registers.reg_rsi;
        vam = virtual_memory_lookup(current->p_pagetable, va);
        char *buffer = (char *) vam.pa;

        fs_dirreader dr;
        int children_count = fs_readdir_init(&fsdesc, path, &dr);
        if (children_count < 0) {
            log_printf("proc %d: LISTDIR, readdir_init failed\n", current->p_pid);
            current->p_registers.reg_rax = children_count;
            break;
        }

        log_printf("children_count : %d\n", children_count);

        for (int i = 0; i < children_count; i++) {
            char name[32]; // TODO: 32 -> NAME_SIZE
            int r = fs_readdir_next(&dr, name);
            if (r < 0) {
                log_printf("proc %d: LISTDIR, readdir_next failed\n", current->p_pid);
                current->p_registers.reg_rax = r;
                break;
            }

            for (int j = 0; j < 32 && name[j]; j++) {
                *buffer = name[j];
                buffer++;
            }
            
            *buffer = '\n';
            buffer++;
        }

        log_printf("buffer : %s\n", (char *) vam.pa);

        *buffer = '\0';

        log_printf("proc %d: LISTDIR, success\n", current->p_pid);
        current->p_registers.reg_rax = 0;
        break;
    }

    case INT_SYS_EXECV: {

        log_printf("proc %d: exception INT_SYS_EXECV (%d)\n", current->p_pid, reg->reg_intno);

        // TODO: Better EXECV

        uintptr_t va = current->p_registers.reg_rdi;
        vamapping vam = virtual_memory_lookup(current->p_pagetable, va);
        char* path = (char*) vam.pa;

        // path is not safe

        if (strcmp(path, "show") == 0) {
            memshow_enabled = 1;
            current->p_exit_code = 0;
            process_kill(current->p_pid);
            break;
        }
        if (strcmp(path, "hide") == 0) {
            memshow_enabled = 0;
            console_clear();

            current->p_exit_code = 0;
            process_kill(current->p_pid);
            break;
        }
        if (strcmp(path, "clear") == 0) {
            console_clear();
            current->p_exit_code = 0;
            process_kill(current->p_pid);
            break;
        }
        if (strcmp(path, "testmalloc") == 0) {
            va = current->p_registers.reg_rsi;
            vam = virtual_memory_lookup(current->p_pagetable, va);
            char** argv = (char**) vam.pa;

            if (argv[1]) {
                vam = virtual_memory_lookup(current->p_pagetable, (uintptr_t) argv[1]);
                char *arg = (char*) vam.pa;
                testmalloc(arg);
            } else {
                testmalloc(NULL);
            }
            
            current->p_exit_code = 0;
            process_kill(current->p_pid);
            break;
        }

        // Check path

        int program_number = -1;
        
        if (strcmp(path, "cat") == 0) {
            log_printf("run cat\n");
            program_number = 3;
        } else if (strcmp(path, "echo") == 0) {
            log_printf("run echo\n");
            program_number = 4;
        } else if (strcmp(path, "ls") == 0) {
            log_printf("run ls\n");
            program_number = 5;
        } else if (strcmp(path, "mkdir") == 0) {
            log_printf("run mkdir\n");
            program_number = 6;
        } else if (strcmp(path, "rm") == 0) {
            log_printf("run rm\n");
            program_number = 7;
        } else if (strcmp(path, "entropy") == 0) {
            log_printf("run entropy\n");
            program_number = 8;
        } else if (strcmp(path, "plane") == 0) {
            log_printf("run plane\n");
            program_number = 9;
        } else if (strcmp(path, "touch") == 0) {
            log_printf("run touch\n");
            program_number = 10;
        } else {
            log_printf("command not found : %s\n", path);
            current->p_registers.reg_rax = -1;
            break;
        }


        log_printf("program_numer : %d\n", program_number);

        // Arguments
        log_printf("Arguments\n");

        va = current->p_registers.reg_rsi;
        vam = virtual_memory_lookup(current->p_pagetable, va);
        char** argv = (char**) vam.pa;

        int argc = 0;
        while (argv[argc]) argc++;

        uintptr_t pargs_va = 0x140000;

        uintptr_t pargs_pa = page_alloc(current->p_pid);
        if (pargs_pa == (uintptr_t) NULL) {
            console_printf(CPOS(24, 0), 0x0C00, "Out of physical memory!");
            assert(0);
        }

        // Copy argv to pargs
        log_printf("copy argv to pargs_pa\n");

        uintptr_t offset = (argc+1)*sizeof(char*);

        for (int i = 0; i < argc; i++) {
            ((uintptr_t*) pargs_pa)[i] = pargs_va+offset;
            vam = virtual_memory_lookup(current->p_pagetable, (uintptr_t) argv[i]);
            strcpy((char*) (pargs_pa+offset), (char*) vam.pa);
            offset += strlen((char*) vam.pa)+1;
        }

        ((char**) pargs_pa)[argc] = NULL;

        // Clear page table
        for (int pn = 0; pn < NPAGETABLEENTRIES; pn++) {
            if (pageinfo[pn].owner == current->p_pid && PAGEADDRESS(pn) != pargs_pa) {
                assert(pageinfo[pn].refcount == 1);
                pageinfo[pn].owner = PO_FREE;
                pageinfo[pn].refcount = 0;
            }
        }

        // Setup process
        process_setup(current->p_pid, program_number, current->p_parent);

        // Setup arguments
        assert(virtual_memory_map(current->p_pagetable, pargs_va, pargs_pa,
                            PAGESIZE, PTE_P | PTE_W | PTE_U, NULL) == 0);

        current->p_registers.reg_rdi = argc;
        current->p_registers.reg_rsi = pargs_va;
        break;
    }

    case INT_SYS_WAIT: {
        log_printf("proc %d: exception INT_SYS_WAIT (%d)\n", current->p_pid, reg->reg_intno);

        pid_t pid = current->p_registers.reg_rdi;
        uintptr_t va = current->p_registers.reg_rsi;
        vamapping vam = virtual_memory_lookup(current->p_pagetable, va);
        int* exit_code = (int*) vam.pa;

        assert(pid >= 1);
        assert(processes[pid].p_parent == current->p_pid);

        current->p_registers.reg_rax = 0;

        if (processes[pid].p_state == P_BROKEN) {
            *exit_code = processes[pid].p_exit_code;
            break;
        }

        current->p_state = P_BLOCKED;
        current->p_wait_pid = pid;
        current->p_wait_exit_code = exit_code;

        break;
    }

    case INT_SYS_FORGET: {
        pid_t pid = current->p_registers.reg_rdi;

        assert(pid >= 1 && pid < NPROC);
        assert(processes[pid].p_parent == current->p_pid);
        assert(processes[pid].p_state == P_BROKEN);

        processes[pid].p_state = P_FREE;
        current->p_registers.reg_rax = 0;
    
        break;
    }

    case INT_SYS_GETCWD: {

        log_printf("proc %d: exception INT_SYS_GETCWD (%d)\n", current->p_pid, reg->reg_intno);

        uintptr_t va = current->p_registers.reg_rdi;
        vamapping vam = virtual_memory_lookup(current->p_pagetable, va);
        char* buffer = (char*) vam.pa;
        size_t size = current->p_registers.reg_rsi;

        strcpy(buffer, current->p_cwd); // TODO: Buffer overflow exploit

        current->p_registers.reg_rax = 0;
        
        break;
    }

    case INT_SYS_CHDIR: {
        log_printf("proc %d: exception INT_SYS_CHDIR (%d)\n", current->p_pid, reg->reg_intno);

        uintptr_t va = current->p_registers.reg_rdi;
        vamapping vam = virtual_memory_lookup(current->p_pagetable, va);
        normpath path = resolve_path((char *) vam.pa);;

        int64_t r = fs_getattr(&fsdesc, path);
        if (r < 0) {
            current->p_registers.reg_rax = r;
            break;
        }
        if (r > 0) {
            current->p_registers.reg_rax = -ENOTDIR;
            break;
        }

        copy_to_buffer(current->p_cwd, path);

        current->p_registers.reg_rax = 0;
        break;
    }

    case INT_TIMER:
        //log_printf("proc %d: exception INT_TIMER (%d)\n", current->p_pid, reg->reg_intno);

        ++ticks;
        schedule();
        break;                  /* will not be reached */

    case INT_PAGEFAULT: {
        log_printf("proc %d: exception INT_PAGEFAULT (%d)\n", current->p_pid, reg->reg_intno);

        // Analyze faulting address and access type.
        uintptr_t addr = rcr2();
        const char* operation = reg->reg_err & PFERR_WRITE
                ? "write" : "read";
        const char* problem = reg->reg_err & PFERR_PRESENT
                ? "protection problem" : "missing page";

        if (!(reg->reg_err & PFERR_USER)) {
            panic("Kernel page fault for %p (%s %s, rip=%p)!\n",
                  addr, operation, problem, reg->reg_rip);
        }
        console_printf(CPOS(24, 0), 0x0C00,
                       "Process %d page fault for %p (%s %s, rip=%p)!\n",
                       current->p_pid, addr, operation, problem, reg->reg_rip);
        current->p_state = P_BROKEN;
        break;
    }

    case INT_SYS_FORK: {
        // TODO: Better FORK
        log_printf("proc %d: exception INT_SYS_FORK (%d)\n", current->p_pid, reg->reg_intno);

        pid_t pid = 0;
        for (pid_t i = 1; i < NPROC; i++) {
            if (processes[i].p_state == P_FREE) {
                pid = i;
                break;
            }
        }

        if (pid == 0) {
            current->p_registers.reg_rax = -1;
            break;
        }

        proc* parent = current;
        current = &processes[pid]; // set current process to child process before any call of pagetable_alloc
        x86_64_pagetable* p_pagetable = pagetable_alloc();

        if (p_pagetable == NULL) {
            parent->p_registers.reg_rax = -1;
            current = parent;
            break;
        }

        for (uintptr_t va = 0; va < MEMSIZE_VIRTUAL; va += PAGESIZE) {
            vamapping vam = virtual_memory_lookup(parent->p_pagetable, va);

            if (vam.pn == -1)
                continue;

            assert(vam.pn >= 0);

            if (pageinfo[vam.pn].owner == parent->p_pid) {
                uintptr_t pa = page_alloc(pid);
                assert(pa != (uintptr_t) NULL);
                memcpy((void*) pa, (void*) vam.pa, PAGESIZE);
                virtual_memory_map(p_pagetable, va, pa, PAGESIZE, vam.perm, pagetable_alloc);
            } else {
                virtual_memory_map(p_pagetable, va, vam.pa, PAGESIZE, vam.perm, pagetable_alloc);
            }
        }

        current->p_parent = parent->p_pid;
        current->p_pagetable = p_pagetable;
        current->p_registers = parent->p_registers;
        current->p_registers.reg_rax = 0;
        current->p_state = P_RUNNABLE;
        current->p_wait_pid = -1;
        strcpy(current->p_cwd, parent->p_cwd);

        parent->p_registers.reg_rax = pid;

        current = parent;

        break;
    }

    case INT_SYS_KILL: {
        current->p_registers.reg_rax = 0;
        pid_t pid = current->p_registers.reg_rdi;
        process_kill(pid);
        break;
    }

    case INT_SYS_SCHED_YIELD:
        //log_printf("proc %d: exception INT_SYS_YIELD (%d)\n", current->p_pid, reg->reg_intno);

        schedule();
        break;                  /* will not be reached */

    default:
        panic("Unexpected exception %d!\n", reg->reg_intno);
        break;                  /* will not be reached */

    }


    // Return to the current process (or run something else).
    if (current->p_state == P_RUNNABLE) {
        run(current);
    } else {
        schedule();
    }
}


// schedule
//    Pick the next process to run and then run it.
//    If there are no runnable processes, spins forever.

void schedule(void) {
    pid_t pid = current->p_pid;
    while (1) {
        pid = (pid + 1) % NPROC;
        if (processes[pid].p_state == P_RUNNABLE) {
            run(&processes[pid]);
        }
        // If Control-C was typed, exit the virtual machine.
        check_keyboard_push();
    }
}


// run(p)
//    Run process `p`. This means reloading all the registers from
//    `p->p_registers` using the `popal`, `popl`, and `iret` instructions.
//
//    As a side effect, sets `current = p`.

void run(proc* p) {
    assert(p->p_state == P_RUNNABLE);
    current = p;

    // Load the process's current pagetable.
    set_pagetable(p->p_pagetable);

    // This function is defined in k-exception.S. It restores the process's
    // registers then jumps back to user mode.
    exception_return(&p->p_registers);

 spinloop: goto spinloop;       // should never get here
}


// pageinfo_init
//    Initialize the `pageinfo[]` array.

void pageinfo_init(void) {
    extern char end[];

    for (uintptr_t addr = 0; addr < MEMSIZE_PHYSICAL; addr += PAGESIZE) {
        int owner;
        if (physical_memory_isreserved(addr)) {
            owner = PO_RESERVED;
        } else if ((addr >= KERNEL_START_ADDR && addr < (uintptr_t) end)
                   || addr == KERNEL_STACK_TOP - PAGESIZE) {
            owner = PO_KERNEL;
        } else {
            owner = PO_FREE;
        }
        pageinfo[PAGENUMBER(addr)].owner = owner;
        pageinfo[PAGENUMBER(addr)].refcount = (owner != PO_FREE);
    }
}


// check_page_table_mappings
//    Check operating system invariants about kernel mappings for page
//    table `pt`. Panic if any of the invariants are false.

void check_page_table_mappings(x86_64_pagetable* pt) {
    extern char start_data[], end[];
    assert(PTE_ADDR(pt) == (uintptr_t) pt);

    // kernel memory is identity mapped; data is writable
    for (uintptr_t va = KERNEL_START_ADDR; va < (uintptr_t) end;
         va += PAGESIZE) {
        vamapping vam = virtual_memory_lookup(pt, va);
        if (vam.pa != va) {
            console_printf(CPOS(22, 0), 0xC000, "%p vs %p\n", va, vam.pa);
        }
        assert(vam.pa == va);
        if (va >= (uintptr_t) start_data) {
            assert(vam.perm & PTE_W);
        }
    }

    // kernel stack is identity mapped and writable
    uintptr_t kstack = KERNEL_STACK_TOP - PAGESIZE;
    vamapping vam = virtual_memory_lookup(pt, kstack);
    assert(vam.pa == kstack);
    assert(vam.perm & PTE_W);
}


// check_page_table_ownership
//    Check operating system invariants about ownership and reference
//    counts for page table `pt`. Panic if any of the invariants are false.

static void check_page_table_ownership_level(x86_64_pagetable* pt, int level,
                                             int owner, int refcount);

void check_page_table_ownership(x86_64_pagetable* pt, pid_t pid) {
    // calculate expected reference count for page tables
    int owner = pid;
    int expected_refcount = 1;
    if (pt == kernel_pagetable) {
        owner = PO_KERNEL;
        for (int xpid = 0; xpid < NPROC; ++xpid) {
            if (processes[xpid].p_state != P_FREE
                && processes[xpid].p_pagetable == kernel_pagetable) {
                ++expected_refcount;
            }
        }
    }
    check_page_table_ownership_level(pt, 0, owner, expected_refcount);
}

static void check_page_table_ownership_level(x86_64_pagetable* pt, int level,
                                             int owner, int refcount) {

    /*log_printf("level : %d\n", level);
    log_printf("owner : %d\n", owner);
    log_printf("refcount : %d\n", refcount);
    log_printf("pt owner : %d\n", pageinfo[PAGENUMBER(pt)].owner);*/


    assert(PAGENUMBER(pt) < NPAGES);
    assert(pageinfo[PAGENUMBER(pt)].owner == owner);
    assert(pageinfo[PAGENUMBER(pt)].refcount == refcount);
    if (level < 3) {
        for (int index = 0; index < NPAGETABLEENTRIES; ++index) {
            if (pt->entry[index]) {
                x86_64_pagetable* nextpt =
                    (x86_64_pagetable*) PTE_ADDR(pt->entry[index]);
                check_page_table_ownership_level(nextpt, level + 1, owner, 1);
            }
        }
    }
}


// check_virtual_memory
//    Check operating system invariants about virtual memory. Panic if any
//    of the invariants are false.

void check_virtual_memory(void) {
    // Process 0 must never be used.
    assert(processes[0].p_state == P_FREE);

    // The kernel page table should be owned by the kernel;
    // its reference count should equal 1, plus the number of processes
    // that don't have their own page tables.
    // Active processes have their own page tables. A process page table
    // should be owned by that process and have reference count 1.
    // All level-2-4 page tables must have reference count 1.

    check_page_table_mappings(kernel_pagetable);
    check_page_table_ownership(kernel_pagetable, -1);

    for (int pid = 0; pid < NPROC; ++pid) {
        if (processes[pid].p_state != P_FREE
            && processes[pid].p_state != P_BROKEN
            && processes[pid].p_pagetable != kernel_pagetable) {
            check_page_table_mappings(processes[pid].p_pagetable);
            check_page_table_ownership(processes[pid].p_pagetable, pid);
        }
    }

    // Check that all referenced pages refer to active processes
    for (int pn = 0; pn < PAGENUMBER(MEMSIZE_PHYSICAL); ++pn) {
        if (pageinfo[pn].refcount > 0 && pageinfo[pn].owner >= 0) {
            assert(processes[pageinfo[pn].owner].p_state != P_FREE);
        }
    }
}


// memshow_physical
//    Draw a picture of physical memory on the CGA console.

static const uint16_t memstate_colors[] = {
    'H' | 0x0D00,
    'K' | 0x0D00, 'R' | 0x0700, '.' | 0x0700, '1' | 0x0C00,
    '2' | 0x0A00, '3' | 0x0900, '4' | 0x0E00, '5' | 0x0F00,
    '6' | 0x0C00, '7' | 0x0A00, '8' | 0x0900, '9' | 0x0E00,
    'A' | 0x0F00, 'B' | 0x0C00, 'C' | 0x0A00, 'D' | 0x0900,
    'E' | 0x0E00, 'F' | 0x0F00
};

void memshow_physical(void) {
    console_printf(CPOS(0, 32), 0x0F00, "PHYSICAL MEMORY");
    for (int pn = 0; pn < PAGENUMBER(MEMSIZE_PHYSICAL); ++pn) {
        if (pn % 64 == 0) {
            console_printf(CPOS(1 + pn / 64, 3), 0x0F00, "0x%06X ", pn << 12);
        }

        int owner = pageinfo[pn].owner;
        if (pageinfo[pn].refcount == 0) {
            owner = PO_FREE;
        }
        uint16_t color = memstate_colors[owner - PO_KERNEL_HEAP];

	if (pn == PAGENUMBER(console)) {
	    color = 'C' | 0x0700;
	}

        // darker color for shared pages
        if (pageinfo[pn].refcount > 1) {
	    color = 'S' | 0x0700;
        }

        console[CPOS(1 + pn / 64, 12 + pn % 64)] = color;
    }
}


// memshow_virtual(pagetable, name)
//    Draw a picture of the virtual memory map `pagetable` (named `name`) on
//    the CGA console.

void memshow_virtual(x86_64_pagetable* pagetable, const char* name) {
    assert((uintptr_t) pagetable == PTE_ADDR(pagetable));

    console_printf(CPOS(10, 26), 0x0F00, "VIRTUAL ADDRESS SPACE FOR %s", name);
    for (uintptr_t va = 0; va < MEMSIZE_VIRTUAL; va += PAGESIZE) {
        vamapping vam = virtual_memory_lookup(pagetable, va);
        uint16_t color;
        if (vam.pn < 0) {
            color = ' ';
        } else {
            assert(vam.pa < MEMSIZE_PHYSICAL);
            int owner = pageinfo[vam.pn].owner;
            if (pageinfo[vam.pn].refcount == 0) {
                owner = PO_FREE;
            }
	    if (vam.pn == PAGENUMBER(console)) {
		color = 'C' | 0x0700;
	    } else {
		color = memstate_colors[owner - PO_KERNEL_HEAP];
	    }
            // reverse video for user-accessible pages
            if (vam.perm & PTE_U) {
                color = ((color & 0x0F00) << 4) | ((color & 0xF000) >> 4)
                    | (color & 0x00FF);
            }
            // darker color for shared pages
            if (pageinfo[vam.pn].refcount > 1) {
                color &= 0x77FF;
            }
        }
        uint32_t pn = PAGENUMBER(va);
        if (pn % 64 == 0) {
            console_printf(CPOS(11 + pn / 64, 3), 0x0F00, "0x%06X ", va);
        }
        console[CPOS(11 + pn / 64, 12 + pn % 64)] = color;
    }
}


// memshow_virtual_animate
//    Draw a picture of process virtual memory maps on the CGA console.
//    Starts with process 1, then switches to a new process every 0.25 sec.

void memshow_virtual_animate(void) {
    static unsigned last_ticks = 0;
    static int showing = 1;

    // switch to a new process every 0.25 sec
    if (last_ticks == 0 || ticks - last_ticks >= HZ / 2) {
        last_ticks = ticks;
        ++showing;
    }

    // the current process may have died -- don't display it if so
    while (showing <= 2*NPROC
           && processes[showing % NPROC].p_state == P_FREE) {
        ++showing;
    }
    showing = showing % NPROC;

    if (processes[showing].p_state != P_FREE) {
        char s[4];
        snprintf(s, 4, "%d ", showing);
        memshow_virtual(processes[showing].p_pagetable, s);
    }
}



