#pragma once
#include <RED4ext/RED4ext.hpp>
bool StartPhysicsPhaseProbe(RED4ext::v1::PluginHandle,const RED4ext::v1::Sdk*);
bool StopPhysicsPhaseProbe(RED4ext::v1::PluginHandle,const RED4ext::v1::Sdk*);
void SamplePhysicsPhasePair(uintptr_t callback,uint32_t pair);
void DrainPhysicsPhaseProbe(RED4ext::v1::PluginHandle,const RED4ext::v1::Sdk*);
