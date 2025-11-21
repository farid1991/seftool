#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <libserialport.h>

#include <core/common.h>
#include <core/serial.h>
#include <emp/common/cmd.h>

#include "babe.h"
#include "connection.h"
#include "csloader.h"
#include "loader.h"
#include "gdfs.h"
#include "payload.h"
#include "break.h"

int is_old_db2010(struct sp_port *port, struct phone_info *phone)
{
	struct gdfs_data_t gdfs = {0};
	gdfs_get_phonename(port, phone, &gdfs);
	if (strstr(gdfs.phone_name, "W700") || strstr(gdfs.phone_name, "W800") || strstr(gdfs.phone_name, "K750") ||
	    strstr(gdfs.phone_name, "Z520"))
		return 1;
	return 0;
}

int loader_send_oflash_ldr_pnx5230(struct sp_port *port, struct phone_info *phone)
{
	switch (phone->erom_cid) {
	case 51:
		return loader_send_qhldr(port, phone, PNX5230_FLLOADER_RED_CID51_R2A016);
	case 52:
		return loader_send_qhldr(port, phone, PNX5230_FLLOADER_RED_CID52_R2A019);
	case 53:
		return loader_send_qhldr(port, phone, PNX5230_FLLOADER_RED_CID53_R2A022);
	default:
		printf("[OFLASH PNX5230] CID%d_%s not supported\n", phone->erom_cid, color_get_name(phone->erom_color));
		return -1;
	}
}

int loader_send_oflash_ldr_db2000(struct sp_port *port, struct phone_info *phone)
{
	// TODO CID16
	// CID29 (both RED and BROWN)
	if (phone->erom_cid == 29) {
		if (loader_send_qhldr(port, phone, DB2000_PILOADER_RED_CID00_R2B) != 0)
			return -1;
		if (loader_send_binary(port, phone, DB2000_FLLOADER_RED_CID29_R3S) != 0)
			return -1;
		return 0;
	}
	// CID36 (both RED and BROWN)
	else if (phone->erom_cid == 36) {
		if (loader_send_qhldr(port, phone, DB2000_PILOADER_RED_CID00_R1F) != 0)
			return -1;
		if (break_cid36(port, phone) != 0)
			return -1;
		if (loader_send_binary(port, phone, DB2010_FLLOADER_CID36_R2AB) != 0)
			return -1;
		return 0;
	}

	if (loader_send_qhldr(port, phone, DB2000_PILOADER_RED_CID00_R2B) != 0)
		return -1;

	switch (phone->erom_cid) {
	case 37:
		return loader_send_binary(port, phone, DB2000_FLLOADER_RED_CID37_R2B);
	case 49:
		return loader_send_binary(port, phone, DB2000_FLLOADER_RED_CID49_R2B);
	default:
		printf("[OFLASH] DB2000 CID:%d not supported\n", phone->erom_cid);
		return -1;
	}
}

