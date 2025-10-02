#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <stdint.h>
#include <libserialport.h>

#include "babe.h"
#include "certz.h"
#include "common.h"
#include "connection.h"
#include "flash.h"
#include "gdfs.h"
#include "loader.h"
#include "payload.h"
#include "serial.h"
#include "sha1.h"

#define TAILBLOCKSSIZE 4
#define MAX_HASH_VALUE 0x000FFFFF

int break_build_bootname(struct sp_port *port, struct phone_info *phone, struct gdfs_data_t *gdfs)
{
    if (loader_activate_gdfs(port) != 0)
        return -1;

    printf("\n");

    gdfs_get_phonename(port, phone, gdfs);
    printf("Model: %s\n", gdfs->phone_name);

    // Copy phone_name but strip trailing 'i/a/c' if present
    char namebuf[5];
    strncpy(namebuf, gdfs->phone_name, sizeof(namebuf) - 1);
    namebuf[sizeof(namebuf) - 1] = '\0';

    size_t len = strlen(namebuf);
    if (len > 0 && !isdigit((unsigned char)namebuf[len - 1]))
        namebuf[len - 1] = '\0';

    snprintf(phone->osename, sizeof(phone->osename), "%s.ose", namebuf);

    // Map phone_name to bootname
    if (strstr(gdfs->phone_name, "W900") != 0)
    {
        strncpy(phone->bootname, "200A_r2b.boot", 14);
    }
    else if (strstr(gdfs->phone_name, "K600") != 0 ||
             strstr(gdfs->phone_name, "K608") != 0 ||
             strstr(gdfs->phone_name, "V600") != 0)
    {
        strncpy(phone->bootname, "2002_r2b.boot", 14);
    }
    else if (strstr(gdfs->phone_name, "W550") != 0 ||
             strstr(gdfs->phone_name, "W600") != 0)
    {
        strncpy(phone->bootname, "440A_p3k.boot", 14);
    }
    else if (strstr(gdfs->phone_name, "K750") != 0 ||
             strstr(gdfs->phone_name, "W800") != 0 ||
             strstr(gdfs->phone_name, "W700") != 0 ||
             strstr(gdfs->phone_name, "Z520") != 0)
    {
        strncpy(phone->bootname, "4402_p3k.boot", 14);
    }
    else
    {
        strncpy(phone->bootname, "4414_p3k.boot", 14);
    }

    // Map chip_id to hdrname
    snprintf(phone->hdrname, sizeof(phone->hdrname), "%scid49Red.hdr", get_chipset_name(phone->chip_id));

    return 0;
}

