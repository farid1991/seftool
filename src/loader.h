#ifndef loader_h
#define loader_h

#include "gdfs.h"

// CERT LOADERS
#define DB2000_CERTLOADER_RED_CID00_R3L "./loader/db2000_cid00_cert_r3l.bin"
#define DB2010_CERTLOADER_RED_CID01_R2E "./loader/db2010_cid00_cert_r2e.bin"
#define DB2020_CERTLOADER_RED_CID01_P3G "./loader/db2020_cid01_cert_p3g.bin"

// MEM PATCHLOADERS
#define DB2020_MEMPLOADER_RED_CID49_R2A006 "./loader/db2020_cid49_mem_patcher_r2a006.bin"
#define DB2020_MEMPLOADER_RED_CID51_R2A006 "./loader/db2020_cid51_mem_patcher_r2a006.bin"
#define DB2020_MEMPLOADER_RED_CID52_R2A006 "./loader/db2020_cid52_mem_patcher_r2a006.bin"
#define DB2020_MEMPLOADER_RED_CID53_R2A012 "./loader/db2020_cid53_mem_patcher_r2a012.bin"
#define DB2020_RECOVERY_LOADER_BLUE01_P3N "./loader/db2020_cid01blue_recovery_p3n.bin"
#define DB2000_BREAK "./loader/2000_3e"
#define DB2000_BREAK_R1F "./loader/2000_3e_r1f_r3l"
#define DB2010_BREAK_R2E "./loader/2010_3e_r2f_r2e"

// CHIP SELECT LOADERS
#define DB2000_CSLOADER_RED_CID49_P4K "./loader/db2000_cid49red_cs_p4k.bin"
#define DB2000_CSLOADER_RED_CID49_P4L "./loader/db2000_cid49red_cs_p4l.bin"

#define DB2010_CSLOADER_RED_CID29_P2C "./loader/db2010_cid29red_cs_p2c.bin"
#define DB2010_CSLOADER_HAK_CID00_V23 "./loader/db2010_cid00_cs_hack_v23.bin"
#define DB2010_CSLOADER_RED_CID49_P3T "./loader/db2010_cid49red_cs_p3t.bin"
#define DB2010_CSLOADER_RED_CID49_R3A010 "./loader/db2010_cid49red_cs_r3a010.bin"
#define DB2012_CSLOADER_RED_CID50_R3B009 "./loader/db2012_cid50red_cs_r3b009.bin"
#define DB2012_CSLOADER_RED_CID51_R3B009 "./loader/db2012_cid51red_cs_r3b009.bin"
#define DB2012_CSLOADER_RED_CID52_R3B009 "./loader/db2012_cid52red_cs_r3b009.bin"
#define DB2012_CSLOADER_RED_CID53_R3B014 "./loader/db2012_cid53red_cs_r3b014.bin"

#define DB2010_CSLOADER_BRN_CID49_V26 "./loader/db2010_cid49brown_cs_v26.bin"
#define DB2010_CSLOADER_BRN_CID49_V23 "./loader/db2010_cid49brown_cs_v23.bin"

#define DB2020_CSLOADER_RED_CID49_R3A009 "./loader/db2020_cid49red_cs_r3a009.bin"
#define DB2020_CSLOADER_RED_CID51_R3A009 "./loader/db2020_cid51red_cs_r3a009.bin"
#define DB2020_CSLOADER_RED_CID52_R3A009 "./loader/db2020_cid52red_cs_r3a009.bin"

#define DB2020_CSLOADER_RED_CID49_R3A013 "./loader/db2020_cid49red_cs_r3a013.bin"
#define DB2020_CSLOADER_RED_CID51_R3A013 "./loader/db2020_cid51red_cs_r3a013.bin"
#define DB2020_CSLOADER_RED_CID52_R3A013 "./loader/db2020_cid52red_cs_r3a013.bin"
#define DB2020_CSLOADER_RED_CID53_R3A013 "./loader/db2020_cid53red_cs_r3a013.bin"

#define PNX5230_CSLOADER_RED_CID51_R3A015 "./loader/PNX5230_cid51red_cs_r3a015.bin"
#define PNX5230_CSLOADER_RED_CID52_R3A015 "./loader/PNX5230_cid52red_cs_r3a015.bin"
#define PNX5230_CSLOADER_RED_CID53_R3A016 "./loader/pnx5230_cid53red_cs_r3a016.bin"

// FLASH LOADERS
#define DB2000_FLLOADER_RED_CID36_R3U "./loader/db2000_cid36red_flash_r3u.bin"
#define DB2000_FLLOADER_RED_CID37_R2B "./loader/db2000_cid37red_flash_r2b.bin"
#define DB2000_FLLOADER_RED_CID49_R2B "./loader/db2000_cid49red_flash_r2b.bin"
#define DB2000_FLLOADER_BRW_CID49_R2A "./loader/db2000_cid49brown_flash_r2a.bin"
#define DB2000_FLLOADER_R2B_DEN_PO "./loader/2000_f_r2b"

#define DB2010_FLLOADER_RED_CID36_R2AB "./loader/db2010_cid36red_flash_r2ab.bin"
#define DB2010_FLLOADER_RED_CID49_R2A003 "./loader/db2010_cid49red_flash_r2a003.bin"
#define DB2010_FLLOADER_RED_CID49_R2A007 "./loader/db2010_cid49red_flash_r2a007.bin"
#define DB2010_FLLOADER_RED_CID49_R2B "./loader/db2010_cid49red_flash_r2b.bin"

#define DB2010_FLLOADER_BRW_CID49_R5A "./loader/db2010_cid49brown_flash_r5a.bin"
#define DB2010_FLLOADER_RED_CID49_R2B_DEN_PO "./loader/db2010_cid49r_flash_r2b_den_po.bin"
#define DB2010_FLLOADER_P5G_DEN_PO "./loader/2010_f_p5g"

