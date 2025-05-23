IMAGE = weensyos.img
all: $(IMAGE)

# '$(V)' controls whether the lab makefiles print verbose commands (the
# actual shell commands run by Make), as well as the "overview" commands
# (such as '+ cc lib/readline.c').
#
# For overview commands only, run f'make all'.
# For overview and verbose commands, run 'make V=1 all'.
V = 0
ifeq ($(V),1)
compile = $(CC) $(CPPFLAGS) $(CFLAGS) $(DEPCFLAGS) $(1)
link = $(LD) $(LDFLAGS) $(1)
run = $(1) $(3)
else
compile = @/bin/echo " " $(2) $< && $(CC) $(CPPFLAGS) $(CFLAGS) $(DEPCFLAGS) $(1)
link = @/bin/echo " " $(2) $(patsubst %.full,%,$@) && $(LD) $(LDFLAGS) $(1)
run = @$(if $(2),/bin/echo " " $(2) $(3) &&,) $(1) $(3)
endif

# `$(D)` controls how QEMU responds to faults. Run `make D=1 run` to
# ask QEMU to print debugging information about interrupts and CPU resets,
# and to quit after the first triple fault instead of rebooting.
#
# `$(NCPU)` controls the number of CPUs QEMU should use. It defaults to 1.
NCPU = 1
LOG ?= file:log.txt
QEMUOPT = -net none -parallel $(LOG) -smp $(NCPU)
ifeq ($(D),1)
QEMUOPT += -d int,cpu_reset,guest_errors -no-reboot
endif
ifneq ($(NOGDB),1)
QEMUGDB ?= -gdb tcp::1234
endif
ifeq ($(STOP),1)
QEMUOPT += -S
endif

-include build/rules.mk


# Object sets

BOOT_OBJS = $(OBJDIR)/bootentry.o $(OBJDIR)/boot.o

KERNEL_C_OBJS = $(OBJDIR)/kernel.o $(OBJDIR)/k-hardware.o $(OBJDIR)/k-loader.o $(OBJDIR)/k-malloc.o $(OBJDIR)/k-filedescriptor.o $(OBJDIR)/k-entropy.o
KERNEL_OBJS = $(OBJDIR)/k-exception.o $(KERNEL_C_OBJS) $(OBJDIR)/lib.o $(OBJDIR)/string.o $(OBJDIR)/aes.o $(OBJDIR)/filesystem.o
KERNEL_LINKER_FILES = link/kernel.ld link/shared.ld

PROCESS_BINARIES = $(OBJDIR)/p-allocator $(OBJDIR)/p-fork \
	$(OBJDIR)/p-shell $(OBJDIR)/p-cat $(OBJDIR)/p-echo $(OBJDIR)/p-ls $(OBJDIR)/p-mkdir $(OBJDIR)/p-rm $(OBJDIR)/p-entropy \
	$(OBJDIR)/p-plane $(OBJDIR)/p-touch
PROCESS_LIB_OBJS = $(OBJDIR)/lib.o $(OBJDIR)/process.o $(OBJDIR)/lib-malloc.o $(OBJDIR)/string.o
ALLOCATOR_OBJS = $(OBJDIR)/p-allocator.o $(PROCESS_LIB_OBJS)

PROCESS_SRC_OBJS = $(OBJDIR)/p-allocator.o $(OBJDIR)/p-fork.o \
	$(OBJDIR)/p-shell.o $(OBJDIR)/p-cat.o $(OBJDIR)/p-echo.o $(OBJDIR)/p-mkdir.o $(OBJDIR)/p-rand.o $(OBJDIR)/p-entropy.o \
	$(OBJDIR)/p-plane.o $(OBJDIR)/p-ls.o $(OBJDIR)/p-touch.o $(OBJDIR)/p-rm.o
PROCESS_OBJS = $(PROCESS_SRC_OBJS) $(PROCESS_LIB_OBJS)
PROCESS_LINKER_FILES = link/process.ld link/shared.ld


# Generic rules for making object files

$(OBJDIR)/lib.o $(OBJDIR)/string.o: $(OBJDIR)/%.o: lib/%.c $(BUILDSTAMPS)
	$(call compile,-c $< -o $@,COMPILE)

$(OBJDIR)/aes.o: $(OBJDIR)/%.o: lib-aes/aes.c $(BUILDSTAMPS)
	$(call compile,-Ilib -c $< -o $@,COMPILE)

$(OBJDIR)/filesystem.o: $(OBJDIR)/%.o: lib-filesystem/filesystem.c $(BUILDSTAMPS)
	$(call compile,-Ilib -Ilib-aes -Ikernel -c $< -o $@,COMPILE)

$(OBJDIR)/bootentry.o: $(OBJDIR)/%.o: boot/%.S $(BUILDSTAMPS)
	$(call compile,-c $< -o $@,ASSEMBLE)

$(OBJDIR)/boot.o: $(OBJDIR)/%.o: boot/%.c $(BUILDSTAMPS)
	$(call compile,-Ilib -Ilib-elf -Os -fomit-frame-pointer -fno-inline -c $< -o $@,COMPILE)

$(OBJDIR)/k-exception.o: $(OBJDIR)/%.o: kernel/%.S $(BUILDSTAMPS)
	$(call compile,-c $< -o $@,ASSEMBLE)

