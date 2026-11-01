#pragma once
#include "VehicleLifecycle.hpp"
#include "NativeProbe.hpp"
#include <RED4ext/Scripting/Natives/entEntity.hpp>
struct FadeContact {uint64_t mount; Dnc::VehicleIdentity driver,traffic; uint64_t at;};
void QueueFadeContact(FadeContact contact);
void UpdateFadeContacts(uint64_t mount,uint64_t now,RED4ext::v1::PluginHandle,const RED4ext::v1::Sdk*);
void InspectFadeTarget(Dnc::VehicleIdentity,RED4ext::ent::Entity*,RED4ext::v1::PluginHandle,const RED4ext::v1::Sdk*);
