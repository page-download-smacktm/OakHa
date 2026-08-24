#include "acorn/ata.h"

enum {
    ATA_DATA = 0x1F0,
    ATA_ERROR = 0x1F1,
    ATA_SECTOR_COUNT = 0x1F2,
    ATA_LBA_LOW = 0x1F3,
    ATA_LBA_MID = 0x1F4,
    ATA_LBA_HIGH = 0x1F5,
    ATA_DRIVE = 0x1F6,
    ATA_STATUS = 0x1F7,
    ATA_COMMAND = 0x1F7,
    ATA_CMD_READ = 0x20,
    ATA_CMD_WRITE = 0x30,
    ATA_CMD_IDENTIFY = 0xEC,
    ATA_BUSY = 0x80,
    ATA_DRQ = 0x08,
    ATA_ERR = 0x01,
};

static int drive_present;

static inline void outb(unsigned short port, unsigned char value)
{
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline unsigned char inb(unsigned short port)
{
    unsigned char value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static inline unsigned short inw(unsigned short port)
{
    unsigned short value;
    __asm__ volatile ("inw %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static inline void outw(unsigned short port, unsigned short value)
{
    __asm__ volatile ("outw %0, %1" : : "a"(value), "Nd"(port));
}

static int wait_status(unsigned char required)
{
    for (unsigned long count = 0; count < 1000000; ++count) {
        unsigned char status = inb(ATA_STATUS);
        if ((status & ATA_BUSY) == 0) {
            if ((status & ATA_ERR) != 0) return 0;
            if ((status & required) == required) return 1;
        }
    }
    return 0;
}

void ata_init(void)
{
    unsigned short identify[256];
    outb(ATA_DRIVE, 0xE0);
    outb(ATA_SECTOR_COUNT, 0);
    outb(ATA_LBA_LOW, 0);
    outb(ATA_LBA_MID, 0);
    outb(ATA_LBA_HIGH, 0);
    outb(ATA_COMMAND, ATA_CMD_IDENTIFY);
    unsigned char status = inb(ATA_STATUS);
    if (status == 0) return;
    if (!wait_status(ATA_DRQ)) return;
    for (unsigned int index = 0; index < 256; ++index)
        identify[index] = inw(ATA_DATA);
    drive_present = identify[0] != 0;
}

static void select_lba(unsigned long lba)
{
    outb(ATA_DRIVE, 0xE0 | ((lba >> 24) & 0x0F));
    outb(ATA_SECTOR_COUNT, 1);
    outb(ATA_LBA_LOW, lba & 0xFF);
    outb(ATA_LBA_MID, (lba >> 8) & 0xFF);
    outb(ATA_LBA_HIGH, (lba >> 16) & 0xFF);
}

int ata_read_sector(unsigned long lba, void *buffer)
{
    if (!drive_present || buffer == (void *)0 || lba >= 0x10000000) return 0;
    select_lba(lba);
    outb(ATA_COMMAND, ATA_CMD_READ);
    if (!wait_status(ATA_DRQ)) return 0;
    unsigned short *words = (unsigned short *)buffer;
    for (unsigned int index = 0; index < 256; ++index) words[index] = inw(ATA_DATA);
    return 1;
}

int ata_write_sector(unsigned long lba, const void *buffer)
{
    if (!drive_present || buffer == (const void *)0 || lba >= 0x10000000) return 0;
    select_lba(lba);
    outb(ATA_COMMAND, ATA_CMD_WRITE);
    if (!wait_status(ATA_DRQ)) return 0;
    const unsigned short *words = (const unsigned short *)buffer;
    for (unsigned int index = 0; index < 256; ++index) outw(ATA_DATA, words[index]);
    return wait_status(0);
}

int ata_self_test(void)
{
    unsigned short written[256];
    unsigned short read_back[256];
    for (unsigned int index = 0; index < 256; ++index)
        written[index] = (unsigned short)(0xA500 | (index & 0xFF));
    if (!ata_write_sector(0, written) || !ata_read_sector(0, read_back)) return 0;
    for (unsigned int index = 0; index < 256; ++index)
        if (written[index] != read_back[index]) return 0;
    return 1;
}