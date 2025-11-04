#include "aht20.h"
#include "clock.h"
#include "i2c-master.h"

uint8_t aht20_init(i2c_t* bus, uint8_t addr7) {
    const uint8_t seq[3] = {0xBE, 0x08, 0x00};
    int rc = i2c_master_write(bus, addr7, seq, sizeof(seq));
    delay_ms(100);
    return (rc == 0) ? 0 : 1;
}

uint8_t aht20_read(i2c_t* bus, uint8_t addr7, float *temp_c, float *hum_pct) {
    const uint8_t trig[3] = {0xAC, 0x33, 0x00};
    if (i2c_master_write(bus, addr7, trig, sizeof(trig)) != 0) return 1;

    delay_ms(80); // measurement time

    uint8_t d[7];
    if (i2c_master_read(bus, addr7, d, sizeof(d)) != 0) return 2;

    if (d[0] & 0x80) return 3; // busy

    uint32_t humRaw  = ((uint32_t)d[1] << 12) | ((uint32_t)d[2] << 4) | (d[3] >> 4);
    uint32_t tempRaw = (((uint32_t)d[3] & 0x0F) << 16) | ((uint32_t)d[4] << 8) | d[5];

    *hum_pct = (humRaw  * 100.0f) / 1048576.0f;
    *temp_c  = (tempRaw * 200.0f) / 1048576.0f - 50.0f;

    return 0;
}
