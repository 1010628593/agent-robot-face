/* Development-only probe of the owned hardware port, independent of detectors/UI.
 * Reports measurements, never converts build success or a heuristic into G0 pass.
 */
#include "bot_audio_g0_probe.h"
#include "bot_audio_port.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include <math.h>
#include <string.h>
#if CONFIG_BOT_AUDIO_G0_PROBE
static const char *TAG = "g0";
static int16_t pcm[441];
static const bot_audio_port_config_t config = {
    .sample_rate=22050, .bits_per_sample=16, .channels=1, .buffer_ms=10
};
typedef struct {
    uint32_t ok, failures, partial, zero, clipped, gaps, max_us;
    uint64_t sum_us, bytes;
    float peak, rms_sum;
} phase_t;
static phase_t phase(const char *name, unsigned blocks) {
    phase_t st={0};
    for(unsigned i=0; i<blocks; ++i) {
        size_t got=0;
        uint64_t start=esp_timer_get_time();
        esp_err_t err=bot_audio_port_read(pcm,sizeof(pcm),&got,100);
        uint32_t elapsed=esp_timer_get_time()-start;
        bot_audio_port_read_meta_t meta;
        bot_audio_port_get_last_read(&meta);
        st.bytes+=got;
        st.sum_us+=elapsed;
        if(elapsed>st.max_us)st.max_us=elapsed;
        if(err==ESP_OK)st.ok++; else st.failures++;
        if(got!=sizeof(pcm))st.partial++;
        if(meta.gap)st.gaps++;
        double energy=0;
        int32_t peak=0;
        for(size_t n=0;n<got/sizeof(int16_t);++n) {
            int32_t value=pcm[n];
            int32_t amplitude=value<0?-value:value;
            if(amplitude>peak)peak=amplitude;
            energy+=(double)value*value;
        }
        if(got && !peak)st.zero++;
        if(peak>=32760)st.clipped++;
        float normalized=(float)peak/32768.f;
        if(normalized>st.peak)st.peak=normalized;
        float rms=got?(float)sqrt(energy/(got/2))/32768.f:0;
        st.rms_sum+=rms;
        memset(pcm,0,sizeof(pcm));
    }
    ESP_LOGI(TAG,"phase=%s ok=%lu failed=%lu partial=%lu zero=%lu clip=%lu gap=%lu bytes=%llu avg_us=%llu max_us=%lu rms_avg=%.5f peak=%.5f",
        name,(unsigned long)st.ok,(unsigned long)st.failures,(unsigned long)st.partial,
        (unsigned long)st.zero,(unsigned long)st.clipped,(unsigned long)st.gaps,
        (unsigned long long)st.bytes,(unsigned long long)(st.sum_us/blocks),(unsigned long)st.max_us,
        (double)(st.rms_sum/blocks),(double)st.peak);
    return st;
}
static size_t internal_heap(void) { return heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT); }
void bot_audio_g0_probe(void) {
    size_t boot=internal_heap();
    size_t dma_boot=heap_caps_get_free_size(MALLOC_CAP_DMA);
    size_t psram_boot=heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    esp_err_t err=bot_audio_port_init(&config);
    if(err==ESP_OK)err=bot_audio_port_open();
    if(err!=ESP_OK) {
        esp_err_t cleanup=bot_audio_port_deinit();
        ESP_LOGE(TAG,"@g0 {\"verdict\":\"open_failed\",\"error\":%d,\"cleanup\":%d}",err,cleanup);
        return;
    }
    size_t opened=internal_heap();
    ESP_LOGI(TAG,"A: keep quiet for 2 seconds NOW");
    phase_t a=phase("A",100);
    /* Do not delay while DMA runs: a prompt wait would intentionally overflow the ring. */
    ESP_LOGI(TAG,"B: clap/talk near MIC1 for 4 seconds NOW");
    phase_t b=phase("B",200);
    esp_err_t close=bot_audio_port_close();
    esp_err_t deinit=bot_audio_port_deinit();
    size_t baseline=internal_heap(); /* shared BSP I2C baseline warmed once */
    unsigned complete=0,failed=0;
    for(unsigned i=0; i<100 && close==ESP_OK && deinit==ESP_OK; ++i) {
        esp_err_t init=bot_audio_port_init(&config);
        esp_err_t open=init==ESP_OK?bot_audio_port_open():init;
        size_t got=0;
        esp_err_t read=open==ESP_OK?bot_audio_port_read(pcm,sizeof(pcm),&got,100):open;
        memset(pcm,0,sizeof(pcm));
        close=bot_audio_port_close();
        deinit=bot_audio_port_deinit();
        bool ok=init==ESP_OK && open==ESP_OK && read==ESP_OK && got==sizeof(pcm) && close==ESP_OK && deinit==ESP_OK;
        if(ok)++complete; else ++failed;
        ESP_LOGI(TAG,"cycle=%u init=%d open=%d read=%d got=%u close=%d deinit=%d internal=%u dma=%u psram=%u",
            i,init,open,read,(unsigned)got,close,deinit,(unsigned)internal_heap(),
            (unsigned)heap_caps_get_free_size(MALLOC_CAP_DMA),(unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    bool response_candidate=a.ok && b.ok && !a.failures && !b.failures && b.peak>a.peak*2.f && b.peak>0.005f;
    ESP_LOGI(TAG,"@g0 {\"verdict\":\"operator_review_required\",\"fs\":22050,\"bits\":16,\"configured_mics\":1,\"active_mics_verified\":0,\"responsive_candidate\":%d,\"cycles_ok\":%u,\"cycles_failed\":%u,\"reopen_ok\":%d,\"close\":%d,\"deinit\":%d,\"heap_boot\":%u,\"heap_open\":%u,\"heap_warm_closed\":%u,\"heap_end\":%u,\"dma_boot\":%u,\"dma_end\":%u,\"psram_boot\":%u,\"psram_end\":%u}",
        response_candidate,complete,failed,complete==100&&!failed,close,deinit,
        (unsigned)boot,(unsigned)opened,(unsigned)baseline,(unsigned)internal_heap(),
        (unsigned)dma_boot,(unsigned)heap_caps_get_free_size(MALLOC_CAP_DMA),
        (unsigned)psram_boot,(unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
}
#endif
