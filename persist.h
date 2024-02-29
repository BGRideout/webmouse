//      *****  Class to Persist COnfiguration Data  *****
//
//  This class implements a set of pseudo files in flash memory.  Each file is a 4K
//  block of memory near the end of available flash.  The file name is a max 15 char
//  string that is encoded at the beginning of the block.  The file size is in the
//  next 4 bytes. The class supports a list of predefined file names defined below.

#ifndef PERSIST_H
#define PERSIST_H

#include <hardware/flash.h>

#define MAX_PERSIST_FILES 1

#define O_READ  1
#define O_WRITE 2

class Persist
{
private:
    int         flash_size_;        // Flash memory size (kilobytes)

    int         file_mode_[MAX_PERSIST_FILES];  // Current file mode
    int         file_pos_[MAX_PERSIST_FILES];   // Current file position
    uint8_t     *file_buf_[MAX_PERSIST_FILES];  // File write buffer
    
    const char  *file_name_[MAX_PERSIST_FILES] = {
                    "config" };

    struct FileHeader
    {
        char    name[16];       // Name
        int     size;           // Size
    };

    static void get_flash_size(void *);
    struct FLASH_DATA
    {
        uint32_t    off;
        size_t      size;
        uint8_t     *data;
    };
    static void do_flash(void *param);
    FileHeader  *file_header(int fd);
    uint8_t     *file_data(int fd);

    Persist();
    
public:
    static Persist *get() {static Persist *singleton = nullptr; if (!singleton) singleton = new Persist(); return singleton;}
    ~Persist();
    void init();

    int open(const char *filename, int mode);
    int close(int fd);
    int read(int fd, uint8_t *rdbuf, size_t len);
    int write(int fd, const uint8_t *wtbuf, size_t len);
};

#endif

