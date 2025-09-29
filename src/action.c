#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <libserialport.h>

#ifdef _WIN32
#include <io.h>
#define access _access
#else
#include <unistd.h>
#endif

#include "common.h"
#include "cmd.h"
#include "csloader.h"
#include "flash.h"
#include "loader.h"
#include "gdfs.h"
#include "serial.h"
#include "action.h"
#include "vkp.h"

action_t action_from_string(const char *a)
{
    if (!a)
        return ACT_NONE;
    if (strcmp(a, "identify") == 0)
        return ACT_IDENTIFY;
    if (strcmp(a, "flash") == 0)
        return ACT_FLASH;
    if (strcmp(a, "read-flash") == 0)
        return ACT_READ_FLASH;
    if (strcmp(a, "read-gdfs") == 0)
        return ACT_READ_GDFS;
    if (strcmp(a, "write-gdfs") == 0)
        return ACT_WRITE_GDFS;
    if (strcmp(a, "write-script") == 0)
        return ACT_WRITE_SCRIPT;
    if (strcmp(a, "unlock") == 0)
        return ACT_UNLOCK;
    if (strcmp(a, "convert") == 0)
        return ACT_CONVERT;
    return ACT_NONE;
}

int unlock_usercode_db2020_pnx5230(struct sp_port *port, struct phone_info *phone)
{
    if (loader_send_csloader(port, phone) != 0)
        return -1;

    if (gdfs_unlock_usercode(port) != 0)
        return -1;

    if (gdfs_terminate_access(port) != 0)
        return -1;

    return 0;
}

int unlock_usercode_db2000_db2010(struct sp_port *port, struct phone_info *phone)
{
    if (loader_enter_flashmode(port, phone) != 0)
        return -1;

    if (loader_activate_gdfs(port) != 0)
        return -1;

    struct gdfs_data_t gdfs = {0};
    gdfs_get_userlock(port, &gdfs);
    printf("\nUser code: %s\n\n", gdfs.user_lock);

    return 0;
}

int action_unlock_usercode(struct sp_port *port, struct phone_info *phone)
{
    switch (phone->chip_id)
    {
    case DB2000:
    case DB2010_1:
    case DB2010_2:
        return unlock_usercode_db2000_db2010(port, phone);

    case DB2020:
    case PNX5230:
        return unlock_usercode_db2020_pnx5230(port, phone);

    default:
        return -1;
    }
}

// Return number of bytes received, or -1 on error
int pnx_send_packet(struct sp_port *port,
                    uint8_t block, uint8_t msb, uint8_t lsb,
                    uint8_t *resp, size_t resp_max)
{
    uint8_t cmd_buf[7] = {'I', 'C', 'G', '1', block, lsb, msb};

    // send data
    if (serial_write(port, cmd_buf, sizeof(cmd_buf)) < 0)
        return -1;

    uint8_t hdr[3];
    int rcv_len = serial_wait_packet(port, hdr, sizeof(hdr), 10 * TIMEOUT);
    if (rcv_len <= 0)
        return -1;

    if (hdr[0] != block || hdr[1] != lsb || hdr[2] != msb)
        return -1;

    uint8_t len[4];
    rcv_len = serial_wait_packet(port, len, sizeof(len), 10 * TIMEOUT);
    if (rcv_len <= 0)
        return -1;

    int datasize = get_word(len);
    if (datasize > (int)resp_max)
        return -1;

    rcv_len = serial_wait_packet(port, resp, datasize, 10 * TIMEOUT);
    if (rcv_len != datasize)
        return -1;

    return datasize;
}

