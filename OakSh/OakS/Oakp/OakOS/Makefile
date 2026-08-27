PROJECT := oak-os
BUILD := build
ISO_DIR := $(BUILD)/iso
KERNEL := $(BUILD)/acorn.elf
ISO := $(BUILD)/oak-os.iso
DISK := $(BUILD)/oak-disk.img
LIMBO_ISO := $(BUILD)/oakos-limbo.iso
LIMBO_DISK := $(BUILD)/oakos-limbo-data.qcow2
VNC_HOST ?= 127.0.0.1
VNC_PORT ?= 6080
VNC_BIND_HOST ?= 0.0.0.0
VNC_DISPLAY ?= 1
VNC_SERVER_PORT ?= 5901
VNC_WEB ?= /usr/share/novnc
USER_ELF := $(BUILD)/user/init.elf
USER_OBJ := $(BUILD)/user/init.o $(BUILD)/user/libc.o
USER_BINARY_OBJ := $(BUILD)/user/init-elf.o
TLS_LIB := third_party/bearssl/build/libbearssl-freestanding.a
DILLO_DIR := third_party/dillo
FLTK_CONFIG ?= fltk-config
FLTK_CFLAGS ?=
FLTK_CXXFLAGS ?=
FLTK_LIBS ?=

CC := gcc
CXX := g++
LD := ld
OBJCOPY := objcopy

CFLAGS := -std=c11 -ffreestanding -fno-builtin -fno-stack-protector -fno-pic -mno-red-zone -mno-mmx -mno-sse -mno-sse2 -Wall -Wextra -Werror -O2 -g -Ikernel/include -Ithird_party/bearssl/inc
CXXFLAGS := -std=c++11 -ffreestanding -fno-builtin -fno-exceptions -fno-rtti -fno-stack-protector -fno-pic -mno-red-zone -mno-mmx -mno-sse -mno-sse2 -Wall -Wextra -Werror -O2 -g -Ikernel/include
ASFLAGS := -g
LDFLAGS := -nostdlib -z max-page-size=0x1000 -z noexecstack -T linker.ld

C_SOURCES := kernel/kernel.c kernel/serial.c kernel/vga.c kernel/framebuffer.c kernel/gui.c kernel/apps/shell.c kernel/apps/calculator.c kernel/apps/files.c kernel/apps/snake.c kernel/apps/minesweeper.c kernel/apps/tetris.c kernel/apps/pong.c kernel/mouse.c kernel/interrupts.c kernel/memory.c kernel/pic.c kernel/timer.c kernel/keyboard.c kernel/scheduler.c kernel/process.c kernel/syscall.c kernel/socket.c kernel/vm.c kernel/elf.c kernel/fs.c kernel/ata.c kernel/e1000.c kernel/network.c kernel/linux_compat.c kernel/entropy.c kernel/tls.c
ASM_SOURCES := boot/boot.S kernel/idt.S kernel/context.S
CXX_SOURCES := kernel/cxx_runtime.cpp kernel/dillo_platform.cpp
OBJECTS := $(C_SOURCES:%.c=$(BUILD)/%.o) $(CXX_SOURCES:%.cpp=$(BUILD)/%.o) $(ASM_SOURCES:%.S=$(BUILD)/%.o) $(USER_BINARY_OBJ)

.PHONY: all kernel iso limbo run run-gui run-vnc stop-vnc debug test bearssl dillo-port cxx-runtime-test dillo-platform-test clean

all: iso

bearssl:
	$(MAKE) -C third_party/bearssl

kernel: $(KERNEL)

$(KERNEL): $(OBJECTS) $(USER_ELF) $(TLS_LIB) linker.ld
	@mkdir -p $(@D)
	$(LD) $(LDFLAGS) -o $@ $(OBJECTS) $(TLS_LIB)
	$(OBJCOPY) -O binary $@ $(BUILD)/acorn.bin

$(BUILD)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: %.S
	@mkdir -p $(@D)
	$(CC) $(ASFLAGS) -c $< -o $@

$(BUILD)/%.o: %.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/kernel/cxx_runtime.o: kernel/cxx_runtime.cpp kernel/include/acorn/memory.h

