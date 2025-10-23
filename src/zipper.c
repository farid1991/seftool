#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <minizip/unzip.h>

#include "common.h"

static int mkdir_recursive(const char *path)
{
    char tmp[4096];
    strncpy(tmp, path, sizeof(tmp));
    tmp[sizeof(tmp) - 1] = '\0';

    for (char *p = tmp + 1; *p; p++)
    {
        if (*p == '/' || *p == '\\')
        {
            *p = '\0';
            MKDIR(tmp);
            *p = '/';
        }
    }

    MKDIR(tmp);
    return 0;
}

int zip_extract(const char *zip_filename, const char *tmp_dir)
{
    unzFile uf = unzOpen(zip_filename);
    if (!uf)
    {
        fprintf(stderr, "Failed to open ZIP: %s\n", zip_filename);
        return -1;
    }

    if (unzGoToFirstFile(uf) != UNZ_OK)
    {
        fprintf(stderr, "No entries in ZIP: %s\n", zip_filename);
        unzClose(uf);
        return -1;
    }

    do
    {
        char filename_inzip[1024];
        unz_file_info fi;

        if (unzGetCurrentFileInfo(uf, &fi, filename_inzip, sizeof(filename_inzip), NULL, 0, NULL, 0) != UNZ_OK)
            continue;

        char outpath[4096];
        snprintf(outpath, sizeof(outpath), "%s/%s", tmp_dir, filename_inzip);

        // Directory entry
        if (filename_inzip[strlen(filename_inzip) - 1] == '/' ||
            filename_inzip[strlen(filename_inzip) - 1] == '\\')
        {
            mkdir_recursive(outpath);
            continue;
        }

        // Ensure directory exists
        char *last_slash = strrchr(outpath, '/');
        if (last_slash)
        {
            *last_slash = '\0';
            mkdir_recursive(outpath);
            *last_slash = '/';
        }

        if (unzOpenCurrentFile(uf) != UNZ_OK)
        {
            fprintf(stderr, "Failed to open file in ZIP: %s\n", filename_inzip);
            unzClose(uf);
            return -1;
        }

        FILE *out = fopen(outpath, "wb");
        if (!out)
        {
            fprintf(stderr, "Failed to create file: %s\n", outpath);
            unzCloseCurrentFile(uf);
            unzClose(uf);
            return -1;
        }

        uint8_t buf[4096];
        int nread;
        while ((nread = unzReadCurrentFile(uf, buf, sizeof(buf))) > 0)
            fwrite(buf, 1, nread, out);

        fclose(out);
        unzCloseCurrentFile(uf);

    } while (unzGoToNextFile(uf) == UNZ_OK);

    unzClose(uf);
    return 0;
}
