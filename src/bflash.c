#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <libserialport.h>

#include "babe.h"
#include "common.h"
#include "connection.h"
#include "cmd.h"
#include "csloader.h"
#include "flash.h"
#include "loader.h"
#include "gdfs.h"
#include "payload.h"
#include "serial.h"
#include "break.h"

int bflash_repair_boot(struct sp_port *port, struct phone_info *phone)
{
    if (phone->chip_id == DB2000)
    {
        connection_close(port);
        printf("\n");
        struct timespec ts = {0, 2000000}; // 20 ms sleep
        nanosleep(&ts, NULL);

        /* reopen & handshake */
        if (connection_open(port, phone) != 0)
        {
            fprintf(stderr, "reconnect failed\n");
            return -1;
        }

        if (loader_send_qhtry_setool(port, phone, DB2000_CRIPPLED_PRELOADER_SETOOL2_R2B) != 0)
            return -1;
        if (loader_send_binary(port, phone, DB2000_FLLOADER_R2B_DEN_PO) != 0)
            return -1;
    }

    if (flash_restore_boot_area(port, phone) != 0)
        return -1;

    printf("\n");

    return 0;
}

int loader_send_bflash_ldr_db2000(struct sp_port *port, struct phone_info *phone)
{
    // TODO CID16
    if (phone->erom_cid == 29)
    {
        if (loader_send_qhldr(port, phone, DB2000_CERTLOADER_RED_CID00_R3L) != 0)
            return -1;
        if (loader_send_binary(port, phone, DB2000_FLLOADER_RED_CID29_R3S) != 0)
            return -1;
        return 0;
    }
    else if (phone->erom_cid == 36)
    {
        if (loader_send_qhldr(port, phone, DB2000_PILOADER_RED_CID00_R1F) != 0)
            return -1;
        if (break_cid36(port, phone) != 0)
            return -1;
        if (loader_send_binary(port, phone, DB2000_FLLOADER_R2B_DEN_PO) != 0)
            return -1;
        return 0;
    }
    else if (phone->erom_cid == 49 && phone->erom_color == BROWN)
    {
        if (loader_send_qhldr(port, phone, DB2000_PILOADER_RED_CID00_R1F) != 0)
            return -1;
        if (break_cid36(port, phone) != 0)
            return -1;
        if (loader_send_binary(port, phone, DB2000_FLLOADER_R2B_DEN_PO) != 0)
            return -1;
        return 0;
    }
    else if (phone->erom_cid == 49 && phone->erom_color == RED)
    {
        if (loader_send_qhldr(port, phone, DB2000_PILOADER_RED_CID03_P3B) != 0)
            return -1;

        struct gdfs_data_t gdfs = {0};
        if (break_build_bootname(port, phone, &gdfs) != 0)
            return -1;

        if (loader_shutdown(port) != 0)
            return -1;

        connection_close(port);
        printf("\n");
        struct timespec ts = {0, 2000000}; // 20 ms sleep
        nanosleep(&ts, NULL);

        /* reopen & handshake */
        if (connection_open(port, phone) != 0)
        {
            fprintf(stderr, "reconnect failed\n");
            return -1;
        }

        if (loader_send_qhldr(port, phone, DB2000_PILOADER_RED_CID00_R2B) != 0)
            return -1;
        if (loader_send_binary(port, phone, DB2000_FLLOADER_RED_CID49_R2B) != 0)
            return -1;
        if (break_cid49(port, phone) != 0)
            return -1;

        nanosleep(&ts, NULL);

        if (loader_send_qhtry_jdflasher(port, phone, DB2000_HEADER_R2B_DEN_PO) != 0)
            return -1;
        if (loader_send_binary(port, phone, DB2000_FLLOADER_R2B_DEN_PO) != 0)
            return -1;

        printf("Security disabled =)\n");

        if (flash_restore_boot_area(port, phone) != 0)
            return -1;

        printf("\n");

        return 0;
    }

    fprintf(stderr, "[BFLASH] This cid & cert is not supported\nconvert to brown first or using '--break-rsa'(cid49 only) or '--anycid' exploit\n");
    return -1;
}