#define DB2012_FLLOADER_RED_CID50_R1A002 "./loader/db2012_cid50red_flash_r1a002.bin"
#define DB2012_FLLOADER_RED_CID51_R2B012 "./loader/db2012_cid51red_flash_r2b012.bin"
#define DB2012_FLLOADER_RED_CID52_R2B012 "./loader/db2012_cid52red_flash_r2b012.bin"
#define DB2012_FLLOADER_RED_CID53_R2B017 "./loader/db2012_cid53red_flash_r2b017.bin"

#define DB2020_FLLOADER_RED_CID49_R2A001 "./loader/db2020_cid49red_flash_r2a001.bin"
#define DB2020_FLLOADER_RED_CID49_R2A005 "./loader/db2020_cid49red_flash_r2a005.bin"
#define DB2020_FLLOADER_RED_CID51_R2A005 "./loader/db2020_cid51red_flash_r2a005.bin"
#define DB2020_FLLOADER_RED_CID52_R2A005 "./loader/db2020_cid52red_flash_r2a005.bin"
#define DB2020_FLLOADER_RED_CID53_R2A015 "./loader/db2020_cid53red_flash_r2a015.bin"
#define DB2020_FLLOADER_R2A005_DEN_PO "./loader/2020_f_R2A005"

#define PNX5230_FLLOADER_RED_CID51_R2A016 "./loader/PNX5230_cid51red_flash_r2a016.bin"
#define PNX5230_FLLOADER_RED_CID52_R2A019 "./loader/PNX5230_cid52red_flash_r2a019.bin"
#define PNX5230_FLLOADER_RED_CID53_R2A022 "./loader/pnx5230_cid53red_flash_r2a022.bin"

// PRODUCT_ID LOADERS
#define DB2000_PILOADER_RED_CID00_R1F "./loader/db2000_cid00_prodid_r1f.bin"
#define DB2000_PILOADER_RED_CID00_R2B "./loader/db2000_cid00_prodid_r2b.bin"
#define DB2000_PRODUCTION_R2Z "./loader/db2000_prod_r2z.bin"
#define DB2000_PILOADER_RED_CID00_R3A "./loader/db2000_cid00_prodid_r3a.bin"
#define DB2000_PILOADER_RED_CID03_P3B "./loader/db2000_cid03_prodid_p3b.bin"
#define DB2010_PILOADER_RED_CID00_R2F "./loader/db2010_cid00_prodid_r2f.bin"
#define DB2010_PILOADER_RED_CID00_R2AB "./loader/db2010_cid00_prodid_r2ab.bin"
#define DB2010_PILOADER_RED_CID00_P3L "./loader/db2010_cid00_prodid_p3l.bin"
#define DB2010_PILOADER_RED_CID00_P4D "./loader/db2010_cid00_prodid_p4d.bin"
#define DB2010_RESPIN_PRODLOADER_SETOOL2 "./loader/DB2010_RESPIN_PRODLOADER_SETOOL2.bin"
#define DB2010_PILOADER_BROWN_CID49_SETOOL "./loader/db2010_cid49brown_prodid_setool.bin"
#define DB2010_PILOADER_BROWN_CID49_R1A002 "./loader/db2010_cid49brown_prodid_r1a002.bin"
#define DB2012_PILOADER_BROWN_CID51_R1A002 "./loader/db2012_cid51brown_prodid_r1a002.bin"
#define DB2020_PILOADER_RED_CID01_P3J "./loader/db2020_cid01_prodid_p3j.bin"
#define DB2020_PILOADER_RED_CID01_P3M "./loader/db2020_cid01_prodid_p3m.bin"
#define DB2020_PILOADER_BROWN_CID49_SETOOL "./loader/db2020_cid49brown_prodloader_setool2.bin"

enum ldr_type_e
{
    LDR_CHIPSELECT,
    LDR_PRODUCT_ID,
    LDR_CERT,
    LDR_FLASH,
    LDR_MEM_PATCHER,
    LDR_UNKNOWN,
};

int loader_read_gdfs_var(struct sp_port *port, struct gdfs_data_t *gdfs, int gd_index,
                         uint8_t block, uint8_t lsb, uint8_t msb);

int loader_send_binary_cmd3e(struct sp_port *port, const char *loader_name);
int loader_send_binary(struct sp_port *port, struct phone_info *phone, const char *loader_name);
int loader_send_qhldr(struct sp_port *port, struct phone_info *phone, const char *loader_name);
int loader_activate_payload(struct sp_port *port, struct phone_info *phone);

int loader_get_erom_data(struct sp_port *port, struct phone_info *phone);
int loader_get_flash_data(struct sp_port *port, struct phone_info *phone);
int loader_get_otp_data(struct sp_port *port, struct phone_info *phone);
int loader_profilephone(struct sp_port *port, struct phone_info *phone);
int loader_activate_gdfs(struct sp_port *port);

int loader_break_db2000_cid29(struct sp_port *port, const char *loader_name);
int loader_break_db2000_cid36(struct sp_port *port, struct phone_info *phone);
int loader_break_db2010_cid36(struct sp_port *port, struct phone_info *phone);

int loader_enter_flashmode(struct sp_port *port, struct phone_info *phone);
int loader_send_csloader(struct sp_port *port, struct phone_info *phone);
int loader_send_oflash_ldr(struct sp_port *port, struct phone_info *phone);
int loader_send_bflash_ldr(struct sp_port *port, struct phone_info *phone);

int loader_shutdown(struct sp_port *port);

#endif // loader_h
