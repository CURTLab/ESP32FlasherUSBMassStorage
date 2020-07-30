#include "vFat16.h"

#define RESERVED_SECTORS 1
#define ROOT_DIR_SECTORS 4
#define NUM_FAT_BLOCKS (m_vmemory / 512)
#define SECTORS_PER_FAT ((NUM_FAT_BLOCKS * 2 + 511) / 512)

#define START_FAT0 RESERVED_SECTORS
#define START_FAT1 (START_FAT0 + SECTORS_PER_FAT)
#define START_ROOTDIR (START_FAT1 + SECTORS_PER_FAT)
#define START_CLUSTERS (START_ROOTDIR + ROOT_DIR_SECTORS)

static void padded_memcpy(char *dst, const char *src, uint8_t len)
{
    while (len)
    {
        if (*src)
            *dst++ = *src++;
        else
            *dst++ = ' ';
        --len;
    }
}

vFat16::vFat16()
    : m_vmemory(0), m_readFilesReserved(0), m_readFilesCount(0), m_writeFilesReserved(0), m_writeFilesCount(0), m_writeFileSize(0), m_lastWriteFileId(0), m_currentWriteFile(nullptr)
{
    m_boot.JumpInstruction[0] = 0xeb;
    m_boot.JumpInstruction[1] = 0x3c;
    m_boot.JumpInstruction[2] = 0x90;
    m_boot.ReservedSectors = RESERVED_SECTORS;
    m_boot.SectorSize = 512;
    m_boot.SectorsPerCluster = 1;
    m_boot.FATCopies = 2;
    m_boot.MediaDescriptor = 0xF8;
    m_boot.SectorsPerTrack = 1;
    m_boot.Heads = 1;
    m_boot.ExtendedBootSig = 0x29;
    m_boot.VolumeSerialNumber = 0x00420042;
    padded_memcpy((char *)m_boot.OEMInfo, "VFAT FS", sizeof(m_boot.OEMInfo));
    padded_memcpy((char *)m_boot.FilesystemIdentifier, "FAT16", sizeof(m_boot.FilesystemIdentifier));

    m_readFilesReserved = 2;
    m_readFiles = (vFAT16_TextFile *)malloc(m_readFilesReserved * sizeof(vFAT16_TextFile));

    m_writeFilesReserved = 2;
    m_writeFiles = (vFAT16_WriteFile *)malloc(m_writeFilesReserved * sizeof(vFAT16_WriteFile));
}

vFat16::~vFat16()
{
    free(m_readFiles);
    free(m_writeFiles);
}

void vFat16::begin(uint32_t vmemory, const char *volumeLabel)
{
    m_vmemory = vmemory;
    m_boot.RootDirectoryEntries = (ROOT_DIR_SECTORS * 512 / 32);
    m_boot.TotalSectors16 = NUM_FAT_BLOCKS - 2;
    m_boot.SectorsPerFAT = SECTORS_PER_FAT;
    padded_memcpy(m_boot.VolumeLabel, volumeLabel, sizeof(m_boot.VolumeLabel));
}

uint32_t vFat16::vmemory() const
{
    return m_vmemory;
}

void vFat16::addTextFile(const char *name, const char *data)
{
    if (m_readFilesCount >= 15)
        return;

    // extend the vector if necessary
    if (m_readFilesCount > m_readFilesReserved)
    {
        m_readFilesReserved *= 2;
        realloc(m_readFiles, sizeof(vFAT16_TextFile) * m_readFilesReserved);
    }

    // add new text file for read only
    m_readFiles[m_readFilesCount].size = strlen(data) + 1;
    m_readFiles[m_readFilesCount].data = (char *)malloc(m_readFiles[m_readFilesCount].size);
    strncpy(m_readFiles[m_readFilesCount].data, data, m_readFiles[m_readFilesCount].size);

    char *dst = m_readFiles[m_readFilesCount].name;
    padded_memcpy(dst, name, 8);
    dst[8] = 'T';
    dst[9] = 'X';
    dst[10] = 'T';
    ++m_readFilesCount;
}

void vFat16::addFileWriteCallback(const char *ext, FileWriteCallback cb)
{
    if (m_writeFilesCount >= 15)
        return;

    // extend the vector if necessary
    if (m_writeFilesCount > m_writeFilesReserved)
    {
        m_writeFilesReserved *= 2;
        realloc(m_writeFiles, sizeof(vFAT16_WriteFile) * m_writeFilesReserved);
    }

    // add new write file extention
    memcpy(m_writeFiles[m_writeFilesCount].ext, ext, 3);
    m_writeFiles[m_writeFilesCount].cb = cb;

    ++m_writeFilesCount;
}