int loader_send_bflash_ldr_db2010(struct sp_port *port, struct phone_info *phone)
{
    // TODO CID16
    if (phone->erom_cid <= 29)
    {
        if (loader_send_qhldr(port, phone, DB2010_CERTLOADER_RED_CID01_R2E) != 0)
            return -1;
        if (break_cid29(port, phone) != 0)
            return -1;
        if (loader_send_binary(port, phone, DB2010_FLLOADER_P5G_DEN_PO) != 0)
            return -1;
        return 0;
    }
    else if (phone->erom_cid == 36)
    {
        if (loader_send_qhldr(port, phone, DB2010_PILOADER_RED_CID00_R2F) != 0)
            return -1;
        if (break_cid36(port, phone) != 0)
            return -1;
        if (loader_send_binary(port, phone, DB2010_FLLOADER_P5G_DEN_PO) != 0)
            return -1;
        return 0;
    }
    else if (phone->erom_cid == 49 && phone->erom_color == BROWN)
    {
        if (loader_send_qhldr(port, phone, DB2010_PILOADER_BROWN_CID49_R1A002) != 0)
            return -1;
        if (loader_send_binary(port, phone, DB2010_FLLOADER_R2B_DEN_PO) != 0)
            return -1;
        return 0;
    }
    else if (phone->erom_cid == 51 && phone->erom_color == BROWN)
    {
        if (loader_send_qhldr(port, phone, DB2010_PILOADER_RED_CID00_P4D) != 0)
            return -1;

        if (loader_send_binary(port, phone, DB2010_RESPIN_PRODLOADER_SETOOL2) != 0)
            return -1;
        return 0;
    }
    else if (phone->erom_color == RED && phone->erom_cid == 49 && phone->break_rsa == 1)
    {
        if (loader_send_qhldr(port, phone, DB2010_PILOADER_RED_CID00_P3L) != 0)
            return -1;

        struct gdfs_data_t gdfs = {0};
        if (break_build_bootname(port, phone, &gdfs) != 0)
            return -1;

        if (loader_send_binary(port, phone, DB2010_FLLOADER_RED_CID49_R2A007) != 0)
            return -1;

        if (break_cid49(port, phone) != 0)
            return -1;

        if (loader_send_qhtry_jdflasher(port, phone, DB2010_HEADER_P3L_DEN_PO) != 0)
            return -1;

        if (loader_send_binary(port, phone, DB2010_FLLOADER_P5G_DEN_PO) != 0)
            return -1;

        printf("Security disabled =)\n");

        if (bflash_repair_boot(port, phone) != 0)
            return -1;

        return 0;
    }
    else if (phone->erom_color == RED && phone->anycid == 1) // ANYCID targets RED CID49+
    {
        if (loader_send_qhtry_setool(port, phone, DB2010_RESPIN_ID_LOADER_SETOOL2) != 0)
        {
            printf("[QHTRY] Run executer first\n");
            return -1;
        }
        if (loader_send_binary(port, phone, DB2010_RESPIN_PRODLOADER_SETOOL2) != 0)
            return -1;

        printf("Security disabled =)\n");

        if (bflash_repair_boot(port, phone) != 0)
            return -1;

        return 0;
    }
    fprintf(stderr, "[BFLASH] This cid & cert is not supported\nconvert to brown first or using '--break-rsa'(cid49 only) or '--anycid' exploit\n");
    return -1;
}

int loader_send_bflash_ldr_db2020(struct sp_port *port, struct phone_info *phone)
{
    if (phone->anycid == 1)
    {
        if (loader_send_qhtry_setool(port, phone, DB2020_PRELOADER_FOR_SETOOL2) != 0)
        {
            printf("[QHTRY] Run executer first\n");
            return -1;
        }
        if (loader_send_binary(port, phone, DB2020_LOADER_FOR_SETOOL2) != 0)
            return -1;

        printf("Security disabled =)\n");

        if (bflash_repair_boot(port, phone) != 0)
            return -1;

        return 0;
    }

    if (loader_send_qhldr(port, phone, DB2020_PILOADER_RED_CID01_P3M) != 0)
        return -1;
    if (phone->erom_color == BROWN)
    {
        if (loader_send_binary(port, phone, DB2020_PILOADER_BROWN_CID49_SETOOL) != 0)
            return -1;
        if (loader_send_binary(port, phone, DB2020_FLLOADER_R2A005_DEN_PO) != 0)
            return -1;
        return 0;
    }

    fprintf(stderr, "[BFLASH] This cid & cert is not supported, convert to brown first or break using '--anycid' exploit\n");
    return -1;
}

