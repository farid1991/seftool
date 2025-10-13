#ifndef csloader_h
#define csloader_h

#include <stdint.h>

// GDFS Server
int csloader_write_gdfs_var(struct sp_port *port, uint8_t block, uint8_t lo, uint8_t hi,
                            uint8_t *data, uint32_t size);
int csloader_write_gdfs(struct sp_port *port, const char *inputfname);
int csloader_read_gdfs(struct sp_port *port, struct phone_info *phone);
int csloader_parse_gdfs_script(struct sp_port *port, const char *inputfname, const char *outputfname);
int csloader_gdfs_get_phonename(struct sp_port *port, struct phone_info *phone);
int csloader_gdfs_get_cxc_article(struct sp_port *port, struct phone_info *phone);

// FSX Server

typedef struct ose_stat_t
{
    uint16_t st_mode;  /* protection */
    int16_t st_nlink;  /* number of hard links */
    int16_t st_uid;    /* user ID of owner */
    int16_t st_gid;    /* group ID of owner */
    uint32_t st_size;  /* total size, in bytes */
    uint32_t st_atime; /* time of last access */
    uint32_t st_mtime; /* time of last modification */
    uint32_t st_ctime; /* time of last status change */
} ose_stat_t;

typedef struct
{
    char name[256]; // e.g. "customize.xml"
    int d_type;     // 1 = dir, 0 = file
} cs_entry_t;

#define OSEATTR_EXECUTABLE 0x0001
#define OSEATTR_WRITEABLE 0x0002
#define OSEATTR_READABLE 0x0004
#define OSEATTR_GROUP_EXECUTABLE 0x0008
#define OSEATTR_GROUP_WRITEABLE 0x0010
#define OSEATTR_GROUP_READABLE 0x0020
#define OSEATTR_OWNER_EXECUTABLE 0x0040
#define OSEATTR_OWNER_WRITEABLE 0x0080
#define OSEATTR_OWNER_READABLE 0x0100
#define OSEATTR_DIRECTORY 0x0200

#define OSEATTR_FILE_VISIBLE (OSEATTR_READABLE | OSEATTR_GROUP_READABLE | OSEATTR_OWNER_READABLE)
#define OSEATTR_FILE_WRITEABLE (OSEATTR_WRITEABLE | OSEATTR_GROUP_WRITEABLE | OSEATTR_OWNER_WRITEABLE)

// total: visible + writeable by all
#define DEFAULT_DIR_ATTRS (OSEATTR_FILE_VISIBLE | OSEATTR_FILE_WRITEABLE | OSEATTR_DIRECTORY)

int csloader_start_fsx_server(struct sp_port *port);
int csloader_get_working_directory(struct sp_port *port, char *cwd, size_t cwd_len);
int csloader_change_directory(struct sp_port *port, const char *dirname);
int csloader_stat(struct sp_port *port, const char *filename, ose_stat_t *st);
int csloader_list(struct sp_port *port, cs_entry_t **out_list, size_t *out_count);
int csloader_make_directory(struct sp_port *port, const char *dirname);
int csloader_upload_zip(struct sp_port *port, const char *zip_filename);
int csloader_put_file(struct sp_port *port, const char *name,
                      size_t filesize, const uint8_t *filebuffer);
int csloader_get_file(struct sp_port *port, const char *local_path, const char *fname);
int csloader_delete_file(struct sp_port *port, const char *filename);
int csloader_delete_directory(struct sp_port *port, const char *dirname);

int csloader_write_directory(struct sp_port *port,
                             const char *src_dir,    // local base dir, e.g. "./tmp"
                             const char *subdirname, // relative subdir (used internally)
                             const char *dest_dir);  // base on phone ("" = derive)
int csloader_read_directory(struct sp_port *port,
                            const char *src_dir,   // internal FS path, e.g. "/tpa/preset/custom"
                            const char *dest_dir); // local path, e.g. "./dump"

int csloader_shutdown_fsx_server(struct sp_port *port);

#endif // csloader_h
