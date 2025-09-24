#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* COMPILATION:  gcc -O2 -o ./decoder/read_system_z ./decoder/crc32fw.c
   USAGE:        sudo ./decoder/read_system_z /dev/sdb
*/

#define SECTOR_SIZE 512
#define CRC_SIZE 4
#define HEADER_SIZE 1
#define PAYLOAD_SIZE (SECTOR_SIZE - CRC_SIZE - HEADER_SIZE)

#define SUPER_SECTOR_1 0
#define SUPER_SECTOR_2 1
#define PATH_PAYLOAD "./decoder/.out/payload.csv"
#define PATH_METADATA "./decoder/.out/meta.csv"

#define RAID_MIRRORS 3

// --- CRC32 (same as firmware) ---
uint32_t crc32_u8bit(const uint8_t *data, size_t len) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 1)
                crc = (crc >> 1) ^ 0xEDB88320;
            else
                crc >>= 1;
        }
    }
    return crc ^ 0xFFFFFFFF;
}

// --- Read exactly N bytes ---
int read_bytes(FILE *f, void *buf, size_t n) {
    size_t r = fread(buf, 1, n, f);
    return (r == n) ? 0 : -1;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <device_or_file>\n", argv[0]);
        return 1;
    }

    const char *path = argv[1];
    FILE *f = fopen(path, "rb");
    if (!f) {
        perror("fopen");
        return 1;
    }

    uint8_t sector[SECTOR_SIZE];

    // --- Read sector 0 ---
    if (read_bytes(f, sector, SECTOR_SIZE) != 0) {
        fprintf(stderr, "Failed to read sector 0\n");
        fclose(f);
        return 1;
    }

    uint32_t last_sector = sector[0] | (sector[1] << 8) | (sector[2] << 16);
    uint16_t last_msg = sector[3] | (sector[4] << 8);
    uint8_t is_first_full = sector[5];

    printf("=== Supersector metadata ===\n");
    printf("last_sector   = %u\n", last_sector);
    printf("last_msg      = %u\n", last_msg);
    printf("is_first_full = %u\n\n", is_first_full);

    // --- Open CSV files ---
    FILE *csv_payload = fopen(PATH_PAYLOAD, "w");
    if (!csv_payload) {
        perror("fopen payload.csv");
        fclose(f);
        return 1;
    }
    fprintf(csv_payload, "status,header,payload(hex...),crc_stored,crc_calc\n");

    FILE *csv_meta = fopen(PATH_METADATA, "w");
    if (!csv_meta) {
        perror("fopen meta.csv");
        fclose(csv_payload);
        fclose(f);
        return 1;
    }
    fprintf(csv_meta, "type,last_sector,last_msg,is_first_full,raw(hex...)\n");

    // --- Sector 0 metadata ---
    fprintf(csv_meta, "sector0,%u,%u,%u,\"", last_sector, last_msg, is_first_full);
    for (int i = 0; i < SECTOR_SIZE; i++) {
        fprintf(csv_meta, "%02x ", sector[i]);
    }
    fprintf(csv_meta, "\"\n");

    // --- Sector 1 metadata (raw only, no CRC check) ---
    if (fseek(f, SUPER_SECTOR_2 * SECTOR_SIZE, SEEK_SET) == 0 &&
        read_bytes(f, sector, SECTOR_SIZE) == 0) {
        fprintf(csv_meta, "sector1,,,,\"");
        for (int i = 0; i < SECTOR_SIZE; i++) {
            fprintf(csv_meta, "%02x ", sector[i]);
        }
        fprintf(csv_meta, "\"\n");
    } else {
        fprintf(stderr, "Failed to read sector 1\n");
    }

    // --- Iterate over sectors 2..last_sector in groups of RAID_MIRRORS ---
    for (uint32_t sec = 2; sec <= last_sector; sec += RAID_MIRRORS) {
        uint32_t stored_crc[RAID_MIRRORS];
        uint32_t calc_crc[RAID_MIRRORS];
        int crc_ok[RAID_MIRRORS];
        uint8_t headers[RAID_MIRRORS];
        uint8_t payloads[RAID_MIRRORS][PAYLOAD_SIZE];

        for (int m = 0; m < RAID_MIRRORS; m++) {
            if (fseek(f, (sec + m) * SECTOR_SIZE, SEEK_SET) != 0 ||
                read_bytes(f, sector, SECTOR_SIZE) != 0) {
                fprintf(stderr, "Read failed for sector %u (mirror %d)\n", sec+m, m);
                goto cleanup;
            }

            headers[m] = sector[0];
            memcpy(payloads[m], &sector[1], PAYLOAD_SIZE);

            stored_crc[m] = sector[508] |
                            (sector[509] << 8) |
                            (sector[510] << 16) |
                            (sector[511] << 24);

            calc_crc[m] = crc32_u8bit(sector, HEADER_SIZE + PAYLOAD_SIZE);
            crc_ok[m] = (stored_crc[m] == calc_crc[m]);
        }

        // --- decide which copy to trust ---
        int chosen = -1;
        for (int m = 0; m < RAID_MIRRORS; m++) {
            if (crc_ok[m]) { chosen = m; break; }
        }

        const char *status = (chosen >= 0) ? "CRC_OK" : "CRC_FAIL";

        printf("Logical sector %u -> %s\n", (sec-2)/RAID_MIRRORS,
               (chosen>=0) ? "valid copy found" : "all copies bad");

        // Save chosen (or first) payload to CSV
        int use = (chosen >= 0) ? chosen : 0;
        fprintf(csv_payload, "%s,%u,\"", status, headers[use]);
        for (int i = 0; i < PAYLOAD_SIZE; i++) {
            fprintf(csv_payload, "%02x ", payloads[use][i]);
        }
        fprintf(csv_payload, "\",%u,%u\n", stored_crc[use], calc_crc[use]);
    }


    cleanup:
        fclose(csv_meta);
        fclose(csv_payload);
        fclose(f);

    printf("\nCSV export complete:\n  %s\n  %s\n", PATH_PAYLOAD, PATH_METADATA);
    return 0;
}
