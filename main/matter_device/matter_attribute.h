/**
 * @file matter_attribute.h
 * @brief Matter Attribute — 设备树的最小数据单元
 *
 * 参考 Matter 1.3 规范: 每个 Attribute 绑定 read/write/report 三个回调，
 * 类型检查在框架层完成，保证数据安全。
 */
#pragma once

#include <functional>
#include <string>
#include <variant>
#include <mutex>

namespace matter {

// ============================================================
// 数据类型
// ============================================================
enum class AttrType : uint8_t {
    BOOL    = 0,
    INT8    = 1,
    INT16   = 2,
    INT32   = 3,
    UINT8   = 4,
    UINT16  = 5,
    UINT32  = 6,
    FLOAT   = 7,
    STRING  = 8,
};

using AttrValue = std::variant<
    bool,
    int8_t, int16_t, int32_t,
    uint8_t, uint16_t, uint32_t,
    float,
    std::string
>;

// ============================================================
// Attribute 定义
// ============================================================
class Attribute {
public:
    using ReadFunc   = std::function<AttrValue()>;
    using WriteFunc  = std::function<bool(const AttrValue &new_val)>;
    using ReportFunc = std::function<void(const AttrValue &old_val, const AttrValue &new_val)>;

    /**
     * @param name        属性名称 (如 "MeasuredValue")
     * @param attr_id     属性 ID (如 0x0000)
     * @param type        数据类型
     * @param access      访问权限: "R"=只读, "RW"=读写, "R*"=只读+上报
     * @param on_read     读取回调 (必须)
     * @param on_write    写入回调 (access 含 "W" 时必须)
     * @param on_report   上报回调 (access 含 "*" 时必须)
     */
    Attribute(const char *name, uint16_t attr_id, AttrType type, const char *access,
              ReadFunc on_read, WriteFunc on_write = nullptr, ReportFunc on_report = nullptr)
        : name_(name), attr_id_(attr_id), type_(type), access_(access),
          on_read_(std::move(on_read)), on_write_(std::move(on_write)),
          on_report_(std::move(on_report)) {}

    const std::string &name() const { return name_; }
    uint16_t id() const { return attr_id_; }
    AttrType type() const { return type_; }
    const std::string &access() const { return access_; }

    /** 读取当前值 (线程安全) */
    AttrValue read() {
        std::lock_guard<std::mutex> lock(mutex_);
        return on_read_ ? on_read_() : AttrValue{};
    }

    /** 写入新值。access 为只读时拒绝写入 */
    bool write(const AttrValue &new_val) {
        if (access_.find('W') == std::string::npos) return false;
        std::lock_guard<std::mutex> lock(mutex_);
        AttrValue old = on_read_ ? on_read_() : AttrValue{};
        if (!on_write_ || !on_write_(new_val)) return false;
        if (on_report_) {
            on_report_(old, new_val);
        }
        return true;
    }

    /** 转为 JSON 片段 (用于 API 序列化) */
    std::string to_json() const;

    /** 数据类型名称 */
    static const char *type_name(AttrType t);

private:
    std::string name_;
    uint16_t attr_id_;
    AttrType type_;
    std::string access_;
    ReadFunc on_read_;
    WriteFunc on_write_;
    ReportFunc on_report_;
    mutable std::mutex mutex_;
};

// ---- 辅助: 便捷创建带类型约束的回调 ----

/**
 * @brief 创建绑定到 C 变量的 Float 读取回调
 *
 * 用法:
 *   auto cb = AttrBinding::FloatReader([]() -> float { return dht22_temperature; });
 */
namespace AttrBinding {
    inline Attribute::ReadFunc FloatReader(std::function<float()> fn) {
        return [fn = std::move(fn)]() -> AttrValue { return fn(); };
    }
    inline Attribute::ReadFunc IntReader(std::function<int32_t()> fn) {
        return [fn = std::move(fn)]() -> AttrValue { return static_cast<int32_t>(fn()); };
    }
    inline Attribute::ReadFunc BoolReader(std::function<bool()> fn) {
        return [fn = std::move(fn)]() -> AttrValue { return fn(); };
    }
} // namespace AttrBinding

} // namespace matter