$(BUILD)/user/init.o: user/init.S
	@mkdir -p $(@D)
	$(CC) $(ASFLAGS) -c $< -o $@

$(BUILD)/user/libc.o: user/libc.S
	@mkdir -p $(@D)
	$(CC) $(ASFLAGS) -c $< -o $@

$(USER_ELF): $(USER_OBJ) user.ld
	@mkdir -p $(@D)
	$(LD) -nostdlib -T user.ld -o $@ $(USER_OBJ)

$(USER_BINARY_OBJ): $(USER_ELF)
	@mkdir -p $(@D)
	$(LD) -r -b binary $< -o $@

$(TLS_LIB):
	$(MAKE) -C third_party/bearssl

dillo-port:
	@test -x "$(DILLO_DIR)/autogen.sh" || { echo "erro: fonte do Dillo ausente" >&2; exit 1; }
	@command -v autoreconf >/dev/null || { echo "erro: autoreconf nao esta instalado" >&2; exit 1; }
	@command -v "$(FLTK_CONFIG)" >/dev/null 2>&1 || { echo "erro: FLTK 1.3 e fltk-config sao necessarios para o port host do Dillo" >&2; exit 1; }
	@test -f "$(DILLO_DIR)/configure" || (cd "$(DILLO_DIR)" && ./autogen.sh)
	@mkdir -p $(BUILD)/dillo
	@test -f "$(BUILD)/dillo/Makefile" || (cd "$(BUILD)/dillo" && FLTK_CONFIG="$(abspath $(FLTK_CONFIG))" FLTK_CFLAGS="$(FLTK_CFLAGS)" FLTK_CXXFLAGS="$(FLTK_CXXFLAGS)" FLTK_LIBS="$(FLTK_LIBS)" ../../third_party/dillo/configure)
	$(MAKE) -C $(BUILD)/dillo V=1
	@echo "Dillo host compilado; o backend OakOS ainda requer port de FLTK, POSIX e rede."

cxx-runtime-test: $(BUILD)/kernel/cxx_runtime.o
	@test "$$(nm -u $(BUILD)/kernel/cxx_runtime.o | sed '/ kmalloc$$/d' | wc -l)" -eq 0 || { echo "erro: runtime C++ possui simbolos externos nao resolvidos" >&2; nm -u $(BUILD)/kernel/cxx_runtime.o; exit 1; }
	@echo "runtime C++ freestanding: OK"

dillo-platform-test: $(BUILD)/kernel/dillo_platform.o
	@nm -g $(BUILD)/kernel/dillo_platform.o | grep -q ' T dillo_platform_available'
	@nm -g $(BUILD)/kernel/dillo_platform.o | grep -q ' T dillo_platform_width'
	@nm -g $(BUILD)/kernel/dillo_platform.o | grep -q ' T dillo_platform_height'
	@nm -g $(BUILD)/kernel/dillo_platform.o | grep -q ' T dillo_platform_clear'
	@nm -g $(BUILD)/kernel/dillo_platform.o | grep -q ' T dillo_platform_fill_rect'
	@nm -g $(BUILD)/kernel/dillo_platform.o | grep -q ' T dillo_platform_draw_text'
	@nm -g $(BUILD)/kernel/dillo_platform.o | grep -q ' T dillo_platform_begin_frame'
	@nm -g $(BUILD)/kernel/dillo_platform.o | grep -q ' T dillo_platform_set_clip'
	@nm -g $(BUILD)/kernel/dillo_platform.o | grep -q ' T dillo_platform_end_frame'
	@echo "ponte grafica Dillo/framebuffer: OK"

iso: $(KERNEL) grub.cfg
	@command -v grub-mkrescue >/dev/null || { echo "erro: grub-mkrescue nao esta instalado" >&2; exit 1; }
	mkdir -p $(ISO_DIR)/boot/grub
	cp $(KERNEL) $(ISO_DIR)/boot/acorn.elf
	cp grub.cfg $(ISO_DIR)/boot/grub/grub.cfg
	grub-mkrescue -o $(ISO) $(ISO_DIR)

