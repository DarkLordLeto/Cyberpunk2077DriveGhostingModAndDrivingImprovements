#pragma once
#include <RED4ext/RED4ext.hpp>
#include <RED4ext/Scripting/Natives/entEntity.hpp>
#include <RED4ext/Scripting/Natives/CMaterialInstance.hpp>
#include <vector>
struct GhostVisual {
    struct Changed {
        RED4ext::WeakHandle<RED4ext::ent::IComponent> component;
        uintptr_t wrapper{},renderer{};
        bool skin{},priorReplace{},hidden{};
    };
    RED4ext::Handle<RED4ext::CMaterialInstance> material;
    std::vector<Changed> changed;
};
bool ApplyGhostVisual(GhostVisual&,RED4ext::ent::Entity*,RED4ext::v1::PluginHandle,const RED4ext::v1::Sdk*);
void RestoreGhostVisual(GhostVisual&,RED4ext::v1::PluginHandle,const RED4ext::v1::Sdk*);

void PrepareGhostDonor(bool mounted);
