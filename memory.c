/*
 *
 */

#include "emulator.h"

#include "memory.h"

uint8_t *memory_ram = NULL;
uint8_t *memory_rom = NULL;

int memory_init(void) {
    memory_ram = (uint8_t*)malloc(65536);
    memory_rom = (uint8_t*)malloc(65536);

    if((!memory_ram) || (!memory_rom))
        return E_MEM_ALLOC;

    /* default poweron at 0x8000 */
    memory_rom[0xfffc] = 0x00;
    memory_rom[0xfffd] = 0x80;

    /* now we are free to load any memory pages from disk */
    return E_MEM_SUCCESS;
}

void memory_deinit(void) {
    if(memory_ram) free(memory_ram);
    if(memory_rom) free(memory_rom);
}

uint8_t memory_read(uint16_t addr) {
    uint8_t *which_bank = memory_ram;

    /* this is hackish as crap */
    if (addr >= 0xa000 && addr <= 0xbfff) /* BASIC ROM */
        which_bank = memory_rom;
    if (addr >= 0xe000)                   /* Kernal ROM */
        which_bank = memory_rom;
    if (addr >= 0xd000 && addr <= 0xdfff) /* Character generator */
        which_bank = memory_rom;

    /* The bank chooser should be hardware, and rom files
     * should be loadable from disk
     */

    return which_bank[addr];
}

void memory_write(uint16_t addr, uint8_t value) {
    memory_ram[addr] = value;
}

int memory_load(uint16_t addr, char *file) {
    uint16_t current_addr = addr;
    ssize_t total_bytes = 0, bytes_read = 0;
    int fd;

    /* we should really mark up an address table to set this
     * as RAM as well */

    DPRINTF(DBG_DEBUG,"Loading %s at $%04x\n", file, addr);

    fd = open(file,O_RDONLY);
    if(fd == -1) {
        DPRINTF(DBG_ERROR, "Cannot open file %s: %s\n", file, strerror(errno));
        return E_MEM_FOPEN;
    }

    while(1) {
        bytes_read = read(fd, &memory_ram[current_addr], 4096);
        if(bytes_read <= 0)
            break;
        current_addr += bytes_read;
        total_bytes += bytes_read;
    }

    DPRINTF(DBG_DEBUG, "Loaded %d bytes\n", total_bytes);
    close(fd);

    return E_MEM_SUCCESS;
}