int dump_sec_units_pnx(struct sp_port *port, const char *backup_name)
{
    FILE *f = fopen(backup_name, "a");
    if (!f)
    {
        fprintf(stderr, "Cannot create %s\n", backup_name);
        return -1;
    }

    uint8_t resp[0x800];
    int len;

    /* list of blocks to dump (block, msb, lsb, skip_first) */
    struct
    {
        uint8_t block;
        uint8_t msb;
        uint8_t lsb;
    } blocks[] = {
        {0x00, 0x00, 0x06}, /* GD_COPS_Dynamic1Variable */
        {0x00, 0x00, 0x0E}, /* GD_COPS_Dynamic2Variable */
        {0x00, 0x00, 0x13}, /* GD_COPS_StaticVariable */
        {0x00, 0x00, 0x18}, /* GD_COPS_ProtectedCustomerSettings */
        {0x00, 0x00, 0xAA}, /* GD_Protected_PlatformSettings */
        {0x01, 0x08, 0x51}  /* block 0x01 unit 0x0851 */
    };

    for (size_t i = 0; i < sizeof(blocks) / sizeof(blocks[0]); ++i)
    {
        len = pnx_send_packet(port, blocks[i].block, blocks[i].msb, blocks[i].lsb,
                              resp, sizeof(resp));
        if (len < 0)
        {
            fclose(f);
            return -1;
        }

        fprintf(f, "gdfswrite:%04X%02X%02X",
                blocks[i].block, blocks[i].msb, blocks[i].lsb);

        for (int j = 0; j < len; ++j)
            fprintf(f, "%02X", resp[j]);

        fprintf(f, "\n");
    }

    fclose(f);
    printf("SECURITY UNITS BACKUP CREATED. %s\n", backup_name);
    return 0;
}

int action_identify_pnx(struct sp_port *port, struct phone_info *phone, struct gdfs_data_t *gdfs)
{
    printf("Phone Info (from GDFS):\n");

    // Phone name
    uint8_t resp[0x800];
    int len = pnx_send_packet(port, 0x02, 0x0D, 0xBB, resp, sizeof(resp));
    if (len < 0)
        return -1;
    wcstombs(gdfs->phone_name, (wchar_t *)resp, len);
    printf("Model: %s\n", gdfs->phone_name);

    // Brand
    len = pnx_send_packet(port, 0x02, 0x0D, 0xE5, resp, sizeof(resp));
    if (len < 0)
        return -1;
    strncpy(gdfs->brand, (char *)resp, len);
    gdfs->brand[len] = '\0';
    printf("Brand: %s\n", gdfs->brand);

    // CXC article
    len = pnx_send_packet(port, 0x02, 0x0E, 0x15, resp, sizeof(resp));
    if (len < 0)
        return -1;
    strncpy(gdfs->cxc_article, (char *)resp, len);
    gdfs->cxc_article[len] = '\0';
    printf("MAPP CXC article: %s\n", gdfs->cxc_article);

    // CXC version
    len = pnx_send_packet(port, 0x02, 0x0E, 0x16, resp, sizeof(resp));
    if (len < 0)
        return -1;
    strncpy(gdfs->cxc_version, (char *)resp, len);
    gdfs->cxc_version[len] = '\0';
    printf("MAPP CXC version: %s\n", gdfs->cxc_version);

    // Language package
    len = pnx_send_packet(port, 0x02, 0x0D, 0xE7, resp, sizeof(resp));
    if (len < 0)
        return -1;
    strncpy(gdfs->langpack, (char *)resp, len);
    gdfs->langpack[len] = '\0';
    printf("Language package: %s\n", gdfs->langpack);

    // CDA article
    len = pnx_send_packet(port, 0x02, 0x0D, 0xE8, resp, sizeof(resp));
    if (len < 0)
        return -1;
    strncpy(gdfs->cda_article, (char *)resp, len);
    gdfs->cda_article[len] = '\0';
    printf("CDA article: %s\n", gdfs->cda_article);

    // CDA revision
    len = pnx_send_packet(port, 0x02, 0x0D, 0xE9, resp, sizeof(resp));
    if (len < 0)
        return -1;
    strncpy(gdfs->cda_revision, (char *)resp, len);
    gdfs->cda_article[len] = '\0';
    printf("CDA revision: %s\n", gdfs->cda_revision);

    // Default article
    len = pnx_send_packet(port, 0x02, 0x0D, 0xEA, resp, sizeof(resp));
    if (len < 0)
        return -1;
    strncpy(gdfs->default_article, (char *)resp, len);
    gdfs->default_article[len] = '\0';
    printf("Default article: %s\n", gdfs->default_article);

    // Default version
    len = pnx_send_packet(port, 0x02, 0x0D, 0xEB, resp, sizeof(resp));
    if (len < 0)
        return -1;
    strncpy(gdfs->default_version, (char *)resp, len);
    gdfs->default_version[len] = '\0';
    printf("Default version: %s\n", gdfs->default_version);

    // SIMLOCK (binary data)
    len = pnx_send_packet(port, 0x00, 0x00, 0x06, resp, sizeof(resp));
    if (len < 0)
        return -1;
    gdfs_parse_simlockdata(gdfs, resp);
    printf("%s\n", gdfs->locked ? "LOCKED" : "SIMLOCKS NOT DETECTED");
    printf("Provider: %s-%s\n\n", gdfs->mcc, gdfs->mnc);

    char backup_path[512];
    snprintf(backup_path, sizeof(backup_path), "./backup/secunits_%s_%s.txt", gdfs->phone_name, phone->otp_imei);
    if (access(backup_path, 0) != 0)
    {
        dump_sec_units_pnx(port, backup_path);
    }

    return 0;
}

