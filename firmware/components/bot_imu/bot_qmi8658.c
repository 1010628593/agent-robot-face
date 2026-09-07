/* QMI8658C minimum polling profile, verified against SensorLib register map.
 * Identity check precedes ALL writes. No reset/PMIC/bus ownership changes.
 * Async burst is accepted only when its surrounding timestamps agree. */
#include "bot_qmi8658.h"
#include <string.h>
static uint32_t timestamp(const uint8_t *b) { return b[0]|((uint32_t)b[1]<<8)|((uint32_t)b[2]<<16); }
static float signed_word(const uint8_t *b) {
    uint32_t n=b[0]|((uint32_t)b[1]<<8);
    return n>=32768?(float)((int32_t)n-65536):(float)n;
}
bot_qmi_result_t bot_qmi_init(bot_qmi_t *q,bot_qmi_bus_t bus) {
    if(!q || !bus.read || !bus.write)return BOT_QMI_CONFIG;
    memset(q,0,sizeof(*q));q->bus=bus;uint8_t id=0;
    if(bus.read(bus.ctx,0,&id,1))return BOT_QMI_IO;
    if(id!=BOT_QMI_WHOAMI)return BOT_QMI_ID;
    /* Disabled -> little endian + auto increment -> +-4g / +-1024dps.
     * ODR code 6: accel nominal 125 Hz, gyro 112.1 Hz (6DOF gyro clock).
     * No FIFO, hardware gesture engine, self-test or interrupt pin setup. */
    const uint8_t settings[][2]={{8,0},{2,0x40},{3,0x16},{4,0x66},{6,0},{9,0},{0x14,0},{8,0x23}};
    for(unsigned i=0;i<sizeof(settings)/sizeof(settings[0]);i++)
        if(bus.write(bus.ctx,settings[i][0],settings[i][1]))return BOT_QMI_IO;
    const uint8_t verify[][2]={{2,0x40},{3,0x16},{4,0x66},{8,0x23}};
    for(unsigned i=0;i<sizeof(verify)/sizeof(verify[0]);i++) {
        uint8_t v=0;if(bus.read(bus.ctx,verify[i][0],&v,1))return BOT_QMI_IO;
        if(v!=verify[i][1])return BOT_QMI_CONFIG;
    }
    q->ready=true;return BOT_QMI_OK;
}
bot_qmi_result_t bot_qmi_read(bot_qmi_t *q,uint32_t now,bot_motion_sample_t *s) {
    if(!q || !s || !q->ready)return BOT_QMI_CONFIG;
    uint8_t status=0,burst[17],after[3];
    if(q->bus.read(q->bus.ctx,0x2e,&status,1))return BOT_QMI_IO;
    if((status&3)!=3)return BOT_QMI_NOT_READY;
    if(q->bus.read(q->bus.ctx,0x30,burst,sizeof(burst)) ||
       q->bus.read(q->bus.ctx,0x30,after,sizeof(after)))return BOT_QMI_IO;
    uint32_t ts=timestamp(burst);
    if(ts!=timestamp(after) || (q->have_timestamp && q->timestamp==ts))return BOT_QMI_NOT_READY;
    bot_motion_sample_t out={.ms=now};
    for(int i=0;i<3;i++) {
        out.accel[i]=signed_word(&burst[5+2*i])/8192.0f;
        out.gyro[i]=signed_word(&burst[11+2*i])/32.0f;
    }
    q->timestamp=ts;q->have_timestamp=true;*s=out;return BOT_QMI_OK;
}
