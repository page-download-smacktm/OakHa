#include "acorn/fs.h"
#include "acorn/ata.h"

#define FS_MAX_NODES 64
#define FS_MAX_NAME 32
#define FS_MAX_DATA 1024
#define FS_SECTOR_SIZE 512
#define FS_TOTAL_SECTORS 32768
#define FS_SUPER_SECTOR 1
#define FS_BITMAP_START 2
#define FS_BITMAP_SECTORS 8
#define FS_NODE_START 10
#define FS_NODE_SECTORS 8
#define FS_DATA_START 18
#define FS_MAGIC 0x4F414B31
#define FS_VERSION 1

typedef struct {
    unsigned int used;
    unsigned int directory;
    unsigned int size;
    unsigned int data_sector;
    unsigned int data_blocks;
    char name[FS_MAX_NAME];
    unsigned char reserved[12];
} disk_node;

typedef struct {
    unsigned int magic;
    unsigned int version;
    unsigned int total_sectors;
    unsigned int bitmap_start;
    unsigned int bitmap_sectors;
    unsigned int node_start;
    unsigned int node_sectors;
    unsigned int data_start;
    unsigned char reserved[32];
} disk_super;

static disk_node nodes[FS_MAX_NODES];
static unsigned char block_bitmap[FS_BITMAP_SECTORS * FS_SECTOR_SIZE];
static unsigned int mounted;

static int bitmap_get(unsigned int sector)
{
    return (block_bitmap[sector / 8] & (1u << (sector % 8))) != 0;
}

static void bitmap_set(unsigned int sector, int used)
{
    unsigned char mask = (unsigned char)(1u << (sector % 8));
    if (used) block_bitmap[sector / 8] |= mask;
    else block_bitmap[sector / 8] &= (unsigned char)~mask;
}

static void reserve_metadata(void)
{
    for (unsigned int sector = 0; sector < FS_DATA_START; ++sector)
        bitmap_set(sector, 1);
}

static int sync_bitmap(void)
{
    unsigned short sector[256];
    for (unsigned int index = 0; index < FS_BITMAP_SECTORS; ++index) {
        unsigned char *source = block_bitmap + index * FS_SECTOR_SIZE;
        unsigned char *destination = (unsigned char *)sector;
        for (unsigned int byte = 0; byte < FS_SECTOR_SIZE; ++byte)
            destination[byte] = source[byte];
        if (!ata_write_sector(FS_BITMAP_START + index, sector)) return 0;
    }
    return 1;
}

static int same_path(const char *left, const char *right)
{
    unsigned int index = 0;
    while (left[index] != '\0' && right[index] != '\0') {
        if (left[index] != right[index]) return 0;
        ++index;
    }
    return left[index] == right[index];
}

static int find_node(const char *path)
{
    for (int index = 0; index < FS_MAX_NODES; ++index)
        if (nodes[index].used && same_path(nodes[index].name, path)) return index;
    return -1;
}

static int valid_path(const char *path)
{
    if (path == (const char *)0 || path[0] != '/') return 0;
    unsigned int length = 0;
    while (path[length] != '\0') {
        if (++length >= FS_MAX_NAME) return 0;
    }
    return 1;
}

static void copy_name(char *destination, const char *source)
{
    unsigned int index = 0;
    while (source[index] != '\0' && index + 1 < FS_MAX_NAME) {
        destination[index] = source[index];
        ++index;
    }
    destination[index] = '\0';
}

static int add_node(const char *path, int directory)
{
    if (!mounted || !valid_path(path) || find_node(path) >= 0) return -1;
    for (int index = 0; index < FS_MAX_NODES; ++index) {
        if (!nodes[index].used) {
            nodes[index].used = 1;
            nodes[index].directory = directory;
            nodes[index].size = 0;
            nodes[index].data_sector = FS_DATA_START + (unsigned int)index * 2;
            nodes[index].data_blocks = 0;
            copy_name(nodes[index].name, path);
            fs_sync();
            return 0;
        }
    }
    return -1;
}

void fs_sync(void)
{
    unsigned short sector[256] = { 0 };
    for (unsigned int index = 0; index < FS_NODE_SECTORS; ++index) {
        unsigned char *destination = (unsigned char *)sector;
        for (unsigned int node = 0; node < 8; ++node) {
            unsigned char *source = (unsigned char *)&nodes[index * 8 + node];
            for (unsigned int byte = 0; byte < sizeof(disk_node); ++byte)
                destination[node * sizeof(disk_node) + byte] = source[byte];
        }
        if (!ata_write_sector(FS_NODE_START + index, sector)) return;
    }
    sync_bitmap();
}