int loader_send_oflash_ldr_db2010(struct sp_port *port, struct phone_info *phone)
{
	// K500/K700
	if (phone->erom_cid <= 29) {
		if (loader_send_qhldr(port, phone, DB2010_CERTLOADER_RED_CID01_R2E) != 0)
			return -1;
		if (break_cid29(port, phone) != 0)
			return -1;
		if (loader_send_binary(port, phone, DB2010_FLLOADER_P5G_DEN_PO) != 0)
			return -1;
		return 0;
	} else if (phone->erom_cid == 36) // CID36 (both RED and BROWN)
	{
		if (loader_send_qhldr(port, phone, DB2010_PILOADER_RED_CID00_R2F) != 0)
			return -1;
		if (break_cid36(port, phone) != 0)
			return -1;
		if (loader_send_binary(port, phone, DB2010_FLLOADER_RED_CID36_R2AB) != 0)
			return -1;
		return 0;
	}

	if (phone->erom_color == BROWN) {
		switch (phone->erom_cid) {
		// DB2010
		case 49:
			if (loader_send_qhldr(port, phone, DB2010_PILOADER_BROWN_CID49_R1A002) != 0)
				return -1;
			return loader_send_binary(port, phone, DB2010_FLLOADER_R2B_DEN_PO);
		// DB2012
		case 51:
			if (loader_send_qhldr(port, phone, DB2012_PILOADER_BROWN_CID51_R1A002) != 0)
				return -1;
			return loader_send_binary(port, phone, DB2010_FLLOADER_P5G_DEN_PO);

		default:
			fprintf(stderr, "[DB201x BROWN] CID:%d is not supported\n", phone->erom_cid);
			return -1;
		}
	}

	// Fallback RED >= 49
	switch (phone->erom_cid) {
	// DB2010
	case 49:
		if (loader_send_qhldr(port, phone, DB2010_PILOADER_RED_CID00_P3L) != 0)
			return -1;
		return loader_send_binary(port, phone, DB2010_FLLOADER_RED_CID49_R2A007);
	// DB2012
	case 50:
		return loader_send_qhldr(port, phone, DB2012_FLLOADER_RED_CID50_R1A002);
	case 51:
		return loader_send_qhldr(port, phone, DB2012_FLLOADER_RED_CID51_R2B012);
	case 52:
		return loader_send_qhldr(port, phone, DB2012_FLLOADER_RED_CID52_R2B012);
	case 53:
		return loader_send_qhldr(port, phone, DB2012_FLLOADER_RED_CID53_R2B017);
	default:
		fprintf(stderr, "[DB201x RED] CID:%d is not supported\n", phone->erom_cid);
		return -1;
	}
}

int loader_send_oflash_ldr_db2020(struct sp_port *port, struct phone_info *phone)
{
	if (loader_send_qhldr(port, phone, DB2020_PILOADER_RED_CID01_P3M) != 0)
		return -1;

	if (phone->erom_color == BROWN) {
		if (loader_send_binary(port, phone, DB2020_PILOADER_BROWN_CID49_SETOOL) != 0)
			return -1;
		return loader_send_binary(port, phone, DB2020_FLLOADER_R2A005_DEN_PO);
	}

	switch (phone->erom_cid) {
	case 49:
		return loader_send_binary(port, phone, DB2020_FLLOADER_RED_CID49_R2A005);
	case 51:
		return loader_send_binary(port, phone, DB2020_FLLOADER_RED_CID51_R2A005);
	case 52:
		return loader_send_binary(port, phone, DB2020_FLLOADER_RED_CID52_R2A005);
	case 53:
		return loader_send_binary(port, phone, DB2020_FLLOADER_RED_CID53_R2A015);
	default:
		fprintf(stderr, "[OFLASH] DB2020 CID:%d not supported\n", phone->erom_cid);
		return -1;
	}
}

int loader_send_oflash_ldr(struct sp_port *port, struct phone_info *phone)
{
	// Correction if user put wrong args
	phone->anycid = 0;
	phone->break_rsa = 0;

	switch (phone->chip_id) {
	case DB2000:
		return loader_send_oflash_ldr_db2000(port, phone);
	case DB2010_1:
	case DB2010_2:
		return loader_send_oflash_ldr_db2010(port, phone);
	case DB2020:
		return loader_send_oflash_ldr_db2020(port, phone);
	case PNX5230:
		return loader_send_oflash_ldr_pnx5230(port, phone);
	default:
		fprintf(stderr, "[send_oflash_ldr] Unknown CHIPID %X\n", phone->chip_id);
		return -1;
	}
}

int loader_send_ofs_ldr_pnx5230(struct sp_port *port, struct phone_info *phone)
{
	if (loader_send_oflash_ldr_pnx5230(port, phone) != 0)
		return -1;

	switch (phone->erom_cid) {
	case 51:
		return loader_send_binary(port, phone, PNX5230_CSLOADER_RED_CID51_R3A015);
	case 52:
		return loader_send_binary(port, phone, PNX5230_CSLOADER_RED_CID52_R3A015);
	case 53:
		return loader_send_binary(port, phone, PNX5230_CSLOADER_RED_CID53_R3A016);
	default:
		printf("[OFS PNX5230] CID%d_%s not supported\n", phone->erom_cid, color_get_name(phone->erom_color));
		return -1;
	}
}

