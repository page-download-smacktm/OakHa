#include "acorn/e1000.h"
#include "acorn/serial.h"

enum {
    PCI_CONFIG_ADDRESS = 0xCF8,
    PCI_CONFIG_DATA = 0xCFC,
    E1000_VENDOR = 0x8086,
    E1000_DEVICE = 0x100E,
    E1000_REG_CTRL = 0x0000,
    E1000_REG_STATUS = 0x0008,
    E1000_REG_EERD = 0x0014,
    E1000_REG_IMS = 0x00D0,
    E1000_REG_RCTL = 0x0100,
    E1000_REG_TCTL = 0x0400,
    E1000_REG_TIPG = 0x0410,
    E1000_REG_RDBAL = 0x2800,
    E1000_REG_RDBAH = 0x2804,
    E1000_REG_RDLEN = 0x2808,
    E1000_REG_RDH = 0x2810,
    E1000_REG_RDT = 0x2818,
    E1000_REG_TDBAL = 0x3800,
    E1000_REG_TDBAH = 0x3804,
    E1000_REG_TDLEN = 0x3808,
    E1000_REG_TDH = 0x3810,
    E1000_REG_TDT = 0x3818,
    E1000_CTRL_RST = 1,
    E1000_RCTL_EN = 1,
    E1000_RCTL_BAM = 1 << 15,
    E1000_RCTL_SECRC = 1 << 26,
    E1000_TCTL_EN = 1,
    E1000_TCTL_PSP = 1 << 1,
    E1000_TX_STATUS_DONE = 1,
    E1000_RX_STATUS_DONE = 1,
    RING_LENGTH = 8,
    BUFFER_SIZE = 2048,
};

struct rx_descriptor {
    unsigned long address;
    unsigned short length;
    unsigned short checksum;
    unsigned char status;
    unsigned char errors;
    unsigned short special;
} __attribute__((packed));

struct tx_descriptor {
    unsigned long address;
    unsigned short length;
    unsigned char checksum_offset;
    unsigned char command;
    unsigned char status;
    unsigned char checksum_start;
    unsigned short special;
} __attribute__((packed));

static volatile unsigned char *registers;
static unsigned char mac_address[6];
static struct rx_descriptor rx_ring[RING_LENGTH] __attribute__((aligned(16)));
static struct tx_descriptor tx_ring[RING_LENGTH] __attribute__((aligned(16)));
static unsigned char rx_buffers[RING_LENGTH][BUFFER_SIZE] __attribute__((aligned(16)));
static unsigned char tx_buffers[RING_LENGTH][BUFFER_SIZE] __attribute__((aligned(16)));
static unsigned int rx_index;
static unsigned int tx_index;
static int device_ready;

static inline void outl(unsigned short port, unsigned int value)
{
    __asm__ volatile ("outl %0, %1" : : "a"(value), "Nd"(port));
}

