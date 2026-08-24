#ifndef ACORN_MEMORY_H
#define ACORN_MEMORY_H

#ifdef __cplusplus
extern "C" {
#endif

void memory_init(unsigned long multiboot_info);
unsigned long memory_free_pages(void);
void *kmalloc(unsigned long size);
unsigned long memory_alloc_frame(void);
void memory_free_frame(unsigned long frame);
int memory_self_test(void);

#ifdef __cplusplus
}
#endif

#endif