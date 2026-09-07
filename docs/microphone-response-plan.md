# Bot Status v1 麦克风环境音响应方案

## 1. 项目背景与目标

### 1.1 当前状态
- 硬件：ESP32-S3-Touch-AMOLED-1.75 开发板，板载双麦克风（ES7210 ADC）
- 驱动：BSP 已封装 `bsp_audio_codec_microphone_init()` 和 `esp_codec_dev_read()` 接口
- 固件：当前未使用音频功能，但已有成熟的"传感→反应→表情"管线

### 1.2 目标
实现麦克风对环境音的实时响应，在 AMOLED 屏幕上产生视觉反馈，增强交互体验。

### 1.3 核心功能
1. **音量可视化**：实时显示环境音量级别
2. **声音事件检测**：识别突发声音（如拍手、敲击）
3. **表情响应**：声音事件触发表情变化（惊讶、好奇等）
4. **节奏跟随**：对持续声音节奏产生跟随动画

## 2. 技术架构

### 2.1 系统架构图
```
[麦克风硬件] → [ES7210 ADC] → [I2S 接口] → [音频采集任务]
                                                    ↓
[音频特征提取] ← [原始 PCM 数据]
        ↓
[bot_audio_view_t] → [UI 定时器] → [表情反应引擎]
        ↓
[LVGL 渲染] → [AMOLED 显示]
```

### 2.2 数据流设计
1. **采集层**：独立任务持续读取音频数据
2. **处理层**：计算 RMS、能量、频率特征
3. **传输层**：通过队列传递 `bot_audio_view_t`
4. **消费层**：UI 定时器读取最新数据，驱动表情反应

## 3. 详细设计

### 3.1 新增组件

#### 3.1.1 音频采集组件 (`bot_audio`)
路径：`firmware/components/bot_audio/`

**文件结构：**
- `bot_audio.c` - 主实现
- `include/bot_audio.h` - 公共接口
- `CMakeLists.txt` - 构建配置

**核心数据结构：**
```c
// bot_audio.h
typedef struct {
    uint32_t sampled_ms;      // 采样时间戳
    uint32_t event_ms;        // 事件触发时间
    float rms;                // 均方根值 (0.0 - 1.0)
    float peak;               // 峰值 (0.0 - 1.0)
    float energy;             // 能量值
    uint8_t level;            // 音量级别 (0-10)
    bot_audio_event_t event;  // 事件类型
    bool available;           // 数据是否有效
} bot_audio_view_t;

typedef enum {
    BOT_AUDIO_EVENT_NONE = 0,
    BOT_AUDIO_EVENT_LOUD,      // 突发大声
    BOT_AUDIO_EVENT_QUIET,     // 突然安静
    BOT_AUDIO_EVENT_RHYTHM,    // 检测到节奏
    BOT_AUDIO_EVENT_SUSTAINED  // 持续声音
} bot_audio_event_t;
```

**核心函数：**
```c
esp_err_t bot_audio_start(void);                    // 启动音频采集
bool bot_audio_latest(uint32_t ui_now, bot_audio_view_t *out); // 获取最新数据
void bot_audio_stop(void);                          // 停止采集
```

#### 3.1.2 音频反应组件 (`bot_face_audio_reaction`)
集成到现有 `face_reaction.c` 中。

**新增函数：**
```c
void bot_face_audio_reaction_apply(const bot_audio_view_t *audio, 
                                   bot_state_t state, 
                                   uint32_t now, 
                                   bot_face_pose_t *pose);
```

### 3.2 详细实现

