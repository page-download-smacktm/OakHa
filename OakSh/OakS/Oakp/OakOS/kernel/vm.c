#include "acorn/vm.h"
#include "acorn/memory.h"

struct address_space {
    unsigned long root;
};

enum { PAGE_TABLE_PRESENT = 1, PAGE_TABLE_WRITABLE = 2, PAGE_SIZE_2M = 0x200000 };

static unsigned long *new_table(void)
{
    unsigned long address = memory_alloc_frame();
    if (address == 0) return (unsigned long *)0;
    unsigned long *table = (unsigned long *)address;
    for (unsigned long index = 0; index < 512; ++index)
        table[index] = 0;
    return table;
}

struct address_space *address_space_create(void)
{
    struct address_space *space =
        (struct address_space *)kmalloc(sizeof(struct address_space));
    if (space == (struct address_space *)0) return (struct address_space *)0;
    unsigned long *root = new_table();
    if (root == (unsigned long *)0) return (struct address_space *)0;
    unsigned long *pdpt = new_table();
    unsigned long *pd = new_table();
    if (pdpt == (unsigned long *)0 || pd == (unsigned long *)0)
        return (struct address_space *)0;
    root[0] = (unsigned long)pdpt | PAGE_TABLE_PRESENT | PAGE_TABLE_WRITABLE | VM_USER;
    pdpt[0] = (unsigned long)pd | PAGE_TABLE_PRESENT | PAGE_TABLE_WRITABLE;
    for (unsigned long index = 0; index < 512; ++index)
        pd[index] = index * PAGE_SIZE_2M | PAGE_TABLE_PRESENT |
            PAGE_TABLE_WRITABLE | 0x80;
    space->root = (unsigned long)root;
    return space;
}

void vm_activate(const struct address_space *space)
{
    if (space == (const struct address_space *)0) return;
    vm_activate_root(space->root);
}

void vm_activate_root(unsigned long root)
{
    if (root == 0) return;
    __asm__ volatile ("mov %0, %%cr3" : : "r"(root) : "memory");
}

static unsigned long vm_current_root(void)
{
    unsigned long root;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(root));
    return root;
}

int vm_map_page(struct address_space *space, unsigned long virtual_address,
    unsigned long physical_address, unsigned long flags)
{
    if (space == (struct address_space *)0 || (virtual_address & 0xFFF) != 0 ||
        (physical_address & 0xFFF) != 0) return 0;
    unsigned long *pml4 = (unsigned long *)space->root;
    unsigned long indexes[] = {
        (virtual_address >> 39) & 0x1FF,
        (virtual_address >> 30) & 0x1FF,
        (virtual_address >> 21) & 0x1FF,
        (virtual_address >> 12) & 0x1FF,
    };
    unsigned long *table = pml4;
    for (unsigned int level = 0; level < 3; ++level) {
        unsigned long entry = table[indexes[level]];
        if ((entry & VM_PRESENT) == 0) {
            unsigned long *next = new_table();
            if (next == (unsigned long *)0) return 0;
            table[indexes[level]] = (unsigned long)next | VM_PRESENT |
                VM_WRITABLE | VM_USER;
            table = next;
        } else {
            table = (unsigned long *)(entry & ~0xFFFUL);
        }
    }
    table[indexes[3]] = physical_address | flags | VM_PRESENT;
    return 1;
}

unsigned long address_space_root(const struct address_space *space)
{
    return space == (const struct address_space *)0 ? 0 : space->root;
}

int vm_self_test(void)
{
    struct address_space *first = address_space_create();
    struct address_space *second = address_space_create();
    unsigned long page = memory_alloc_frame();
    if (first == (struct address_space *)0 || second == (struct address_space *)0 ||
        page == 0 || address_space_root(first) == address_space_root(second))
        return 0;
    if (!vm_map_page(first, 0x40000000, page, VM_WRITABLE | VM_USER)) return 0;
    unsigned long current = vm_current_root();
    vm_activate(first);
    unsigned long activated = vm_current_root();
    __asm__ volatile ("mov %0, %%cr3" : : "r"(current) : "memory");
    return activated == address_space_root(first) && vm_current_root() == current;
}