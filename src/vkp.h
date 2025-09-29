#ifndef vkp_h
#define vkp_h

#include <stdint.h>
#include <stddef.h>

typedef struct
{
    uint32_t addr;
    uint8_t data[2];
} vkp_line_t;

typedef struct
{
    vkp_line_t *lines; // dynamic array of lines
    size_t count;      // how many are used
    size_t capacity;   // allocated capacity
} vkp_set_t;

typedef struct
{
    vkp_set_t patch;
    int errorline;
    char errorstring[256]; // replace std::string with fixed buffer
    uint32_t delta;
} vkp_patch_t;

void vkp_patch_init(vkp_patch_t *patch);
void vkp_patch_free(vkp_patch_t *patch);
int vkp_load_file(const char *filename, vkp_patch_t *patch);
size_t vkp_collect_unique_blocks(const vkp_patch_t *patch, size_t flashblocksize,
                                 uint32_t *blocks, size_t maxblocks);

#endif // vkp_h