# G0: 音频契约核验报告

**状态：核验完成，可进入实施阶段**

基于修订方案v1.1要求，对BSP音频接口进行全面核验。

## 1. BSP接口核验

### 1.1 可用API
| API | 状态 | 说明 |
|-----|------|------|
| `bsp_audio_init(const i2s_std_config_t *i2s_config)` | ✅ 可用 | 初始化I2S外设，支持自定义配置 |
| `bsp_audio_codec_microphone_init(void)` | ✅ 可用 | 初始化ES7210麦克风编解码器 |
| `esp_codec_dev_read(handle, data, len)` | ✅ 可用 | 读取音频数据 |
| `esp_codec_dev_close(handle)` | ✅ 可用 | 关闭编解码器设备 |
| `i2s_del_channel(chan_handle)` | ✅ 可用 | 释放I2S通道资源 |

### 1.2 不可用API
| API | 状态 | 说明 |
|-----|------|------|
| `bsp_audio_init_voice_24k()` | ❌ 不存在 | 当前锁定BSP版本无此函数 |
| `bsp_audio_deinit()` | ❌ 不存在 | 需手动调用`i2s_del_channel()` |

### 1.3 降级决策
**采用22050Hz单声道STD模式**，理由：
1. 当前锁定BSP版本(3.0.1)只支持`bsp_audio_init(NULL)`的22050Hz mono STD路径
2. 维护版有voice_24k，但注册包与本机实际文件一致性未确认
3. 修订方案允许降级路径：`active_mics=1`

## 2. 引脚与共享资源核验

### 2.1 I2S引脚分配
| 引脚 | 功能 | 共享情况 | 风险 |
|------|------|----------|------|
| GPIO9 | I2S_BCLK/SCLK | ES8311与ES7210共用 | 低 |
| GPIO42 | I2S_MCLK | ES8311与ES7210共用 | 低 |
| GPIO45 | I2S_LRCK/WS | ES8311与ES7210共用 | 低 |
| GPIO8 | I2S_DOUT | 播放数据输出 | 低 |
| GPIO10 | I2S_DIN | 麦克风接收数据 | 低 |
| GPIO46 | PA_CTRL | 功放控制 | 中 |

### 2.2 共享资源保护
- **I2C总线**：已由`bsp_i2c_init()`初始化，IMU(QMI8658)和触摸(CST9217)共用
- **功放控制**：由BSP管理，不要直接操作GPIO46
- **电源管理**：AXP2101管理音频电源路径

### 2.3 安全约束
1. 不重建IMU/触控使用的I2C总线
2. 不碰PMIC供电轨
3. 使用BSP已验证的PA静音方式

## 3. 物理输入核验

### 3.1 ES7210物理连接
| 输入 | 物理设备 | 用途 |
|------|----------|------|
| MIC1 | 板载前置麦克风1 | 环境音采集 |
| MIC2 | 板载前置麦克风2 | 环境音采集 |
| MIC3 | ES8311播放参考信号 | 回声处理（首版不使用） |
| MIC4 | 未连接 | 忽略 |

### 3.2 BSP通道映射
**重要发现**：BSP的四个接收时隙顺序为MIC1、MIC3（参考）、MIC2、MIC4。

这意味着：
- 时隙0：MIC1（物理麦克风1）
- 时隙1：MIC3（播放参考，非麦克风）
- 时隙2：MIC2（物理麦克风2）
- 时隙3：MIC4（未连接）

**不能假定前两个时隙就是双麦克风**！

### 3.3 首版策略
**采用单麦克风降级**，理由：
1. 当前BSP配置为单声道模式，只返回一个通道数据
2. 通道筛选行为未验证，可能存在压缩返回数据的情况
3. 修订方案允许`active_mics=1`的降级路径

## 4. 返回PCM契约核验

### 4.1 当前BSP默认配置
```c
#define BSP_I2S_DUPLEX_MONO_CFG(_sample_rate)                                                         \
    {                                                                                                 \
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(_sample_rate),                                          \
        .slot_cfg = I2S_STD_PHILIP_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO), \
        .gpio_cfg = BSP_I2S_GPIO_CFG,                                                                 \
    }
```

### 4.2 PCM参数
| 参数 | 值 | 说明 |
|------|-----|------|
| 采样率(Fs) | 22050 Hz | 默认配置 |
| 位深 | 16-bit | I2S_DATA_BIT_WIDTH_16BIT |
| 模式 | STD (标准I2S) | I2S_SLOT_MODE_MONO |
| 声道数 | 1 (单声道) | BSP默认配置 |
| 字节序 | 小端 | ESP32-S3默认 |

### 4.3 缓冲区计算
- 10ms窗口：22050 × 0.01 × 2 bytes = 441 bytes
- 20ms窗口：22050 × 0.02 × 2 bytes = 882 bytes
- 采用样本相位累加处理220/221帧交替

## 5. 通道筛选行为核验

### 5.1 当前行为
由于采用单声道STD模式，`esp_codec_dev_read()`只返回一个通道的数据。

### 5.2 验证需求
需要实机验证：
1. 是否真的只返回一个通道
2. 如果是多通道，数据是否被压缩
3. 使用channel_mask后API的行为