static void format_fs(void)
{
    for (unsigned int index = 0; index < FS_MAX_NODES; ++index) {
        nodes[index].used = 0;
        nodes[index].directory = 0;
        nodes[index].size = 0;
        nodes[index].data_sector = 0;
        nodes[index].data_blocks = 0;
        nodes[index].name[0] = '\0';
    }
    for (unsigned int index = 0; index < sizeof(block_bitmap); ++index)
        block_bitmap[index] = 0;
    reserve_metadata();
    nodes[0].used = 1;
    nodes[0].directory = 1;
    copy_name(nodes[0].name, "/");
    mounted = 1;
    fs_sync();

    unsigned short sector[256] = { 0 };
    disk_super *super = (disk_super *)sector;
    super->magic = FS_MAGIC;
    super->version = FS_VERSION;
    super->total_sectors = FS_TOTAL_SECTORS;
    super->bitmap_start = FS_BITMAP_START;
    super->bitmap_sectors = FS_BITMAP_SECTORS;
    super->node_start = FS_NODE_START;
    super->node_sectors = FS_NODE_SECTORS;
    super->data_start = FS_DATA_START;
    ata_write_sector(FS_SUPER_SECTOR, sector);
}

int fs_mount(void)
{
    unsigned short sector[256];
    if (!ata_read_sector(FS_SUPER_SECTOR, sector)) return 0;
    disk_super *super = (disk_super *)sector;
    if (super->magic != FS_MAGIC || super->version != FS_VERSION ||
        super->total_sectors != FS_TOTAL_SECTORS ||
        super->bitmap_start != FS_BITMAP_START ||
        super->bitmap_sectors != FS_BITMAP_SECTORS ||
        super->node_start != FS_NODE_START ||
        super->node_sectors != FS_NODE_SECTORS || super->data_start != FS_DATA_START)
        return 0;
    for (unsigned int index = 0; index < FS_BITMAP_SECTORS; ++index) {
        if (!ata_read_sector(FS_BITMAP_START + index, sector)) return 0;
        unsigned char *source = (unsigned char *)sector;
        for (unsigned int byte = 0; byte < FS_SECTOR_SIZE; ++byte)
            block_bitmap[index * FS_SECTOR_SIZE + byte] = source[byte];
    }
    for (unsigned int index = 0; index < FS_NODE_SECTORS; ++index) {
        if (!ata_read_sector(FS_NODE_START + index, sector)) return 0;
        unsigned char *source = (unsigned char *)sector;
        for (unsigned int node = 0; node < 8; ++node) {
            unsigned char *destination = (unsigned char *)&nodes[index * 8 + node];
            for (unsigned int byte = 0; byte < sizeof(disk_node); ++byte)
                destination[byte] = source[node * sizeof(disk_node) + byte];
        }
    }
    mounted = nodes[0].used && nodes[0].directory && same_path(nodes[0].name, "/");
    return mounted;
}

void fs_init(void)
{
    if (!fs_mount()) format_fs();
}

int fs_mkdir(const char *path) { return add_node(path, 1); }
int fs_create(const char *path) { return add_node(path, 0); }

long fs_write(const char *path, const void *data, unsigned long length)
{
    int index = find_node(path);
    if (index < 0 || nodes[index].directory || data == (const void *)0 || length > FS_MAX_DATA)
        return -1;
    unsigned int blocks = (unsigned int)((length + FS_SECTOR_SIZE - 1) / FS_SECTOR_SIZE);
    unsigned int old_blocks = nodes[index].data_blocks;
    for (unsigned int block = 0; block < old_blocks; ++block)
        bitmap_set(nodes[index].data_sector + block, 0);
    unsigned int first_sector = 0;
    for (unsigned int sector_number = FS_DATA_START;
        sector_number + blocks <= FS_TOTAL_SECTORS; ++sector_number) {
        int free = 1;
        for (unsigned int block = 0; block < blocks; ++block)
            if (bitmap_get(sector_number + block)) free = 0;
        if (free) {
            first_sector = sector_number;
            break;
        }
    }
    if (blocks != 0 && first_sector == 0) return -1;
    unsigned short sector[256];
    for (unsigned int block = 0; block < blocks; ++block) {
        unsigned char *destination = (unsigned char *)sector;
        for (unsigned int byte = 0; byte < FS_SECTOR_SIZE; ++byte) {
            unsigned long offset = block * FS_SECTOR_SIZE + byte;
            destination[byte] = offset < length ? ((const unsigned char *)data)[offset] : 0;
        }
        if (!ata_write_sector(first_sector + block, sector)) return -1;
        bitmap_set(first_sector + block, 1);
    }
    nodes[index].size = (unsigned int)length;
    nodes[index].data_sector = first_sector;
    nodes[index].data_blocks = blocks;
    fs_sync();
    return (long)length;
}

