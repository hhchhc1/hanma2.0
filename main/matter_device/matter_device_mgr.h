/**
 * @file matter_device_mgr.h
 * @brief Matter 设备管理器 — 单例，管理所有 Endpoint，对外提供统一 API
 *
 * 核心职责:
 *   1. 注册所有 Endpoint（启动时一次性构建设备树）
 *   2. 提供全局查询: FindAttribute("温度", "MeasuredValue")
 *   3. 生成设备描述符 JSON（供 Web UI 和 MCP 自动消费）
 *   4. MCP 工具自动注册（通用 read/write/invoke）
 *
 * 用三层 Matter 模型重构智能家居:
 *   原代码: 手写 6 个 MCP 工具 + 手写 Web API
 *   现在:   自动遍历设备树 → 3 个通用 MCP 工具 + 1 个通用 Web API
 */
#pragma once

#include "matter_endpoint.h"
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace matter {

class DeviceManager {
public:
    static DeviceManager &GetInstance() {
        static DeviceManager instance;
        return instance;
    }

    /**
     * @brief 添加 Endpoint 到设备树
     */
    void AddEndpoint(std::unique_ptr<Endpoint> ep) {
        std::lock_guard<std::mutex> lock(mutex_);
        endpoints_.push_back(std::move(ep));
    }

    /**
     * @brief 查找 Endpoint
     */
    Endpoint *FindEndpoint(uint8_t id) {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto &ep : endpoints_) {
            if (ep->id() == id) return ep.get();
        }
        return nullptr;
    }

    /**
     * @brief 全局属性查询 (全设备树搜索)
     *
     * @param cluster_name  Cluster 名 (如 "Temperature Measurement")
     * @param attr_name     Attribute 名 (如 "MeasuredValue")
     * @param ep_out        [输出] 找到属性所在的 Endpoint (可空)
     * @return 找到的属性指针，未找到返回 nullptr
     */
    Attribute *FindAttribute(const char *cluster_name, const char *attr_name,
                             Endpoint **ep_out = nullptr) {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto &ep : endpoints_) {
            for (auto &c : ep->clusters()) {
                if (c->name() == cluster_name) {
                    Attribute *a = c->FindAttribute(attr_name);
                    if (a) {
                        if (ep_out) *ep_out = ep.get();
                        return a;
                    }
                }
            }
        }
        return nullptr;
    }

    /**
     * @brief 全局属性读取
     *
     * 用法:
     *   AttrValue temp = DeviceManager::GetInstance().ReadAttribute(
     *       "Temperature Measurement", "MeasuredValue");
     */
    AttrValue ReadAttribute(const char *cluster, const char *attr) {
        Attribute *a = FindAttribute(cluster, attr);
        return a ? a->read() : AttrValue{};
    }

    /**
     * @brief 全局属性写入
     *
     * 用法:
     *   DeviceManager::GetInstance().WriteAttribute(
     *       "OnOff", "OnOff", AttrValue{true});
     */
    bool WriteAttribute(const char *cluster, const char *attr, const AttrValue &val) {
        Attribute *a = FindAttribute(cluster, attr);
        return a ? a->write(val) : false;
    }

    /**
     * @brief 全局命令调用
     *
     * 用法:
     *   DeviceManager::GetInstance().InvokeCommand(
     *       "Fan Control", "SetSpeed", AttrValue{static_cast<uint8_t>(50)});
     */
    bool InvokeCommand(const char *cluster_name, const char *cmd_name, const AttrValue &arg) {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto &ep : endpoints_) {
            for (auto &c : ep->clusters()) {
                if (c->name() == cluster_name) {
                    return c->InvokeCommand(cmd_name, arg);
                }
            }
        }
        return false;
    }

    /**
     * @brief 生成 Matter 设备描述符 JSON
     *
     * 供 Web UI (GET /api/matter/descriptor) 和 MCP 消费。
     *
     * 返回格式:
     * {
     *   "nodes": [{
     *     "id": 1,
     *     "name": "本地环境传感器",
     *     "deviceType": 770,   // 0x0302
     *     "clusters": [{
     *       "id": 1026,        // 0x0402
     *       "name": "Temperature Measurement",
     *       "attributes": [{ "id": 0, "name": "MeasuredValue", "type": "float", "access": "R" }],
     *       "commands": [{ "id": 0, "name": "Identify" }]
     *     }]
     *   }]
     * }
     */
    std::string GenerateDescriptorJson();

    /**
     * @brief 获取所有 Endpoint (只读遍历)
     */
    const std::vector<std::unique_ptr<Endpoint>> &endpoints() const {
        // 注意：此方法非线程安全，调用方需自行加锁
        return endpoints_;
    }

    // ============================================================
    // MCP 工具回调 — 这些函数可以被 McpServer 注册为通用工具
    // ============================================================

    /**
     * @brief MCP 工具: matter.read_attribute
     *
     * JSON 参数: {"cluster": "Temperature Measurement", "attribute": "MeasuredValue"}
     * JSON 返回: {"value": 25.5}
     */
    std::string McpReadAttribute(const std::string &cluster, const std::string &attr);

    /**
     * @brief MCP 工具: matter.write_attribute
     *
     * JSON 参数: {"cluster": "OnOff", "attribute": "OnOff", "value": true}
     * JSON 返回: {"success": true}
     */
    std::string McpWriteAttribute(const std::string &cluster, const std::string &attr,
                                  const std::string &value_json);

    /**
     * @brief MCP 工具: matter.invoke_command
     *
     * JSON 参数: {"cluster": "Fan Control", "command": "SetSpeed", "arg": 50}
     * JSON 返回: {"success": true}
     */
    std::string McpInvokeCommand(const std::string &cluster, const std::string &cmd,
                                 const std::string &arg_json);

private:
    DeviceManager() = default;
    mutable std::mutex mutex_;
    std::vector<std::unique_ptr<Endpoint>> endpoints_;
};

} // namespace matter
