#pragma once
#include <RED4ext/RED4ext.hpp>
// Returns -1 if metadata/signature/invocation is unavailable; never assumes false.
inline int ReadVehicleBool(RED4ext::IScriptable* object,const char* name) {
    if (!object) return -1;
    auto* type=RED4ext::CRTTISystem::Get()->GetClass("vehicleBaseObject");
    if (!type || !object->GetType()->IsA(type)) return -1;
    auto* function=type->GetFunction(name);
    if (!function || function->params.size()!=0 || !function->returnType ||
        !function->returnType->type || function->returnType->type->GetName()!=RED4ext::CName("Bool") ||
        function->returnType->type->GetSize()!=sizeof(bool)) return -1;
    bool result=false;
    return RED4ext::ExecuteFunction(static_cast<void*>(object),function,&result) ? int(result) : -1;
}