int loader_send_ofs_ldr_db2020(struct sp_port *port, struct phone_info *phone)
{
	if (loader_send_qhldr(port, phone, DB2020_PILOADER_RED_CID01_P3M) != 0)
		return -1;

	if (phone->erom_color == BROWN) {
		if (loader_send_binary(port, phone, DB2020_LOADER_FOR_SETOOL2) != 0)
			return -1;
		if (loader_send_binary(port, phone, DB2020_CSLOADER_R3A006_DEN_PO) != 0)
			return -1;
		return 0;
	}

	switch (phone->erom_cid) {
	case 49:
		return loader_send_binary(port, phone, DB2020_CSLOADER_RED_CID49_R3A009);
	case 51:
		return loader_send_binary(port, phone, DB2020_CSLOADER_RED_CID51_R3A009);
	case 52:
		return loader_send_binary(port, phone, DB2020_CSLOADER_RED_CID52_R3A009);
	case 53:
		return loader_send_binary(port, phone, DB2020_CSLOADER_RED_CID53_R3A013);
	default:
		fprintf(stderr, "[OFS DB2020] Unknown CID! %d\n", phone->erom_cid);
		return -1;
	}
}

int loader_send_ofs_ldr_db2010(struct sp_port *port, struct phone_info *phone)
{
	if (phone->erom_cid <= 29) {
		if (loader_send_qhldr(port, phone, DB2010_CERTLOADER_RED_CID01_R2E) != 0)
			return -1;
		if (break_cid29(port, phone) != 0)
			return -1;
		if (loader_send_binary(port, phone, DB2010_CSLOADER_R2C_DEN_PO) != 0)
			return -1;
		return 0;
	} else if (phone->erom_cid == 36) // Both RED and BROWN
	{
		if (loader_send_qhldr(port, phone, DB2010_PILOADER_RED_CID00_R2F) != 0)
			return -1;
		if (break_cid36(port, phone) != 0)
			return -1;
		if (phone->chip_id == DB2010_1)
			return loader_send_binary(port, phone, DB2010_CSLOADER_R2C_DEN_PO);
		return loader_send_binary(port, phone, DB2010_CSLOADER_HAK_CID00_V23);
	}

	if (phone->erom_color == BROWN) {
		switch (phone->erom_cid) {
		case 49:
			if (loader_send_qhldr(port, phone, DB2010_PILOADER_RED_CID00_R2AB) != 0)
				return -1;
			if (loader_activate_gdfs(port) != 0)
				return -1;
			if (is_old_db2010(port, phone))
				return loader_send_binary(port, phone, DB2010_CSLOADER_BRN_CID49_V23);
			return loader_send_binary(port, phone, DB2010_CSLOADER_BRN_CID49_V26);
		case 51:
			if (loader_send_qhldr(port, phone, DB2010_PILOADER_RED_CID00_P4D) != 0)
				return -1;
			if (loader_send_binary(port, phone, DB2010_RESPIN_PRODLOADER_SETOOL2) != 0)
				return -1;
			if (loader_send_binary(port, phone, DB2012_CSLOADER_RED_CID51_R3B009) != 0)
				return -1;
			return 0;
		default:
			fprintf(stderr, "[OFS DB2010 BROWN] Unknown CID! %d\n", phone->erom_cid);
			return -1;
		}
	} else if (phone->erom_color == RED) {
		switch (phone->erom_cid) {
		case 49:
			if (loader_send_qhldr(port, phone, DB2010_PILOADER_RED_CID00_P3L) != 0)
				return -1;
			if (loader_activate_gdfs(port) != 0)
				return -1;
			if (is_old_db2010(port, phone))
				return loader_send_binary(port, phone, DB2010_CSLOADER_RED_CID49_P3T);
			else
				return loader_send_binary(port, phone, DB2010_CSLOADER_RED_CID49_R3A010);
		case 50:
			if (loader_send_qhldr(port, phone, DB2010_PILOADER_RED_CID00_P4D) != 0)
				return -1;
			if (loader_send_binary(port, phone, DB2012_CSLOADER_RED_CID50_R3B009) != 0)
				return -1;
			return 0;
		case 51:
			if (loader_send_qhldr(port, phone, DB2010_PILOADER_RED_CID00_P4D) != 0)
				return -1;
			if (loader_send_binary(port, phone, DB2012_CSLOADER_RED_CID51_R3B009) != 0)
				return -1;
			return 0;
		case 52:
			if (loader_send_qhldr(port, phone, DB2010_PILOADER_RED_CID00_P4D) != 0)
				return -1;
			if (loader_send_binary(port, phone, DB2012_CSLOADER_RED_CID52_R3B009) != 0)
				return -1;
			return 0;
		case 53:
			if (loader_send_qhldr(port, phone, DB2010_PILOADER_RED_CID00_P4D) != 0)
				return -1;
			if (loader_send_binary(port, phone, DB2012_CSLOADER_RED_CID53_R3B014) != 0)
				return -1;
			return 0;
		default:
			fprintf(stderr, "[OFS DB2010 RED] CID %d not supported yet\n", phone->erom_cid);
			return -1;
		}
	}
	printf("CID%d_%s not supported\n", phone->erom_cid, color_get_name(phone->erom_color));
	return -1;
}

