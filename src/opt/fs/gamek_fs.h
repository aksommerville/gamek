/* gamek_fs.h
 * Conveniences for filesystem access.
 */
 
#ifndef GAMEK_FS_H
#define GAMEK_FS_H

/* Read or write an entire regular file in one shot.
 */
int gamek_file_read(void *dstpp,const char *path);
int gamek_file_write(const char *path,const void *src,int srcc);

int gamek_mkdirp(const char *path);

// Position of last separator.
int gamek_path_find_separator(const char *path,int pathc);

#endif
