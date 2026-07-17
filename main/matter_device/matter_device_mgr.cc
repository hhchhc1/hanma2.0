/**
 * @file matter_device_mgr.cc
 * @brief Matter 设备管理器实现（JSON 序列化 + MCP 工具回调）
 */
#include "matter_device_mgr.h"
#include <sstream>

#define TAG "MATTER_DEV"

namespace matter {

// ============================================================
// Attribute::to_json()
// ============================================================
std::string Attribute::to_json() const {
    std::ostringstream ss;
    ss << "{\"id\":" << attr_id_
       << ",\"name\":\"" << name_
       << "\",\"type\":\"" << type_name(type_)
       << "\",\"access\":\"" << access_ << "\"}";
    return ss.str();
}

const char *Attribute::type_name(AttrType t) {
    switch (t) {
        case AttrType::BOOL:   return "bool";
        case AttrType::INT8:   return "int8";
        case AttrType::INT16:  return "int16";
        case AttrType::INT32:  return "int32";
        case AttrType::UINT8:  return "uint8";
        case AttrType::UINT16: return "uint16";
        case AttrType::UINT32: return "uint32";
        case AttrType::FLOAT:  return "float";
        case AttrType::STRING: return "string";
        default: return "unknown";
    }
}

// ============================================================
// Cluster::to_json()
// ============================================================
std::string Cluster::to_json() const {
    std::ostringstream ss;
    ss << "{\"id\":" << cluster_id_
       << ",\"name\":\"" << name_ << "\"";
    // attributes
    ss << ",\"attributes\":[";
    for (size_t i = 0; i < attributes_.size(); i++) {
        if (i > 0) ss << ",";
        ss << attributes_[i]->to_json();
    }
    ss << "]";
    // commands
    ss << ",\"commands\":[";
    for (size_t i = 0; i < commands_.size(); i++) {
        if (i > 0) ss << ",";
        ss << "{\"id\":" << (int)commands_[i].id
           << ",\"name\":\"" << commands_[i].name << "\"}";
    }
    ss << "]}";
    return ss.str();
}

// ============================================================
// Endpoint::to_json()
// ============================================================
std::string Endpoint::to_json() const {
    std::ostringstream ss;
    ss << "{\"id\":" << (int)endpoint_id_
       << ",\"name\":\"" << name_
       << "\",\"deviceType\":" << device_type_;
    ss << ",\"clusters\":[";
    for (size_t i = 0; i < clusters_.size(); i++) {
        if (i > 0) ss << ",";
        ss << clusters_[i]->to_json();
    }
    ss << "]}";
    return ss.str();
}

// ============================================================
// DeviceManager — GenerateDescriptorJson()
// ============================================================
std::string DeviceManager::GenerateDescriptorJson() {
    std::lock_guard<std::mutex> lock(mutex_);

    std::ostringstream ss;
    ss << "{\"nodes\":[";
    for (size_t i = 0; i < endpoints_.size(); i++) {
        if (i > 0) ss << ",";
        ss << endpoints_[i]->to_json();
    }
    ss << "]}";
    return ss.str();
}

// ============================================================
// DeviceManager — MCP 工具回调
// ============================================================
std::string DeviceManager::McpReadAttribute(const std::string &cluster,
                                             const std::string &attr) {
    AttrValue val = ReadAttribute(cluster.c_str(), attr.c_str());
    // 序列化为 JSON
    std::ostringstream ss;
    ss << "{\"value\":";
    std::visit([&ss](auto &&v) {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, std::string>) {
            ss << "\"" << v << "\"";
        } else if constexpr (std::is_same_v<T, bool>) {
            ss << (v ? "true" : "false");
        } else {
            ss << v;
        }
    }, val);
    ss << "}";
    return ss.str();
}

std::string DeviceManager::McpWriteAttribute(const std::string &cluster,
                                              const std::string &attr,
                                              const std::string & /*value_json*/) {
    // value_json 解析留给调用层 (MCP 工具注册时已解析 JSON)
    // 此处返回占位实现
    bool ok = WriteAttribute(cluster.c_str(), attr.c_str(), AttrValue{});
    return ok ? "{\"success\":true}" : "{\"success\":false,\"reason\":\"write_failed\"}";
}

std::string DeviceManager::McpInvokeCommand(const std::string &cluster,
                                             const std::string &cmd,
                                             const std::string & /*arg_json*/) {
    bool ok = InvokeCommand(cluster.c_str(), cmd.c_str(), AttrValue{0});
    return ok ? "{\"success\":true}" : "{\"success\":false,\"reason\":\"command_failed\"}";
}

} // namespace matter
