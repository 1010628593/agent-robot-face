#ifdef NDEBUG
#undef NDEBUG
#endif
#include "bot_qmi8658.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
typedef struct { uint8_t regs[128];unsigned writes,reads;bool fail,torn,ignore_config; } fake_t;
static int read_reg(void *ctx,uint8_t r,uint8_t *out,size_t n) {
    fake_t *f=ctx;f->reads++;if(f->fail)return -1;
    assert((size_t)r+n<=sizeof(f->regs));memcpy(out,f->regs+r,n);
    if(f->torn && r==0x30 && n>3)f->regs[0x30]++;
    return 0;
}
static int write_reg(void *ctx,uint8_t r,uint8_t val) {
    fake_t *f=ctx;f->writes++;if(f->fail)return -1;
    assert(r==2 || r==3 || r==4 || r==6 || r==8 || r==9 || r==0x14);
    if(!f->ignore_config)f->regs[r]=val;
    return 0;
}
static void word(fake_t *f,unsigned r,int n) { f->regs[r]=(uint16_t)n&255;f->regs[r+1]=(uint16_t)n>>8; }
int main(void) {
    fake_t f={0};bot_qmi_t q;bot_qmi_bus_t bus={&f,read_reg,write_reg};bot_motion_sample_t s={0};
    assert(bot_qmi_init(&q,bus)==BOT_QMI_ID && f.writes==0);
    f.regs[0]=5;assert(bot_qmi_init(&q,bus)==BOT_QMI_OK);
    assert(f.regs[2]==0x40 && f.regs[3]==0x16 && f.regs[4]==0x66);
    assert((f.regs[8]&3)==3);
    assert(bot_qmi_read(&q,100,&s)==BOT_QMI_NOT_READY);
    f.regs[0x2e]=1;assert(bot_qmi_read(&q,100,&s)==BOT_QMI_NOT_READY);
    f.regs[0x2e]=2;assert(bot_qmi_read(&q,100,&s)==BOT_QMI_NOT_READY);
    f.regs[0x2e]=3;f.regs[0x30]=1;
    word(&f,0x35,8192);word(&f,0x37,-8192);word(&f,0x39,4096);
    word(&f,0x3b,3200);word(&f,0x3d,-3200);word(&f,0x3f,0);
    assert(bot_qmi_read(&q,110,&s)==BOT_QMI_OK);
    assert(s.ms==110 && s.accel[0]==1 && s.accel[1]==-1 && s.accel[2]==.5f);
    assert(s.gyro[0]==100 && s.gyro[1]==-100);
    assert(bot_qmi_read(&q,120,&s)==BOT_QMI_NOT_READY);
    f.regs[0x30]=2;f.torn=true;assert(bot_qmi_read(&q,130,&s)==BOT_QMI_NOT_READY);
    f.torn=false;f.fail=true;assert(bot_qmi_read(&q,140,&s)==BOT_QMI_IO);
    assert(s.ms==110); /* failed read never publishes new/fabricated sample */
    assert(bot_qmi_init(&q,bus)==BOT_QMI_IO && !q.ready);
    f.fail=false;f.ignore_config=true;f.regs[2]=0;
    assert(bot_qmi_init(&q,bus)==BOT_QMI_CONFIG && !q.ready);
    f.ignore_config=false;assert(bot_qmi_init(&q,bus)==BOT_QMI_OK);
    f.regs[0x30]=f.regs[0x31]=f.regs[0x32]=255;
    assert(bot_qmi_read(&q,150,&s)==BOT_QMI_OK);
    f.regs[0x30]=f.regs[0x31]=f.regs[0x32]=0;
    assert(bot_qmi_read(&q,160,&s)==BOT_QMI_OK); /* sample-counter rollover */
    assert(bot_qmi_init(&q,(bot_qmi_bus_t){0})==BOT_QMI_CONFIG && !q.ready);
    puts("QMI: identity, config, conversion, readiness, torn/duplicate/error passed");return 0;
}
