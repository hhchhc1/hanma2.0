# 悍马 2.0 — 慧勤智远 ESP32-P4 智能语音助手

基于 **ESP32-P4** 开发板的智能语音助手，支持 **7 寸 MIPI 触摸屏**显示、**WiFi/蓝牙** 无线通信、**摄像头**人脸检测、**语音唤醒与控制**等丰富功能。

## 硬件平台

- **主控**：慧勤智远 ESP32-P4 + ESP32-C6 (SDIO WiFi)
- **屏幕**：7 寸 1024×600 MIPI 触摸屏 (ST7703)
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
│   └── assets/              # UI 资源文件
├── components/              # 本地组件
├── managed_components/      # 托管组件 (ESP-Registry)
└── partitions-16MiB.csv    # 分区表
```

## 注意事项

- 仅支持慧勤智远 WKS 7 寸 1024×600 MIPI 屏
- 需外接 PSRAM（ESP32-P4 必备）
