/**
 * @file smart_home_matter_init.h
 * @brief 将现有智能家居传感器/执行器映射到 Matter 设备树
 *
 * 调用 smarthome_matter_init() 后:
 *   - DeviceManager 中注册 3 个 Endpoint
 *   - Web API 可以访问 GET /api/matter/descriptor 获取设备树
 *   - MCP 可以通过 matter.read_attribute 等通用工具访问所有设备
 *
 * 这是"类 Matter 协议架构"创新的核心集成点。
 */
#pragma once

#include "matter_device_mgr.h"
#include <memory>

#ifdef __cplusplus
extern "C" {
#endif

// 前向声明 — 这些变量定义在各自的 .c/.cc 文件中
extern volatile float dht22_temperature;
extern volatile float dht22_humidity;
extern volatile uint8_t dht22_valid;
extern volatile float bh1750_lux;
extern volatile uint8_t bh1750_valid;

#ifdef __cplusplus
}
#endif

// 智能家居状态（原 extra_screens.cc 中的 static 变量，提取到这里供 Matter 访问）
namespace smart_home_state {
    extern bool fan_on;
    extern bool light_on;
    extern bool auto_mode;
    void set_fan_hw(bool on);
    void set_light_hw(bool on);
} // namespace smart_home_state

/**
 * @brief 初始化 Matter 设备树，将现有传感器/执行器映射到标准 Matter 模型
 *
 * 在 main() 或 Application 初始化阶段调用一次。
 */
static inline void smarthome_matter_init() {
    using namespace matter;
    using namespace smart_home_state;

    auto &dm = DeviceManager::GetInstance();

    // ============================================================
    // Endpoint 1: 本地环境传感器 (DeviceType: 0x0302 = Temperature Sensor)
    // ============================================================
    {
        auto ep = std::make_unique<Endpoint>("本地环境传感器", 1, 0x0302);

        // Cluster: Temperature Measurement (0x0402)
        {
            auto cl = std::make_unique<Cluster>("Temperature Measurement", 0x0402);
            cl->AddAttribute(std::make_unique<Attribute>(
                "MeasuredValue", 0x0000, AttrType::FLOAT, "R*",
                AttrBinding::FloatReader([]() -> float { return dht22_temperature; })
            ));
            ep->AddCluster(std::move(cl));
        }

        // Cluster: RelativeHumidity Measurement (0x0405)
        {
            auto cl = std::make_unique<Cluster>("RelativeHumidity Measurement", 0x0405);
            cl->AddAttribute(std::make_unique<Attribute>(
                "MeasuredValue", 0x0000, AttrType::FLOAT, "R*",
                AttrBinding::FloatReader([]() -> float { return dht22_humidity; })
            ));
            ep->AddCluster(std::move(cl));
        }

        // Cluster: Illuminance Measurement (0x0400)
        {
            auto cl = std::make_unique<Cluster>("Illuminance Measurement", 0x0400);
            cl->AddAttribute(std::make_unique<Attribute>(
                "MeasuredValue", 0x0000, AttrType::FLOAT, "R*",
                AttrBinding::FloatReader([]() -> float { return bh1750_lux; })
            ));
            ep->AddCluster(std::move(cl));
        }

        dm.AddEndpoint(std::move(ep));
    }

    // ============================================================
    // Endpoint 2: 本地执行器 (DeviceType: 0x0101 = Dimmable Light)
    // ============================================================
    {
        auto ep = std::make_unique<Endpoint>("本地执行器", 2, 0x0101);

        // Cluster: Fan Control (0x0202)
        {
            auto cl = std::make_unique<Cluster>("Fan Control", 0x0202);
            cl->AddAttribute(std::make_unique<Attribute>(
                "SpeedPercent", 0x0000, AttrType::UINT8, "RW",
                AttrBinding::IntReader([]() -> int32_t { return fan_on ? 100 : 0; }),
                [](const AttrValue &v) -> bool {
                    int speed = std::get<uint8_t>(v);
                    set_fan_hw(speed > 0);
                    fan_on = (speed > 0);
                    return true;
                }
            ));
            cl->AddCommand("SetSpeed", 0x00, [](const AttrValue &arg) -> bool {
                int speed = std::get<uint8_t>(arg);
                set_fan_hw(speed > 0);
                fan_on = (speed > 0);
                return true;
            });
            ep->AddCluster(std::move(cl));
        }

        // Cluster: OnOff (0x0006) — 用于灯光控制
        {
            auto cl = std::make_unique<Cluster>("OnOff", 0x0006);
            cl->AddAttribute(std::make_unique<Attribute>(
                "OnOff", 0x0000, AttrType::BOOL, "RW",
                AttrBinding::BoolReader([]() -> bool { return light_on; }),
                [](const AttrValue &v) -> bool {
                    bool on = std::get<bool>(v);
                    set_light_hw(on);
                    light_on = on;
                    return true;
                }
            ));
            cl->AddCommand("Toggle", 0x02, [](const AttrValue &) -> bool {
                bool new_state = !light_on;
                set_light_hw(new_state);
                light_on = new_state;
                return true;
            });
            ep->AddCluster(std::move(cl));
        }

        dm.AddEndpoint(std::move(ep));
    }

    // ============================================================
    // Endpoint 3: 远程传感器 Room2 (DeviceType: 0x0302)
    //   数据来自 ESP32-C3，通过 POST /api/room2/sensors 上报
    // ============================================================
    {
        // 需要从 smart_home_web_server.cc 获取 room2 数据
        // 通过 smart_home_get_room2_sensors() 回调
        extern void smart_home_get_room2_sensors(float *t, float *h, float *l, int64_t *ts);

        auto ep = std::make_unique<Endpoint>("Room2 远程传感器", 3, 0x0302);

        {
            auto cl = std::make_unique<Cluster>("Temperature Measurement", 0x0402);
            cl->AddAttribute(std::make_unique<Attribute>(
                "MeasuredValue", 0x0000, AttrType::FLOAT, "R*",
                AttrBinding::FloatReader([]() -> float {
                    float t, h, l; int64_t ts;
                    smart_home_get_room2_sensors(&t, &h, &l, &ts);
                    return t;
                })
            ));
            ep->AddCluster(std::move(cl));
        }

        {
            auto cl = std::make_unique<Cluster>("RelativeHumidity Measurement", 0x0405);
            cl->AddAttribute(std::make_unique<Attribute>(
                "MeasuredValue", 0x0000, AttrType::FLOAT, "R*",
                AttrBinding::FloatReader([]() -> float {
                    float t, h, l; int64_t ts;
                    smart_home_get_room2_sensors(&t, &h, &l, &ts);
                    return h;
                })
            ));
            ep->AddCluster(std::move(cl));
        }

        {
            auto cl = std::make_unique<Cluster>("Illuminance Measurement", 0x0400);
            cl->AddAttribute(std::make_unique<Attribute>(
                "MeasuredValue", 0x0000, AttrType::FLOAT, "R*",
                AttrBinding::FloatReader([]() -> float {
                    float t, h, l; int64_t ts;
                    smart_home_get_room2_sensors(&t, &h, &l, &ts);
                    return l;
                })
            ));
            ep->AddCluster(std::move(cl));
        }

        dm.AddEndpoint(std::move(ep));
    }
}