int action_identify(struct sp_port *port, struct phone_info *phone)
{
    struct gdfs_data_t gdfs = {0};

    if (phone->chip_id == PNX5230)
        return action_identify_pnx(port, phone, &gdfs);

    if (loader_enter_flashmode(port, phone) != 0)
        return -1;

    if (loader_activate_gdfs(port) != 0)
        return -1;

    printf("\nPhone Info (from GDFS):\n");

    gdfs_get_phonename(port, phone, &gdfs);
    printf("Model: %s\n", gdfs.phone_name);

    gdfs_get_brand(port, phone, &gdfs);
    printf("Brand: %s\n", gdfs.brand);

    if (phone->chip_id != DB2000 || phone->chip_id != DB2010_1)
    {
        gdfs_get_cxc_article(port, phone, &gdfs);
        printf("MAPP CXC article: %s\n", gdfs.cxc_article);

        gdfs_get_cxc_version(port, phone, &gdfs);
        printf("MAPP CXC version: %s\n", gdfs.cxc_version);
    }

    gdfs_get_language(port, phone, &gdfs);
    printf("Language Package: %s\n", gdfs.langpack);

    gdfs_get_cda_article(port, phone, &gdfs);
    printf("CDA article: %s\n", gdfs.cda_article);

    gdfs_get_cda_revision(port, phone, &gdfs);
    printf("CDA revision: %s\n", gdfs.cda_revision);

    gdfs_get_default_article(port, phone, &gdfs);
    printf("Default article: %s\n", gdfs.default_article);

    gdfs_get_default_version(port, phone, &gdfs);
    printf("Default version: %s\n", gdfs.default_version);

    gdfs_get_simlock(port, &gdfs);
    printf("%s\n", gdfs.locked ? "LOCKED" : "SIMLOCKS NOT DETECTED");
    printf("Provider: %s-%s\n\n", gdfs.mcc, gdfs.mnc);

    if (phone->chip_id != DB2020)
    {
        gdfs_get_userlock(port, &gdfs);
        printf("User code: %s\n\n", gdfs.user_lock);
    }

    char backup_path[512];
    snprintf(backup_path, sizeof(backup_path), "./backup/secunits_%s.txt", phone->otp_imei);
    if (access(backup_path, 0) != 0)
    {
        gdfs_dump_sec_units(port, phone, backup_path);
    }

    return 0;
}

int action_flash_fw(struct sp_port *port, struct phone_info *phone, const char *main_fw, const char *fs_fw)
{
    if (loader_send_oflash_ldr(port, phone) != 0)
        return -1;

    if (flash_babe_fw(port, main_fw, 1) != 0)
        return -1;

    if (fs_fw)
    {
        if (flash_babe_fw(port, fs_fw, 1) != 0)
            return -1;
    }

    return 0;
}

int action_read_flash(struct sp_port *port, struct phone_info *phone, uint32_t addr, uint32_t size)
{
    if (loader_send_bflash_ldr(port, phone) != 0)
        return -1;

    if (phone->anycid == 1)
    {
        if (flash_restore_boot_area(port, phone) != 0)
            return -1;
    }

    if (flash_read(port, phone, addr, size) != 0)
        return -1;

    return 0;
}