bool vFat16::write(const uint8_t *buffer, uint32_t block, uint16_t num)
{    
    // Ignore boot sector
    if ((block == 0) || (m_writeFilesCount == 0))
        return true;

    if (block == START_ROOTDIR && (m_writeFileSize <= 0))
    {
        // If block is root directory, try to read the files within the root
        // 512 / 32 is 16 minus the volume root dir is 15 possible files
        // it will not work if we have more that 15 files
        for (uint16_t j = m_readFilesCount; j < 15; ++j)
        {
            vFAT16_DirEntry d;
            memcpy(&d, buffer + (1 + j) * sizeof(d), sizeof(d));
            // ignore dumb file and system files
            if (d.size == 0 && d.name[0] == '\0')
                break;
            if (d.size == 0 || d.attrs == 0xf)
                continue;
            // if the extention is registered, read the filesize and find
            // the first with the registered extention
            for (uint16_t i = 0; i < m_writeFilesCount; ++i)
            {
                if ((d.ext[0] == m_writeFiles[i].ext[0]) &&
                    (d.ext[1] == m_writeFiles[i].ext[1]) &&
                    (d.ext[2] == m_writeFiles[i].ext[2]) &&
                    (m_lastWriteFileId != j))
                {
                    m_lastWriteFileId = j;
#ifdef DebugSerial
                    char name[9];
                    name[8] = '\0';
                    memcpy(name, d.name, 8);
                    DebugSerial.println("Found Write File: ");
                    DebugSerial.print("Name: ");
                    DebugSerial.print(name);
                    DebugSerial.print(", Size: ");
                    DebugSerial.println(d.size);
#endif
                    m_writeFileSize = d.size;
                    m_currentWriteFile = m_writeFiles + i;
                    break;
                }
            }
        }
    }

    while (num && (m_writeFileSize > 0) && (m_currentWriteFile != nullptr))
    {
        // check if block is in file sector and call the callback for the
        // current file
        const uint32_t blockStartCluster = START_CLUSTERS;
        if (block >= blockStartCluster)
        {
            if (!(m_currentWriteFile->cb)(buffer, block - blockStartCluster, m_writeFileSize))
                return false;
            m_writeFileSize -= 512;
        }
        --num;
    }

    return true;
}

bool vFat16::read(uint8_t *buffer, uint32_t block_no, uint16_t block_count)
{
    if (m_vmemory == 0)
        return false;

    uint16_t sectionIdx = block_no;

    if (block_no == 0)
    {
        memcpy(buffer, &m_boot, sizeof(vFAT16_BootBlock));
        buffer += sizeof(vFAT16_BootBlock);
        memset(buffer, 0, 512 - sizeof(vFAT16_BootBlock) - 2);
        buffer += 512 - sizeof(vFAT16_BootBlock) - 2;
        *buffer++ = 0x55;
        *buffer = 0xaa;
    }
    else if (block_no < START_ROOTDIR)
    {
        sectionIdx -= START_FAT0;
        if (sectionIdx >= SECTORS_PER_FAT)
            sectionIdx -= SECTORS_PER_FAT;
        uint16_t i = 0;
        if (sectionIdx == 0)
        {
            *buffer++ = 0xf0;
            for (uint16_t i = 1; i < 4 + m_readFilesCount * 2; ++i, ++buffer)
                *buffer = 0xff;
        }
        memset(buffer, 0, 512 - i);
    }
    else if (block_no < START_CLUSTERS)
    {
        sectionIdx -= START_ROOTDIR;
        if (sectionIdx == 0)
        {
            vFAT16_DirEntry d;
            memset(&d, 0, sizeof(d));
            padded_memcpy(d.name, m_boot.VolumeLabel, 11);
            d.attrs = 0x28;
            memcpy(buffer, &d, sizeof(d));
            buffer += sizeof(d);
            for (uint16_t i = 0; i < m_readFilesCount; ++i)
            {
                memset(&d, 0, sizeof(d));
                d.attrs = 0x01;
                d.size = m_readFiles[i].size;
                d.startCluster = i + 2;
                padded_memcpy(d.name, m_readFiles[i].name, 11);
                memcpy(buffer, &d, sizeof(d));
                buffer += sizeof(d);
            }
            memset(buffer, 0, 512 - (m_readFilesCount + 1) * 32);
        }
        else
        {
            memset(buffer, 0, 512);
        }
    }
    else
    {
        sectionIdx -= START_CLUSTERS;
        if (sectionIdx < m_readFilesCount)
        {
            uint32_t i = m_readFiles[sectionIdx].size;
            memcpy(buffer, m_readFiles[sectionIdx].data, i);
            buffer += i;
            memset(buffer, 0, 512 - i);
        }
        else
        {
            memset(buffer, 0, 512);
        }
    }
    return true;
}