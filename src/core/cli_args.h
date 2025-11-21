#ifndef cli_args_h
#define cli_args_h

typedef struct {
	const char *port_name;
	int baudrate;
	const char *action;
	const char *unlock_target;
	const char *gdfs_filename;
	const char *flash_mainfw;
	const char *flash_fsfw;
	const char *flash_cda;
	const char *cnv_filename;
	const char *cnv_mode;
	const char *dest_path;
	const char *src_path;
	const char **src_list;
	int upload_count;
	const char *flash_avr_fw;
	const char *flash_arm_fw;
	const char **script_filenames;
	int script_count;
	uint32_t dump_addr;
	uint32_t dump_size;
	uint32_t mem_addr;
	int anycid;
	int break_rsa;
	int save_as_babe;
	const char *eeprom_file;
	const char *avr_device_name;
} cli_args;

#endif // cli_args
