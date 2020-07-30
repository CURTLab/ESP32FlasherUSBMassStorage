#ifndef VFAT16DEF_H
#define VFAT16DEF_H

#include <stdint.h>
#include "Arduino.h"

#if 1
#include <USBComposite.h>
extern USBCompositeSerial CompositeSerial;
#define DebugSerial CompositeSerial
#endif

typedef struct {
    uint8_t JumpInstruction[3];
    uint8_t OEMInfo[8];
    uint16_t SectorSize;
    uint8_t SectorsPerCluster;
    uint16_t ReservedSectors;
    uint8_t FATCopies;
    uint16_t RootDirectoryEntries;
    uint16_t TotalSectors16;
    uint8_t MediaDescriptor;
    uint16_t SectorsPerFAT;
    uint16_t SectorsPerTrack;
    uint16_t Heads;
    uint32_t HiddenSectors;
    uint32_t TotalSectors32;
    uint8_t PhysicalDriveNum;
    uint8_t Reserved;
    uint8_t ExtendedBootSig;
    uint32_t VolumeSerialNumber;
    char VolumeLabel[11];
    uint8_t FilesystemIdentifier[8];
} __attribute__((packed)) vFAT16_BootBlock;

typedef struct {
    char name[8];
    char ext[3];
    uint8_t attrs;
    uint8_t reserved;
    uint8_t createTimeFine;
    uint16_t createTime;
    uint16_t createDate;
    uint16_t lastAccessDate;
    uint16_t highStartCluster;
    uint16_t updateTime;
    uint16_t updateDate;
    uint16_t startCluster;
    uint32_t size;
} __attribute__((packed)) vFAT16_DirEntry;

typedef bool (*FileWriteCallback)(const uint8_t *data, uint32_t block_no, uint32_t fileSize);

typedef struct {
    char name[11];
    char *data;
    uint16_t size;
    //FileDataCallback cb;
} vFAT16_TextFile;

typedef struct {
    char ext[3];
    FileWriteCallback cb;
} vFAT16_WriteFile;

#endif // VFAT16DEF_H