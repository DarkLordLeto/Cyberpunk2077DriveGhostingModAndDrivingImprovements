from pathlib import Path
p=Path(r'F:\Work\GameDev\2077Mod\DriveNoCollision')
f=p/'src/LiveProbe.cpp';s=f.read_text().replace('#include "PairProbe.hpp"','#include "PairProbe.hpp"\n#include "VehicleQueries.hpp"');needle='    status("vehicle-attached");';insert='''    const int driver=ReadVehicleBool(vehicle.GetPtr(),"IsPlayerDriver");
    static int previousDriver=-2;
    static uint64_t previousDriverVehicle=0;
    if (previousDriver!=driver || previousDriverVehicle!=entity->entityID.hash) {
        sdk->logger->InfoF(h,"DNC_DRIVER entity=%llu verified=%u isDriver=%d",
            static_cast<unsigned long long>(entity->entityID.hash),unsigned(driver>=0),driver);
        previousDriver=driver;previousDriverVehicle=entity->entityID.hash;
    }
''';assert needle in s;s=s.replace(needle,insert+needle);f.write_text(s)
f=p/'src/PairProbe.cpp';s=f.read_text().replace('#include "VerifiedBuild.hpp"','#include "VerifiedBuild.hpp"\n#include "VehicleQueries.hpp"\n#include <RED4ext/Scripting/Natives/entEntity.hpp>\n#include <unordered_set>');s=s.replace('std::array<Record,64> matches;', '''// Reserve independent budgets for up to eight mounted intervals. No slot reuse.
std::array<Record,512> matches;
std::array<std::atomic<uint32_t>,8> mountSamples{};
std::atomic<uint64_t> mounted{0xffffffff};
uint32_t nextMount=0; // Running callback only.
bool ownerReaderReady=false;''');s=s.replace('const auto player=playerProxy.load(std::memory_order_relaxed);','const auto observation=mounted.load(std::memory_order_relaxed);\n    const auto player=static_cast<uint32_t>(observation);\n    const auto bank=static_cast<uint32_t>(observation>>32);');s=s.replace('const auto matchSequence=playerMatch ? matching.fetch_add(1,std::memory_order_relaxed) : uint64_t{64};','''uint64_t matchSequence=matches.size();
    if (playerMatch) {
        matching.fetch_add(1,std::memory_order_relaxed);
        if (bank<mountSamples.size()) {
            const auto local=mountSamples[bank].fetch_add(1,std::memory_order_relaxed);
            if (local<64) matchSequence=uint64_t(bank)*64+local;
        }
    }''');s=s.replace('void ObservePlayerProxy(uint32_t proxy) {playerProxy.store(proxy,std::memory_order_relaxed);}', '''void ObservePlayerProxy(uint32_t proxy) {
    const auto previous=mounted.load(std::memory_order_relaxed);
    if (static_cast<uint32_t>(previous)==proxy) return;
    if (proxy==0xffffffff) mounted.store(0xffffffff,std::memory_order_relaxed);
    else mounted.store((uint64_t(nextMount++)<<32)|proxy,std::memory_order_relaxed);
}''');s=s.replace('std::atomic<uint32_t> playerProxy{0xffffffff};\n','');s=s.replace('    target=address;', '''    target=address;
    ownerReaderReady=std::memcmp(base+DncBuild::ProxyOwnerRva,DncBuild::ProxyOwnerBytes.data(),DncBuild::ProxyOwnerBytes.size())==0 &&
        std::memcmp(base+DncBuild::ResolveSharedProxyRva,DncBuild::ResolveSharedProxyBytes.data(),DncBuild::ResolveSharedProxyBytes.size())==0;''')
marker='void DrainPairProbe(RED4ext::v1::PluginHandle h,const RED4ext::v1::Sdk* sdk) {'
helper='''namespace {
void LogProxyOwner(uint32_t proxy,RED4ext::v1::PluginHandle h,const RED4ext::v1::Sdk* sdk) {
    if (!ownerReaderReady) return;
    const auto* base=reinterpret_cast<const unsigned char*>(GetModuleHandleW(nullptr));
    using GetOwner=void* (*)(RED4ext::WeakHandle<RED4ext::IScriptable>*,uint32_t,uint32_t);
    auto getOwner=reinterpret_cast<GetOwner>(const_cast<unsigned char*>(base)+DncBuild::ProxyOwnerRva);
    auto* entityType=RED4ext::CRTTISystem::Get()->GetClass("entEntity");
    auto* vehicleType=RED4ext::CRTTISystem::Get()->GetClass("vehicleBaseObject");
    if (!entityType || !vehicleType) return;
    for (uint32_t slot=0;slot<2;++slot) {
        RED4ext::WeakHandle<RED4ext::IScriptable> weak;
        getOwner(&weak,proxy,slot); // Native helper increments the copied weak reference.
        auto owner=weak.Lock();
        if (!owner) {
            sdk->logger->InfoF(h,"DNC_PAIR_OWNER proxy=%08x slot=%u resolved=0",proxy,slot);continue;
        }
        const auto* type=owner->GetType();
        const bool entity=type->IsA(entityType),vehicle=type->IsA(vehicleType);
        uint64_t id=entity ? static_cast<RED4ext::ent::Entity*>(owner.GetPtr())->entityID.hash : 0;
        const int playerVehicle=vehicle ? ReadVehicleBool(owner.GetPtr(),"IsPlayerVehicle") : -1;
        const int traffic=vehicle ? ReadVehicleBool(owner.GetPtr(),"HasTrafficSlot") : -1;
        sdk->logger->InfoF(h,"DNC_PAIR_OWNER proxy=%08x slot=%u resolved=1 class=%s entity=%llu vehicle=%u playerVehicle=%d trafficSlot=%d",
            proxy,slot,type->GetName().ToString(),static_cast<unsigned long long>(id),unsigned(vehicle),playerVehicle,traffic);
    }
}
}
'''
assert marker in s;s=s.replace(marker,helper+marker);s=s.replace('    const auto drain=[&]', '    static std::unordered_set<uint32_t> resolved;\n    const auto drain=[&]');s=s.replace('            r.logged=true;', '''            r.logged=true;
            if (ownerReaderReady && kind[0]=='P' && resolved.size()<256) {
                for (uint32_t proxy:{r.actor0,r.actor1})
                    if (resolved.insert(proxy).second) LogProxyOwner(proxy,h,sdk);
            }''');s=s.replace('    target=nullptr;\n    DrainPairProbe', '    target=nullptr;\n    ownerReaderReady=false; // Do not resolve entities during shutdown.\n    DrainPairProbe');f.write_text(s)
f=p/'tools/generate-native-guards.py';s=f.read_text();s=s.replace("text += ['}']", """for name,rva,size in [('ProxyOwner',0x314a70,0x8d),('ResolveSharedProxy',0x27d430,0x132)]:
 code=pe.get_data(rva,size)
 text += [f'inline constexpr uint32_t {name}Rva = 0x{rva:x};',f'inline constexpr std::array<unsigned char,{size}> {name}Bytes = {{'+','.join(f'0x{x:02x}' for x in code)+'};']
text += ['}']""");f.write_text(s)
for name in ['src/Main.cpp','CMakeLists.txt']:
 f=p/name;s=f.read_text().replace('0.9.0','0.10.0').replace('SEMVER(0, 9, 0)','SEMVER(0, 10, 0)');f.write_text(s)
print('Prepared owner/driver probe 0.10.0')