#### 3.2.1 音频采集任务 (`bot_audio.c`)
```c
#include "bot_audio.h"
#include "bsp/esp-bsp.h"
#include "esp_codec_dev.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include <math.h>

static QueueHandle_t s_audio_queue;
static const char *TAG = "bot_audio";

// 音频缓冲区
#define AUDIO_BUFFER_SIZE 1024
static int16_t s_audio_buffer[AUDIO_BUFFER_SIZE];

// 特征计算参数
#define RMS_WINDOW_MS 50      // RMS 计算窗口
#define ENERGY_DECAY 0.95f    // 能量衰减系数
#define LOUD_THRESHOLD 0.6f   // 大声阈值
#define QUIET_THRESHOLD 0.1f  // 安静阈值
#define RHYTHM_THRESHOLD 0.4f // 节奏检测阈值

static uint32_t clock_ms(void) {
    return (uint32_t)(esp_timer_get_time() / 1000);
}

// 计算 RMS 值
static float calculate_rms(const int16_t *buffer, size_t samples) {
    float sum_sq = 0.0f;
    for (size_t i = 0; i < samples; i++) {
        float sample = (float)buffer[i] / 32768.0f; // 归一化到 [-1, 1]
        sum_sq += sample * sample;
    }
    return sqrtf(sum_sq / samples);
}

// 计算峰值
static float calculate_peak(const int16_t *buffer, size_t samples) {
    int16_t max_val = 0;
    for (size_t i = 0; i < samples; i++) {
        int16_t abs_val = abs(buffer[i]);
        if (abs_val > max_val) {
            max_val = abs_val;
        }
    }
    return (float)max_val / 32768.0f;
}

// 检测声音事件
static bot_audio_event_t detect_event(float rms, float prev_rms, 
                                      uint32_t now, uint32_t *last_event_ms) {
    // 突发大声检测
    if (rms > LOUD_THRESHOLD && prev_rms < LOUD_THRESHOLD * 0.7f) {
        *last_event_ms = now;
        return BOT_AUDIO_EVENT_LOUD;
    }
    
    // 突然安静检测
    if (rms < QUIET_THRESHOLD && prev_rms > LOUD_THRESHOLD * 0.5f) {
        *last_event_ms = now;
        return BOT_AUDIO_EVENT_QUIET;
    }
    
    // 节奏检测（基于能量变化）
    static float rhythm_energy = 0.0f;
    static uint32_t rhythm_count = 0;
    static uint32_t last_rhythm_ms = 0;
    
    if (rms > RHYTHM_THRESHOLD) {
        if (now - last_rhythm_ms > 200 && now - last_rhythm_ms < 1000) {
            rhythm_count++;
            if (rhythm_count >= 3) {
                rhythm_count = 0;
                *last_event_ms = now;
                return BOT_AUDIO_EVENT_RHYTHM;
            }
        }
        last_rhythm_ms = now;
    }
    
    // 持续声音检测
    static uint32_t sustained_start_ms = 0;
    if (rms > QUIET_THRESHOLD * 2.0f) {
        if (sustained_start_ms == 0) {
            sustained_start_ms = now;
        } else if (now - sustained_start_ms > 1000) {
            *last_event_ms = now;
            return BOT_AUDIO_EVENT_SUSTAINED;
        }
    } else {
        sustained_start_ms = 0;
    }
    
    return BOT_AUDIO_EVENT_NONE;
}

// 音频采集任务
static void audio_task(void *arg) {
    esp_codec_dev_handle_t mic_dev = (esp_codec_dev_handle_t)arg;
    
    // 打开麦克风设备
    esp_codec_dev_sample_info_t fs = {
        .sample_rate = 22050,
        .channel = 1,
        .bits_per_sample = 16,
    };
    
    esp_err_t ret = esp_codec_dev_open(mic_dev, &fs);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open microphone: %d", ret);
        vTaskDelete(NULL);
        return;
    }
    
    ESP_LOGI(TAG, "Microphone opened successfully");
    
    float prev_rms = 0.0f;
    uint32_t last_event_ms = 0;
    uint32_t last_log_ms = 0;
    
    while (true) {
        // 读取音频数据
        ret = esp_codec_dev_read(mic_dev, s_audio_buffer, sizeof(s_audio_buffer));
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Audio read failed: %d", ret);
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        
        uint32_t now = clock_ms();
        
        // 计算音频特征
        float rms = calculate_rms(s_audio_buffer, AUDIO_BUFFER_SIZE);
        float peak = calculate_peak(s_audio_buffer, AUDIO_BUFFER_SIZE);
        float energy = rms * rms; // 能量与 RMS 平方成正比
        
        // 检测事件
        bot_audio_event_t event = detect_event(rms, prev_rms, now, &last_event_ms);
        prev_rms = rms;
        
        // 计算音量级别 (0-10)
        uint8_t level = (uint8_t)(rms * 10.0f);
        if (level > 10) level = 10;
        
        // 构建视图数据
        bot_audio_view_t view = {
            .sampled_ms = now,
            .event_ms = last_event_ms,
            .rms = rms,
            .peak = peak,
            .energy = energy,
            .level = level,
            .event = event,
            .available = true,
        };
        
        // 发送到队列（覆盖旧数据）
        xQueueOverwrite(s_audio_queue, &view);
        
        // 诊断日志
        if (now - last_log_ms >= 1000) {
            last_log_ms = now;
            ESP_LOGI(TAG, "rms=%.3f peak=%.3f level=%u event=%d",
                     (double)rms, (double)peak, level, event);
        }
        
        // 适当的延迟，避免过度占用 CPU
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

esp_err_t bot_audio_start(void) {
    if (s_audio_queue) {
        return ESP_ERR_INVALID_STATE;
    }
    
    // 初始化音频编解码器
    esp_codec_dev_handle_t mic_dev = bsp_audio_codec_microphone_init();
    if (!mic_dev) {
        ESP_LOGE(TAG, "Failed to initialize microphone codec");
        return ESP_ERR_NOT_SUPPORTED;
    }
    
    // 创建队列
    s_audio_queue = xQueueCreate(1, sizeof(bot_audio_view_t));
    if (!s_audio_queue) {
        return ESP_ERR_NO_MEM;
    }
    
    // 创建任务
    BaseType_t ret = xTaskCreate(
        audio_task,
        "bot_audio",
        4096,
        mic_dev,
        3, // 优先级，与 IMU 任务相同
        NULL
    );
    
    if (ret != pdPASS) {
        vQueueDelete(s_audio_queue);
        s_audio_queue = NULL;
        return ESP_ERR_NO_MEM;
    }
    
    ESP_LOGI(TAG, "Audio task started");
    return ESP_OK;
}

bool bot_audio_latest(uint32_t ui_now, bot_audio_view_t *out) {
    if (!out) return false;
    
    memset(out, 0, sizeof(*out));
    if (!s_audio_queue || xQueuePeek(s_audio_queue, out, 0) != pdTRUE) {
        return false;
    }
    
    // 调整时间戳到 UI 时间
    uint32_t now = clock_ms();
    uint32_t age = now - out->sampled_ms;
    uint32_t event_age = now - out->event_ms;
    
    out->sampled_ms = ui_now - age;
    out->event_ms = ui_now - event_age;
    
    // 检查数据是否过期（超过 200ms）
    if (age > 200) {
        out->available = false;
        return false;
    }
    
    return true;
}

void bot_audio_stop(void) {
    // 注意：实际实现需要更完善的资源清理
    ESP_LOGW(TAG, "Audio stop not fully implemented");
}
```