long fs_read(const char *path, void *data, unsigned long capacity)
{
    int index = find_node(path);
    if (index < 0 || nodes[index].directory || data == (void *)0) return -1;
    unsigned short sector[256];
    unsigned long length = nodes[index].size < capacity ? nodes[index].size : capacity;
    for (unsigned long byte = 0; byte < length; ++byte) {
        unsigned long block = byte / FS_SECTOR_SIZE;
        unsigned long offset = byte % FS_SECTOR_SIZE;
        if (!ata_read_sector(nodes[index].data_sector + block, sector)) return -1;
        ((unsigned char *)data)[byte] = ((unsigned char *)sector)[offset];
    }
    return (long)length;
}

int fs_exists(const char *path) { return find_node(path) >= 0; }

int fs_is_directory(const char *path)
{
    int index = find_node(path);
    return index >= 0 && nodes[index].directory;
}

long fs_list(const char *directory, char *output, unsigned long capacity)
{
    unsigned long length = 0;
    unsigned int directory_length = 0;
    if (!fs_is_directory(directory) || output == (char *)0 || capacity == 0) return -1;
    while (directory[directory_length] != '\0') ++directory_length;
    for (unsigned int index = 1; index < FS_MAX_NODES; ++index) {
        if (!nodes[index].used) continue;
        unsigned int start = directory_length;
        if (directory_length != 1) {
            int prefix_match = 1;
            for (unsigned int character = 0; character < directory_length; ++character)
                if (nodes[index].name[character] != directory[character]) prefix_match = 0;
            if (!prefix_match || nodes[index].name[directory_length] != '/') continue;
            start = directory_length + 1;
        } else if (nodes[index].name[0] != '/') continue;
        if (nodes[index].name[start] == '\0') continue;
        unsigned int end = start;
        while (nodes[index].name[end] != '\0' && nodes[index].name[end] != '/') ++end;
        if (nodes[index].name[end] != '\0' && end != directory_length + 1) continue;
        if (length != 0 && length + 1 < capacity) output[length++] = ' ';
        for (unsigned int character = start; character < end && length + 1 < capacity; ++character)
            output[length++] = nodes[index].name[character];
    }
    output[length] = '\0';
    return (long)length;
}

int fs_remove(const char *path)
{
    int index = find_node(path);
    if (index <= 0) return 0;
    for (unsigned int block = 0; block < nodes[index].data_blocks; ++block)
        bitmap_set(nodes[index].data_sector + block, 0);
    nodes[index].used = 0;
    nodes[index].name[0] = '\0';
    fs_sync();
    return 1;
}

int fs_self_test(void)
{
    static const char text[] = "Persistent Oak";
    char output[sizeof(text)] = { 0 };
    fs_init();
    if (!fs_exists("/")) return 0;
    if (fs_exists("/persistent") &&
        (fs_read("/persistent", output, sizeof(output)) != sizeof(text) - 1 ||
        !same_path(output, text))) return 0;
    if (!fs_exists("/persistent") && fs_create("/persistent") != 0) return 0;
    if (!fs_exists("/persistent-dir") && fs_mkdir("/persistent-dir") != 0) return 0;
    if (fs_write("/persistent", text, sizeof(text) - 1) != sizeof(text) - 1) return 0;
    mounted = 0;
    if (!fs_mount()) return 0;
    if (!fs_exists("/persistent-dir")) return 0;
    if (fs_read("/persistent", output, sizeof(output)) != sizeof(text) - 1) return 0;
    return same_path(output, text);
}
