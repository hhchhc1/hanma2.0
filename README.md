# 悍马 2.0 — ESP32-P4 智能语音助手

> 慧勤智远 | ESP32-P4 + ESP32-C6 | 7 寸 MIPI 触摸屏 | LVGL | 小智 AI

## 项目简介

一款基于 ESP32-P4 双核 MCU 的综合智能面板设备，集 AI 语音助手、智能家居控制、人脸检测解锁、摄像头监控、OTA 升级于一体。

---

## 三大架构创新（复赛答辩核心）

### 🏗️ 创新一：POSIX 标准化线程模型

**对标 OpenVela (NuttX) 的 POSIX 标准分层设计理念**

将项目中全部 FreeRTOS 原生 API（xTaskCreate、xQueue、xSemaphore、xEventGroup）封装为统一的 POSIX 标准接口（pthread、消息队列、条件变量），使上层应用代码具备跨 RTOS 可移植性。

```
新增 main/posix_compat/ 模块：
├── posix_event.h   — 事件组封装（底层 FreeRTOS，ISR 线程安全）
├── posix_queue.h   — 消息队列封装
├── posix_task.h    — pthread_create 封装（含核心绑定）
├── posix_timer.h   — 定时器封装（std::thread + sleep_until）
└── posix_compat.h  — 聚合头文件
```

**改造前：**
```c
// 68 个文件散布着裸 FreeRTOS API，与 RTOS 深度绑定
xTaskCreate(func, "dht22", 4096, NULL, 5, NULL);
xEventGroupWaitBits(eg, MASK, pdTRUE, pdFALSE, portMAX_DELAY);
xQueueSend(queue, &data, portMAX_DELAY);
```

**改造后：**
```cpp
// 统一的 POSIX 风格接口，底层实现可替换
posix::task_create({"dht22", 4096, 5, -1}, func);
event_group->wait_bits(MASK, true, false, 0);
queue.send(data);
```

**答辩要点：** 未来移植到 NuttX/Zephyr 等 POSIX RTOS 时，只需重写 posix_compat 层 4 个头文件，68 个业务代码文件一行不动。

---

### 🛡️ 创新二：内存隔离保护机制

**解决 FreeRTOS 扁平内存模型下野指针导致整机死机的固有问题**

自研轻量级内存保护框架，通过三层防护实现"软隔离"：

```
新增 main/mem_protect/ 模块：
├── mem_pool.h/cc   — 5 域分区内存池
│   ├── DOMAIN_AUDIO    (512 KB) — 音频 PCM + Opus 编解码
│   ├── DOMAIN_DISPLAY  (256 KB) — LVGL 对象 + 图片缓存
│   ├── DOMAIN_CAMERA   (512 KB) — 帧缓冲 + 运动检测
│   ├── DOMAIN_NETWORK  (128 KB) — WebSocket + HTTP 缓冲
│   └── DOMAIN_SYSTEM   (128 KB) — 状态机 + MCP 消息
├── mem_guard.h    — 哨兵魔数检测
│   [0xDEADBEEF][size][ts] ... 数据 ... [0xBEEFDEAD]
│   释放时校验首尾魔数 → 越界写入立即报警
└── mem_pmp.h      — RISC-V PMP 代码段写保护
    硬件层面禁止写入 .text / .rodata 段
```

**改造前：** 全局 malloc/free，野指针踩了哪个模块的数据无从得知。

**改造后：**
```cpp
// 每个模块在自己的域分配
pcm = mp_malloc(DOMAIN_AUDIO, 8192);
// 跨域释放 → 被拒绝 + ESP_LOGE("DOMAIN MISMATCH")
// 越界写入 → 释放时哨兵校验失败 + 打印调用栈
// 写代码段 → CPU 硬件异常（PMP）
```

**答辩要点：** ESP32-P4 没有 MMU，但通过 RISC-V PMP + 软件哨兵 + 域分区实现了"轻量级内存隔离"，在答辩中直接对标真正的虚拟内存保护机制。

---

### 🔌 创新三：类 Matter 协议设备模型

**智能家居设备采用 Matter 1.3 标准的 Endpoint → Cluster → Attribute 三层架构**

```
新增 main/matter_device/ 模块：
├── matter_attribute.h      — 属性值（read/write/report 回调 + 类型系统）
├── matter_cluster.h        — 功能簇容器
├── matter_endpoint.h       — 逻辑设备端点
├── matter_device_mgr.h/cc  — 设备管理器（单例模式 + JSON 序列化）
├── smart_home_matter_init.h — 现有设备 → Matter 模型映射
└── matter_device.h         — 聚合头文件
```

**设备建模实例：**