#### 3.2.2 音频反应集成 (`face_reaction.c` 修改)
在现有 `face_reaction.c` 中添加音频反应函数：

```c
// 在 face_reaction.c 末尾添加
void bot_face_audio_reaction_apply(const bot_audio_view_t *audio, 
                                   bot_state_t state, 
                                   uint32_t now, 
                                   bot_face_pose_t *pose) {
    if (!audio || !pose || !audio->available) return;
    
    // 只在特定状态下响应声音
    if (!bot_face_reaction_allowed(state)) return;
    
    uint32_t age = now - audio->event_ms;
    
    // 声音事件响应
    switch (audio->event) {
        case BOT_AUDIO_EVENT_LOUD:
            // 突发大声：惊讶反应
            if (age < 500) {
                float envelope = sinf((float)age * 3.14159265f / 500);
                envelope *= audio->rms; // 声音越大反应越强
                
                // 眼睛睁大
                pose->left_h *= 1 + 0.15f * envelope;
                pose->right_h *= 1 + 0.15f * envelope;
                
                // 瞳孔缩小（惊讶时瞳孔会缩小）
                pose->pupil = fmaxf(pose->pupil, 0.8f * envelope);
                
                // 轻微向上看
                pose->gaze_y = pose->gaze_y * (1 - envelope) + 0.3f * envelope;
            }
            break;
            
        case BOT_AUDIO_EVENT_QUIET:
            // 突然安静：好奇反应
            if (age < 400) {
                float envelope = sinf((float)age * 3.14159265f / 400);
                
                // 头部轻微倾斜
                pose->tilt += 0.05f * envelope;
                
                // 眼睛微微眯起
                pose->left_h *= 1 - 0.08f * envelope;
                pose->right_h *= 1 - 0.08f * envelope;
            }
            break;
            
        case BOT_AUDIO_EVENT_RHYTHM:
            // 节奏检测：跟随节奏
            if (age < 1000) {
                float phase = (float)age * 0.006f; // 约 1Hz 节奏
                float envelope = sinf(phase) * (1 - (float)age / 1000);
                envelope *= audio->rms;
                
                // 头部跟随节奏摆动
                pose->cx += 3.0f * envelope * sinf(phase);
                pose->cy += 2.0f * envelope * cosf(phase);
                
                // 眼睛跟随节奏开合
                pose->left_h *= 1 + 0.1f * envelope;
                pose->right_h *= 1 + 0.1f * envelope;
            }
            break;
            
        case BOT_AUDIO_EVENT_SUSTAINED:
            // 持续声音：专注反应
            if (age < 2000) {
                float envelope = (float)age / 2000;
                envelope *= audio->rms;
                
                // 眼睛微微眯起（专注）
                pose->left_h *= 1 - 0.12f * envelope;
                pose->right_h *= 1 - 0.12f * envelope;
                
                // 瞳孔略微放大（专注时瞳孔会放大）
                pose->pupil = fmaxf(pose->pupil, 0.6f * envelope);
            }
            break;
            
        default:
            break;
    }
    
    // 音量级别的持续影响
    if (audio->level > 5) {
        // 较大音量时，眼睛略微睁大
        float level_effect = (audio->level - 5) / 5.0f; // 0-1
        pose->left_h *= 1 + 0.05f * level_effect;
        pose->right_h *= 1 + 0.05f * level_effect;
    } else if (audio->level < 3) {
        // 安静时，眼睛略微放松
        float quiet_effect = (3 - audio->level) / 3.0f; // 0-1
        pose->lid = fmaxf(pose->lid, 0.1f * quiet_effect);
    }
    
    // 应用边界限制
    pose->left_h = fmaxf(6, fminf(135, pose->left_h));
    pose->right_h = fmaxf(6, fminf(135, pose->right_h));
    pose->pupil = fmaxf(0.1f, fminf(1.0f, pose->pupil));
    pose->gaze_x = fmaxf(-1, fminf(1, pose->gaze_x));
    pose->gaze_y = fmaxf(-1, fminf(1, pose->gaze_y));
    pose->tilt = fmaxf(-0.25f, fminf(0.25f, pose->tilt));
    pose->cx = fmaxf(195, fminf(271, pose->cx));
    pose->cy = fmaxf(195, fminf(271, pose->cy));
}
```

