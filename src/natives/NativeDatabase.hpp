#pragma once

#include "natives/NativeInvoker.hpp"

#include <cstddef>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace smf::natives {

enum class NativeValueType {
    Void,
    Boolean,
    Integer,
    UnsignedInteger,
    Float,
    Double,
    Pointer,
    String,
    Vector3,
    Entity,
    Ped,
    Player,
    Vehicle,
    Object,
    Hash,
    Unknown
};

struct NativeParameterInfo {
    std::string name;
    NativeValueType type{NativeValueType::Unknown};
    bool output{};
    bool optional{};
};

struct NativeInfo {
    NativeHash hash{};
    std::string name;
    std::string nameSpace;
    std::string category;
    std::string description;
    NativeValueType returnType{NativeValueType::Void};
    std::vector<NativeParameterInfo> parameters;
    bool approvedForHighLevelSdk{};
};

class NativeDatabase final {
public:
    bool Register(NativeInfo nativeInfo, bool replaceExisting = false);
    std::size_t RegisterMany(
        std::vector<NativeInfo> nativeInfos,
        bool replaceExisting = false);

    bool Remove(NativeHash hash);
    void Clear() noexcept;

    [[nodiscard]] bool Contains(NativeHash hash) const;
    [[nodiscard]] std::optional<NativeInfo> Find(NativeHash hash) const;
    [[nodiscard]] std::optional<NativeInfo> FindByName(
        std::string_view name) const;

    [[nodiscard]] std::vector<NativeInfo> Search(
        std::string_view query,
        std::size_t maximumResults = 0) const;
    [[nodiscard]] std::vector<NativeInfo> ByCategory(
        std::string_view category) const;
    [[nodiscard]] std::vector<NativeInfo> Snapshot() const;

    [[nodiscard]] std::size_t Size() const noexcept;

private:
    [[nodiscard]] static std::string Normalize(std::string_view text);

    mutable std::mutex mutex_;
    std::unordered_map<NativeHash, NativeInfo> entries_;
    std::unordered_map<std::string, NativeHash> names_;
};

} // namespace smf::natives
