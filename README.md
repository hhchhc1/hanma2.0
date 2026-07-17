# 悍马 2.0 — 慧勤智远 ESP32-P4 智能语音助手

基于 **ESP32-P4** 开发板的智能语音助手，支持 **7 寸 MIPI 触摸屏**显示、**WiFi/蓝牙** 无线通信、**摄像头**人脸检测、**语音唤醒与控制**等丰富功能。

## 硬件平台

- **主控**：慧勤智远 ESP32-P4 + ESP32-C6 (SDIO WiFi)
- **屏幕**：7 寸 1024×600 MIPI 触摸屏
- **摄像头**：MIPI CSI 摄像头
- **音频**：I2S 麦克风 + 扬声器
- **传感器**：DHT22 温湿度、BH1750 光照强度

## 功能特性

- **语音助手**：支持"你好小智"唤醒词唤醒、语音对话控制
- **智能家居**：Smart Home Control 界面，实时显示温湿度/光照，控制风扇/灯光
- **人脸检测**：摄像头检测人脸后自动解锁屏幕
- **OTA 升级**：支持远程固件更新
- **MQTT 通信**：通过 MQTT 与服务器交互
- **微信风格聊天 UI**：对话气泡界面

---

## 三大架构创新

### 一、POSIX 标准化线程模型（对标 OpenVela）

将项目中全部 FreeRTOS 原生 API 封装为统一的 POSIX 标准接口，使上层应用代码具备跨 RTOS 可移植性。

```cpp
// 改造前：68 个文件散布裸 FreeRTOS API
xTaskCreate(func, "dht22", 4096, NULL, 5, NULL);
xEventGroupWaitBits(eg, MASK, pdTRUE, pdFALSE, portMAX_DELAY);

// 改造后：统一 POSIX 风格接口，底层实现可替换
posix::task_create({"dht22", 4096, 5, -1}, func);
event_group->wait_bits(MASK, true, false, 0);
```

新增 `main/posix_compat/` 模块（posix_event.h / posix_queue.h / posix_task.h / posix_timer.h），上层应用不再直接依赖 FreeRTOS。未来移植到 NuttX / Zephyr 只需替换底层 4 个头文件，68 个业务文件一行不动。

### 二、内存隔离保护机制

自研轻量级内存保护框架，解决 FreeRTOS 扁平内存模型下野指针导致整机死机的固有问题。

```
main/mem_protect/
├── mem_pool.h/cc   — 5 域分区内存池（AUDIO / DISPLAY / CAMERA / NETWORK / SYSTEM）
├── mem_guard.h     — 魔数哨兵检测（越界写入即时捕获）
└── mem_pmp.h       — RISC-V PMP 代码段硬件写保护
```

每个模块在自己的域分配内存，跨域释放被拒绝并告警；释放时校验首尾魔数，越界写入立即报警；代码段通过 PMP 硬件锁定，野指针写入触发 CPU 异常。

### 三、类 Matter 协议设备模型

智能家居设备采用 Matter 标准的 Endpoint → Cluster → Attribute 三层架构，实现设备模型标准化。

```
MatterNode
├── Endpoint 1: 本地环境传感器
│   ├── Temperature Measurement → DHT22
│   ├── Humidity Measurement    → DHT22
│   └── Light Measurement       → BH1750
├── Endpoint 2: 本地执行器
│   ├── Fan Control → GPIO36/7 风扇
│   └── OnOff       → GPIO12 灯泡
└── Endpoint 3: Room2 远程传感器 (ESP32-C3)
```

新增 `main/matter_device/` 模块（matter_attribute.h / matter_cluster.h / matter_endpoint.h / matter_device_mgr.h），MCP 工具从原来 6 个手写接口变为 3 个通用工具自动适配；Web API 新增 `GET /api/matter/descriptor` 返回完整设备树 JSON。

---

## 开发环境

- **ESP-IDF**：v5.5.1
- **LVGL**：v8.4.0
- **目标芯片**：esp32p4

## 快速开始

```bash
# 配置项目
idf.py set-target esp32p4
idf.py menuconfig

# 编译
idf.py build

# 烧录
idf.py -p COMxxx flash

# 监视日志
idf.py -p COMxxx monitor
```

## 固件分区

| 分区 | 大小 | 说明 |
|------|------|------|
| nvs | 24KB | 非易失存储 |
| phy_init | 4KB | PHY 初始化数据 |
| model | 960KB | 语音唤醒模型 |
| factory | 8MB | 应用程序 |
| storage | 4MB | SPIFFS 存储 |

## 项目结构

```
├── main/                    # 主程序
│   ├── application.cc       # 应用主逻辑
│   ├── boards/              # 板级支持包
│   ├── display/             # 显示/LVGL 相关
│   ├── audio/               # 音频服务
│   ├── camera/              # 相机显示
│   ├── sensors/             # 传感器驱动
│   ├── posix_compat/        # POSIX 标准化层
│   ├── mem_protect/         # 内存隔离保护
│   ├── matter_device/       # 类Matter设备模型
│   └── assets/              # UI 资源文件
├── components/              # 本地组件
├── managed_components/      # 托管组件 (ESP-Registry)
└── partitions-16MiB.csv     # 分区表
```

## 注意事项

- 仅支持慧勤智远 WKS 7 寸 1024×600 MIPI 屏
- 需外接 PSRAM（ESP32-P4 必备）