int break_cid49(struct sp_port *port, struct phone_info *phone)
{
    char bootname[128], osename[128], hdrname[128];
    snprintf(bootname, sizeof(bootname), "./break49/%s", phone->bootname);
    snprintf(osename, sizeof(osename), "./break49/%s", phone->osename);
    snprintf(hdrname, sizeof(hdrname), "./break49/%s", phone->hdrname);

    size_t bootsize, osesize, hdrsize;

    printf("\nLoading BOOT: %s\n", bootname);
    uint8_t *boot = load_file(bootname, &bootsize);
    if (!boot)
    {
        fprintf(stderr, "can't read %s\n", bootname);
        return -1;
    }

    printf("Loading OSE : %s\n", osename);
    uint8_t *ose = load_file(osename, &osesize);
    if (!ose)
    {
        fprintf(stderr, "can't read %s\n", osename);
        free(boot);
        return -1;
    }

    printf("Loading HDR : %s\n", hdrname);
    uint8_t *hdr = load_file(hdrname, &hdrsize);
    if (!hdr)
    {
        fprintf(stderr, "can't read %s\n", hdrname);
        free(boot);
        free(ose);
        return -1;
    }

    printf("\nCalculating hash...\n");

    struct babehdr_t *babe = (struct babehdr_t *)hdr;
    uint32_t numblocks = babe->payloadsize1;
    if (numblocks > 2)
        numblocks = 2; // !!!

    size_t ourbootsize = hdrsize + BLOCK_SIZE + 8 + (numblocks - 1) * (TAILBLOCKSSIZE + 8);

    uint8_t *ourboot = malloc(ourbootsize);
    if (!ourboot)
    {
        free(boot);
        free(ose);
        free(hdr);
        return -1;
    }

    size_t pos = 0;
    memcpy(ourboot + pos, hdr, hdrsize);
    pos += hdrsize;
    memcpy(ourboot + pos, boot, bootsize);
    pos += bootsize;
    ((uint32_t *)(ourboot + pos))[0] = 0; // fixedspeed is 0 for serial
    ((uint32_t *)(ourboot + pos))[1] = 0x1C2000 / phone->baudrate;
    pos += 8;
    memcpy(ourboot + pos, ose, osesize);
    pos += osesize;
    ourboot[pos] = 0;

    size_t oas = hdrsize;
    size_t newas = hdrsize + BLOCK_SIZE;
    for (uint32_t i = 1; i < numblocks; i++)
    {
        ((uint32_t *)(ourboot + newas))[0] =
            ((uint32_t *)(ourboot + oas))[0] +
            ((uint32_t *)(ourboot + oas))[1];
        ((uint32_t *)(ourboot + newas))[1] = TAILBLOCKSSIZE;
        oas = newas;
        newas += (TAILBLOCKSSIZE + 8);
    }

    // find cert
    uint32_t platform = get_platform(phone->chip_id);
    size_t foundcert;
    for (foundcert = 0; foundcert < (sizeof(certz) / sizeof(certz[0])); foundcert++)
    {
        if ((certz[foundcert].platform & platform) &&
            certz[foundcert].cid == phone->erom_cid &&
            certz[foundcert].color == phone->erom_color)
            break;
    }

    // printf("Platform: %X\n", certz[foundcert].platform);
    // printf("CID: %d\n", certz[foundcert].cid);
    // printf("COLOR: %s\n", color_get_name(certz[foundcert].color));

    if (foundcert == (sizeof(certz) / sizeof(certz[0])))
    {
        fprintf(stderr, "unknown cert\n");
        free(ourboot);
        free(boot);
        free(ose);
        free(hdr);
        return -1;
    }

    // init SHA
    SHA1_CTX sha;
    sha1_init(&sha);
    sha1_update(&sha, ourboot, 0x3C);
    sha1_update(&sha, certz[foundcert].cert, 0x1E8);
    sha1_update(&sha, ourboot + 0x3C + 0x1E8, 0x300 - (0x3C + 0x1E8));

    oas = hdrsize;
    for (uint32_t b = 0; b < numblocks; b++)
    {
        // uint32_t blockaddr = ((uint32_t *)(ourboot + oas))[0];
        uint32_t blocksize = ((uint32_t *)(ourboot + oas))[1];

        uint32_t x;
        for (x = 0; x != MAX_HASH_VALUE; x++)
        {
            ((uint32_t *)(ourboot + oas + 8 + blocksize - 4))[0] = x;

            SHA1_CTX sha2 = sha;
            uint8_t hash[20];
            sha1_update(&sha2, ourboot + oas, 8 + blocksize);
            sha1_final(&sha2, hash);

            if (hash[19] == ourboot[0x380 + b])
                break;
        }
        if (x == MAX_HASH_VALUE)
        {
            fprintf(stderr, "can't calculate hash\n");
            free(ourboot);
            free(boot);
            free(ose);
            free(hdr);
            return -1;
        }
        sha1_update(&sha, ourboot + oas, 8 + blocksize);
        oas += 8 + blocksize;
    }

    /* flash our patched bootloader */
    int rc = flash_babe(port, ourboot, ourbootsize, 0);

    free(ourboot);
    free(boot);
    free(ose);
    free(hdr);

    serial_send_ack(port);

    /* close/signal user, wait for confirmation, then reopen and handshake */
    connection_close(port);

    char line[16];
    while (1)
    {
        printf("\nREMOVE BATTERY FROM PHONE, THEN INSERT IT BACK\n");
        printf("THEN PRESS Y AND ENTER TO CONTINUE: ");
        fflush(stdout);

        if (!fgets(line, sizeof(line), stdin))
        {
            fprintf(stderr, "input error\n");
            return -1;
        }
        char *p = line;
        while (*p && isspace((unsigned char)*p))
            p++;
        if (p[0] == 'Y' || p[0] == 'y')
            break;
    }

    printf("\n");

    /* reopen & handshake */
    if (connection_open(port, phone) != 0)
    {
        fprintf(stderr, "reconnect failed\n");
        return -1;
    }

    return rc;
}

int break_cid36(struct sp_port *port, struct phone_info *phone)
{
    printf("Breaking rabbit hole...=) \n");

    if (phone->chip_id == DB2000)
    {
        if (loader_send_binary(port, phone, DB2000_CERTLOADER_RED_CID00_R3L) != 0)
            return -1;

        if (loader_send_binary_cmd3e(port, DB2000_BREAK_R1F) != 0)
            return -1;
    }
    else if (phone->chip_id == DB2010_1 || phone->chip_id == DB2010_2)
    {
        if (loader_send_binary(port, phone, DB2010_CERTLOADER_RED_CID01_R2E) != 0)
            return -1;

        if (loader_send_binary_cmd3e(port, DB2010_BREAK_R2E) != 0)
            return -1;
    }
    else
    {
        fprintf(stderr, "ChipID %X is not supported", phone->chip_id);
        return -1;
    }

    printf("Security disabled =)\n");

    return 0;
}