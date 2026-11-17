#include "RaceTracking.hpp"
#include "VerifiedBuild.hpp"
#include <RED4ext/GameEngine.hpp>
#include <array>
#include <atomic>
#include <cstring>

// Player mappin tracking for the race mode script.
//
// Scripts can move and register mappins but cannot make one the player's
// tracked mappin; only the world map does that natively. In the verified 2.5
// executable, WorldMapMenuGameController.TrackMappin (native 0x2d5913c) reads the
// NewMappinID at IMappin+0x40 and calls slot +0x220 of the mappin system vtable
// (0x36aad48). That slot is 0x2d449bc: it locks the system, stores the ID at
// +0x1b8 as the tracked mappin and calls 0x2d426a8, which untracks the previous
// mappin and tracks the new one. The GPS route follows the tracked mappin.
namespace {
constexpr uint32_t MappinSystemVtableRva = 0x36aad48;
constexpr uint32_t TrackMappinSlot = 0x220;
constexpr uint32_t TrackMappinRva = 0x2d449bc;
constexpr std::array<unsigned char, 32> TrackMappinBytes = {
    0x48, 0x89, 0x5c, 0x24, 0x10, 0x48, 0x89, 0x74, 0x24, 0x18, 0x57, 0x48, 0x83, 0xec, 0x20, 0x48,
    0x8b, 0xda, 0x48, 0x8b, 0xf1, 0x48, 0x8d, 0x91, 0x98, 0x01, 0x00, 0x00, 0x48, 0x8d, 0x4c, 0x24};

RED4ext::v1::PluginHandle pluginHandle{};
const RED4ext::v1::Sdk* pluginSdk = nullptr;
std::atomic<uint32_t> calls{0}, refused{0};

bool BuildMatches(const unsigned char* base)
{
    if (!base) return false;
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0 || dos->e_lfanew > 4096) return false;
    const auto* pe = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
    if (pe->Signature != IMAGE_NT_SIGNATURE || pe->FileHeader.TimeDateStamp != DncBuild::Timestamp ||
        pe->OptionalHeader.SizeOfImage != DncBuild::ImageSize) return false;
    if (std::memcmp(base + TrackMappinRva, TrackMappinBytes.data(), TrackMappinBytes.size())) return false;
    uintptr_t slot = 0;
    std::memcpy(&slot, base + MappinSystemVtableRva + TrackMappinSlot, sizeof(slot));
    return slot == reinterpret_cast<uintptr_t>(base + TrackMappinRva);
}

void* MappinSystem()
{
    auto* engine = RED4ext::CGameEngine::Get();
    if (!engine || !engine->framework || !engine->framework->gameInstance) return nullptr;
    auto* type = RED4ext::CRTTISystem::Get()->GetClass("gamemappinsMappinSystem");
    if (!type) return nullptr;
    return engine->framework->gameInstance->GetSystem(type);
}

void TrackMappin(RED4ext::IScriptable*, RED4ext::CStackFrame* frame, bool* out, int64_t)
{
    uint64_t id = 0;
    RED4ext::GetParameter(frame, &id);
    frame->code++; // ParamEnd
    bool ok = false;
    const auto* base = reinterpret_cast<const unsigned char*>(GetModuleHandleW(nullptr));
    if (id != 0 && BuildMatches(base)) {
        if (auto* system = MappinSystem()) {
            uintptr_t vtable = 0;
            std::memcpy(&vtable, system, sizeof(vtable));
            if (vtable == reinterpret_cast<uintptr_t>(base + MappinSystemVtableRva)) {
                using Track = void (*)(void*, uint64_t);
                reinterpret_cast<Track>(const_cast<unsigned char*>(base) + TrackMappinRva)(system, id);
                ok = true;
            }
        }
    }
    const auto n = ok ? ++calls : ++refused;
    if (pluginSdk && n <= 3)
        pluginSdk->logger->InfoF(pluginHandle, "DNC_RACE_TRACK id=%llu ok=%u", static_cast<unsigned long long>(id), unsigned(ok));
    if (out) *out = ok;
}

void RED4EXT_CALL PostRegister()
{
    auto* rtti = RED4ext::CRTTISystem::Get();
    auto* func = RED4ext::CGlobalFunction::Create("DriveNoCollision_TrackMappin", "DriveNoCollision_TrackMappin", &TrackMappin);
    func->flags = {.isNative = true, .isStatic = true};
    func->AddParam("Uint64", "id");
    func->SetReturnType("Bool");
    rtti->RegisterFunction(func);
}
}

void RaceTracking::Register(RED4ext::v1::PluginHandle handle, const RED4ext::v1::Sdk* sdk)
{
    pluginHandle = handle;
    pluginSdk = sdk;
    RED4ext::CRTTISystem::Get()->AddPostRegisterCallback(&PostRegister);
}
