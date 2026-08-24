#include "acorn/process.h"
#include "acorn/memory.h"
#include "acorn/vm.h"

enum {
    ELF_CLASS_64 = 2,
    ELF_DATA_LSB = 1,
    ELF_TYPE_EXEC = 2,
    ELF_MACHINE_X86_64 = 62,
    ELF_PT_LOAD = 1,
    ELF_PF_W = 2,
};

struct elf64_header {
    unsigned char ident[16];
    unsigned short type;
    unsigned short machine;
    unsigned int version;
    unsigned long entry;
    unsigned long program_header_offset;
    unsigned long section_header_offset;
    unsigned int flags;
    unsigned short header_size;
    unsigned short program_header_entry_size;
    unsigned short program_header_count;
    unsigned short section_header_entry_size;
    unsigned short section_header_count;
    unsigned short string_table_index;
} __attribute__((packed));

struct elf64_program_header {
    unsigned int type;
    unsigned int flags;
    unsigned long offset;
    unsigned long virtual_address;
    unsigned long physical_address;
    unsigned long file_size;
    unsigned long memory_size;
    unsigned long alignment;
} __attribute__((packed));

static int range_inside(unsigned long offset, unsigned long length,
    unsigned long total)
{
    return offset <= total && length <= total - offset;
}

int elf_load(struct address_space *space, const void *image,
    unsigned long image_size, unsigned long *entry)
{
    if (space == (struct address_space *)0 || image == (const void *)0 ||
        image_size < sizeof(struct elf64_header)) return 0;
    const struct elf64_header *header = (const struct elf64_header *)image;
    if (header->ident[0] != 0x7F || header->ident[1] != 'E' ||
        header->ident[2] != 'L' || header->ident[3] != 'F' ||
        header->ident[4] != ELF_CLASS_64 || header->ident[5] != ELF_DATA_LSB ||
        header->type != ELF_TYPE_EXEC || header->machine != ELF_MACHINE_X86_64 ||
        header->program_header_entry_size != sizeof(struct elf64_program_header) ||
        !range_inside(header->program_header_offset,
            (unsigned long)header->program_header_count * sizeof(struct elf64_program_header),
            image_size)) return 0;

    int loaded = 0;
    const unsigned char *bytes = (const unsigned char *)image;
    for (unsigned int index = 0; index < header->program_header_count; ++index) {
        const struct elf64_program_header *program =
            (const struct elf64_program_header *)(bytes + header->program_header_offset +
                index * sizeof(struct elf64_program_header));
        if (program->type != ELF_PT_LOAD) continue;
        if (program->file_size > program->memory_size ||
            !range_inside(program->offset, program->file_size, image_size) ||
            program->virtual_address + program->memory_size < program->virtual_address)
            return 0;
        unsigned long flags = VM_USER;
        if ((program->flags & ELF_PF_W) != 0) flags |= VM_WRITABLE;
        unsigned long first = program->virtual_address & ~0xFFFUL;
        unsigned long last = (program->virtual_address + program->memory_size + 0xFFF) & ~0xFFFUL;
        for (unsigned long virtual_address = first; virtual_address < last;
             virtual_address += 0x1000) {
            unsigned long physical_address = memory_alloc_frame();
            if (physical_address == 0 ||
                !vm_map_page(space, virtual_address, physical_address, flags)) return 0;
            unsigned char *page = (unsigned char *)physical_address;
            for (unsigned long byte = 0; byte < 0x1000; ++byte) page[byte] = 0;
            for (unsigned long byte = 0; byte < program->file_size; ++byte) {
                unsigned long address = program->virtual_address + byte;
                if (address >= virtual_address && address < virtual_address + 0x1000)
                    page[address - virtual_address] = bytes[program->offset + byte];
            }
        }
        loaded = 1;
    }
    if (loaded && entry != (unsigned long *)0) *entry = header->entry;
    return loaded;
}

int elf_loader_self_test(void)
{
    struct {
        struct elf64_header header;
        struct elf64_program_header program;
        unsigned char payload[4];
    } image = { 0 };
    image.header.ident[0] = 0x7F;
    image.header.ident[1] = 'E';
    image.header.ident[2] = 'L';
    image.header.ident[3] = 'F';
    image.header.ident[4] = ELF_CLASS_64;
    image.header.ident[5] = ELF_DATA_LSB;
    image.header.type = ELF_TYPE_EXEC;
    image.header.machine = ELF_MACHINE_X86_64;
    image.header.program_header_offset = sizeof(struct elf64_header);
    image.header.header_size = sizeof(struct elf64_header);
    image.header.program_header_entry_size = sizeof(struct elf64_program_header);
    image.header.program_header_count = 1;
    image.header.entry = 0x40000000;
    image.program.type = ELF_PT_LOAD;
    image.program.flags = 5;
    image.program.offset = sizeof(struct elf64_header) + sizeof(struct elf64_program_header);
    image.program.virtual_address = 0x40000000;
    image.program.file_size = sizeof(image.payload);
    image.program.memory_size = 0x1000;
    image.payload[0] = 0xC3;
    image.payload[1] = 0x90;
    image.payload[2] = 0xCC;
    image.payload[3] = 0xF4;

    struct process *process = process_create((void (*)(void))1);
    return process != (struct process *)0 &&
        process_load_elf(process, &image, sizeof(image)) &&
        process_entry(process) == image.header.entry;
}