#pragma once
#include <RED4ext/RED4ext.hpp>
#include "SuppressionController.hpp"
bool StartPairTracking(RED4ext::v1::PluginHandle,const RED4ext::v1::Sdk*);
bool StopPairTracking(RED4ext::v1::PluginHandle,const RED4ext::v1::Sdk*);
bool ReserveTrackedPair(void* callback,uint32_t pair,uint32_t first,uint32_t second,uint16_t flags,Dnc::VehiclePairIdentity identity);
void DrainPairTracking(RED4ext::v1::PluginHandle,const RED4ext::v1::Sdk*);
