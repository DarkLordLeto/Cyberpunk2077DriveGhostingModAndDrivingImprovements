#pragma once
#include <RED4ext/RED4ext.hpp>
namespace CarQuery {
void Start(RED4ext::v1::PluginHandle, const RED4ext::v1::Sdk*, const unsigned char* phys);
bool Stop(RED4ext::v1::PluginHandle, const RED4ext::v1::Sdk*);
void Mount(uint64_t token);
void Contact(uint64_t token, uint32_t proxy, const void* actor);
// Keeps a traffic proxy invisible to ray and sweep queries for lifetimeMs.
void Ghost(uint64_t token, uint32_t proxy, uint64_t lifetimeMs);
void Drain(RED4ext::v1::PluginHandle, const RED4ext::v1::Sdk*);
}
