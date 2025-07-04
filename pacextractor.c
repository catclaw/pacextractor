#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#ifdef _WIN32
#include <io.h>
#endif

typedef struct {
    int16_t someField[24];
    int32_t someInt;
    int16_t productName[256];
    int16_t firmwareName[256];
    int32_t partitionCount;
    int32_t partitionsListStart;
    int32_t someIntFields1[5];
    int16_t productName2[50];
    int16_t someIntFields2[6];
    int16_t someIntFields3[2];
} PacHeader;

typedef struct {
    uint32_t length;
    int16_t partitionName[256];
    int16_t fileName[512];
    uint32_t partitionSize;
    int32_t someFileds1[2];
    uint32_t partitionAddrInPac;
    int32_t someFileds2[3];
    int32_t dataArray[];
} PartitionHeader;

void getString(int16_t* baseString, char* resString) {
    if(*baseString == 0) {
        *resString = 0;
        return;
    }
    int length = 0;
    do {
        *resString = (char)(0xFF & *baseString);
        resString++;
        baseString++;
        if(++length > 256)
            break;
    } while(*baseString > 0);
    *resString = 0;
}

void printProgressBar(uint64_t current, uint64_t total) {
    const int barWidth = 50;
    double fraction = (total > 0) ? ((double)current / (double)total) : 0.0;
    int filled = (int)(fraction * barWidth);

    printf("\r\t[");
    for (int i = 0; i < barWidth; i++) {
        if (i < filled)
            printf("Û");
        else
            printf(" ");
    }
    printf("] %3d%% (%llu of %llu bytes)", 
           (int)(fraction * 100),
           (unsigned long long)current,
           (unsigned long long)total);
    fflush(stdout);
}

