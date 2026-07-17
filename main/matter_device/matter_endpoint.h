/**
 * @file matter_endpoint.h
 * @brief Matter Endpoint — 代表一个逻辑设备
 *
 * Endpoint 是 Matter 协议中的一级结构。每个 Endpoint 代表一个独立的物理或逻辑设备，
 * 包含多个 Cluster。例如 "本地环境传感器" 是一个 Endpoint。
 */
#pragma once

#include "matter_cluster.h"
#include <memory>
#include <vector>

namespace matter {

class Endpoint {
public:
    /**
     * @param name         端点名 (如 "本地环境传感器")
     * @param endpoint_id  端点 ID (1~255)
     * @param device_type  Matter 设备类型 ID (如 0x0302 = Temperature Sensor)
     */
    Endpoint(const char *name, uint8_t endpoint_id, uint16_t device_type)
        : name_(name), endpoint_id_(endpoint_id), device_type_(device_type) {}

    const std::string &name() const { return name_; }
    uint8_t id() const { return endpoint_id_; }
    uint16_t device_type() const { return device_type_; }

    /**
     * @brief 添加一个 Cluster 到此 Endpoint
     */
    Cluster &AddCluster(std::unique_ptr<Cluster> cluster) {
        clusters_.push_back(std::move(cluster));
        return *clusters_.back();
    }

    /**
     * @brief 按名称查找 Cluster
     */
    Cluster *FindCluster(const char *name) {
        for (auto &c : clusters_) {
            if (c->name() == name) return c.get();
        }
        return nullptr;
    }

    /**
     * @brief 按 ID 查找 Cluster
     */
    Cluster *FindClusterById(uint16_t id) {
        for (auto &c : clusters_) {
            if (c->id() == id) return c.get();
        }
        return nullptr;
    }

    /**
     * @brief 便捷方法: 读取某个 Cluster 下的 Attribute 值
     * @param cluster_name   Cluster 名称
     * @param attr_name      Attribute 名称
     * @return 成功返回属性值，失败返回空 variant
     */
    AttrValue ReadAttribute(const char *cluster_name, const char *attr_name) {
        Cluster *c = FindCluster(cluster_name);
        if (!c) return AttrValue{};
        Attribute *a = c->FindAttribute(attr_name);
        if (!a) return AttrValue{};
        return a->read();
    }

    /**
     * @brief 便捷方法: 写入某个 Cluster 下的 Attribute
     */
    bool WriteAttribute(const char *cluster_name, const char *attr_name, const AttrValue &val) {
        Cluster *c = FindCluster(cluster_name);
        if (!c) return false;
        Attribute *a = c->FindAttribute(attr_name);
        if (!a) return false;
        return a->write(val);
    }

    const std::vector<std::unique_ptr<Cluster>> &clusters() const {
        return clusters_;
    }

    /** 转为 JSON 片段 */
    std::string to_json() const;

private:
    std::string name_;
    uint8_t endpoint_id_;
    uint16_t device_type_;
    std::vector<std::unique_ptr<Cluster>> clusters_;
};

} // namespace matter
