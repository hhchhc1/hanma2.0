/**
 * @file matter_cluster.h
 * @brief Matter Cluster — 功能域容器，容纳多个 Attribute 和 Command
 *
 * Cluster 是 Matter 协议中仅次于 Endpoint 的二级结构。
 * 每个 Cluster 代表一个独立的功能域（如温度测量、风扇控制）。
 */
#pragma once

#include "matter_attribute.h"
#include <vector>
#include <string>
#include <memory>

namespace matter {

class Cluster {
public:
    using CommandFunc = std::function<bool(const AttrValue &arg)>;

    /**
     * @param name         簇名称 (如 "Temperature Measurement")
     * @param cluster_id   标准 Cluster ID (如 0x0402)
     * @param description  功能描述
     */
    Cluster(const char *name, uint16_t cluster_id, const char *description = "")
        : name_(name), cluster_id_(cluster_id), description_(description) {}

    const std::string &name() const { return name_; }
    uint16_t id() const { return cluster_id_; }

    /**
     * @brief 添加属性到此 Cluster
     */
    Attribute &AddAttribute(std::unique_ptr<Attribute> attr) {
        attributes_.push_back(std::move(attr));
        return *attributes_.back();
    }

    /**
     * @brief 添加命令到此 Cluster
     * @param name        命令名 (如 "SetSpeed")
     * @param cmd_id      命令 ID
     * @param handler     命令处理函数，返回 true 表示成功
     */
    void AddCommand(const char *name, uint8_t cmd_id, CommandFunc handler) {
        commands_.push_back({name, cmd_id, std::move(handler)});
    }

    /**
     * @brief 按名称查找属性 (线程安全 — 返回的指针可安全调用 read/write)
     */
    Attribute *FindAttribute(const char *name) {
        for (auto &a : attributes_) {
            if (a->name() == name) return a.get();
        }
        return nullptr;
    }

    /**
     * @brief 按 ID 查找属性
     */
    Attribute *FindAttributeById(uint16_t id) {
        for (auto &a : attributes_) {
            if (a->id() == id) return a.get();
        }
        return nullptr;
    }

    /**
     * @brief 执行命令
     * @param name  命令名称
     * @param arg   命令参数
     * @return 命令执行成功返回 true
     */
    bool InvokeCommand(const char *name, const AttrValue &arg) {
        for (auto &c : commands_) {
            if (c.name == name && c.handler) {
                return c.handler(arg);
            }
        }
        return false;
    }

    /**
     * @brief 获取所有属性 (只读遍历)
     */
    const std::vector<std::unique_ptr<Attribute>> &attributes() const {
        return attributes_;
    }

    /** 命令定义 */
    struct CommandDef {
        std::string name;
        uint8_t id;
        CommandFunc handler;
    };

    /**
     * @brief 获取所有命令
     */
    const std::vector<CommandDef> &commands() const {
        return commands_;
    }

    /** 转为 JSON 片段 */
    std::string to_json() const;

private:

    std::string name_;
    uint16_t cluster_id_;
    std::string description_;
    std::vector<std::unique_ptr<Attribute>> attributes_;
    std::vector<CommandDef> commands_;
};

} // namespace matter
