#include <RED4ext/RED4ext.hpp>
#include <string_view>
#include "LiveProbe.hpp"
#include "PairProbe.hpp"
#include "RaceTracking.hpp"
#include "GpsRoute.hpp"
#include <RED4ext/GameStates.hpp>
#include <chrono>
#include "UpdateProfile.hpp"

namespace {
RED4ext::v1::PluginHandle pluginHandle{};
const RED4ext::v1::Sdk* sdk = nullptr;
// The reflection dump was research output; it is no longer written at load.
constexpr bool TypeDump = false;

// Metadata inspection only. No game objects, functions or physics values are modified.
void RED4EXT_CALL InspectTypes()
{
    auto* rtti = RED4ext::CRTTISystem::Get();
    if (!rtti) {
        sdk->logger->Error(pluginHandle, "DNC_DIAGNOSTICS_FAILED: RTTI unavailable");
        return;
    }
    RED4ext::DynArray<RED4ext::rtti::IType*> nativeTypes;
    rtti->GetNativeTypes(nativeTypes);
    unsigned inspected = 0;
    for (const auto* nativeType : nativeTypes) {
        if (!nativeType) continue;
        const auto* typeName = nativeType->GetName().ToString();
        if (!typeName) continue;
        const std::string_view name(typeName);
        if (!(name.starts_with("vehicle") || name.starts_with("physics") ||
              name.starts_with("ent") || name.starts_with("worldWeather") || name == "worldRuntimeSystemWeather" ||
              name == "gameObject" || name == "entEntity" ||
              name == "entIComponent" || name == "entIPlacedComponent")) continue;
        ++inspected;
        const auto* type = rtti->GetClass(nativeType->GetName());
        if (!type) {
            sdk->logger->InfoF(pluginHandle, "DNC_NONCLASS %s kind=%u size=%u", typeName,
                static_cast<unsigned>(nativeType->GetType()), nativeType->GetSize());
            continue;
        }
        sdk->logger->InfoF(pluginHandle, "DNC_TYPE %s size=%u parent=%s", typeName,
            type->GetSize(), type->parent ? type->parent->GetName().ToString() : "none");
        for (const auto* prop : type->props) {
            if (!prop) continue;
            sdk->logger->InfoF(pluginHandle, "DNC_PROPERTY %s.%s type=%s offset=%u", typeName,
                prop->name.ToString(), prop->type ? prop->type->GetName().ToString() : "unknown",
                prop->valueOffset);
        }
        // Declared instance/static functions; inherited members are in the parent class entry.
        const auto logFunction = [&](const RED4ext::CBaseFunction* func) {
            if (!func) return;
            sdk->logger->InfoF(pluginHandle, "DNC_FUNCTION %s.%s return=%s", typeName,
                func->fullName.ToString(), func->returnType && func->returnType->type
                    ? func->returnType->type->GetName().ToString() : "unavailable-or-void");
            for (const auto* param : func->params) {
                if (!param) continue;
                sdk->logger->InfoF(pluginHandle, "DNC_PARAM %s type=%s out=%u optional=%u",
                    param->name.ToString(), param->type ? param->type->GetName().ToString() : "unknown",
                    static_cast<unsigned>(param->flags.isOut), static_cast<unsigned>(param->flags.isOptional));
            }
        };
        for (const auto* func : type->funcs) logFunction(func);
        for (const auto* func : type->staticFuncs) logFunction(func);
    }
    sdk->logger->InfoF(pluginHandle, "DNC_SCAN_COUNT %u", inspected);
    sdk->logger->Info(pluginHandle, "DNC_DIAGNOSTICS_COMPLETE: metadata scan complete");
}
}

