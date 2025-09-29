#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include "vkp.h"

static const char *chhexvalues = "0123456789ABCDEFabcdef";
static const char *chspace = " \t";

int vkp_add_line(vkp_patch_t *v, uint32_t addr, uint8_t d0, uint8_t d1)
{
    // expand if needed
    if (v->patch.count >= v->patch.capacity)
    {
        size_t newcap = v->patch.capacity ? v->patch.capacity * 2 : 64;
        vkp_line_t *newlines = realloc(v->patch.lines, newcap * sizeof(vkp_line_t));
        if (!newlines)
            return 0; // allocation failed
        v->patch.lines = newlines;
        v->patch.capacity = newcap;
    }

    v->patch.lines[v->patch.count].addr = addr;
    v->patch.lines[v->patch.count].data[0] = d0;
    v->patch.lines[v->patch.count].data[1] = d1;
    v->patch.count++;
    return 1;
}

static int isthatchar(const char *addr, size_t size, uint32_t position,
                      uint32_t *elementsize, const char *ch)
{
    if (position >= size)
        return 0;
    if (!strchr(ch, addr[position]))
        return 0;
    *elementsize = 1;
    return 1;
}

static int space(const char *addr, size_t size, uint32_t position, uint32_t *elementsize)
{
    return isthatchar(addr, size, position, elementsize, chspace);
}

static int eol(const char *addr, size_t size, uint32_t position, uint32_t *elementsize)
{
    *elementsize = 0;
    int ret = 0;
    if (position >= size)
        return 1;
    if (addr[*elementsize + position] == 13)
    {
        (*elementsize)++;
        ret = 1;
    }
    if (position + *elementsize >= size)
        return ret;
    if (addr[*elementsize + position] == 10)
    {
        (*elementsize)++;
        ret = 1;
    }
    return ret;
}

static int eos(const char *addr, size_t size, uint32_t position, uint32_t *elementsize)
{
    uint32_t t;
    *elementsize = 0;
    while (space(addr, size, *elementsize + position, &t))
        *elementsize += t;
    if (*elementsize + position >= size)
        return 1;
    if (addr[*elementsize + position] == ';')
    {
        while (!eol(addr, size, *elementsize + position, &t))
            (*elementsize)++;
    }
    if (eol(addr, size, *elementsize + position, &t))
    {
        *elementsize += t;
        return 1;
    }
    return 0;
}

static int anyline(const char *addr, size_t size, uint32_t position, uint32_t *elementsize)
{
    uint32_t t;
    *elementsize = 0;
    while (!eol(addr, size, *elementsize + position, &t))
        (*elementsize)++;
    return 1;
}

static int deltaoffset(vkp_patch_t *v, const char *addr, uint32_t size,
                       uint32_t position, uint32_t *elementsize)
{
    if (!isthatchar(addr, size, position, elementsize, "+-"))
        return 0;

    uint32_t t;
    int i;
    char tmp[10];
    tmp[0] = addr[position];
    for (i = 0; i < 8; i++)
    {
        if (!isthatchar(addr, size, *elementsize + position, &t, chhexvalues))
            break;
        tmp[i + 1] = addr[*elementsize + position];
        *elementsize += t;
    }
    if (i == 0)
        return 0;
    tmp[i + 1] = 0;
    if (!eos(addr, size, *elementsize + position, &t))
        return 0;
    *elementsize += t;
    sscanf(tmp, "%x", &t);
    v->delta = (uint32_t)t;
    return 1;
}