limbo: iso $(DISK)
	@command -v qemu-img >/dev/null || { echo "erro: qemu-img nao esta instalado" >&2; exit 1; }
	@$(MAKE) -s stop-vnc
	cp $(ISO) $(LIMBO_ISO)
	qemu-img convert -U -f raw -O qcow2 $(DISK) $(LIMBO_DISK)
	@echo "ISO Limbo: $(LIMBO_ISO)"
	@echo "Disco persistente: $(LIMBO_DISK)"

$(DISK):
	@mkdir -p $(@D)
	dd if=/dev/zero of=$@ bs=1M count=16 status=none

run: iso $(DISK)
	@command -v qemu-system-x86_64 >/dev/null || { echo "erro: qemu-system-x86_64 nao esta instalado" >&2; exit 1; }
	qemu-system-x86_64 -cpu max -nic model=e1000 -cdrom $(ISO) -drive file=$(DISK),format=raw,if=ide -nographic -no-reboot -no-shutdown

run-gui: iso $(DISK)
	@command -v qemu-system-x86_64 >/dev/null || { echo "erro: qemu-system-x86_64 nao esta instalado" >&2; exit 1; }
	@test -n "$(DISPLAY)$(WAYLAND_DISPLAY)" || { echo "erro: este Codespace nao possui DISPLAY; use 'make run-vnc' e um cliente VNC" >&2; exit 1; }
	qemu-system-x86_64 -cpu max -nic model=e1000 -vga std -cdrom $(ISO) -drive file=$(DISK),format=raw,if=ide -serial stdio -no-reboot -no-shutdown

run-vnc: iso $(DISK)
	@command -v qemu-system-x86_64 >/dev/null || { echo "erro: qemu-system-x86_64 nao esta instalado" >&2; exit 1; }
	@command -v websockify >/dev/null || { echo "erro: websockify nao esta instalado" >&2; exit 1; }
	@test -d "$(VNC_WEB)" || { echo "erro: diretorio do noVNC nao encontrado: $(VNC_WEB)" >&2; exit 1; }
	@$(MAKE) -s stop-vnc
	@echo "noVNC disponivel em http://$(VNC_HOST):$(VNC_PORT)/vnc.html?autoconnect=true&resize=scale&show_dot=true"
	@qemu-system-x86_64 -cpu max -machine pc -nic model=e1000 -vga std -cdrom $(ISO) -drive file=$(DISK),format=raw,if=ide -serial stdio -display vnc=$(VNC_HOST):$(VNC_DISPLAY) -no-reboot -no-shutdown & qemu_pid=$$!; \
	websockify --web="$(VNC_WEB)" --heartbeat=30 $(VNC_BIND_HOST):$(VNC_PORT) $(VNC_HOST):$(VNC_SERVER_PORT) & proxy_pid=$$!; \
	cleanup() { kill $$proxy_pid $$qemu_pid 2>/dev/null || true; wait $$proxy_pid $$qemu_pid 2>/dev/null || true; }; \
	trap cleanup EXIT INT TERM; \
	while kill -0 $$qemu_pid 2>/dev/null && kill -0 $$proxy_pid 2>/dev/null; do sleep 1; done

stop-vnc:
	@session_pids=$$(ps -eo pid=,comm=,args= | awk '$$2 == "qemu-system-x86" || $$2 == "websockify" || ($$2 == "python3" && $$0 ~ /websockify/) {print $$1}'); \
	for pid in $$session_pids; do kill $$pid 2>/dev/null || true; done

debug: iso $(DISK)
	@command -v qemu-system-x86_64 >/dev/null || { echo "erro: qemu-system-x86_64 nao esta instalado" >&2; exit 1; }
	qemu-system-x86_64 -cpu max -nic model=e1000 -cdrom $(ISO) -drive file=$(DISK),format=raw,if=ide -serial stdio -S -s

test: kernel
	@test "$$(readelf -h $(KERNEL) | awk '/Class:/ { print $$2 }')" = ELF64
	@readelf -S $(KERNEL) | grep -q '.text'
	@echo "testes de imagem: OK (ELF64 e secao .text presentes)"

clean:
	rm -rf $(BUILD)