int loader_send_bflash_ldr_pnx5230(struct sp_port *port, struct phone_info *phone)
{
    struct gdfs_data_t gdfs = {0};
    pnx_get_rest_name(port, phone, &gdfs);

    phone->anycid = 1;

    if (loader_send_qhtry_setool(port, phone, PNX5230_PRELOADER_FOR_SETOOL2) != 0)
    {
        printf("[QHTRY] Run executer first\n");
        return -1;
    }
    if (loader_send_binary(port, phone, PNX5230_PRODUCTION_LOADER_FOR_SETOOL2) != 0)
        return -1;

    printf("Security disabled =)\n");

    if (bflash_repair_boot(port, phone) != 0)
        return -1;

    return 0;
}

int loader_send_bflash_ldr(struct sp_port *port, struct phone_info *phone)
{
    switch (phone->chip_id)
    {
    case DB2000:
        return loader_send_bflash_ldr_db2000(port, phone);
    case DB2010_1:
    case DB2010_2:
        return loader_send_bflash_ldr_db2010(port, phone);
    case DB2020:
        return loader_send_bflash_ldr_db2020(port, phone);
    case PNX5230:
        return loader_send_bflash_ldr_pnx5230(port, phone);
    }

    fprintf(stderr, "[send_bflash_ldr] This cid & cert is not supported, convert to brown first\n");
    return -1;
}

int loader_send_bfs_ldr_db2000(struct sp_port *port, struct phone_info *phone)
{
    // TODO CID16&29
    if (phone->erom_cid == 36)
    {
        if (loader_send_qhldr(port, phone, DB2000_PILOADER_RED_CID00_R1F) != 0)
            return -1;
        if (break_cid36(port, phone) != 0)
            return -1;
        if (loader_send_binary(port, phone, DB2000_CSLOADER_R4B_SETOOL) != 0)
            return -1;
        return 0;
    }
    else if (phone->erom_cid == 49 && phone->erom_color == BROWN)
    {
        if (loader_send_qhldr(port, phone, DB2000_PILOADER_RED_CID00_R1F) != 0)
            return -1;
        if (break_cid36(port, phone) != 0)
            return -1;
        if (loader_send_binary(port, phone, DB2000_CSLOADER_R4B_SETOOL) != 0)
            return -1;
        return 0;
    }
    else if (phone->erom_cid == 49 && phone->erom_color == RED)
    {
        if (loader_send_qhldr(port, phone, DB2000_PILOADER_RED_CID03_P3B) != 0)
            return -1;

        struct gdfs_data_t gdfs = {0};
        if (break_build_bootname(port, phone, &gdfs) != 0)
            return -1;

        if (loader_shutdown(port) != 0)
            return -1;

        connection_close(port);
        printf("\n");
        struct timespec ts = {0, 2000000}; // 20 ms sleep
        nanosleep(&ts, NULL);

        /* reopen & handshake */
        if (connection_open(port, phone) != 0)
        {
            fprintf(stderr, "reconnect failed\n");
            return -1;
        }

        if (loader_send_qhldr(port, phone, DB2000_PILOADER_RED_CID00_R2B) != 0)
            return -1;
        if (loader_send_binary(port, phone, DB2000_FLLOADER_RED_CID49_R2B) != 0)
            return -1;
        if (break_cid49(port, phone) != 0)
            return -1;

        nanosleep(&ts, NULL);

        if (loader_send_qhtry_setool(port, phone, DB2000_CRIPPLED_PRELOADER_SETOOL2_R2B) != 0)
            return -1;

        if (loader_send_binary(port, phone, DB2000_SEMC_PRODUCTION_R2D) != 0)
            return -1;

        printf("Security disabled =)\n");

        if (loader_send_binary(port, phone, DB2000_FILE_SYSTEM_LOADER_SETOOL2_V24) != 0)
            return -1;

        return 0;
    }

    fprintf(stderr, "[BFS] This cid & cert is not supported\nconvert to brown first or using '--break-rsa'(cid49 only)\n");
    return -1;
}