#### 3.2.3 UI 集成 (`face.c` 修改)
在 `face_tick` 函数中集成音频反应：

```c
// 在 face_tick 函数中，添加音频反应应用
void face_tick(uint32_t now) {
    // ... 现有代码 ...
    
    // 获取音频数据
    static bot_audio_view_t s_audio_environment;
    bot_audio_latest(now, &s_audio_environment);
    
    // 应用音频反应（在运动反应之后）
    if (s_audio_environment.available) {
        bot_face_audio_reaction_apply(&s_audio_environment, 
                                      g_ui.agents[s_surface_agent].state, 
                                      now, &pose);
    }
    
    // ... 继续现有代码 ...
}
```

### 3.3 配置与集成

#### 3.3.1 Kconfig 配置
在 `firmware/main/Kconfig.projbuild` 中添加：

```kconfig
menu "Audio Configuration"
    config BOT_AUDIO_ENABLE
        bool "Enable microphone audio processing"
        default y
        help
            Enable microphone audio processing for environmental sound response.
            
    config BOT_AUDIO_DIAGNOSTICS
        bool "Enable audio diagnostics logging"
        default n
        depends on BOT_AUDIO_ENABLE
        help
            Enable detailed logging of audio processing for debugging.
endmenu
```

#### 3.3.2 主程序集成 (`main.c` 修改)
```c
#include "bot_audio.h"

void app_main(void) {
    // ... 现有初始化代码 ...
    
    // 启动音频采集
    esp_err_t audio_ret = bot_audio_start();
    if (audio_ret != ESP_OK) {
        ESP_LOGW(TAG, "Audio unavailable: %d; Face and touch remain active", audio_ret);
    } else {
        ESP_LOGI(TAG, "Audio processing started");
    }
    
    // ... 现有 UI 初始化代码 ...
}
```