$(KERNEL_C_OBJS): $(OBJDIR)/%.o: kernel/%.c $(BUILDSTAMPS) 
	$(call compile,-Ilib -Ilib-aes -Ilib-elf -Ilib-filesystem -DWEENSYOS_KERNEL -c $< -o $@,COMPILE)

$(PROCESS_SRC_OBJS): $(OBJDIR)/%.o: processes/%.c $(BUILDSTAMPS)
	$(call compile,-Ilib -O1 -DWEENSYOS_PROCESS -c $< -o $@,COMPILE)

$(OBJDIR)/process.o $(OBJDIR)/lib-malloc.o: $(OBJDIR)/%.o: lib/%.c $(BUILDSTAMPS)
	$(call compile,-O1 -DWEENSYOS_PROCESS -c $< -o $@,COMPILE)


# Specific rules for WeensyOS

$(OBJDIR)/kernel.full: $(KERNEL_OBJS) $(PROCESS_BINARIES) $(KERNEL_LINKER_FILES)
	$(call link,-T $(KERNEL_LINKER_FILES) -o $@ $(KERNEL_OBJS) -b binary $(PROCESS_BINARIES),LINK)

$(OBJDIR)/p-%.full: $(OBJDIR)/p-%.o $(PROCESS_LIB_OBJS) $(PROCESS_LINKER_FILES)
	$(call link,-T $(PROCESS_LINKER_FILES) -o $@ $< $(PROCESS_LIB_OBJS),LINK)

$(OBJDIR)/%: $(OBJDIR)/%.full
	$(call run,$(OBJDUMP) -S $< >$@.asm)
	$(call run,$(NM) -n $< >$@.sym)
	$(call run,$(OBJCOPY) -j .text -j .rodata -j .data -j .bss $<,STRIP,$@)

$(OBJDIR)/bootsector: $(BOOT_OBJS) link/boot.ld link/shared.ld
	$(call link,-T link/boot.ld link/shared.ld -o $@.full $(BOOT_OBJS),LINK)
	$(call run,$(OBJDUMP) -S $@.full >$@.asm)
	$(call run,$(NM) -n $@.full >$@.sym)
	$(call run,$(OBJCOPY) -S -O binary -j .text $@.full $@)

$(OBJDIR)/mkbootdisk: build/mkbootdisk.c $(BUILDSTAMPS)
	$(call run,$(HOSTCC) -I. -o $(OBJDIR)/mkbootdisk,HOSTCOMPILE,build/mkbootdisk.c)

weensyos.img: $(OBJDIR)/mkbootdisk $(OBJDIR)/bootsector $(OBJDIR)/kernel
	$(call run,dd if=/dev/zero of=$(OBJDIR)/filesystem.img bs=1024 count=1024)
	$(call run,$(OBJDIR)/mkbootdisk $(OBJDIR)/bootsector $(OBJDIR)/kernel @1024 $(OBJDIR)/filesystem.img > $@,CREATE $@)


run-%: run-qemu-%
	@:

run-qemu-%: run-$(QEMUDISPLAY)-%
	@:

run-graphic-%: %.img check-qemu
	$(call run,$(QEMU_PRELOAD) $(QEMU) $(QEMUOPT) $(QEMUIMG),QEMU $<)

run-console-%: %.img check-qemu
	$(call run,$(QEMU_PRELOAD) $(QEMU) $(QEMUOPT) -display curses $(QEMUIMG),QEMU $<)

run-monitor-%: %.img check-qemu
	$(call run,$(QEMU_PRELOAD) $(QEMU) $(QEMUOPT) -monitor stdio $(QEMUIMG),QEMU $<)

run-gdb-%: run-gdb-$(QEMUDISPLAY)-%
	@:

run-gdb-graphic-%: %.img check-qemu
	$(call run,$(QEMU_PRELOAD) $(QEMU) $(QEMUOPT) $(QEMUGDB) $(QEMUIMG) &,QEMU $<)
	$(call run,sleep 0.5; gdb -x weensyos.gdb,GDB)

run-gdb-console-%: %.img check-qemu
	$(call run,$(QEMU_PRELOAD) $(QEMU) $(QEMUOPT) -display curses $(QEMUGDB) $(QEMUIMG),QEMU $<)

run: run-qemu-$(basename $(IMAGE))
run-qemu: run-qemu-$(basename $(IMAGE))
run-graphic: run-graphic-$(basename $(IMAGE))
run-console: run-console-$(basename $(IMAGE))
run-monitor: run-monitor-$(basename $(IMAGE))
run-quit: run-quit-$(basename $(IMAGE))
run-gdb: run-gdb-$(basename $(IMAGE))
run-gdb-graphic: run-gdb-graphic-$(basename $(IMAGE))
run-gdb-console: run-gdb-console-$(basename $(IMAGE))
run-graphic-gdb: run-gdb-graphic-$(basename $(IMAGE))
run-console-gdb: run-gdb-console-$(basename $(IMAGE))


# Kill all my qemus
kill:
	-killall -u $$(whoami) $(QEMU)
	@sleep 0.2; if ps -U $$(whoami) | grep $(QEMU) >/dev/null; then killall -9 -u $$(whoami) $(QEMU); fi
