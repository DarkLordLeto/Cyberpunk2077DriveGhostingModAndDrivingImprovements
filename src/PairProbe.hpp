#pragma once
#include <RED4ext/RED4ext.hpp>
void StartPairProbe(RED4ext::v1::PluginHandle, const RED4ext::v1::Sdk*);
bool StopPairProbe(RED4ext::v1::PluginHandle, const RED4ext::v1::Sdk*);
void DrainPairProbe(RED4ext::v1::PluginHandle, const RED4ext::v1::Sdk*);
void ObservePlayerProxy(uint32_t proxy);

void ObserveNearbyProxy(uint32_t proxy,RED4ext::v1::PluginHandle,const RED4ext::v1::Sdk*,bool nearby=true);

void ObserveDriver(uint64_t entity,uint32_t proxy,int verifiedDriver,int owned,bool bike,uint64_t rider);
void ObserveNearbyVehicle(uint64_t entity,uint32_t proxy,int owned,int traffic,int driver);

void PublishNearbyObservations();
