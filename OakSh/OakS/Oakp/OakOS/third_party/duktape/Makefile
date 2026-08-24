CC ?= gcc
BUILD ?= build
CFLAGS ?= -std=c11 -ffreestanding -fno-builtin -fno-stack-protector -fno-pic -mno-red-zone -msse2 -mfpmath=sse -Wall -Wextra -Werror -O2 -DDUK_USE_DATE_NOW=0

.PHONY: all clean
all: $(BUILD)/duktape.o

$(BUILD)/duktape.o: duktape.c duktape.h
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -c duktape.c -o $@

clean:
	rm -rf $(BUILD)