int loader_send_bfs_ldr_db2010(struct sp_port *port, struct phone_info *phone)
{
    if (phone->erom_cid <= 29)
    {
        if (loader_send_qhldr(port, phone, DB2010_CERTLOADER_RED_CID01_R2E) != 0)
            return -1;
        if (break_cid29(port, phone) != 0)
            return -1;
        if (loader_send_binary(port, phone, DB2010_CSLOADER_R2C_DEN_PO) != 0)
            return -1;
        return 0;
    }
    else if (phone->erom_cid == 36) // Both RED and BROWN
    {
        if (loader_send_qhldr(port, phone, DB2010_PILOADER_RED_CID00_R2F) != 0)
            return -1;
        if (break_cid36(port, phone) != 0)
            return -1;
        if (phone->chip_id == DB2010_1)
            return loader_send_binary(port, phone, DB2010_CSLOADER_R2C_DEN_PO);
        return loader_send_binary(port, phone, DB2010_CSLOADER_HAK_CID00_V23);
    }

    if (phone->erom_color == BROWN)
    {
        switch (phone->erom_cid)
        {
        case 49:
            if (loader_send_qhldr(port, phone, DB2010_PILOADER_RED_CID00_R2AB) != 0)
                return -1;
            if (strstr(phone->phone_name, "K750") ||
                strstr(phone->phone_name, "W700") ||
                strstr(phone->phone_name, "W800"))
            {
                return loader_send_binary(port, phone, DB2010_FS_LOADER_SETOOL2_V23);
            }
            break;
        case 51:
            if (loader_send_qhldr(port, phone, DB2010_PILOADER_RED_CID00_P4D) != 0)
                return -1;
            if (loader_send_binary(port, phone, DB2010_RESPIN_PRODLOADER_SETOOL2) != 0)
                return -1;
            break;
        default:
            fprintf(stderr, "[OFS DB2010 BROWN] Unknown CID! %d\n", phone->erom_cid);
            return -1;
        }
        // K310/K320/K510/W200/W300/W810/Z530/Z550/Z558
        return loader_send_binary(port, phone, DB2010_FS_LOADER_SETOOL2_V26);
    }
    else if (phone->erom_color == RED)
    {
        if (loader_send_bflash_ldr_db2010(port, phone) != 0)
            return -1;
        if (loader_send_binary(port, phone, DB2010_FS_LOADER_SETOOL2_V26) != 0)
            return -1;
        return 0;
    }

    fprintf(stderr, "[BFLASH] This cid & cert is not supported, convert to brown first or break using '--anycid' exploit\n");
    return -1;
}

int loader_send_bfs_ldr_db2020(struct sp_port *port, struct phone_info *phone)
{
    if (phone->anycid == 1)
    {
        if (loader_send_bflash_ldr_db2020(port, phone) != 0)
            return -1;

        if (loader_send_binary(port, phone, DB2020_CSLOADER_R3A006_DEN_PO) != 0)
            return -1;
        return 0;
    }

    if (loader_send_qhldr(port, phone, DB2020_PILOADER_RED_CID01_P3M) != 0)
        return -1;
    if (phone->erom_color == BROWN)
    {
        if (loader_send_binary(port, phone, DB2020_PILOADER_BROWN_CID49_SETOOL) != 0)
            return -1;
        if (loader_send_binary(port, phone, DB2020_CSLOADER_R3A006_DEN_PO) != 0)
            return -1;
        return 0;
    }

    fprintf(stderr, "[BFLASH] This cid & cert is not supported, convert to brown first or break using '--anycid' exploit\n");
    return -1;
}

int loader_send_bfs_ldr_pnx5230(struct sp_port *port, struct phone_info *phone)
{
    if (loader_send_bflash_ldr_pnx5230(port, phone) != 0)
        return -1;

    if (loader_send_binary(port, phone, PNX5230_FILESYSTEMLOADER_FOR_SETOOL2) != 0)
        return -1;

    return 0;
}

int loader_send_bfs_ldr(struct sp_port *port, struct phone_info *phone)
{
    switch (phone->chip_id)
    {
    case DB2000:
        return loader_send_bfs_ldr_db2000(port, phone);
    case DB2010_1:
    case DB2010_2:
        return loader_send_bfs_ldr_db2010(port, phone);
    case DB2020:
        return loader_send_bfs_ldr_db2020(port, phone);
    case PNX5230:
        return loader_send_bfs_ldr_pnx5230(port, phone);

    default:
        fprintf(stderr, "[send_bfs_ldr] unknown CHIPID %X\n", phone->chip_id);
        return -1;
    }
}