int action_restore_gdfs(struct sp_port *port, struct phone_info *phone, const char *inputfname)
{
    if (loader_send_csloader(port, phone) != 0)
        return -1;

    if (csloader_write_gdfs(port, inputfname) != 0)
        return -1;

    if (gdfs_terminate_access(port) != 0)
        return -1;

    return 0;
}

int action_backup_gdfs(struct sp_port *port, struct phone_info *phone)
{
    if (loader_send_csloader(port, phone) != 0)
        return -1;

    if (csloader_read_gdfs(port, phone) != 0)
        return -1;

    if (gdfs_terminate_access(port) != 0)
        return -1;

    return 0;
}

int action_exec_scripts(struct sp_port *port, struct phone_info *phone,
                        int nfiles, const char **filenames)
{
    int rc = 0;
    int has_vkp = 0;
    int has_txt = 0;

    // First pass: detect what we got
    for (int i = 0; i < nfiles; i++)
    {
        const char *ext = strrchr(filenames[i], '.');
        if (ext && strcasecmp(ext, ".vkp") == 0)
            has_vkp = 1;
        else
            has_txt = 1;
    }

    if (has_vkp && has_txt)
    {
        fprintf(stderr, "Error: cannot mix VKP patches and GDFS scripts in one run.\n");
        return 0;
    }

    if (has_vkp)
    {
        // --- Prepare bflash loader once ---
        if (loader_send_bflash_ldr(port, phone) != 0)
            return -1;

        if (phone->anycid == 1)
        {
            if (flash_restore_boot_area(port, phone) != 0)
                return -1;
        }

        int patched_count = 0;
        int skipped_count = 0;

        // --- Loop over VKP patches ---
        for (int i = 0; i < nfiles; i++)
        {
            const char *fname = filenames[i];
            vkp_patch_t patch;
            vkp_patch_init(&patch);

            if (vkp_load_file(fname, &patch) != 0)
            {
                fprintf(stderr, "Failed to parse VKP file: %s\n", fname);
                vkp_patch_free(&patch);
                rc = -1;
                continue; // try next patch
            }

            printf("\n%s parsed successfully, %zu byte(s)\n",
                   fname, patch.patch.count);

            int vkp_rc = flash_vkp(port, fname, &patch, 0, phone->flashblocksize);
            if (vkp_rc == FLASH_VKP_SKIP)
            {
                skipped_count++;
                vkp_patch_free(&patch);
                continue; // keep processing others
            }
            if (vkp_rc != FLASH_VKP_OK)
            {
                vkp_patch_free(&patch);
                rc = -1;
                break;
            }
            patched_count++;
            vkp_patch_free(&patch);
        }

        printf("\nSummary: %d patched, %d skipped\n\n", patched_count, skipped_count);
    }
    else
    {
        // --- CSLOADER once ---
        if (loader_send_csloader(port, phone) != 0)
            return -1;

        for (int i = 0; i < nfiles; i++)
        {
            const char *fname = filenames[i];
            printf("Try execute gdfs script: %s\n", fname);

            char script_name[512];
            snprintf(script_name, sizeof(script_name),
                     "./script_%s_%s.txt", phone->phone_name, phone->otp_imei);

            if (csloader_parse_gdfs_script(port, fname, script_name) != 0)
            {
                rc = -1;
                break;
            }
        }

        if (rc == 0)
        {
            if (gdfs_terminate_access(port) != 0)
                rc = -1;
        }
    }

    return rc;
}

int action_convert(const char *cnv_mode, const char *cnv_filename, uint32_t mem_addr)
{
    char outname[256];

    if (strcmp(cnv_mode, "raw2babe") == 0)
    {
        snprintf(outname, sizeof(outname), "%s.ssw", cnv_filename);
        if (flash_cnv_raw_to_babe_file(cnv_filename, outname, mem_addr) != 0)
        {
            fprintf(stderr, "Error: failed to convert raw to babe\n");
            return -1;
        }
    }
    else if (strcmp(cnv_mode, "babe2raw") == 0)
    {
        snprintf(outname, sizeof(outname), "%s.bin", cnv_filename);
        if (flash_cnv_babe_to_raw_file(cnv_filename, outname) != 0)
        {
            fprintf(stderr, "Error: failed to convert babe to raw\n");
            return -1;
        }
    }

    return 0;
}
