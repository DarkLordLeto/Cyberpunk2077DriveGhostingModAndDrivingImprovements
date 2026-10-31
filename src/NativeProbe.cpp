#include "NativeProbe.hpp"
#include "BodyBoundsInventory.hpp"
#include <array>
#include "PairProbe.hpp"
#include "VerifiedBuild.hpp"
#include <RED4ext/Scripting/Natives/Generated/Vector4.hpp>
#include <cstring>
#include <cmath>
#include <RED4ext/Scripting/Natives/Generated/physics/CollisionFilterResource.hpp>
#include <RED4ext/Scripting/Natives/Generated/physics/StateValue.hpp>
#include <RED4ext/Scripting/Natives/Generated/physics/SimulationFilter.hpp>
namespace {
bool CheckImage(const unsigned char* base) {
    if (!base) return false;
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0 || dos->e_lfanew > 4096) return false;
    const auto* pe = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
    if (pe->Signature != IMAGE_NT_SIGNATURE || pe->FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64 ||
        pe->FileHeader.TimeDateStamp != DncBuild::Timestamp || pe->OptionalHeader.SizeOfImage != DncBuild::ImageSize) return false;
    const auto matches = [base](uint32_t rva, const auto& bytes) {
        return rva < DncBuild::ImageSize && bytes.size() <= DncBuild::ImageSize-rva &&
            std::memcmp(base+rva, bytes.data(), bytes.size()) == 0;
    };
    return matches(DncBuild::ChassisIsKinematicRva, DncBuild::ChassisIsKinematicBytes) &&
        matches(DncBuild::ChassisLinearVelocityRva, DncBuild::ChassisLinearVelocityBytes) &&
        matches(DncBuild::ChassisAngularVelocityRva, DncBuild::ChassisAngularVelocityBytes) &&
        matches(DncBuild::ProxyReadStateRva, DncBuild::ProxyReadStateBytes) &&
        matches(DncBuild::FilterResourceGetterRva, DncBuild::FilterResourceGetterBytes) &&
        matches(DncBuild::ShapeStateReaderRva, DncBuild::ShapeStateReaderBytes) &&
        matches(DncBuild::PhysicalProxyReaderRva, DncBuild::PhysicalProxyReaderBytes) &&
        matches(DncBuild::OtherProxyReaderRva, DncBuild::OtherProxyReaderBytes);
}
}
namespace {
bool Readable(const void* ptr, size_t size) {
    if (!ptr || !size) return false;
    auto current = reinterpret_cast<uintptr_t>(ptr);
    if (current > UINTPTR_MAX-size) return false;
    const auto end=current+size;
    while (current < end) {
        MEMORY_BASIC_INFORMATION mbi{};
        if (!VirtualQuery(reinterpret_cast<const void*>(current),&mbi,sizeof(mbi)) ||
            mbi.State != MEM_COMMIT || (mbi.Protect & (PAGE_NOACCESS|PAGE_GUARD))) return false;
        const auto protection=mbi.Protect & 0xff;
        if (!(protection==PAGE_READONLY || protection==PAGE_READWRITE || protection==PAGE_WRITECOPY ||
              protection==PAGE_EXECUTE_READ || protection==PAGE_EXECUTE_READWRITE || protection==PAGE_EXECUTE_WRITECOPY)) return false;
        const auto next=reinterpret_cast<uintptr_t>(mbi.BaseAddress)+mbi.RegionSize;
        if (next<=current) return false;
        current=next;
    }
    return true;
}
void LogCollisionGroups(const unsigned char* base, const RED4ext::physics::SimulationFilter& filter,
                        RED4ext::v1::PluginHandle h, const RED4ext::v1::Sdk* sdk) {
    // Both pointer dereferences match the guarded, 12-byte resource getter.
    void* singleton=nullptr;
    const auto* slot=base+DncBuild::PhysicsSingletonSlotRva;
    if (!Readable(slot,sizeof(singleton))) return;
    std::memcpy(&singleton,slot,sizeof(singleton));
    if (!Readable(singleton,0x58)) {
        sdk->logger->Warn(h,"DNC_GROUPS_UNAVAILABLE: physics singleton missing"); return;
    }
    RED4ext::physics::CollisionFilterResource* resource=nullptr;
    std::memcpy(&resource,static_cast<const unsigned char*>(singleton)+0x50,sizeof(resource));
    auto* type=RED4ext::CRTTISystem::Get()->GetClass("physicsCollisionFilterResource");
    if (!type || type->GetSize()!=sizeof(*resource) || !Readable(resource,sizeof(*resource))) {
        sdk->logger->Warn(h,"DNC_GROUPS_UNAVAILABLE: resource layout/readability mismatch"); return;
    }
    // Reflected offsets must agree with the SDK and disassembled filter builder.
    auto* groupsProperty=type->GetProperty("collisionGroups");
    if (!groupsProperty || groupsProperty->valueOffset!=0xa8 ||
        resource->collisionGroups.Size()>64 || resource->queryGroups.Size()>64) {
        sdk->logger->Warn(h,"DNC_GROUPS_UNAVAILABLE: group layout/count rejected"); return;
    }
    const auto count=resource->collisionGroups.Size();
    sdk->logger->InfoF(h,"DNC_GROUP_TABLE count=%u",unsigned(count));
    for (uint32_t i=0;i<count;++i) {
        const uint64_t bit=uint64_t{1}<<i;
        sdk->logger->InfoF(h,"DNC_COLLISION_GROUP index=%u name=%s mask1=%u mask2=%u",i,
            resource->collisionGroups[i].ToString(),unsigned((filter.mask1&bit)!=0),unsigned((filter.mask2&bit)!=0));
    }
    sdk->logger->Info(h,"DNC_GROUP_TABLE_COMPLETE: observed names only; no filter changes");
}
}
void InspectNativeChassis(void* chassis, RED4ext::v1::PluginHandle h, const RED4ext::v1::Sdk* sdk) {
    const auto* base = reinterpret_cast<const unsigned char*>(GetModuleHandleW(nullptr));
    if (!chassis || !CheckImage(base)) {
        sdk->logger->Warn(h, "DNC_NATIVE_SKIPPED: executable or function bytes differ from verified build");
        return;
    }
    // Offset corroborated by three independent native getters in the installed 2.5 binary.
    uint32_t proxyId = 0;
    std::memcpy(&proxyId, static_cast<const unsigned char*>(chassis)+0x160, sizeof(proxyId));
    ObservePlayerProxy(proxyId);
    using IsKinematic = bool (*)(void*);
    using GetVelocity = RED4ext::Vector4* (*)(void*, RED4ext::Vector4*);
    alignas(16) RED4ext::Vector4 linear{}, angular{};
    const bool kinematic = reinterpret_cast<IsKinematic>(const_cast<unsigned char*>(base)+DncBuild::ChassisIsKinematicRva)(chassis);
    reinterpret_cast<GetVelocity>(const_cast<unsigned char*>(base)+DncBuild::ChassisLinearVelocityRva)(chassis,&linear);
    reinterpret_cast<GetVelocity>(const_cast<unsigned char*>(base)+DncBuild::ChassisAngularVelocityRva)(chassis,&angular);
    if (!(std::isfinite(linear.X) && std::isfinite(linear.Y) && std::isfinite(linear.Z) &&
          std::isfinite(angular.X) && std::isfinite(angular.Y) && std::isfinite(angular.Z))) {
        sdk->logger->Warn(h, "DNC_NATIVE_INVALID: non-finite velocity; no mutations attempted");
        return;
    }
    sdk->logger->InfoF(h, "DNC_NATIVE_CHASSIS proxy=%08x kinematic=%u linear=(%.4f,%.4f,%.4f) angular=(%.4f,%.4f,%.4f)",
        proxyId, unsigned(kinematic), linear.X,linear.Y,linear.Z,angular.X,angular.Y,angular.Z);
    // Derived from the installed binary's own callers. The reader acquires/releases
    // the proxy reference internally. Null cache requests the direct state path.
    using ReadState = bool (*)(void*, uint32_t, uint32_t, uint32_t, uint32_t, void*, uint32_t);
    auto readState = reinterpret_cast<ReadState>(const_cast<unsigned char*>(base)+DncBuild::ProxyReadStateRva);
    struct Vector3Output { float x{}, y{}, z{}; } crossCheck;
    static_assert(sizeof(Vector3Output) == 12);
    const bool velocityRead = readState(nullptr, proxyId, 0, 0,
        static_cast<uint32_t>(RED4ext::physics::StateValue::LinearVelocity), &crossCheck, sizeof(crossCheck));
    const bool comparable = velocityRead && std::isfinite(crossCheck.x) && std::isfinite(crossCheck.y) &&
        std::isfinite(crossCheck.z) && std::abs(crossCheck.x-linear.X) < 0.1f &&
        std::abs(crossCheck.y-linear.Y) < 0.1f && std::abs(crossCheck.z-linear.Z) < 0.1f;
    sdk->logger->InfoF(h, "DNC_PROXY_READ_CHECK success=%u matches=%u linear=(%.4f,%.4f,%.4f)",
        unsigned(velocityRead), unsigned(comparable), crossCheck.x, crossCheck.y, crossCheck.z);
    if (!comparable) {
        sdk->logger->Warn(h, "DNC_LIVE_FILTER_SKIPPED: direct state velocity cross-check failed");
        return;
    }
    RED4ext::physics::SimulationFilter filter{};
    static_assert(sizeof(filter) == 16);
    // Only body 0 / shape 0; resource counts are not assumed to be live shape counts.
    const bool filterRead = readState(nullptr, proxyId, 0, 0,
        static_cast<uint32_t>(RED4ext::physics::StateValue::SimulationFilter), &filter, sizeof(filter));
    if (filterRead) {
        LogCollisionGroups(base, filter, h, sdk);
        sdk->logger->InfoF(h, "DNC_LIVE_SIM_FILTER body=0 shape=0 mask=%016llx:%016llx",
            static_cast<unsigned long long>(filter.mask1), static_cast<unsigned long long>(filter.mask2));
    } else {
        sdk->logger->Info(h, "DNC_LIVE_FILTER_UNSUPPORTED: engine rejected SimulationFilter read; output ignored");
        return;
    }
    // The reviewed state-32 implementation checks getNbShapes() and returns false
    // for out-of-range indices. Stay on body 0; resource counts are not used.
    // A rejection ends the scan, but is not claimed to prove an exact shape count.
    uint32_t bottomMask = 0;
    std::memcpy(&bottomMask, static_cast<const unsigned char*>(chassis)+0x174, sizeof(bottomMask));
    sdk->logger->InfoF(h, "DNC_SHAPE_SCAN_BEGIN body=0 limit=64 bottomMask=%08x", bottomMask);
    uint32_t accepted = 0;
    constexpr uint32_t limit = 64;
    for (uint32_t shape = 0; shape < limit; ++shape) {
        RED4ext::physics::SimulationFilter current{};
        if (shape == 0) current = filter;
        else if (!readState(nullptr, proxyId, 0, shape,
            static_cast<uint32_t>(RED4ext::physics::StateValue::SimulationFilter), &current, sizeof(current))) {
            sdk->logger->InfoF(h, "DNC_SHAPE_SCAN_END body=0 accepted=%u firstRejected=%u capped=0", accepted, shape);
            return;
        }
        const bool bottom = shape < 32 && (bottomMask & (uint32_t{1} << shape)) != 0;
        sdk->logger->InfoF(h, "DNC_SHAPE_FILTER body=0 shape=%u bottom=%u mask=%016llx:%016llx",
            shape, unsigned(bottom), static_cast<unsigned long long>(current.mask1),
            static_cast<unsigned long long>(current.mask2));
        ++accepted;
    }
    sdk->logger->WarnF(h, "DNC_SHAPE_SCAN_END body=0 accepted=%u capped=1; inventory incomplete", accepted);
}



