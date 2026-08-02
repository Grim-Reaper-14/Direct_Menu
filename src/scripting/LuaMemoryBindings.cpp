#include "scripting/LuaMemoryBindings.hpp"

#include "core/MemoryManagerAPI.hpp"
#include "core/SignatureManager.hpp"

#include <cstdint>
#include <string>

namespace smf::scripting {
namespace {

template <typename T>
sol::object ReadRemote(
    sol::this_state state,
    core::MemoryManagerAPI& memory,
    const std::uintptr_t address) {
    T value{};
    std::size_t bytesRead = 0;
    if (address == 0 ||
        !memory.ReadRemoteBytes(
            address,
            &value,
            sizeof(value),
            &bytesRead) ||
        bytesRead != sizeof(value)) {
        return sol::make_object(state, sol::nil);
    }

    return sol::make_object(state, value);
}

} // namespace

void LuaMemoryBindings::Register(
    sol::state& lua,
    core::MemoryManagerAPI& memory,
    core::SignatureManager& signatures) {
    sol::table memoryTable = lua.create_named_table("Memory");

    memoryTable.set_function("is_attached", [&memory] {
        return memory.IsProcessAttached();
    });

    memoryTable.set_function("process_id", [&memory] {
        return memory.ProcessId();
    });

    memoryTable.set_function("module_base", [&memory](const std::string& moduleName) {
        const std::wstring wideName(moduleName.begin(), moduleName.end());
        return memory.ModuleBaseAddress(wideName);
    });

    memoryTable.set_function("resolve_rip", [&memory](
        const std::uintptr_t instructionAddress,
        const std::size_t displacementOffset,
        const std::size_t instructionSize) -> sol::optional<std::uintptr_t> {
        const auto address = memory.ResolveRelativeAddress(
            instructionAddress,
            displacementOffset,
            instructionSize);
        return address ? sol::optional<std::uintptr_t>{*address} : sol::nullopt;
    });

    memoryTable.set_function("read_u8", [&memory](sol::this_state state, const std::uintptr_t address) {
        return ReadRemote<std::uint8_t>(state, memory, address);
    });
    memoryTable.set_function("read_i32", [&memory](sol::this_state state, const std::uintptr_t address) {
        return ReadRemote<std::int32_t>(state, memory, address);
    });
    memoryTable.set_function("read_u32", [&memory](sol::this_state state, const std::uintptr_t address) {
        return ReadRemote<std::uint32_t>(state, memory, address);
    });
    memoryTable.set_function("read_i64", [&memory](sol::this_state state, const std::uintptr_t address) {
        return ReadRemote<std::int64_t>(state, memory, address);
    });
    memoryTable.set_function("read_u64", [&memory](sol::this_state state, const std::uintptr_t address) {
        return ReadRemote<std::uint64_t>(state, memory, address);
    });
    memoryTable.set_function("read_float", [&memory](sol::this_state state, const std::uintptr_t address) {
        return ReadRemote<float>(state, memory, address);
    });
    memoryTable.set_function("read_double", [&memory](sol::this_state state, const std::uintptr_t address) {
        return ReadRemote<double>(state, memory, address);
    });
    memoryTable.set_function("read_bool", [&memory](sol::this_state state, const std::uintptr_t address) {
        return ReadRemote<bool>(state, memory, address);
    });
    memoryTable.set_function("read_pointer", [&memory](sol::this_state state, const std::uintptr_t address) {
        return ReadRemote<std::uintptr_t>(state, memory, address);
    });

    sol::table signaturesTable = lua.create_named_table("Signatures");

    signaturesTable.set_function("resolve", [&signatures](const std::string& name)
        -> sol::optional<std::uintptr_t> {
        const auto address = signatures.Resolve(name, false);
        return address ? sol::optional<std::uintptr_t>{*address} : sol::nullopt;
    });

    signaturesTable.set_function("rescan", [&signatures](const std::string& name)
        -> sol::optional<std::uintptr_t> {
        const auto address = signatures.Resolve(name, true);
        return address ? sol::optional<std::uintptr_t>{*address} : sol::nullopt;
    });

    signaturesTable.set_function("cached", [&signatures](const std::string& name)
        -> sol::optional<std::uintptr_t> {
        const auto address = signatures.Cached(name);
        return address ? sol::optional<std::uintptr_t>{*address} : sol::nullopt;
    });

    signaturesTable.set_function("is_cached", [&signatures](const std::string& name) {
        return signatures.IsCached(name);
    });

    signaturesTable.set_function("invalidate", [&signatures](const std::string& name) {
        return signatures.Invalidate(name);
    });

    signaturesTable.set_function("invalidate_all", [&signatures] {
        signatures.InvalidateAll();
    });

    signaturesTable.set_function("count", [&signatures] {
        return signatures.DefinitionCount();
    });

    signaturesTable.set_function("resolve_all", [&signatures](
        sol::this_state state,
        const bool forceRescan) {
        sol::state_view view(state);
        sol::table results = view.create_table();

        for (const auto& result : signatures.ResolveAll(forceRescan)) {
            sol::table entry = view.create_table();
            entry["name"] = result.name;
            entry["address"] = result.address;
            entry["found"] = result.found;
            results.add(std::move(entry));
        }

        return results;
    });
}

} // namespace smf::scripting
