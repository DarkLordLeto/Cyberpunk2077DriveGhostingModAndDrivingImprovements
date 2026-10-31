#pragma once
#include <RED4ext/RED4ext.hpp>
void InspectNativeChassis(void* chassis, RED4ext::v1::PluginHandle handle, const RED4ext::v1::Sdk* sdk);

bool ReadVerifiedChassisProxy(void* chassis,uint32_t& proxy);

#include "BoundsClearance.hpp"
bool ReadVerifiedWorldBounds(uint32_t proxy,Dnc::WorldBounds& bounds);

bool ReadVerifiedProxyBounds(uint32_t proxy,Dnc::WorldBounds& bounds,uint32_t& bodyCount);