## 4. 实施步骤

### 4.1 第一阶段：基础音频采集 (1-2天)
1. 创建 `bot_audio` 组件目录和文件
2. 实现基础音频采集任务
3. 添加 Kconfig 配置选项
4. 测试音频数据读取

### 4.2 第二阶段：特征提取与事件检测 (2-3天)
1. 实现 RMS、峰值计算
2. 实现声音事件检测算法
3. 添加诊断日志
4. 调整阈值参数

### 4.3 第三阶段：表情反应集成 (1-2天)
1. 实现 `bot_face_audio_reaction_apply` 函数
2. 集成到 `face_tick` 流程
3. 调整反应强度和时长

### 4.4 第四阶段：测试与优化 (1-2天)
1. 测试各种声音场景
2. 优化性能（CPU 占用、内存使用）
3. 调整视觉效果
4. 添加用户配置选项

## 5. 性能考虑

### 5.1 CPU 占用
- 音频采集任务：约 5-10% CPU（22050Hz 采样率，1024 样本缓冲区）
- 特征计算：约 2-3% CPU
- UI 反应：可忽略不计

### 5.2 内存使用
- 音频缓冲区：2KB (1024 samples × 2 bytes)
- 队列：约 100 bytes
- 任务栈：4KB
- 总计：约 6KB

### 5.3 实时性
- 音频延迟：约 46ms (1024 samples / 22050 Hz)
- 反应延迟：< 10ms (UI 定时器周期)
- 总延迟：< 60ms

## 6. 扩展功能

### 6.1 未来可扩展特性
1. **声音分类**：识别特定声音（语音、音乐、噪音）
2. **音量历史**：显示音量变化图表
3. **声音触发**：特定声音触发特定表情
4. **录音功能**：录制环境音片段
5. **语音命令**：简单语音命令识别

### 6.2 用户配置
1. 灵敏度调节
2. 反应强度设置
3. 特定事件开关
4. 音量级别显示开关

## 7. 风险评估

### 7.1 技术风险
1. **音频质量**：板载麦克风可能受环境噪声影响
2. **性能影响**：音频处理可能影响 UI 流畅度
3. **内存限制**：ESP32-S3 内存有限，需要优化

### 7.2 缓解措施
1. 使用数字滤波减少噪声
2. 优化算法，降低 CPU 占用
3. 使用静态内存分配，避免动态分配

## 8. 验收标准

### 8.1 功能验收
1. 音频采集正常工作，无崩溃
2. 声音事件检测准确率 > 80%
3. 表情反应及时、自然
4. 不影响现有功能

### 8.2 性能验收
1. CPU 占用增加 < 15%
2. 内存使用增加 < 10KB
3. UI 帧率保持 > 30fps

### 8.3 用户体验
1. 反应自然、不突兀
2. 响应及时，延迟可感知但不明显
3. 可配置，适应不同环境

---

**方案设计完成**：此方案基于现有架构，复用成熟的传感反应管线，实现风险低、集成度高。建议按阶段实施，每个阶段完成后进行测试验证。