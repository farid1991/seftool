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

int loader_send_binary_cmd3e(struct sp_port *port, const char *loader_name);
int loader_send_binary(struct sp_port *port, struct phone_info *phone, const char *loader_name);
int loader_send_qhldr(struct sp_port *port, struct phone_info *phone, const char *loader_name);
int loader_activate_payload(struct sp_port *port, struct phone_info *phone);

int loader_get_erom_data(struct sp_port *port, struct phone_info *phone);
int loader_get_flash_data(struct sp_port *port, struct phone_info *phone);
int loader_get_otp_data(struct sp_port *port, struct phone_info *phone);
int loader_profilephone(struct sp_port *port, struct phone_info *phone);
int loader_activate_gdfs(struct sp_port *port);

int loader_enter_flashmode(struct sp_port *port, struct phone_info *phone);
int loader_send_csloader(struct sp_port *port, struct phone_info *phone);
int loader_send_oflash_ldr(struct sp_port *port, struct phone_info *phone);
int loader_send_bflash_ldr(struct sp_port *port, struct phone_info *phone);

int loader_shutdown(struct sp_port *port);

#endif // loader_h
