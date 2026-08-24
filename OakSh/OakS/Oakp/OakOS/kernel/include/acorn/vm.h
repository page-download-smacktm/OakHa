#ifndef ACORN_VM_H
#define ACORN_VM_H

struct address_space;

enum {
    VM_PRESENT = 1,
    VM_WRITABLE = 2,
    VM_USER = 4,
};

struct address_space *address_space_create(void);
void vm_activate(const struct address_space *space);
void vm_activate_root(unsigned long root);
int vm_map_page(struct address_space *space, unsigned long virtual_address,
    unsigned long physical_address, unsigned long flags);
unsigned long address_space_root(const struct address_space *space);
int vm_self_test(void);

#endif