int loader_send_ofs_ldr_db2000(struct sp_port *port, struct phone_info *phone)
{
	// TODO CID16&29
	switch (phone->erom_cid) {
		// case 29:
		//     if (loader_send_qhldr(port, phone, DB2000_CERTLOADER_RED_CID00_R3L) != 0)
		//         return -1;
		//     if (break_cid29(port, phone) != 0)
		//         return -1;
		//     return loader_send_binary(port, phone, phone->is_z1010 ? DB2000_VIOLA_FILE_SYSTEM_LOADER_R1E : DB2000_SEMC_FILE_SYSTEM_LOADER_R2B);
	case 36:
		if (loader_send_qhldr(port, phone, DB2000_PILOADER_RED_CID00_R1F) != 0)
			return -1;
		if (break_cid36(port, phone) != 0)
			return -1;
		if (loader_send_binary(port, phone, DB2000_CSLOADER_R4B_SETOOL) != 0)
			return -1;
		return 0;
	case 37:
		if (loader_send_qhldr(port, phone, DB2000_PILOADER_RED_CID00_R2B) != 0)
			return -1;
		if (loader_send_binary(port, phone, DB2000_CSLOADER_RED_CID37_P4L) != 0)
			return -1;
		return 0;
	case 49:
		if (loader_send_qhldr(port, phone, DB2000_PILOADER_RED_CID00_R2B) != 0)
			return -1;
		if (loader_send_binary(port, phone, DB2000_CSLOADER_RED_CID49_P4L) != 0)
			return -1;
		return 0;

	default:
		printf("CID%d_%s not supported\n", phone->erom_cid, color_get_name(phone->erom_color));
		return -1;
	}
}

int loader_send_ofs_ldr(struct sp_port *port, struct phone_info *phone)
{
	switch (phone->chip_id) {
	case DB2000:
		return loader_send_ofs_ldr_db2000(port, phone);
	case DB2010_1:
	case DB2010_2:
		return loader_send_ofs_ldr_db2010(port, phone);
	case DB2020:
		return loader_send_ofs_ldr_db2020(port, phone);
	case PNX5230:
		return loader_send_ofs_ldr_pnx5230(port, phone);
	default:
		fprintf(stderr, "ChipID %X not supported\n", phone->chip_id);
		return -1;
	}
	return 0;
}
