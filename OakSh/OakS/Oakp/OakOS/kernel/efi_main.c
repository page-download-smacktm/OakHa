typedef unsigned short CHAR16;
typedef unsigned long EFI_STATUS;
typedef void *EFI_HANDLE;

typedef struct {
    EFI_STATUS ( *OutputString )(void *This, CHAR16 *String);
} EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL;

typedef struct {
    unsigned long long Signature;
    unsigned int Revision;
    unsigned int HeaderSize;
    unsigned int CRC32;
    unsigned int Reserved;
    void *Vendor;
    void *ConsoleInHandle;
    void *ConIn;
    void *ConsoleOutHandle;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *ConOut;
    void *StandardErrorHandle;
    void *StdErr;
    void *RuntimeServices;
    void *BootServices;
    unsigned int NumberOfTableEntries;
    void *ConfigurationTable;
} EFI_SYSTEM_TABLE;

#define EFI_SUCCESS 0UL

static void efi_print(EFI_SYSTEM_TABLE *system_table, const CHAR16 *text)
{
    if (system_table == (EFI_SYSTEM_TABLE *)0 ||
        system_table->ConOut == (EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *)0)
        return;
    system_table->ConOut->OutputString(system_table->ConOut, (CHAR16 *)text);
}

void efi_main(EFI_HANDLE image_handle, EFI_SYSTEM_TABLE *system_table)
{
    CHAR16 message[20];
    unsigned int index = 0;
    const CHAR16 text[] = {'O', 'a', 'k', ' ', 'O', 'S', ' ', 'U', 'E', 'F',
        'I', ' ', 's', 't', 'u', 'b', '\r', '\n', '\0'};
    (void)image_handle;
    for (index = 0; text[index] != '\0'; ++index)
        message[index] = text[index];
    message[index] = '\0';
    if (system_table != (EFI_SYSTEM_TABLE *)0)
        efi_print(system_table, message);
    for (;;) {
        __asm__ volatile ("hlt");
    }
    return;
}
