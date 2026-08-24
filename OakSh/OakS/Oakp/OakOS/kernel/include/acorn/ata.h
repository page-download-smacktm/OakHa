#ifndef ACORN_ATA_H
#define ACORN_ATA_H

void ata_init(void);
int ata_read_sector(unsigned long lba, void *buffer);
int ata_write_sector(unsigned long lba, const void *buffer);
int ata_self_test(void);

#endif