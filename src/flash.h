#ifndef flash_h
#define flash_h

#include <stdint.h>

#include "vkp.h"

#define BLOCK_SIZE 0x10000
#define MAX_BLOCKS 480

#define FLASH_VKP_ERR -1
#define FLASH_VKP_OK 0
#define FLASH_VKP_SKIP 1

// Unified choice enum
typedef enum
{
    CHOICE_UNINSTALL = 0,
    CHOICE_SKIP = 1,
    CHOICE_ABORT = 2,
    CHOICE_CONTINUE = 3
} user_choice_t;

int flash_detect_fw_version(struct sp_port *port, struct phone_info *phone);

uint8_t *flash_read_raw(struct sp_port *port, uint32_t addr, size_t size);
int flash_read(struct sp_port *port, struct phone_info *phone,
               uint32_t addr, size_t size);

int flash_babe(struct sp_port *port, uint8_t *addr, size_t size, int flashfull);
int flash_babe_fw(struct sp_port *port, const char *filename, int flashfull);

int flash_raw(struct sp_port *port, const char *filename, uint32_t raw_addr);
uint8_t *flash_convert_raw_to_babe(uint8_t *raw, size_t size, uint32_t raw_addr, size_t *babe_size_out);

int flash_restore_boot_area(struct sp_port *port, struct phone_info *phone);

int flash_vkp(struct sp_port *port, const char *filename, vkp_patch_t *patch,
              int remove_flag, size_t flashblocksize);

int flash_cnv_raw_to_babe_file(const char *raw_filename, const char *babe_filename, uint32_t raw_addr);
int flash_cnv_babe_to_raw_file(const char *babe_filename, const char *raw_filename);

#endif // flash_h