#ifndef BOT_QMI8658_H
#define BOT_QMI8658_H
#include "bot_motion.h"
#include <stddef.h>
#define BOT_QMI_ADDRESS 0x6bu
#define BOT_QMI_WHOAMI 0x05u
/* Callbacks return 0 on success. Only this sensor's device handle is passed. */
typedef struct {
    void *ctx;
    int (*read)(void *,uint8_t,uint8_t *,size_t);
    int (*write)(void *,uint8_t,uint8_t);
} bot_qmi_bus_t;
typedef struct { bot_qmi_bus_t bus; uint32_t timestamp; bool ready, have_timestamp; } bot_qmi_t;
typedef enum { BOT_QMI_OK, BOT_QMI_NOT_READY, BOT_QMI_IO, BOT_QMI_ID, BOT_QMI_CONFIG } bot_qmi_result_t;
bot_qmi_result_t bot_qmi_init(bot_qmi_t *q,bot_qmi_bus_t bus);
bot_qmi_result_t bot_qmi_read(bot_qmi_t *q,uint32_t now,bot_motion_sample_t *sample);
#endif
