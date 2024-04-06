//      *****  Persist class implementation  *****
//

#include "persist.h"
#include <string.h>
#include <stdio.h>
#include <pico/stdlib.h>
#include <pico/multicore.h>
#include <pico/flash.h>
#include <hardware/watchdog.h>
#include <pico/btstack_flash_bank.h>

Persist::Persist() : flash_size_(0)
{

}

void Persist::init()
{
    printf("Initialize persistent store\n");
    int status = flash_safe_execute(get_flash_size, nullptr, 2000);
    if (status == PICO_OK && flash_size_ > 0)
    {
        printf("Flash size: %d\n", flash_size_);
        if ((PICO_FLASH_BANK_STORAGE_OFFSET + PICO_FLASH_BANK_TOTAL_SIZE) / 1024 == flash_size_)
        {
            flash_size_ -= PICO_FLASH_BANK_TOTAL_SIZE / 1024;
            printf("Adjust size for BTStack flash storage to %d\n", flash_size_);
        }
    }
    else
    {
        printf("Error %d getting flash size %d\n", status, flash_size_);
    }
    for (int ii = 0; ii < MAX_PERSIST_FILES; ii++)
    {
        file_mode_[ii] = -1;
        file_pos_[ii] = -1;
        file_buf_[ii] = nullptr;
    }
}

Persist::~Persist()
{
    for (int ii = 0; ii < MAX_PERSIST_FILES; ii++)
    {
        if (file_buf_[ii])
        {
            delete [] file_buf_[ii];
        }
    }
}

void Persist::get_flash_size(void *)
{
    uint8_t buf[8];
    memset(buf, 0, sizeof(buf));
    buf[0] = 0x9f;

    flash_do_cmd(buf, buf, sizeof(buf));

    int size = (1 << buf[3]) / 1024;

    get()->flash_size_ = size;
}

Persist::FileHeader *Persist::file_header(int fd)
{
    FileHeader *ret = nullptr;
    if (fd >= 0 && fd < MAX_PERSIST_FILES)
    {
        long ptr = XIP_BASE + flash_size_ * 1024 - (fd + 1) * 4096;
        ret = reinterpret_cast<FileHeader *>(ptr);
    }
    return ret;
}

uint8_t *Persist::file_data(int fd)
{
    uint8_t *ret = reinterpret_cast<uint8_t *>(file_header(fd));
    if (ret)
    {
        ret += sizeof(FileHeader);
    }
    return ret;
}

int Persist::open(const char *filename, int mode)
{
    int ret = -1;

    for (int ii = 0;  ii < MAX_PERSIST_FILES; ii++)
    {
        if (strcmp(filename, file_name_[ii]) == 0)
        {
            ret = ii;
            break;
        }
    }

    if (ret != -1)
    {
        if (mode == O_READ)
        {
            FileHeader *hdr = file_header(ret);
            if (hdr && strncmp(filename, hdr->name, 16) == 0)
            {
                file_mode_[ret] = O_READ;
                file_pos_[ret] = 0;
            }
            else
            {
                ret = -1;
            }
        }
        else if (mode == O_WRITE)
        {
            if (!file_buf_[ret])
            {
                file_buf_[ret] = new uint8_t[4096];
            }
            FileHeader *hdr = reinterpret_cast<FileHeader *>(file_buf_[ret]);
            strncpy(hdr->name, filename, sizeof(hdr->name));
            hdr->name[sizeof(hdr->name)] = 0;
            hdr->size = 0;
            file_mode_[ret] = O_WRITE;
            file_pos_[ret] = 0;
        }
        else
        {
            ret = -1;
        }
    }

    return ret;
}

int Persist::close(int fd)
{
    int ret = -1;
    if (fd >= 0 && fd < MAX_PERSIST_FILES)
    {
        if (file_buf_[fd])
        {
            FileHeader *hdr = reinterpret_cast<FileHeader *>(file_buf_[fd]);
            hdr->size = file_pos_[fd];
            uint8_t *flash = reinterpret_cast<uint8_t *>(file_header(fd));
            uint32_t off = reinterpret_cast<uint32_t>(flash) - XIP_BASE;

            FLASH_DATA param;
            param.off = off;
            param.size = 4096;
            param.data = file_buf_[fd];
            int status = flash_safe_execute(do_flash, &param, 2000);
            if (status == PICO_OK)
            {
                printf("Saved file %s\n", hdr->name);
                ret = 0;
            }
            else
            {
                printf("Failed to get lockout to write file %s. status = %d\n", hdr->name, status);
            }

            delete [] file_buf_[fd];
            file_buf_[fd] = nullptr;
        }
        else
        {
            ret = 0;
        }
        file_mode_[fd] = -1;
        file_pos_[fd] = -1;
    }
    if (ret != 0) printf("File close status: %d\n", ret);
    return ret;
}

int Persist::read(int fd, uint8_t *rdbuf, size_t len)
{
    int ret = -1;
    if (fd >= 0 && fd < MAX_PERSIST_FILES)
    {
        if (file_mode_[fd] == O_READ)
        {
            FileHeader *hdr = file_header(fd);
            int avail = hdr->size - file_pos_[fd];
            if (len > avail)
            {
                len = avail;
            }
            memcpy(rdbuf, &file_data(fd)[file_pos_[fd]], len);
            ret = len;
            file_pos_[fd] += len;
        }
    }
    return ret;
}

int Persist::write(int fd, const uint8_t *wtbuf, size_t len)
{
    int ret = -1;
    if (fd >= 0 && fd < MAX_PERSIST_FILES)
    {
        if (file_mode_[fd] == O_WRITE && file_buf_[fd] != nullptr)
        {
            FileHeader *hdr = reinterpret_cast<FileHeader *>(file_buf_[fd]);
            uint8_t *ptr = file_buf_[fd] + sizeof(FileHeader) + file_pos_[fd];
            int avail = 4096 - sizeof(FileHeader) - file_pos_[fd];
            if (len > avail)
            {
                len = avail;
            }
            memcpy(ptr, wtbuf, len);
            ret = len;
            file_pos_[fd] += len;
        }
    }
    return ret;
}

void Persist::do_flash(void *param)
{
    FLASH_DATA *data = static_cast<FLASH_DATA *>(param);
    flash_range_erase(data->off, data->size);
    flash_range_program(data->off, data->data, data->size);
}
