CC ?= gcc
AR ?= ar
BUILD ?= build
CFLAGS ?= -std=c11 -ffreestanding -fno-builtin -fno-stack-protector -fno-pic -mno-red-zone -mno-mmx -mno-sse -mno-sse2 -Wall -Wextra -Werror -O2 -Iinc -Isrc -I../../kernel/include
SOURCES := $(shell find src -type f -name '*.c' ! -path 'src/rand/sysrng.c' ! -path 'src/tool/*' ! -path 'src/codec/*.c' -o -path 'src/codec/*.c')
OBJECTS := $(patsubst src/%.c,$(BUILD)/%.o,$(SOURCES))

.PHONY: all clean

all: $(BUILD)/libbearssl-freestanding.a

$(BUILD)/libbearssl-freestanding.a: $(OBJECTS)
	@mkdir -p $(@D)
	$(AR) rcs $@ $(OBJECTS)

$(BUILD)/%.o: src/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD)
