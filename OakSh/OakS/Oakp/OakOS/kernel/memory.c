#include "acorn/memory.h"
#include "acorn/serial.h"

enum {
    PAGE_SIZE = 4096,
    MAX_MEMORY = 1UL << 30,
    FRAME_COUNT = MAX_MEMORY / PAGE_SIZE,
    BITMAP_SIZE = FRAME_COUNT / 8,
    MULTIBOOT_INFO_MEMORY_MAP = 1 << 6,
    MULTIBOOT_MEMORY_AVAILABLE = 1,
};

struct multiboot_info {
    unsigned int flags;
    unsigned int mem_lower;
    unsigned int mem_upper;
    unsigned int boot_device;
    unsigned int command_line;
    unsigned int modules_count;
    unsigned int modules_addr;
    unsigned int symbols[4];
    unsigned int mmap_length;
    unsigned int mmap_addr;
} __attribute__((packed));

struct multiboot_mmap_entry {
    unsigned int size;
    unsigned long long base;
    unsigned long long length;
    unsigned int type;
} __attribute__((packed));

extern unsigned char __kernel_start;
extern unsigned char __kernel_end;

static unsigned char frame_bitmap[BITMAP_SIZE];
static unsigned long free_pages;
static unsigned long heap_next;
static unsigned long heap_limit;

static void set_frame(unsigned long frame)
{
    frame_bitmap[frame / 8] |= (unsigned char)(1 << (frame % 8));
}

static void clear_frame(unsigned long frame)
{
    frame_bitmap[frame / 8] &= (unsigned char)~(1 << (frame % 8));
}

static void mark_usable(unsigned long long base, unsigned long long length)
{
    unsigned long long first = (base + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    unsigned long long last = (base + length) & ~(PAGE_SIZE - 1);
    if (first >= MAX_MEMORY) return;
    if (last > MAX_MEMORY) last = MAX_MEMORY;
    for (unsigned long long address = first; address < last; address += PAGE_SIZE) {
        clear_frame((unsigned long)(address / PAGE_SIZE));
        ++free_pages;
    }
}

static void mark_reserved(unsigned long start, unsigned long end)
{
    start &= ~(PAGE_SIZE - 1);
    end = (end + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    if (start >= MAX_MEMORY) return;
    if (end > MAX_MEMORY) end = MAX_MEMORY;
    for (unsigned long address = start; address < end; address += PAGE_SIZE) {
        unsigned long frame = address / PAGE_SIZE;
        if ((frame_bitmap[frame / 8] & (1 << (frame % 8))) == 0) {
            set_frame(frame);
            --free_pages;
        }
    }
}

void memory_init(unsigned long multiboot_info)
{
    for (unsigned long index = 0; index < BITMAP_SIZE; ++index)
        frame_bitmap[index] = 0xFF;
    free_pages = 0;

    struct multiboot_info *info = (struct multiboot_info *)multiboot_info;
    if ((info->flags & MULTIBOOT_INFO_MEMORY_MAP) != 0) {
        unsigned long offset = 0;
        while (offset < info->mmap_length) {
            struct multiboot_mmap_entry *entry =
                (struct multiboot_mmap_entry *)(info->mmap_addr + offset);
            if (entry->type == MULTIBOOT_MEMORY_AVAILABLE)
                mark_usable(entry->base, entry->length);
            offset += entry->size + sizeof(entry->size);
        }
    }

    mark_reserved(0, 0x100000);
    mark_reserved((unsigned long)&__kernel_start, (unsigned long)&__kernel_end);
    heap_next = ((unsigned long)&__kernel_end + 15) & ~15UL;
    heap_limit = heap_next + 1024 * 1024;

    serial_write("memory: free pages: ");
    serial_write_hex(free_pages);
    serial_write("\n");
}

unsigned long memory_free_pages(void)
{
    return free_pages;
}

unsigned long memory_alloc_frame(void)
{
    for (unsigned long frame = 0x100000 / PAGE_SIZE;
         frame < FRAME_COUNT; ++frame) {
        if ((frame_bitmap[frame / 8] & (1 << (frame % 8))) == 0) {
            set_frame(frame);
            --free_pages;
            return frame * PAGE_SIZE;
        }
    }
    return 0;
}

void memory_free_frame(unsigned long frame_address)
{
    unsigned long frame = frame_address / PAGE_SIZE;
    if (frame >= FRAME_COUNT || frame_address == 0) return;
    if ((frame_bitmap[frame / 8] & (1 << (frame % 8))) != 0) {
        clear_frame(frame);
        ++free_pages;
    }
}

void *kmalloc(unsigned long size)
{
    if (size == 0) return (void *)0;
    unsigned long aligned_size = (size + 15) & ~15UL;
    if (heap_next > heap_limit - aligned_size) return (void *)0;
    void *result = (void *)heap_next;
    heap_next += aligned_size;
    return result;
}

int memory_self_test(void)
{
    void *first = kmalloc(64);
    void *second = kmalloc(128);
    unsigned long first_address = (unsigned long)first;
    unsigned long second_address = (unsigned long)second;
    return first != (void *)0 && second != (void *)0 &&
        (first_address & 15) == 0 && (second_address & 15) == 0 &&
        second_address >= first_address + 64;
}