```
MatterNode — 悍马 2.0 智能面板
│
├── Endpoint 1: 本地环境传感器 (DeviceType 0x0302)
│   ├── Cluster: Temperature Measurement (0x0402) → DHT22
│   ├── Cluster: Humidity Measurement    (0x0405) → DHT22
│   └── Cluster: Light Measurement       (0x0400) → BH1750
│
├── Endpoint 2: 本地执行器 (DeviceType 0x0101)
│   ├── Cluster: Fan Control (0x0202) → GPIO36/7 风扇
│   │   ├── Attribute: SpeedPercent  → fan_on ? 100 : 0
│   │   └── Command:   SetSpeed      → set_fan_hw()
│   └── Cluster: OnOff       (0x0006) → GPIO12 灯泡
│       ├── Attribute: OnOff → light_on
│       └── Command:   Toggle → !light_on
│
└── Endpoint 3: Room2 远程传感器 (DeviceType 0x0302)
    └── ...（来自 ESP32-C3 的温湿度 / 光照）
```

**改造前：** 手写 6 个独立 MCP 工具 + 7 个独立 Web API 端点，加新设备要同步改 3 处。

**改造后：**
- **MCP 自动生成：** 遍历设备树 → 3 个通用工具（`matter.read_attribute` / `matter.write_attribute` / `matter.invoke_command`）
- **Web 自动生成：** `GET /api/matter/descriptor` 返回完整设备树 JSON，前端自动渲染控制面板
- **未来扩展：** 接入真实 Matter 网络（Thread / WiFi）只需替换底层通信，上层模型完全复用

---

## 硬件平台

| 组件 | 型号 |
|------|------|
| 主控 | ESP32-P4 双核 400MHz RISC-V |
| WiFi | ESP32-C6（SDIO 协处理器） |
| 屏幕 | 7 寸 1024×600 MIPI DSI（EK79007 驱动） |
| 触摸 | GT911 电容触摸（I2C） |
| 摄像头 | OV5647 MIPI CSI |
| 音频 | ES8311 编解码器（I2S 全双工 24kHz） |
| 传感器 | DHT22 温湿度 + BH1750 光照 |
| 存储 | 16MB Flash + 32MB PSRAM + SD 卡 |

## 软件架构

```
ESP-IDF v5.5.1 (FreeRTOS)
    │
    ├── Application (C++ 状态机 + 事件循环)
    │   ├── AudioService → ES8311 + Opus + AFE 唤醒词
    │   ├── Protocol     → MQTT / WebSocket（云端通信）
    │   ├── Ota          → HTTPS 固件升级
    │   ├── McpServer    → AI 工具调用
    │   └── SmartHome    → Matter 设备模型 + Web 管理
    │
    ├── Board (硬件抽象层)
    │   ├── Display      → MIPI DSI + LVGL
    │   ├── Camera       → MIPI CSI + PPA 缩放
    │   ├── Backlight    → PWM 调光
    │   └── Network      → SDIO WiFi (C6)
    │
    ├── posix_compat/    ← 创新一：POSIX 标准化层
    ├── mem_protect/     ← 创新二：内存隔离保护
    └── matter_device/   ← 创新三：Matter 设备模型
```

## 快速开始

```bash
# 配置 ESP-IDF 环境
. /path/to/esp-idf/export.sh

# 编译烧录
idf.py build flash monitor

# 连接设备热点
WiFi: Xiaozhi-SmartHome
IP:   192.168.4.1
```

## 项目结构

```
main/
├── application.cc/h         # 应用层状态机
├── audio/                   # 音频子系统（I2S + Opus + AFE）
├── display/                 # 显示子系统（LVGL + MIPI DSI）
├── camera/                  # 摄像头子系统（V4L2 + MIPI CSI）
├── protocols/               # 云端通信协议（MQTT / WebSocket）
├── boards/                  # 板级支持包（wks-p4-cb）
├── sensors/                 # 传感器驱动（DHT22 / BH1750）
├── APP/                     # LVGL 应用（桌面 / 智能家居 / AI 聊天 / 相机）
│
├── posix_compat/            ← 创新一
├── mem_protect/             ← 创新二
└── matter_device/           ← 创新三
```

## 版本历史

### v2.0 — 2026-07-17

- ✨ POSIX 标准化线程模型（对标 OpenVela）
- ✨ 内存隔离保护机制（域分区 + 哨兵 + PMP）
- ✨ 类 Matter 协议设备模型（三层架构）
- 🐛 修复：风扇上电误转、灯泡不亮、摄像头延迟、人脸绿框
- 🐛 修复：音乐/视频退出死锁、auto_mode 错误提示
- 🔧 优化：Web 摄像头降采样 + 预分配缓冲（延迟 ~20ms）

### v1.0 — 2026-07

- 🎉 初始版本：ESP32-P4 智能语音助手完整实现