static int patchstring(vkp_patch_t *v, const char *addr, size_t size,
                       uint32_t position, uint32_t *elementsize)
{
    vkp_line_t tmp[256]; // local storage for one line

    *elementsize = 0;
    int i;
    uint32_t t;
    char tmpaddr[9];
    uint32_t addrvalue;
    char tmpval[3];
    tmpval[2] = 0;

    for (i = 0; i < 8; i++)
    {
        if (!isthatchar(addr, size, *elementsize + position, &t, chhexvalues))
            break;
        tmpaddr[i] = addr[*elementsize + position];
        *elementsize += t;
    }
    if (i == 0)
        return 0;
    tmpaddr[i] = 0;
    sscanf(tmpaddr, "%x", &addrvalue);

    if (!isthatchar(addr, size, *elementsize + position, &t, ":"))
        return 0;
    *elementsize += t;
    if (!isthatchar(addr, size, *elementsize + position, &t, " "))
        return 0;
    *elementsize += t;

    int hcount = 0;
    while (isthatchar(addr, size, *elementsize + position, &t, chhexvalues))
    {
        tmpval[0] = addr[*elementsize + position];
        *elementsize += t;
        if (!isthatchar(addr, size, *elementsize + position, &t, chhexvalues))
            return 0;
        tmpval[1] = addr[*elementsize + position];
        *elementsize += t;

        sscanf(tmpval, "%x", &t);
        tmp[hcount].addr = addrvalue + hcount + v->delta;
        tmp[hcount].data[0] = (uint8_t)t;
        hcount++;
    }

    if (!isthatchar(addr, size, *elementsize + position, &t, " "))
        return 0;
    *elementsize += t;

    for (int j = 0; j < hcount; j++)
    {
        if (!isthatchar(addr, size, *elementsize + position, &t, chhexvalues))
            return 0;
        tmpval[0] = addr[*elementsize + position];
        *elementsize += t;
        if (!isthatchar(addr, size, *elementsize + position, &t, chhexvalues))
            return 0;
        tmpval[1] = addr[*elementsize + position];
        *elementsize += t;

        sscanf(tmpval, "%x", &t);
        tmp[j].data[1] = (uint8_t)t;
    }

    if (!eos(addr, size, *elementsize + position, &t))
        return 0;
    *elementsize += t;

    for (size_t j = 0; j < (size_t)hcount; j++)
    {
        // check for duplicates
        int dup = 0;
        for (size_t k = 0; k < v->patch.count; k++)
        {
            if (v->patch.lines[k].addr == tmp[j].addr)
            {
                dup = 1;
                break;
            }
        }
        if (dup)
            return 0;
        vkp_add_line(v, tmp[j].addr, tmp[j].data[0], tmp[j].data[1]);
    }

    return 1;
}

int vkp_dovkp(vkp_patch_t *v, const char *addr, uint32_t size)
{
    v->patch.count = 0;
    v->errorline = 0;
    v->errorstring[0] = '\0';
    v->delta = 0;

    uint32_t position;
    int linenum;
    uint32_t elementsize;

    for (position = 0, linenum = 1; position < size; position += elementsize, linenum++)
    {
        if (eos(addr, size, position, &elementsize) ||
            deltaoffset(v, addr, size, position, &elementsize) ||
            patchstring(v, addr, size, position, &elementsize))
            continue;

        anyline(addr, size, position, &elementsize);
        snprintf(v->errorstring, sizeof(v->errorstring), "%.*s",
                 (int)elementsize, addr + position);
        v->errorline = linenum;
        return linenum;
    }
    return 0;
}

void vkp_set_init(vkp_set_t *set)
{
    set->lines = NULL;
    set->count = 0;
    set->capacity = 0;
}

void vkp_set_free(vkp_set_t *set)
{
    if (set->lines)
    {
        free(set->lines);
        set->lines = NULL;
    }
    set->count = 0;
    set->capacity = 0;
}

void vkp_patch_init(vkp_patch_t *patch)
{
    vkp_set_init(&patch->patch);
    patch->errorline = 0;
    patch->errorstring[0] = '\0';
    patch->delta = 0;
}

void vkp_patch_free(vkp_patch_t *patch)
{
    vkp_set_free(&patch->patch);
}

// Load .vkp file and parse into patch
int vkp_load_file(const char *filename, vkp_patch_t *patch)
{
    FILE *fp = fopen(filename, "rb");
    if (!fp)
    {
        fprintf(stderr, "[VKP] file (%s) does not exist!\n", filename);
        return -1;
    }

    fseek(fp, 0, SEEK_END);
    size_t size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    char *buf = malloc(size + 1);
    if (!buf)
    {
        fclose(fp);
        fprintf(stderr, "[VKP] malloc failed\n");
        return -1;
    }

    if (fread(buf, 1, size, fp) != (size_t)size)
    {
        perror("fread");
        free(buf);
        fclose(fp);
        return -1;
    }
    fclose(fp);
    buf[size] = '\0'; // safety

    // Parse
    int err = vkp_dovkp(patch, buf, size);
    free(buf);

    if (err != 0)
    {
        fprintf(stderr, "[VKP] parse error at line %d: %s\n",
                patch->errorline, patch->errorstring);
        return -1;
    }

    return 0;
}

// return number of unique blocks
size_t vkp_collect_unique_blocks(const vkp_patch_t *patch,
                                 size_t flashblocksize,
                                 uint32_t *blocks, size_t maxblocks)
{
    size_t count = 0;
    for (size_t i = 0; i < patch->patch.count; i++)
    {
        uint32_t block = patch->patch.lines[i].addr & ~(flashblocksize - 1);

        int seen = 0;
        for (size_t j = 0; j < count; j++)
        {
            if (blocks[j] == block)
            {
                seen = 1;
                break;
            }
        }

        if (!seen && count < maxblocks)
        {
            blocks[count++] = block;
        }
    }
    return count;
}