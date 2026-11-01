#include "GhostVisual.hpp"
#include "GhostShellPart.hpp"
#include "VerifiedGhostVisual.hpp"
#include <RED4ext/ResourceLoader.hpp>
#include <RED4ext/Scripting/Natives/CMesh.hpp>
#include <RED4ext/Scripting/Natives/Generated/CMaterialTemplate.hpp>
#include <RED4ext/Scripting/Natives/Generated/CMaterialParameterScalar.hpp>
#include <cstring>
#include <string_view>
namespace {
using namespace RED4ext;
using namespace GhostVisualBuild;
const auto imageBase=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
bool Verified() {
    static const bool ok=[] {
        auto check=[](uintptr_t rva,const auto& bytes){return !std::memcmp(reinterpret_cast<void*>(imageBase+rva),bytes.data(),bytes.size());};
        return check(VisibilityRva,VisibilityBytes) && check(RenderDataRva,RenderDataBytes) && check(ReleaseDataRva,ReleaseDataBytes) && check(OverrideRva,OverrideBytes) &&
               check(MeshEventRva,MeshEventBytes) && check(SkinEventRva,SkinEventBytes);
    }();return ok;
}
struct Data { void* value{}; };
using Acquire=Data*(*)(IMaterial*,Data*);
using Release=void(*)(void*);
using OverrideFn=void(*)(void*,const Data*,const Data*,bool);
void Send(uintptr_t wrapper,const Data& data,bool replace) {
    Data empty;
    reinterpret_cast<OverrideFn>(imageBase+OverrideRva)(reinterpret_cast<void*>(wrapper),&data,&empty,replace);
}
template<class T> bool Read(uintptr_t a,T& out) {
    SIZE_T n=0;return a && ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(a),&out,sizeof(T),&n) && n==sizeof(T);
}
struct Binding {uintptr_t wrapper{},renderer{};bool priorReplace{};};
bool Bind(ent::IComponent* component,bool skin,Binding& out,bool allowOverlay=false) {
    const auto address=reinterpret_cast<uintptr_t>(component);
    const size_t handle=skin?0x1e0:0x140,overlay=skin?0x218:0x1c0,mode=skin?0x25b:0x1d0;
    uintptr_t oldObject=0,oldControl=0,table=0;uint8_t flags=0;
    if(component->GetType()->GetSize()<=mode || !Read(address+overlay,oldObject) || !Read(address+overlay+8,oldControl) ||
       (!allowOverlay && (oldObject || oldControl)) || !Read(address+mode,flags) || !Read(address+handle,out.wrapper) || !out.wrapper ||
       !Read(out.wrapper+0x10,out.renderer) || !out.renderer || !Read(out.renderer,table) || table!=imageBase+0x3059ab0) return false;
    out.priorReplace=skin ? bool(flags&2) : bool(flags);return true;
}
// Captured depot path of the loaded window material's vehicle_glass parent on patch 2.5.
constexpr ResourcePath donorPath{uint64_t{0x1ecc2b2ba357fdf7ULL}};
SharedPtr<ResourceToken<CResource>> donorToken;
Handle<CMaterialInstance> cachedGlass;
Handle<CMaterialInstance> PrivateGlass(CMaterialInstance* original,CClass* mi,CClass* mt) {
    auto* rtti=CRTTISystem::Get();
    if(!original->baseMaterial.IsLoaded() || original->params.size()>256) return {};
    auto parent=original->baseMaterial.Get();
    for(unsigned depth=0;parent && !parent->GetType()->IsA(mt) && depth<8;++depth){
        if(!parent->baseMaterial.IsLoaded()) {parent.Reset();break;}
        auto next=parent->baseMaterial.Get();parent=next;
    }
    if(!parent || !parent->GetType()->IsA(mt)) return {};
    auto* shader=static_cast<CMaterialTemplate*>(parent.GetPtr());
    if(shader->name!=CName("vehicle_glass")) return {};
    auto* scalar=rtti->GetClass("CMaterialParameterScalar");bool exposesOpacity=false;
    if(!scalar || shader->parameters[2].size()>512) return {};
    for(const auto& parameter:shader->parameters[2]) if(parameter && parameter->GetType()->IsA(scalar) && parameter->parameterName==CName("Opacity")) exposesOpacity=true;
    if(!exposesOpacity) return {};
    auto* fresh=static_cast<CMaterialInstance*>(mi->CreateInstance());if(!fresh) return {};
    Handle<CMaterialInstance> result(fresh);
    fresh->baseMaterial=original->baseMaterial;fresh->enableMask=original->enableMask;fresh->resourceVersion=original->resourceVersion;
    for(const auto& parameter:original->params) if(parameter.name!=CName("Opacity") && parameter.name!=CName("OpacityBackFace")) fresh->params.PushBack(parameter);
    for(const char* key:{"Opacity","OpacityBackFace"}) {
        MaterialParameterInstance parameter{};parameter.name=CName(key);parameter.data=Variant(0.5f);fresh->params.PushBack(parameter);
    }
    return result;
}
}
void PrepareGhostDonor(bool mounted) {
    if(!mounted) {cachedGlass.Reset();donorToken.Reset();return;}
    if(donorToken || !Verified()) return;
    if(auto* loader=ResourceLoader::Get()) donorToken=loader->LoadAsync<CResource>(donorPath);
}
bool ApplyGhostVisual(GhostVisual& state,RED4ext::ent::Entity* entity,RED4ext::v1::PluginHandle h,const RED4ext::v1::Sdk* sdk) {
    if(!Verified() || !entity || entity->components.size()>512 || !state.changed.empty()) return false;
    const char* source="universal";
    auto material=cachedGlass;
    if(!material && donorToken && donorToken->IsLoaded()) {
        auto* rtti=CRTTISystem::Get();auto* mi=rtti->GetClass("CMaterialInstance");auto* mt=rtti->GetClass("CMaterialTemplate");
        auto donor=donorToken->Get();
        if(mi && mt && mi->GetSize()==sizeof(CMaterialInstance) && mt->GetSize()==sizeof(CMaterialTemplate) && donor && donor->GetType()->IsA(mi))
            material=PrivateGlass(static_cast<CMaterialInstance*>(donor.GetPtr()),mi,mt);

    }
    if(!material){sdk->logger->InfoF(h,"DNC_GLASS_SKIP entity=%llu reason=material donorLoaded=%u",static_cast<unsigned long long>(entity->entityID.hash),unsigned(donorToken && donorToken->IsLoaded()));return false;}
    Data render;reinterpret_cast<Acquire>(imageBase+RenderDataRva)(material.GetPtr(),&render);
    if(!render.value){sdk->logger->InfoF(h,"DNC_GLASS_SKIP entity=%llu reason=renderData source=%s",static_cast<unsigned long long>(entity->entityID.hash),source);return false;}
    if(!cachedGlass) cachedGlass=material;
    struct Drop {void* value;~Drop(){reinterpret_cast<Release>(imageBase+ReleaseDataRva)(value);}} drop{render.value};
    auto* rtti=RED4ext::CRTTISystem::Get();auto* mesh=rtti->GetClass("entMeshComponent");auto* skinned=rtti->GetClass("entSkinnedMeshComponent");
    if(!mesh || !skinned) return false;
    state.material=material;state.changed.reserve(entity->components.size());
    unsigned panels=0,hidden=0;
    for(const auto& c:entity->components) {
        if(!c) continue;const auto* type=c->GetType();const bool skin=type->IsA(skinned);
        if(!skin && !type->IsA(mesh)) continue;
        const char* name=c->name.ToString();
        const bool shell=name && Dnc::GhostShellPart(name);
        Binding binding;if(!Bind(c.GetPtr(),skin,binding,!shell)) continue;
        uint8_t visibility=0;
        if(!Read(binding.wrapper+0x23,visibility) || !(visibility&2)) continue;
        if(shell) {
            state.changed.push_back({WeakHandle<ent::IComponent>(c),binding.wrapper,binding.renderer,skin,binding.priorReplace,false});
            Send(binding.wrapper,render,true);++panels;
        } else {
            state.changed.push_back({WeakHandle<ent::IComponent>(c),binding.wrapper,binding.renderer,skin,binding.priorReplace,true});
            reinterpret_cast<void(*)(void*,bool)>(imageBase+VisibilityRva)(reinterpret_cast<void*>(binding.wrapper),false);++hidden;
        }
    }
    sdk->logger->InfoF(h,"DNC_GLASS_APPLY entity=%llu shell=%u hidden=%u source=%s requestedOpacity=0.50",static_cast<unsigned long long>(entity->entityID.hash),panels,hidden,source);
    return !state.changed.empty();
}
void RestoreGhostVisual(GhostVisual& state,RED4ext::v1::PluginHandle h,const RED4ext::v1::Sdk* sdk) {
    if(state.changed.empty()){state.material.Reset();return;}
    unsigned restored=0;Data empty;
    for(const auto& entry:state.changed){auto c=entry.component.Lock();Binding current;
        if(!c || !Bind(c.GetPtr(),entry.skin,current,entry.hidden) || current.wrapper!=entry.wrapper || current.renderer!=entry.renderer) continue;
        if(entry.hidden) {
            uint8_t visibility=0;
            if(!Read(current.wrapper+0x23,visibility)) continue;
            if(!(visibility&2)) reinterpret_cast<void(*)(void*,bool)>(imageBase+VisibilityRva)(reinterpret_cast<void*>(current.wrapper),true);
        } else Send(current.wrapper,empty,entry.priorReplace);
        ++restored;
    }
    sdk->logger->InfoF(h,"DNC_GLASS_RESTORE applied=%u restored=%u",unsigned(state.changed.size()),restored);
    state.changed.clear();state.material.Reset();
}
