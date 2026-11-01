#pragma once
#include <RED4ext/RED4ext.hpp>
#include <RED4ext/ResourceLoader.hpp>
#include <RED4ext/Scripting/Natives/CMesh.hpp>
#include <RED4ext/Scripting/Natives/CMaterialInstance.hpp>
#include <RED4ext/Scripting/Natives/Generated/CMaterialTemplate.hpp>
#include <RED4ext/Scripting/Natives/Generated/CMaterialParameterScalar.hpp>
#include <RED4ext/Scripting/Natives/Generated/ent/IComponent.hpp>
#include <RED4ext/Scripting/Natives/Generated/mesh/MeshAppearance.hpp>
#include <string_view>
#include <algorithm>
#include <array>
namespace Dnc {
inline void InspectGhostMaterial(RED4ext::ent::IComponent* component,RED4ext::v1::PluginHandle h,const RED4ext::v1::Sdk* sdk) {
    using namespace RED4ext;
    auto* rtti=CRTTISystem::Get();
    auto* type=component->GetType();
    auto* meshProp=type->GetProperty("mesh");auto* appProp=type->GetProperty("meshAppearance");
    if(!meshProp || !meshProp->type || meshProp->type->GetSize()!=sizeof(ResourcePath) ||
       meshProp->type->GetName()!=CName("raRef:CMesh") || meshProp->valueOffset>type->GetSize()-sizeof(ResourcePath)) {
        sdk->logger->InfoF(h,"DNC_MATERIAL_SCHEMA component=%s meshType=%s",component->name.ToString(),meshProp && meshProp->type ? meshProp->type->GetName().ToString() : "missing");return;
    }
    const auto path=*meshProp->GetValuePtr<ResourcePath>(component);
    const auto app=appProp && appProp->type && appProp->type->GetName()==CName("CName") && appProp->valueOffset<=type->GetSize()-sizeof(CName) ? *appProp->GetValuePtr<CName>(component) : CName{};
    struct Asset {uint64_t path,appearance;};
    static std::array<Asset,24> inspected{};
    static size_t used=0;
    if(used==inspected.size() || std::any_of(inspected.begin(),inspected.begin()+used,[&](const Asset& a){return a.path==path.hash && a.appearance==app.hash;})) return;
    inspected[used++]={path.hash,app.hash};
    sdk->logger->InfoF(h,"DNC_MATERIAL_MESH component=%s path=%llx appearance=%s",component->name.ToString(),static_cast<unsigned long long>(path.hash),app.ToString());
    auto* loader=ResourceLoader::Get();auto* meshType=rtti->GetClass("CMesh");auto* instanceType=rtti->GetClass("CMaterialInstance");
    auto* templateType=rtti->GetClass("CMaterialTemplate");auto* scalarType=rtti->GetClass("CMaterialParameterScalar");
    if(!path.hash || !loader || !meshType || meshType->GetSize()!=sizeof(CMesh) || !instanceType || instanceType->GetSize()!=sizeof(CMaterialInstance) ||
       !templateType || templateType->GetSize()!=sizeof(CMaterialTemplate) || !scalarType || scalarType->GetSize()!=sizeof(CMaterialParameterScalar)) return;
    auto token=loader->FindToken<>(path);
    if(!token || !token->IsLoaded()){sdk->logger->Info(h,"DNC_MATERIAL_TOKEN_UNAVAILABLE");return;}
    auto held=token->Get();if(!held || !held->GetType()->IsA(meshType)) return;
    auto* mesh=static_cast<CMesh*>(held.GetPtr());
    if(mesh->materialEntries.size()>2048 || mesh->localMaterialInstances.size()>2048 || mesh->externalMaterials.size()>2048 || mesh->appearances.size()>512) return;
    Handle<mesh::MeshAppearance> selected;
    for(const auto& entry:mesh->appearances) if(entry && entry->name==app){selected=entry;break;}
    if(selected && selected->chunkMaterials.size()>512) return;
    unsigned reported=0;
    for(const auto& entry:mesh->materialEntries) {
        if(selected && std::find(selected->chunkMaterials.begin(),selected->chunkMaterials.end(),entry.name)==selected->chunkMaterials.end()) continue;
        if(reported++==8) break;
        Handle<IMaterial> material=entry.material;
        if(!material) material=entry.materialWeak.Lock();
        if(!material && entry.isLocalInstance && entry.index<mesh->localMaterialInstances.size()) material=mesh->localMaterialInstances[entry.index];
        if(!material && !entry.isLocalInstance && entry.index<mesh->externalMaterials.size()) {
            auto existing=loader->FindToken<IMaterial>(mesh->externalMaterials[entry.index].path);
            if(existing && existing->IsLoaded()) material=existing->Get();
        }
        sdk->logger->InfoF(h,"DNC_MATERIAL_SLOT component=%s material=%s loaded=%u appearanceMatched=%u",component->name.ToString(),entry.name.ToString(),unsigned(bool(material)),unsigned(bool(selected)));
        for(unsigned depth=0;material && depth<8;++depth) {
            auto* kind=material->GetType();
            if(kind->IsA(templateType)) {
                auto* shader=static_cast<CMaterialTemplate*>(material.GetPtr());
                sdk->logger->InfoF(h,"DNC_MATERIAL_SHADER name=%s maskable=%u type=%d",shader->name.ToString(),unsigned(shader->canBeMasked),int(shader->materialType));
                for(unsigned platform=0;platform<3;++platform) {
                    auto& list=shader->parameters[platform];if(list.size()>512) continue;
                    for(const auto& param:list) if(param && param->GetType()->IsA(scalarType)) {
                        auto* scalar=static_cast<CMaterialParameterScalar*>(param.GetPtr());
                        const char* name=scalar->parameterName.ToString();if(!name) continue;
                        const std::string_view n{name};
                        if(n.find("pacity")!=n.npos || n.find("ransparen")!=n.npos || n.find("ither")!=n.npos || n.find("issolve")!=n.npos || n.find("lpha")!=n.npos || n.find("ask")!=n.npos)
                            sdk->logger->InfoF(h,"DNC_MATERIAL_SCALAR platform=%u name=%s default=%.3f min=%.3f max=%.3f",platform,name,scalar->scalar,scalar->min,scalar->max);
                    }
                }
                break;
            }
            if(!kind->IsA(instanceType)) break;
            auto* instance=static_cast<CMaterialInstance*>(material.GetPtr());
            sdk->logger->InfoF(h,"DNC_MATERIAL_INSTANCE depth=%u enableMask=%u base=%llx",depth,unsigned(instance->enableMask),static_cast<unsigned long long>(instance->baseMaterial.path.hash));
            if(!instance->baseMaterial.IsLoaded()) break;
            auto parent=instance->baseMaterial.Get();material=parent;
        }
    }
}
}