RED4EXT_C_EXPORT bool RED4EXT_CALL Main(RED4ext::v1::PluginHandle handle,
    RED4ext::v1::EMainReason reason, const RED4ext::v1::Sdk* api)
{
    if (reason == RED4ext::v1::EMainReason::Load) {
        if (!api || !api->logger) return false;
        pluginHandle = handle;
        sdk = api;
        auto* rtti = RED4ext::CRTTISystem::Get();
        if (!rtti) return false;
        RaceTracking::Register(handle, api);
        GpsRoute::Register(handle, api);
        if (!api->gameStates) return false;
        RED4ext::v1::GameState state{};
        state.OnUpdate = [](RED4ext::CGameApplication*) -> bool {
            static bool inspected = false;
            if (!inspected) {
                inspected = true;
                sdk->logger->Info(pluginHandle, "DNC_SCAN_PHASE Running");
                if constexpr (TypeDump) InspectTypes();
            }
            // Report frames where the plugin itself costs noticeable game-thread time.
            using Clock = std::chrono::steady_clock;
            const auto begin = Clock::now();
            PollLiveVehicle(pluginHandle, sdk);
            const auto polled = Clock::now();
            DrainPairProbe(pluginHandle, sdk);
            const auto done = Clock::now();
            const auto pollUs = std::chrono::duration_cast<std::chrono::microseconds>(polled - begin).count();
            const auto drainUs = std::chrono::duration_cast<std::chrono::microseconds>(done - polled).count();
            // One summary line per second that contained a frame costing 2 ms or more.
            static long long second = -1, maxPoll = 0, maxDrain = 0, totalUs = 0;
            static unsigned slowFrames = 0, frames = 0, summaries = 0;
            const auto current = std::chrono::duration_cast<std::chrono::seconds>(done.time_since_epoch()).count();
            if (current != second) {
                if (slowFrames && summaries < 3000) {
                    ++summaries;
                    sdk->logger->InfoF(pluginHandle, "DNC_SLOW_UPDATE frames=%u slowFrames=%u maxPollUs=%lld maxDrainUs=%lld totalUs=%lld",
                        frames, slowFrames, maxPoll, maxDrain, totalUs);
                    const auto& t = DncProfile::microseconds;
                    sdk->logger->InfoF(pluginHandle, "DNC_UPDATE_SECTIONS getPlayer=%lld getVehicle=%lld driverFlag=%lld chassis=%lld ownedFlag=%lld driverState=%lld nearby=%lld audit=%lld bodies=%lld fade=%lld discovery=%lld ownerRefresh=%lld",
                        t[DncProfile::GetPlayer], t[DncProfile::GetVehicle], t[DncProfile::DriverFlag], t[DncProfile::ChassisRead],
                        t[DncProfile::OwnedFlag], t[DncProfile::DriverState], t[DncProfile::Nearby], t[DncProfile::Audit],
                        t[DncProfile::Bodies], t[DncProfile::Fade], t[DncProfile::Discovery], t[DncProfile::OwnerRefresh]);
                }
                DncProfile::microseconds = {};
                second = current; maxPoll = maxDrain = totalUs = 0; slowFrames = frames = 0;
            }
            ++frames; totalUs += pollUs + drainUs;
            if (pollUs > maxPoll) maxPoll = pollUs;
            if (drainUs > maxDrain) maxDrain = drainUs;
            if (pollUs + drainUs >= 2000) ++slowFrames;
            // RED4ext 1.31 StateSystem::Run erases callbacks returning true.
            return false;
        };
        if (!api->gameStates->Add(handle, RED4ext::EGameStateType::Running, &state)) return false;
        StartPairProbe(handle, sdk);
        sdk->logger->Info(handle, "DNC_LOADED: traffic ghosting, universal glass, bike bodies and query pass-through, race tracking and GPS road routes 0.32.0; target patch 2.5");
    } else if (reason == RED4ext::v1::EMainReason::Unload && sdk) {
        if (!StopPairProbe(handle, sdk)) return false;
        sdk->logger->Info(handle, "DNC_UNLOADED");
    }
    return true;
}

RED4EXT_C_EXPORT void RED4EXT_CALL Query(RED4ext::v1::PluginInfo* info)
{
    info->name = L"DriveNoCollision";
    info->author = L"DriveNoCollision contributors";
    info->version = RED4EXT_V1_SEMVER(0, 32, 0);
    info->runtime = RED4EXT_V1_RUNTIME_VERSION_2_50;
    info->sdk = RED4EXT_V1_SDK_VERSION_CURRENT;
}

RED4EXT_C_EXPORT uint32_t RED4EXT_CALL Supports()
{
    return RED4EXT_API_VERSION_1;
}