bool ReadVerifiedChassisProxy(void* chassis,uint32_t& proxy) {
    if(!chassis || !CheckImage(reinterpret_cast<const unsigned char*>(GetModuleHandleW(nullptr))) ||
       !Readable(chassis,0x164)) return false;
    std::memcpy(&proxy,static_cast<const unsigned char*>(chassis)+0x160,sizeof(proxy));
    return proxy!=0xffffffff;
}

bool ReadVerifiedWorldBounds(uint32_t proxy,Dnc::WorldBounds& bounds) {
    const auto* base=reinterpret_cast<const unsigned char*>(GetModuleHandleW(nullptr));
    if(proxy==0xffffffff || !CheckImage(base)) return false;
    using ReadState=bool(*)(void*,uint32_t,uint32_t,uint32_t,uint32_t,void*,uint32_t);
    auto read=reinterpret_cast<ReadState>(const_cast<unsigned char*>(base)+DncBuild::ProxyReadStateRva);
    Dnc::WorldBounds result;
    // Exact guarded branch 0x614fbb: PxActor bounds at slot +0x48, inflation
    // 1.01, adds world origin, writes min/max float4 with W=1. States 35/39.
    if(!read(nullptr,proxy,0,0,35,&result,sizeof(result)) || !Dnc::ValidBounds(result)) return false;
    bounds=result;return true;
}

