#include "PairProbe.hpp"
#include "VehicleQueries.hpp"
#include "LiveProbe.hpp"
#include "NativeProbe.hpp"
#include <RED4ext/GameEngine.hpp>
#include <RED4ext/Scripting/Natives/entEntity.hpp>
#include <RED4ext/Scripting/Natives/Generated/vehicle/ChassisComponent.hpp>
#include <RED4ext/Scripting/Natives/Generated/physics/SystemResource.hpp>
#include <RED4ext/Scripting/Natives/Generated/physics/SystemBody.hpp>
#include <RED4ext/Scripting/Natives/Generated/physics/ICollider.hpp>
#include <RED4ext/Scripting/Natives/physicsFilterData.hpp>
#include <chrono>
#include <string_view>

namespace {
using namespace RED4ext;
std::string_view TypeName(const CProperty* p) {
    if (!p || !p->type) return {};
    const auto* name = p->type->GetName().ToString();
    return name ? name : "";
}
CBaseFunction* FindGlobal(const char* name) {
    DynArray<CBaseFunction*> funcs;
    CRTTISystem::Get()->GetGlobalFunctions(funcs);
    CBaseFunction* result = nullptr;
    for (auto* f : funcs) {
        if (f && f->shortName == CName(name)) {
            if (result) return nullptr; // Do not guess between overloads.
            result = f;
        }
    }
    return result;
}
bool CallHandle(CBaseFunction* f, StackArgs_t& args, Handle<IScriptable>& result) {
    const auto ret = TypeName(f->returnType);
    if (ret.starts_with("whandle:")) {
        WeakHandle<IScriptable> weak;
        if (!ExecuteGlobalFunction(f->fullName, &weak, args)) return false;
        result = weak.Lock();
        return true;
    }
    if (ret.starts_with("handle:")) return ExecuteGlobalFunction(f->fullName, &result, args);
    return false;
}
void ProbeNearby(IScriptable* context,float radius,v1::PluginHandle h,const v1::Sdk* sdk) {
    static bool checked=false;
    static CBaseFunction* query=nullptr;
    auto* rtti=CRTTISystem::Get();
    auto* gameObject=rtti->GetClass("gameObject");
    if(!checked) {
        checked=true;
        query=gameObject ? gameObject->GetFunction("GetEntitiesAroundObject") : nullptr;
        if(!query || query->flags.isStatic || query->params.size()!=2 ||
            TypeName(query->params[0])!="Float" || TypeName(query->params[1])!="gameTargetSearchFilter" ||
            !query->params[1]->flags.isOptional || TypeName(query->returnType)!="array:handle:entEntity" ||
            query->returnType->type->GetSize()!=sizeof(DynArray<Handle<ent::Entity>>)) query=nullptr;
        sdk->logger->InfoF(h,"DNC_NEARBY_READY ready=%u nearRadius=18 farRadius=60 defaultFilter=1",unsigned(query!=nullptr));
    }
    if(!query || !context || !context->GetType()->IsA(gameObject)) return;
    const auto queryStart=std::chrono::steady_clock::now();
    StackArgs_t args;
    args.emplace_back(query->params[0]->type,&radius);
    DynArray<Handle<ent::Entity>> found;
    if(!ExecuteFunction(static_cast<void*>(context),query,&found,args)) {
        sdk->logger->Warn(h,"DNC_NEARBY_FAILED: script invocation rejected");query=nullptr;return;
    }
    auto* vehicleType=rtti->GetClass("vehicleBaseObject");
    auto* chassisType=rtti->GetClass("vehicleChassisComponent");
    unsigned vehicles=0,proxies=0;
    const uint32_t inspected=found.size()<256 ? found.size() : 256;
    for(uint32_t i=0;i<inspected;++i) {
        const auto& entity=found[i];
        if(!entity || !vehicleType || !chassisType || !entity->GetType()->IsA(vehicleType) ||
            entity->status!=EntityStatus::Attached || entity->components.size()>512) continue;
        ++vehicles;
        for(const auto& component:entity->components) {
            if(!component || !component->GetType()->IsA(chassisType)) continue;
            uint32_t proxy=0xffffffff;
            if(ReadVerifiedChassisProxy(component.GetPtr(),proxy)) {
                ObserveNearbyProxy(proxy,h,sdk);
                ObserveNearbyVehicle(entity->entityID.hash,proxy,ReadVehicleBool(entity.GetPtr(),"IsPlayerVehicle"),
                    ReadVehicleBool(entity.GetPtr(),"HasTrafficSlot"),ReadVehicleBool(entity.GetPtr(),"IsPlayerDriver"));
                ++proxies;
            }
            break;
        }
    }
    PublishNearbyObservations();
    sdk->logger->InfoF(h,"DNC_NEARBY_RESULT returned=%u inspected=%u vehicles=%u proxies=%u truncated=%u elapsedUs=%llu radius=%.0f",
        unsigned(found.size()),unsigned(inspected),vehicles,proxies,unsigned(found.size()>inspected),static_cast<unsigned long long>(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-queryStart).count()),static_cast<double>(radius));
}
void LogResource(const Ref<physics::SystemResource>& ref, const char* label,
                 v1::PluginHandle h, const v1::Sdk* sdk) {
    if (!ref.IsLoaded()) {
        sdk->logger->InfoF(h, "DNC_RESOURCE %s not loaded; no load requested", label);
        return;
    }
    auto resource = ref.Get(); // Holds an existing loaded resource only.
    if (!resource || resource->bodies.size() > 128) { sdk->logger->Warn(h, "DNC_RESOURCE_SKIPPED: null resource or body count exceeds cap"); return; }
    sdk->logger->InfoF(h, "DNC_RESOURCE %s bodies=%u", label, unsigned(resource->bodies.size()));
    unsigned bodyIndex = 0;
    for (const auto& body : resource->bodies) {
        if (!body || body->collisionShapes.size() > 256) { sdk->logger->WarnF(h, "DNC_BODY_SKIPPED index=%u null=%u", bodyIndex, unsigned(!body)); ++bodyIndex; continue; }
        sdk->logger->InfoF(h, "DNC_BODY %s index=%u shapes=%u", label, bodyIndex, unsigned(body->collisionShapes.size()));
        unsigned shapeIndex = 0;
        for (const auto& shape : body->collisionShapes) {
            if (shape && shape->filterData) {
                const auto& f = shape->filterData;
                sdk->logger->InfoF(h,
                    "DNC_ASSET_FILTER %s body=%u shape=%u preset=%s sim=%016llx:%016llx query=%016llx:%016llx",
                    label, bodyIndex, shapeIndex, f->preset.ToString(),
                    static_cast<unsigned long long>(f->simulationFilter.mask1),
                    static_cast<unsigned long long>(f->simulationFilter.mask2),
                    static_cast<unsigned long long>(f->queryFilter.mask1),
                    static_cast<unsigned long long>(f->queryFilter.mask2));
            }
            if (!shape || !shape->filterData) sdk->logger->InfoF(h, "DNC_SHAPE_FILTER_MISSING %s body=%u shape=%u nullShape=%u", label, bodyIndex, shapeIndex, unsigned(!shape));
            ++shapeIndex;
        }
        ++bodyIndex;
    }
}
}

