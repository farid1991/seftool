#ifndef loader_h
#define loader_h

enum ldr_type_e
{
    LDR_CHIPSELECT,
    LDR_PRODUCT_ID,
    LDR_CERT,
    LDR_FLASH,
    LDR_MEM_PATCHER,
    LDR_UNKNOWN,
};

int loader_send_binary_noact(struct sp_port *port, const char *loader_name);
int loader_send_binary_cmd3e(struct sp_port *port, const char *loader_name);
int loader_send_binary(struct sp_port *port, struct phone_info *phone, const char *loader_name);
int loader_send_qhldr(struct sp_port *port, struct phone_info *phone, const char *loader_name);
int loader_send_unsigned_bin(struct sp_port *port, const char *loader_name, uint32_t ram_addr);
int loader_activate_payload(struct sp_port *port, struct phone_info *phone);
int loader_send_qhtry_jdflasher(struct sp_port *port, struct phone_info *phone, const char *loader_name);
int loader_send_qhtry_setool(struct sp_port *port, struct phone_info *phone, const char *loader_name);

int loader_get_erom_data(struct sp_port *port, struct phone_info *phone);
int loader_get_flash_data(struct sp_port *port, struct phone_info *phone);
int loader_get_otp_data(struct sp_port *port, struct phone_info *phone);
int loader_profilephone(struct sp_port *port, struct phone_info *phone);
int loader_activate_gdfs(struct sp_port *port);

int loader_enter_flashmode(struct sp_port *port, struct phone_info *phone);
int loader_send_oflash_ldr(struct sp_port *port, struct phone_info *phone);
int loader_send_ofs_ldr(struct sp_port *port, struct phone_info *phone);
int loader_send_bflash_ldr(struct sp_port *port, struct phone_info *phone);
int loader_send_bfs_ldr(struct sp_port *port, struct phone_info *phone);
int bflash_repair_boot(struct sp_port *port, struct phone_info *phone);

int loader_shutdown(struct sp_port *port);

int loader_send_packet_pnx(struct sp_port *port,
                           uint8_t block, uint8_t msb, uint8_t lsb,
                           uint8_t *resp, size_t resp_max);

#endif // loader_h