bool ReadVerifiedProxyBounds(uint32_t proxy,Dnc::WorldBounds& bounds,uint32_t& bodyCount) {
    const auto* base=reinterpret_cast<const unsigned char*>(GetModuleHandleW(nullptr));
    if(proxy==0xffffffff || !CheckImage(base) ||
       std::memcmp(base+DncBuild::ProxyOwnerRva,DncBuild::ProxyOwnerBytes.data(),DncBuild::ProxyOwnerBytes.size()) ||
       std::memcmp(base+DncBuild::ResolveSharedProxyRva,DncBuild::ResolveSharedProxyBytes.data(),DncBuild::ResolveSharedProxyBytes.size()) ||
       std::memcmp(base+DncBuild::ReleaseSharedProxyRva,DncBuild::ReleaseSharedProxyBytes.data(),DncBuild::ReleaseSharedProxyBytes.size())) return false;
    void* manager=nullptr;
    std::memcpy(&manager,base+DncBuild::ProxyManagerSlotRva,sizeof(manager));
    if(!manager) return false;
    struct SharedProxy { void* object{};void* control{}; } reference;
    using Resolve=SharedProxy*(*)(void*,SharedProxy*,uint32_t);
    using Release=void(*)(SharedProxy*);
    auto resolve=reinterpret_cast<Resolve>(const_cast<unsigned char*>(base)+DncBuild::ResolveSharedProxyRva);
    auto release=reinterpret_cast<Release>(const_cast<unsigned char*>(base)+DncBuild::ReleaseSharedProxyRva);
    resolve(manager,&reference,proxy);
    struct Cleanup { SharedProxy* value;Release release;~Cleanup(){release(value);} } cleanup{&reference,release};
    if(!Readable(reference.object,0x60)) return false;
    const auto* object=static_cast<const unsigned char*>(reference.object);
    const unsigned char* table=nullptr;
    std::memcpy(&table,object,sizeof(table));
    if(!Readable(table,0x58)) return false;
    uintptr_t reader=0;std::memcpy(&reader,table+0x50,sizeof(reader));
    const unsigned char* listHeader=nullptr;
    if(reader==reinterpret_cast<uintptr_t>(base+DncBuild::PhysicalProxyReaderRva)) listHeader=object+0x50;
    else if(reader==reinterpret_cast<uintptr_t>(base+DncBuild::OtherProxyReaderRva)) {
        std::memcpy(&listHeader,object+0x40,sizeof(listHeader));
    } else return false;
    if(!Readable(listHeader,16)) return false;
    uint32_t count=0;const void* bodyList=nullptr;
    std::memcpy(&count,listHeader+12,4);std::memcpy(&bodyList,listHeader,sizeof(bodyList));
    if(!count || count>64 || !Readable(bodyList,count*sizeof(uintptr_t))) return false;
    std::array<uintptr_t,64> before{},after{};
    std::memcpy(before.data(),bodyList,count*sizeof(uintptr_t));
    std::array<Dnc::BodyBoundsSample,64> samples{};
    using ReadProxy=bool(*)(void*,uint32_t,uint32_t,uint32_t,void*,uint32_t);
    auto read=reinterpret_cast<ReadProxy>(reader);
    for(uint32_t body=0;body<count;++body) {
        if(!before[body]) return false;
        samples[body].index=body;
        if(!read(reference.object,body,0,35,&samples[body].bounds,sizeof(Dnc::WorldBounds))) return false;
    }
    // Same proxy reference throughout; reject observable body-list replacement.
    if(reader==reinterpret_cast<uintptr_t>(base+DncBuild::OtherProxyReaderRva)) {
        const unsigned char* currentHeader=nullptr;std::memcpy(&currentHeader,object+0x40,sizeof(currentHeader));
        if(currentHeader!=listHeader) return false;
    }
    uint32_t currentCount=0;const void* currentList=nullptr;
    std::memcpy(&currentCount,listHeader+12,4);std::memcpy(&currentList,listHeader,sizeof(currentList));
    if(currentCount!=count || currentList!=bodyList) return false;
    std::memcpy(after.data(),bodyList,count*sizeof(uintptr_t));
    if(before!=after) return false;
    Dnc::WorldBounds combined;
    if(!Dnc::CombineBodyBounds(count,std::span<const Dnc::BodyBoundsSample>(samples.data(),count),combined)) return false;
    bounds=combined;bodyCount=count;return true;
}
