#include "fs.h"

#include "fatfs/ff.h"

static FATFS fs;
static FIL file;

bool InitFS()
{
    return f_mount(&fs, "0:", 0) == FR_OK;
}

void DeinitFS()
{
    f_mount(NULL, "0:", 0);
}

bool FileOpen(const char* path, bool truncate)
{
    unsigned flags = FA_READ | FA_WRITE;
    flags |= truncate ? FA_CREATE_ALWAYS : FA_OPEN_ALWAYS;
    bool ret = (f_open(&file, path, flags) == FR_OK);
    f_lseek(&file, 0);
    return ret;
}

size_t FileRead(void* buf, size_t size, size_t foffset)
{
    UINT bytes_read = 0;
    f_lseek(&file, foffset);
    f_read(&file, buf, size, &bytes_read);
    return bytes_read;
}

size_t FileWrite(void* buf, size_t size, size_t foffset)
{
    UINT bytes_written = 0;
    f_lseek(&file, foffset);
    f_write(&file, buf, size, &bytes_written);
    return bytes_written;
}

size_t FileGetSize()
{
    return f_size(&file);
}

void FileClose()
{
    f_close(&file);
}