RED4ext::Handle<RED4ext::IScriptable> TrafficOccupantMount(RED4ext::Handle<RED4ext::IScriptable>& puppet) {
    using namespace RED4ext;
    static auto* f=FindGlobal("GetMountedVehicle");
    Handle<IScriptable> result;
    if(!f || f->params.size()!=1 || !puppet || (TypeName(f->returnType)!="whandle:vehicleBaseObject" && TypeName(f->returnType)!="handle:vehicleBaseObject")) return result;
    const auto arg=TypeName(f->params[0]);
    if(arg!="handle:gameObject" && arg!="whandle:gameObject") return result;
    WeakHandle<IScriptable> weak(puppet);StackArgs_t args;
    args.emplace_back(f->params[0]->type,arg=="whandle:gameObject" ? static_cast<void*>(&weak) : static_cast<void*>(&puppet));
    if(!CallHandle(f,args,result)) result.Reset();
    return result;
}

void PollLiveVehicle(RED4ext::v1::PluginHandle h, const RED4ext::v1::Sdk* sdk) {
    using namespace RED4ext;
    using Clock = std::chrono::steady_clock;
    static auto next = Clock::time_point{};
    if (Clock::now() < next) return;
    next = Clock::now() + std::chrono::milliseconds(100);
    static bool initialized = false, available = false;
    static CBaseFunction* getPlayer = nullptr;
    static CBaseFunction* getVehicle = nullptr;
    static uint64_t lastVehicle = 0;
    static unsigned polls = 0;
    ++polls;
    if (polls == 1 || polls % 300 == 0)
        sdk->logger->InfoF(h, "DNC_LIVE_POLL %u", polls);
    const auto status = [h, sdk](const char* value) {
        if (std::string_view(value)!="vehicle-attached") ObservePlayerProxy(0xffffffff);
        static std::string_view previous;
        if (previous != value) {
            sdk->logger->InfoF(h, "DNC_LIVE_STATUS %s", value);
            previous = value; // All callers supply string literals.
        }
    };
    if (!initialized) {
        initialized = true;
        getPlayer = FindGlobal("GetPlayer");
        getVehicle = FindGlobal("GetMountedVehicle");
        for (auto* f : {getPlayer, getVehicle}) {
            if (!f) continue;
            sdk->logger->InfoF(h, "DNC_LOOKUP %s return=%s parameters=%u", f->fullName.ToString(),
                std::string(TypeName(f->returnType)).c_str(), unsigned(f->params.size()));
            for (auto* p : f->params)
                sdk->logger->InfoF(h, "DNC_LOOKUP_PARAM %s", std::string(TypeName(p)).c_str());
        }
        const auto handleReturn = [](CBaseFunction* f) {
            return f && (TypeName(f->returnType).starts_with("handle:") ||
                         TypeName(f->returnType).starts_with("whandle:"));
        };
        available = handleReturn(getPlayer) && handleReturn(getVehicle) &&
            getPlayer->params.size() == 1 && getVehicle->params.size() == 1;
        if (available) {
            auto playerArg = TypeName(getPlayer->params[0]);
            auto vehicleArg = TypeName(getVehicle->params[0]);
            available = (playerArg == "ScriptGameInstance" || playerArg == "GameInstance") &&
                (vehicleArg == "handle:gameObject" || vehicleArg == "whandle:gameObject" ||
                 vehicleArg == "handle:PlayerPuppet" || vehicleArg == "whandle:PlayerPuppet");
        }
        // Refuse SDK layout mismatches before reading component/resource storage.
        const auto sizeMatches = [](const char* name, uint32_t size) {
            auto* c = CRTTISystem::Get()->GetClass(name);
            return c && c->GetSize() == size;
        };
        available = available && sizeMatches("entEntity", sizeof(ent::Entity)) &&
            sizeMatches("vehicleChassisComponent", sizeof(vehicle::ChassisComponent)) &&
            sizeMatches("physicsSystemResource", sizeof(physics::SystemResource)) &&
            sizeMatches("physicsSystemBody", sizeof(physics::SystemBody)) &&
            sizeMatches("physicsICollider", sizeof(physics::ICollider)) &&
            sizeMatches("physicsFilterData", sizeof(physics::FilterData));
        sdk->logger->Info(h, available ? "DNC_LIVE_READY: load a save and enter a vehicle" :
            "DNC_LIVE_UNAVAILABLE: lookup signature missing, ambiguous or unsupported; no calls made");
    }
    if (!available) { ObservePlayerProxy(0xffffffff); return; }
    auto* engine = CGameEngine::Get();
    if (!engine || !engine->framework || !engine->framework->gameInstance) { status("waiting-for-game-instance"); lastVehicle = 0; return; }
    ScriptGameInstance game;
    if (!game.instance) { status("waiting-for-script-game-instance"); return; }
    StackArgs_t playerArgs;
    playerArgs.emplace_back(getPlayer->params[0]->type, &game);
    Handle<IScriptable> player;
    if (!CallHandle(getPlayer, playerArgs, player)) { status("player-call-failed"); lastVehicle = 0; return; }
    if (!player) { status("waiting-for-player"); lastVehicle = 0; return; }
    WeakHandle<IScriptable> weakPlayer(player);
    StackArgs_t vehicleArgs;
    void* arg = TypeName(getVehicle->params[0]).starts_with("whandle:")
        ? static_cast<void*>(&weakPlayer) : static_cast<void*>(&player);
    vehicleArgs.emplace_back(getVehicle->params[0]->type, arg);
    Handle<IScriptable> vehicle;
    if (!CallHandle(getVehicle, vehicleArgs, vehicle)) { status("mounted-vehicle-call-failed"); lastVehicle = 0; return; }
    if (!vehicle) {
        status("player-not-mounted");
        if (lastVehicle) sdk->logger->Info(h, "DNC_LIVE_UNMOUNTED");
        lastVehicle = 0;
        return;
    }
    auto* rtti = CRTTISystem::Get();
    auto* vehicleType = rtti->GetClass("vehicleBaseObject");
    if (!vehicleType || !vehicle->GetType()->IsA(vehicleType)) { status("mounted-object-not-vehicle"); return; }
    auto* entity = static_cast<ent::Entity*>(vehicle.GetPtr());
    if (entity->status != EntityStatus::Attached || entity->components.size() > 512) { status("waiting-for-attached-vehicle"); return; }
    const int driver=ReadVehicleBool(vehicle.GetPtr(),"IsPlayerDriver");
    static int previousDriver=-2;
    static uint64_t previousDriverVehicle=0;
    if (previousDriver!=driver || previousDriverVehicle!=entity->entityID.hash) {
        sdk->logger->InfoF(h,"DNC_DRIVER entity=%llu verified=%u isDriver=%d",
            static_cast<unsigned long long>(entity->entityID.hash),unsigned(driver>=0),driver);
        previousDriver=driver;previousDriverVehicle=entity->entityID.hash;
    }
    uint32_t currentProxy=0xffffffff;
    auto* currentChassisType=rtti->GetClass("vehicleChassisComponent");
    for(const auto& component:entity->components) {
        if(component && currentChassisType && component->GetType()->IsA(currentChassisType)) {
            ReadVerifiedChassisProxy(component.GetPtr(),currentProxy); break;
        }
    }
    ObservePlayerProxy(currentProxy);
    auto* bikeType=rtti->GetClass("vehicleBikeBaseObject");
    ObserveDriver(entity->entityID.hash,currentProxy,driver,ReadVehicleBool(vehicle.GetPtr(),"IsPlayerVehicle"),bikeType && vehicle->GetType()->IsA(bikeType),static_cast<ent::Entity*>(player.GetPtr())->entityID.hash);
    status("vehicle-attached");
    static auto nextNearby=Clock::time_point{};
    static unsigned scan=0;
    if(driver==1 && currentProxy!=0xffffffff && (lastVehicle!=entity->entityID.hash || Clock::now()>=nextNearby)) {
        const bool newMount=lastVehicle!=entity->entityID.hash;
        if(newMount) scan=0;
        nextNearby=Clock::now()+std::chrono::milliseconds(150);
        // Dense traffic can fill the broad query with unrelated entities. Query
        // the immediate approach twice for every longer-range look ahead.
        const float radius=(scan++%3==2) ? 60.0f : 18.0f;
        ObserveNearbyProxy(currentProxy,h,sdk);
        ProbeNearby(player.GetPtr(),radius,h,sdk);
    }
    if (lastVehicle == entity->entityID.hash) return;
    lastVehicle = entity->entityID.hash;
    sdk->logger->InfoF(h, "DNC_LIVE_VEHICLE id=%llu class=%s appearance=%s components=%u",
        static_cast<unsigned long long>(lastVehicle), entity->GetType()->GetName().ToString(),
        entity->appearanceName.ToString(), unsigned(entity->components.size()));
    auto* chassisType = rtti->GetClass("vehicleChassisComponent");
    for (const auto& component : entity->components) {
        if (!component) continue;
        sdk->logger->InfoF(h, "DNC_COMPONENT name=%s class=%s", component->name.ToString(),
            component->GetType()->GetName().ToString());
        if (chassisType && component->GetType()->IsA(chassisType)) {
            auto* chassis = static_cast<vehicle::ChassisComponent*>(component.GetPtr());
            InspectNativeChassis(chassis, h, sdk);
            LogResource(chassis->collisionResource, "chassis", h, sdk);
            LogResource(chassis->optionalPlayerOnlyCollisionResource, "player-only", h, sdk);
        }
    }
    sdk->logger->Info(h, "DNC_LIVE_COMPLETE: resource data only; no physics or rendering writes");
}



