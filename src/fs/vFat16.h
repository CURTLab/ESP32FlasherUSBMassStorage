#ifndef VFAT16_H
#define VFAT16_H

#include "vFat16Defs.h"

class vFat16
{
public:
    vFat16();
    ~vFat16();

    void begin(uint32_t vmemory = 8000, const char *volumeLabel = "VFATSYS");

    uint32_t vmemory() const;

    void addTextFile(const char *name, const char *data);
    void addFileWriteCallback(const char *ext, FileWriteCallback cb);

    bool write(const uint8_t *buffer, uint32_t block_no, uint16_t block_count);
    bool read(uint8_t *buffer, uint32_t block_no, uint16_t block_count);

private:
    uint32_t m_vmemory;
    vFAT16_BootBlock m_boot;
    // read files vector
    vFAT16_TextFile *m_readFiles;
    uint8_t m_readFilesReserved;
    uint8_t m_readFilesCount;

    // write files vector
    vFAT16_WriteFile *m_writeFiles;
    uint8_t m_writeFilesReserved;
    uint8_t m_writeFilesCount;

    // write file callback variables
    int32_t m_writeFileSize;
    uint8_t m_lastWriteFileId;
    const vFAT16_WriteFile *m_currentWriteFile;

};

#endif // VFAT16_H