int main(int argc, char** argv) {
    if(argc < 2) {
        printf("command format:\n   pacextractor <firmware name>.pac\n");
        return EXIT_FAILURE;
    }

    FILE *fd = fopen(argv[1], "rb");
    if (fd == NULL) {
        printf("file %s not found\n", argv[1]);
        return EXIT_FAILURE;
    }

#ifdef _WIN32
    __int64 fileSize;
    _fseeki64(fd, 0, SEEK_END);
    fileSize = _ftelli64(fd);
    _fseeki64(fd, 0, SEEK_SET);
#else
    off_t fileSize;
    fseeko(fd, 0, SEEK_END);
    fileSize = ftello(fd);
    fseeko(fd, 0, SEEK_SET);
#endif

    if(fileSize < sizeof(PacHeader)) {
        printf("file %s is not firmware\n", argv[1]);
        fclose(fd);
        return EXIT_FAILURE;
    }

    PacHeader pacHeader;
    size_t rb = fread(&pacHeader, sizeof(PacHeader), 1, fd);
    if(rb != 1) {
        printf("Error while reading PAC header\n");
        fclose(fd);
        return EXIT_FAILURE;
    }

    char buffer[1048576]; // 1 MB buffer
    char buffer1[1024];
    getString(pacHeader.firmwareName, buffer);
    printf("Firmware name: %s\n", buffer);

    uint32_t curPos = pacHeader.partitionsListStart;
    PartitionHeader** partHeaders = malloc(sizeof(PartitionHeader*) * pacHeader.partitionCount);
    if (!partHeaders) {
        printf("Memory allocation failure\n");
        fclose(fd);
        return EXIT_FAILURE;
    }

    int i;
    for (i = 0; i < pacHeader.partitionCount; i++) {
#ifdef _WIN32
        if (curPos >= (uint64_t)fileSize) {
            printf("\n[INFO] Reached end of PAC file. No more partitions.\n");
            break;
        }
        _fseeki64(fd, curPos, SEEK_SET);
#else
        if (curPos >= (uint64_t)fileSize) {
            printf("\n[INFO] Reached end of PAC file. No more partitions.\n");
            break;
        }
        fseeko(fd, curPos, SEEK_SET);
#endif

        uint32_t length;
        rb = fread(&length, sizeof(uint32_t), 1, fd);
        if (rb != 1) {
            printf("Partition header read error\n");
            break;
        }

        if ((uint64_t)curPos + length > (uint64_t)fileSize) {
            printf("Partition header reads beyond end of file.\n");
            break;
        }

        partHeaders[i] = malloc(length);
        if (!partHeaders[i]) {
            printf("Memory allocation error for partition header\n");
            break;
        }

#ifdef _WIN32
        _fseeki64(fd, curPos, SEEK_SET);
#else
        fseeko(fd, curPos, SEEK_SET);
#endif

        rb = fread(partHeaders[i], length, 1, fd);
        if (rb != 1) {
            printf("Partition header read error\n");
            free(partHeaders[i]);
            break;
        }
        curPos += length;

        getString(partHeaders[i]->partitionName, buffer);
        getString(partHeaders[i]->fileName, buffer1);

        printf("\n---------------------------------------------\n");
        printf("Partition name: %s\n", buffer);
        printf("File name     : %s\n", buffer1);
        printf("Size          : %u bytes (0x%X)\n", 
               partHeaders[i]->partitionSize, 
               partHeaders[i]->partitionSize);
        printf("Offset in PAC : %u bytes (0x%X)\n", 
               partHeaders[i]->partitionAddrInPac, 
               partHeaders[i]->partitionAddrInPac);
#ifdef _WIN32
        printf("PAC file size : %lld bytes (0x%llX)\n", fileSize, fileSize);
#else
        printf("PAC file size : %lld bytes (0x%llX)\n", (long long)fileSize, (long long)fileSize);
#endif
        printf("---------------------------------------------\n");

        if (partHeaders[i]->partitionSize == 0 || strlen(buffer1) == 0) {
            printf("[INFO] Skipping empty partition.\n");
            free(partHeaders[i]);
            continue;
        }

        uint64_t dataEnd = (uint64_t)partHeaders[i]->partitionAddrInPac +
                           (uint64_t)partHeaders[i]->partitionSize;
        if (dataEnd > (uint64_t)fileSize) {
            printf("[ERROR] Partition %s exceeds PAC file boundaries.\n", buffer1);
            printf("[ERROR] Cannot extract this partition.\n");
            free(partHeaders[i]);
            continue;
        }

#ifdef _WIN32
        _fseeki64(fd, partHeaders[i]->partitionAddrInPac, SEEK_SET);
#else
        fseeko(fd, partHeaders[i]->partitionAddrInPac, SEEK_SET);
#endif

        remove(buffer1);
        FILE *fd_new = fopen(buffer1, "wb");
        if (!fd_new) {
            printf("[ERROR] Cannot create output file %s\n", buffer1);
            free(partHeaders[i]);
            continue;
        }

        printf("Extracting %s...\n", buffer1);
        uint32_t dataSizeLeft = partHeaders[i]->partitionSize;
        uint64_t totalSize = dataSizeLeft;
        uint64_t written = 0;

        printProgressBar(0, totalSize);

        while (dataSizeLeft > 0) {
            uint32_t copyLength = (dataSizeLeft > sizeof(buffer)) ? sizeof(buffer) : dataSizeLeft;
            rb = fread(buffer, 1, copyLength, fd);
            if (rb != copyLength) {
                printf("\n[ERROR] Partition image extraction error while reading %s\n", buffer1);
                fclose(fd_new);
                free(partHeaders[i]);
                fclose(fd);
                return EXIT_FAILURE;
            }
            size_t wb = fwrite(buffer, 1, copyLength, fd_new);
            if (wb != copyLength) {
                printf("\n[ERROR] Partition image extraction error while writing %s\n", buffer1);
                fclose(fd_new);
                free(partHeaders[i]);
                fclose(fd);
                return EXIT_FAILURE;
            }
            dataSizeLeft -= copyLength;
            written += copyLength;
            printProgressBar(written, totalSize);
        }
        printf("\n");

        fclose(fd_new);
        free(partHeaders[i]);
    }

    free(partHeaders);
    fclose(fd);

    printf("\nExtraction completed.\n");
    return EXIT_SUCCESS;
}