### 5.3 首版策略
假设返回单通道数据，`active_mics=1`。实机验证后可能调整。

## 6. 增益核验

### 6.1 默认增益
ES7210默认增益设置为**30.0 dB**（见`es7210.c`第457行）。

### 6.2 增益范围
ES7210支持0-30 dB增益，步进未知。

### 6.3 增益调整策略
1. 首版使用固定增益30 dB
2. 改增益需重置检测器并重新校准
3. 记录增益变化到`stream_epoch`

## 7. Read行为核验

### 7.1 阻塞行为
- 函数：`i2s_channel_read(rx_chan, data, size, &bytes_read, DEFAULT_WAIT_TIMEOUT)`
- 超时：1000ms（`DEFAULT_WAIT_TIMEOUT`）
- 阻塞等待数据，不主动sleep

### 7.2 部分读取
- 返回实际读取的字节数
- 成功返回`ESP_CODEC_DEV_OK`
- 失败返回`ESP_CODEC_DEV_DRV_ERR`

### 7.3 填零行为
如果`in_reconfig`为true，返回全零数据并sleep 10ms。

### 7.4 超时处理
需要处理超时情况，避免无限阻塞。

## 8. 生命周期核验

### 8.1 初始化流程
```c
bsp_i2c_init() → bsp_audio_init(NULL) → bsp_audio_codec_microphone_init()
```

### 8.2 关闭流程
```c
esp_codec_dev_close(handle) → i2s_del_channel(rx_chan) → i2s_del_channel(tx_chan)
```

### 8.3 关键发现
1. **无全局deinit**：BSP没有提供`bsp_audio_deinit()`函数
2. **资源所有权**：I2S通道由BSP创建，但删除需手动调用
3. **共享TX/RX**：删除RX通道可能影响TX通道（共享时钟）
4. **Codec清理**：`esp_codec_dev_close()`只关闭设备，不释放I2S资源

### 8.4 安全关闭策略
1. 先调用`esp_codec_dev_close()`关闭编解码器
2. 等待当前read返回（有界超时）
3. 调用`i2s_del_channel()`释放I2S资源
4. 清理PCM工作区和特征历史

## 9. 声音输出核验

### 9.1 功放控制
- GPIO46控制NS4150B功放
- BSP管理功放状态
- 采集时需要TX时钟，但不应打开功放

### 9.2 静音策略
1. 采集期间保持PA静音
2. 不随意释放共享时钟
3. 使用BSP已验证的静音方式

## 10. 风险评估与缓解

### 10.1 高风险项
| 风险 | 影响 | 缓解措施 |
|------|------|----------|
| BSP版本不匹配 | API不可用 | 采用降级路径，记录实际配置 |
| 共享资源冲突 | 触摸/IMU异常 | 复用BSP句柄，不重建I2C |
| 关闭不完整 | 资源泄漏 | 严格的关闭流程，超时保护 |
| 阻塞read | UI卡死 | 有界超时，独立任务 |

### 10.2 中风险项
| 风险 | 影响 | 缓解措施 |
|------|------|----------|
| 增益过高/过低 | 削波/信噪比差 | 固定增益，记录变化 |
| 通道映射错误 | 数据错误 | 首版单通道，实机验证 |
| 时钟共享 | 音频质量 | 保持BSP配置，不修改时钟 |

## 11. 实施建议

### 11.1 首版配置
```c
// 建议的音频配置
#define AUDIO_SAMPLE_RATE      22050
#define AUDIO_BITS_PER_SAMPLE  16
#define AUDIO_CHANNELS         1  // 单声道
#define AUDIO_BUFFER_MS        10 // 10ms缓冲
#define AUDIO_BUFFER_SIZE      (AUDIO_SAMPLE_RATE * AUDIO_BUFFER_MS / 1000 * AUDIO_BITS_PER_SAMPLE / 8)
```

### 11.2 任务设计
1. 独立音频任务，优先级3（与IMU相同）
2. 阻塞read，不主动sleep
3. 错误退避：0.5/1/2秒
4. 最多3次重试

### 11.3 资源预算
- 音频缓冲区：441 bytes (10ms)
- 双缓冲：882 bytes
- 任务栈：4KB
- 特征历史：1-2KB
- 总计：约7KB

## 12. 验收标准

### 12.1 G0验收项
- [x] BSP接口可用性确认
- [x] 引脚与共享资源确认
- [x] 物理输入确认
- [x] PCM参数确认
- [ ] 通道筛选行为验证（需实机）
- [ ] Read行为验证（需实机）
- [ ] 生命周期验证（需实机）

### 12.2 下一步
1. 创建`bot_audio_port`组件，实现BSP适配
2. 实现基本采集任务，验证read行为
3. 实现关闭流程，验证资源释放
4. 实机测试，收集实际参数

---

**核验结论**：当前锁定BSP版本支持22050Hz单声道STD采集，可作为首版实施路径。需要实机验证通道筛选、read行为和生命周期管理。建议采用降级策略，`active_mics=1`，后续根据实机结果调整。