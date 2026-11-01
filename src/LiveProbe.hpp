#pragma once
#include <RED4ext/RED4ext.hpp>
void PollLiveVehicle(RED4ext::v1::PluginHandle handle, const RED4ext::v1::Sdk* sdk);

RED4ext::Handle<RED4ext::IScriptable> TrafficOccupantMount(RED4ext::Handle<RED4ext::IScriptable>&);