static inline unsigned int inl(unsigned short port)
{
    unsigned int value;
    __asm__ volatile ("inl %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static unsigned int pci_read(unsigned char bus, unsigned char slot,
    unsigned char function, unsigned char offset)
{
    unsigned int address = 0x80000000U |
        ((unsigned int)bus << 16) | ((unsigned int)slot << 11) |
        ((unsigned int)function << 8) | (offset & 0xFC);
    outl(PCI_CONFIG_ADDRESS, address);
    return inl(PCI_CONFIG_DATA);
}

static unsigned long find_device(void)
{
    for (unsigned int bus = 0; bus < 256; ++bus)
        for (unsigned int slot = 0; slot < 32; ++slot) {
            unsigned int identity = pci_read((unsigned char)bus,
                (unsigned char)slot, 0, 0);
            if ((identity & 0xFFFF) == E1000_VENDOR &&
                ((identity >> 16) & 0xFFFF) == E1000_DEVICE) {
                unsigned int bar = pci_read((unsigned char)bus,
                    (unsigned char)slot, 0, 0x10);
                if ((bar & 1) == 0 && (bar & 0xFFFFFFF0U) != 0)
                    return bar & 0xFFFFFFF0U;
            }
        }
    return 0;
}

static unsigned int read_register(unsigned int offset)
{
    return *(volatile unsigned int *)(registers + offset);
}

static void write_register(unsigned int offset, unsigned int value)
{
    *(volatile unsigned int *)(registers + offset) = value;
}

void e1000_init(void)
{
    unsigned long base = find_device();
    if (base == 0) return;
    registers = (volatile unsigned char *)base;
    write_register(E1000_REG_IMS, 0);
    write_register(E1000_REG_CTRL, E1000_CTRL_RST);
    for (unsigned int count = 0; count < 100000; ++count)
        if ((read_register(E1000_REG_CTRL) & E1000_CTRL_RST) == 0) break;
    mac_address[0] = read_register(0x5400) & 0xFF;
    mac_address[1] = (read_register(0x5400) >> 8) & 0xFF;
    mac_address[2] = (read_register(0x5400) >> 16) & 0xFF;
    mac_address[3] = (read_register(0x5400) >> 24) & 0xFF;
    mac_address[4] = read_register(0x5404) & 0xFF;
    mac_address[5] = (read_register(0x5404) >> 8) & 0xFF;
    for (unsigned int index = 0; index < RING_LENGTH; ++index) {
        rx_ring[index].address = (unsigned long)rx_buffers[index];
        rx_ring[index].status = 0;
        tx_ring[index].address = (unsigned long)tx_buffers[index];
        tx_ring[index].status = E1000_TX_STATUS_DONE;
    }
    write_register(E1000_REG_RDBAL, (unsigned long)rx_ring);
    write_register(E1000_REG_RDBAH, 0);
    write_register(E1000_REG_RDLEN, sizeof(rx_ring));
    write_register(E1000_REG_RDH, 0);
    write_register(E1000_REG_RDT, RING_LENGTH - 1);
    write_register(E1000_REG_TDBAL, (unsigned long)tx_ring);
    write_register(E1000_REG_TDBAH, 0);
    write_register(E1000_REG_TDLEN, sizeof(tx_ring));
    write_register(E1000_REG_TDH, 0);
    write_register(E1000_REG_TDT, 0);
    write_register(E1000_REG_RCTL, E1000_RCTL_EN | E1000_RCTL_BAM | E1000_RCTL_SECRC);
    write_register(E1000_REG_TCTL, E1000_TCTL_EN | E1000_TCTL_PSP | 0x00000FF0);
    write_register(E1000_REG_TIPG, 0x0060200A);
    rx_index = 0;
    tx_index = 0;
    device_ready = 1;
}

int e1000_available(void) { return device_ready; }

void e1000_get_mac(unsigned char *mac)
{
    if (mac == (unsigned char *)0) return;
    for (unsigned int index = 0; index < 6; ++index) mac[index] = mac_address[index];
}

int e1000_send(const void *data, unsigned int length)
{
    if (!device_ready || data == (const void *)0 || length == 0 || length > BUFFER_SIZE)
        return 0;
    struct tx_descriptor *descriptor = &tx_ring[tx_index];
    if ((descriptor->status & E1000_TX_STATUS_DONE) == 0) return 0;
    const unsigned char *source = (const unsigned char *)data;
    for (unsigned int index = 0; index < length; ++index) tx_buffers[tx_index][index] = source[index];
    descriptor->length = (unsigned short)length;
    descriptor->command = 0x0B;
    descriptor->status = 0;
    unsigned int next = (tx_index + 1) % RING_LENGTH;
    write_register(E1000_REG_TDT, next);
    for (unsigned int count = 0; count < 100000; ++count)
        if ((descriptor->status & E1000_TX_STATUS_DONE) != 0) break;
    int sent = (descriptor->status & E1000_TX_STATUS_DONE) != 0;
    tx_index = next;
    return sent;
}

int e1000_receive(void *data, unsigned int capacity)
{
    if (!device_ready || data == (void *)0 || capacity == 0) return 0;
    struct rx_descriptor *descriptor = &rx_ring[rx_index];
    if ((descriptor->status & E1000_RX_STATUS_DONE) == 0) return 0;
    unsigned int length = descriptor->length;
    if (length > capacity) length = capacity;
    unsigned char *destination = (unsigned char *)data;
    for (unsigned int index = 0; index < length; ++index) destination[index] = rx_buffers[rx_index][index];
    descriptor->status = 0;
    write_register(E1000_REG_RDT, rx_index);
    rx_index = (rx_index + 1) % RING_LENGTH;
    return (int)length;
}

int e1000_self_test(void)
{
    return device_ready && (read_register(E1000_REG_STATUS) != 0);
}