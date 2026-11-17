#include "GpsRoute.hpp"
#include "VerifiedBuild.hpp"
#include <RED4ext/GameEngine.hpp>
#include <RED4ext/Scripting/Natives/Vector4.hpp>
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstring>
#include <limits>
#include <mutex>
#include <vector>

// GPS road routes for race mode checkpoints.
//
// The game computes GPS routes with a road query service. In the verified 2.5
// executable it is reached as [[GameInstance+0x78]+0x130]: the debug class
// NavigationFunctionalTests (natives RunGPSQuery 0x2f27904 and UpdateGPSQuery
// 0x2f27a30) reads it that way from its game system's GameInstance pointer
// (IGameSystem+0x40), and the game systems that plan vehicle drives store the
// same [..+0x78]+0x130 pointer when they attach.
//
// A query is built the way the player GPS builds its route (0x44d7af..0x44d86f):
// a 0x60-byte request {destination, open bounds, origin, open bounds, flags},
// turned into a 0xd0-byte query by 0x42ad34, submitted by 0x525948 (returns an
// id, -1 on failure) and destroyed by 0x42b378. The result is fetched later by
// 0x524bbc into a 0x68-byte result; while the route is still being computed it
// fails with the "Result not ready yet." message at 0x305d6f8. A ready result
// owns the route at +0x00: 12-byte points at route+0x30, count at route+0x3c.
// It is moved out with 0x524f44, cleared with 0x8dc110 and released by moving
// an empty pointer in, exactly as UpdateGPSQuery does between two routes.
namespace {
struct Signature
{
    uint32_t rva;
    std::array<unsigned char, 24> bytes;
};

constexpr uint32_t BuildQueryRva = 0x42ad34;
constexpr uint32_t DestroyQueryRva = 0x42b378;
constexpr uint32_t SubmitRva = 0x525948;
constexpr uint32_t FetchRva = 0x524bbc;
constexpr uint32_t ClearRouteRva = 0x8dc110;
constexpr uint32_t TakeRouteRva = 0x524f44;
constexpr uint32_t DestroyResultRva = 0x524f74;
constexpr uint32_t ResultArrayVtableRva = 0x310fd20;
constexpr uint32_t NotReadyTextRva = 0x305d6f8;
constexpr char NotReadyText[] = "Result not ready yet.";

constexpr uintptr_t GameInstanceServicesOffset = 0x78;
constexpr uintptr_t RoadQueryServiceOffset = 0x130;
constexpr uintptr_t ServiceGraphOffsetA = 0x48;
constexpr uintptr_t ServiceGraphOffsetB = 0x50;
constexpr size_t ServiceProbeSize = 0xd8;
constexpr size_t RequestSize = 0x60;
constexpr size_t QuerySize = 0xd0;
constexpr size_t ResultSize = 0x68;
constexpr size_t ResultMessageOffset = 0x58;
constexpr uintptr_t RoutePointsOffset = 0x30;
constexpr uintptr_t RouteCountOffset = 0x3c;
constexpr uint32_t MaxRoutePoints = 200000;

constexpr Signature Signatures[] = {
    {BuildQueryRva, {0x4c, 0x8b, 0xdc, 0x53, 0x48, 0x83, 0xec, 0x60, 0x48, 0x8b, 0x42, 0x50, 0x48, 0x8b, 0xd9, 0x33,
                     0xc9, 0x49, 0x89, 0x43, 0xb8, 0x48, 0x8b, 0x42}},
    {DestroyQueryRva, {0x40, 0x53, 0x48, 0x83, 0xec, 0x20, 0x48, 0x8b, 0xd9, 0x48, 0x81, 0xc1, 0xc0, 0x00, 0x00, 0x00,
                       0xe8, 0x33, 0x00, 0x00, 0x00, 0x48, 0x8d, 0x8b}},
    {SubmitRva, {0x48, 0x8b, 0xc4, 0x48, 0x89, 0x58, 0x08, 0x48, 0x89, 0x68, 0x10, 0x48, 0x89, 0x70, 0x18, 0x48,
                 0x89, 0x78, 0x20, 0x41, 0x56, 0x48, 0x83, 0xec}},
    {FetchRva, {0x48, 0x8b, 0xc4, 0x48, 0x89, 0x58, 0x08, 0x48, 0x89, 0x68, 0x18, 0x48, 0x89, 0x70, 0x20, 0x89,
                0x50, 0x10, 0x57, 0x41, 0x56, 0x41, 0x57, 0x48}},
    {ClearRouteRva, {0x40, 0x53, 0x48, 0x83, 0xec, 0x20, 0x83, 0x61, 0x0c, 0x00, 0x48, 0x8b, 0xd9, 0x83, 0x61, 0x1c,
                     0x00, 0x83, 0x61, 0x2c, 0x00, 0x83, 0x61, 0x3c}},
    {TakeRouteRva, {0x40, 0x53, 0x48, 0x83, 0xec, 0x20, 0x4c, 0x8b, 0x02, 0x48, 0x8b, 0xd9, 0x48, 0x83, 0x22, 0x00,
                    0x48, 0x8b, 0x01, 0x4c, 0x89, 0x01, 0x48, 0x8d}},
    {DestroyResultRva, {0x40, 0x53, 0x48, 0x83, 0xec, 0x20, 0x48, 0x8b, 0xd9, 0x48, 0x83, 0xc1, 0x08, 0xe8, 0xba, 0xa1,
                        0x1a, 0x00, 0x48, 0x8b, 0xcb, 0x48, 0x83, 0xc4}},
    // UpdateGPSQuery reading the route: count at +0x3c, 12-byte points at +0x30.
    {0x2f27af1, {0x39, 0x5f, 0x3c, 0x74, 0x71, 0x41, 0x89, 0x5e, 0x0c, 0x39, 0x5f, 0x3c, 0x76, 0x3a, 0xf3, 0x0f,
                 0x10, 0x0d, 0x29, 0x22, 0x89, 0x00, 0x48, 0x8d}},
    // RunGPSQuery reaching the service: [[system+0x40]+0x78]+0x130.
    {0x2f279cc, {0x48, 0x8b, 0x48, 0x40, 0x48, 0x8b, 0x41, 0x78, 0x48, 0x8d, 0x4d, 0xa0, 0x48, 0x8b, 0x98, 0x30,
                 0x01, 0x00, 0x00, 0x48, 0x8b, 0xd3, 0xe8, 0x4d}},
};

using BuildQueryFn = void* (*)(void* query, void* service, const void* request);
using DestroyFn = void (*)(void* object);
using SubmitFn = int32_t (*)(void* service, void* query);
using FetchFn = bool (*)(void* service, int32_t id, void* result);
using ClearRouteFn = void (*)(void* route);
using TakeRouteFn = void** (*)(void** holder, void** source);

struct Point
{
    float x, y, z;
};

struct PendingQuery
{
    int32_t id;
    float x, y, z;
};

RED4ext::v1::PluginHandle pluginHandle{};
const RED4ext::v1::Sdk* pluginSdk = nullptr;
std::atomic<int> buildState{0};
std::atomic<uintptr_t> loggedService{0};
std::atomic<uint32_t> startLogs{0}, pollLogs{0};
std::mutex routeLock;
std::vector<Point> lastRoute;
std::vector<PendingQuery> pendingQueries;

const unsigned char* ImageBase()
{
    return reinterpret_cast<const unsigned char*>(GetModuleHandleW(nullptr));
}

template <class T> T Native(uint32_t rva)
{
    return reinterpret_cast<T>(const_cast<unsigned char*>(ImageBase()) + rva);
}

bool ReadBytes(uintptr_t address, void* out, size_t size)
{
    if (address < 0x10000) return false;
    SIZE_T read = 0;
    return ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<LPCVOID>(address), out, size, &read) && read == size;
}

template <class T> bool ReadValue(uintptr_t address, T& out)
{
    return ReadBytes(address, &out, sizeof(out));
}

bool CheckBuild(const unsigned char* base)
{
    if (!base) return false;
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0 || dos->e_lfanew > 4096) return false;
    const auto* pe = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
    if (pe->Signature != IMAGE_NT_SIGNATURE || pe->FileHeader.TimeDateStamp != DncBuild::Timestamp ||
        pe->OptionalHeader.SizeOfImage != DncBuild::ImageSize) return false;
    for (const auto& signature : Signatures)
        if (std::memcmp(base + signature.rva, signature.bytes.data(), signature.bytes.size())) return false;
    return std::memcmp(base + NotReadyTextRva, NotReadyText, sizeof(NotReadyText)) == 0;
}

bool BuildOk()
{
    int state = buildState.load();
    if (state == 0) {
        state = CheckBuild(ImageBase()) ? 1 : -1;
        buildState = state;
        if (pluginSdk)
            pluginSdk->logger->InfoF(pluginHandle, "DNC_GPS_ROUTE build=%s", state > 0 ? "verified" : "unknown, route queries disabled");
    }
    return state > 0;
}

uintptr_t RoadQueryService()
{
    auto* engine = RED4ext::CGameEngine::Get();
    if (!engine || !engine->framework || !engine->framework->gameInstance) return 0;
    const auto gameInstance = reinterpret_cast<uintptr_t>(engine->framework->gameInstance);
    uintptr_t services = 0, service = 0, graphA = 0, graphB = 0;
    if (!ReadValue(gameInstance + GameInstanceServicesOffset, services) || !services) return 0;
    if (!ReadValue(services + RoadQueryServiceOffset, service) || !service) return 0;
    if (!ReadValue(service + ServiceGraphOffsetA, graphA) || !graphA) return 0;
    if (!ReadValue(service + ServiceGraphOffsetB, graphB) || !graphB) return 0;
    std::array<unsigned char, ServiceProbeSize> probe{};
    if (!ReadBytes(service, probe.data(), probe.size())) return 0;
    if (loggedService.exchange(service) != service && pluginSdk)
        pluginSdk->logger->InfoF(pluginHandle, "DNC_GPS_SERVICE game=%p services=%p service=%p",
                                 reinterpret_cast<void*>(gameInstance), reinterpret_cast<void*>(services),
                                 reinterpret_cast<void*>(service));
    return service;
}

template <size_t N> void SetText(char (&out)[N], const char* text)
{
    size_t i = 0;
    for (; text[i] && i + 1 < N; ++i) out[i] = text[i];
    out[i] = 0;
}

bool Finite(const RED4ext::Vector4& v)
{
    return std::isfinite(v.X) && std::isfinite(v.Y) && std::isfinite(v.Z) && std::fabs(v.X) < 1.0e6f &&
           std::fabs(v.Y) < 1.0e6f && std::fabs(v.Z) < 1.0e5f;
}

bool ReadRoute(void* route, std::vector<Point>& points)
{
    const auto address = reinterpret_cast<uintptr_t>(route);
    uint32_t count = 0;
    uintptr_t data = 0;
    if (!address || !ReadValue(address + RouteCountOffset, count) || !ReadValue(address + RoutePointsOffset, data))
        return false;
    if (count == 0 || count > MaxRoutePoints || !data) return false;
    points.resize(count);
    if (!ReadBytes(data, points.data(), sizeof(Point) * count)) return false;
    for (const auto& p : points)
        if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z)) return false;
    return true;
}

float Distance2(const Point& p, const PendingQuery& q)
{
    const float dx = p.x - q.x, dy = p.y - q.y, dz = p.z - q.z;
    return dx * dx + dy * dy + dz * dz;
}

void Start(RED4ext::IScriptable*, RED4ext::CStackFrame* frame, int32_t* out, int64_t)
{
    RED4ext::Vector4 from;
    RED4ext::Vector4 to;
    RED4ext::GetParameter(frame, &from);
    RED4ext::GetParameter(frame, &to);
    frame->code++; // ParamEnd
    int32_t id = -1;
    const char* note = "submitted";
    if (!Finite(from) || !Finite(to)) {
        note = "bad points";
    } else if (!BuildOk()) {
        note = "unverified build";
    } else if (const auto serviceAddress = RoadQueryService()) {
        auto* service = reinterpret_cast<void*>(serviceAddress);
        alignas(16) unsigned char request[RequestSize] = {};
        alignas(16) unsigned char query[QuerySize + 0x30] = {};
        const float open = std::numeric_limits<float>::infinity();
        const float destination[4] = {to.X, to.Y, to.Z, 1.0f};
        const float bounds[4] = {open, open, open, open};
        const float origin[4] = {from.X, from.Y, from.Z, 1.0f};
        std::memcpy(request + 0x00, destination, sizeof(destination));
        std::memcpy(request + 0x10, bounds, sizeof(bounds));
        std::memcpy(request + 0x20, origin, sizeof(origin));
        std::memcpy(request + 0x30, bounds, sizeof(bounds));
        request[0x40] = 1; // the player GPS route flags
        request[0x42] = 1;
        request[0x59] = 1;
        Native<BuildQueryFn>(BuildQueryRva)(query, service, request);
        query[0x79] = 0; // the player GPS clears this word after building
        query[0x7a] = 0;
        id = Native<SubmitFn>(SubmitRva)(service, query);
        Native<DestroyFn>(DestroyQueryRva)(query);
        if (id >= 0) {
            std::lock_guard lock(routeLock);
            if (pendingQueries.size() > 64) pendingQueries.erase(pendingQueries.begin());
            pendingQueries.push_back({id, from.X, from.Y, from.Z});
        } else {
            id = -1;
            note = "refused";
        }
    } else {
        note = "no road service";
    }
    if (pluginSdk && ++startLogs <= 12)
        pluginSdk->logger->InfoF(pluginHandle, "DNC_GPS_ROUTE_START id=%d %s from=(%.1f,%.1f,%.1f) to=(%.1f,%.1f,%.1f)", id,
                                 note, from.X, from.Y, from.Z, to.X, to.Y, to.Z);
    if (out) *out = id;
}

void Poll(RED4ext::IScriptable*, RED4ext::CStackFrame* frame, int32_t* out, int64_t)
{
    int32_t id = -1;
    RED4ext::GetParameter(frame, &id);
    frame->code++; // ParamEnd
    int32_t status = -2;
    char message[96] = "";
    std::vector<Point> points;
    if (id >= 0 && BuildOk()) {
        if (const auto serviceAddress = RoadQueryService()) {
            auto* service = reinterpret_cast<void*>(serviceAddress);
            const auto* base = ImageBase();
            alignas(16) unsigned char result[ResultSize + 0x18] = {};
            const auto arrayVtable = reinterpret_cast<uintptr_t>(base + ResultArrayVtableRva);
            std::memcpy(result + 0x08, &arrayVtable, sizeof(arrayVtable));
            std::memcpy(result + 0x18, &arrayVtable, sizeof(arrayVtable));
            std::memcpy(result + 0x28, &arrayVtable, sizeof(arrayVtable));
            if (Native<FetchFn>(FetchRva)(service, id, result)) {
                void* route = nullptr;
                Native<TakeRouteFn>(TakeRouteRva)(&route, reinterpret_cast<void**>(result));
                status = ReadRoute(route, points) ? static_cast<int32_t>(points.size()) : -1;
                if (route) {
                    Native<ClearRouteFn>(ClearRouteRva)(route);
                    void* empty = nullptr;
                    Native<TakeRouteFn>(TakeRouteRva)(&route, &empty);
                }
                if (status <= 0) {
                    status = -1;
                    SetText(message, "empty route");
                }
            } else {
                uintptr_t text = 0;
                std::memcpy(&text, result + ResultMessageOffset, sizeof(text));
                if (text == reinterpret_cast<uintptr_t>(base + NotReadyTextRva)) {
                    status = 0;
                } else {
                    status = -1;
                    if (!text || !ReadBytes(text, message, sizeof(message) - 1)) SetText(message, "no message");
                    message[sizeof(message) - 1] = 0;
                }
            }
            Native<DestroyFn>(DestroyResultRva)(result);
        }
    }
    if (status != 0) {
        std::lock_guard lock(routeLock);
        for (auto it = pendingQueries.begin(); it != pendingQueries.end(); ++it) {
            if (it->id != id) continue;
            // The route may come back destination first; hand it out origin first.
            if (status > 0 && Distance2(points.back(), *it) < Distance2(points.front(), *it))
                std::reverse(points.begin(), points.end());
            pendingQueries.erase(it);
            break;
        }
        if (status > 0) lastRoute = std::move(points);
    }
    if (pluginSdk && status != 0 && ++pollLogs <= 24) {
        if (status > 0) {
            std::lock_guard lock(routeLock);
            const auto& a = lastRoute.front();
            const auto& b = lastRoute.back();
            pluginSdk->logger->InfoF(pluginHandle, "DNC_GPS_ROUTE id=%d points=%d first=(%.1f,%.1f,%.1f) last=(%.1f,%.1f,%.1f)",
                                     id, status, a.x, a.y, a.z, b.x, b.y, b.z);
        } else {
            pluginSdk->logger->InfoF(pluginHandle, "DNC_GPS_ROUTE id=%d status=%d %s", id, status, message);
        }
    }
    if (out) *out = status;
}

void RoutePoint(RED4ext::IScriptable*, RED4ext::CStackFrame* frame, RED4ext::Vector4* out, int64_t)
{
    int32_t index = -1;
    RED4ext::GetParameter(frame, &index);
    frame->code++; // ParamEnd
    RED4ext::Vector4 point;
    {
        std::lock_guard lock(routeLock);
        if (index >= 0 && static_cast<size_t>(index) < lastRoute.size()) {
            const auto& p = lastRoute[static_cast<size_t>(index)];
            point = RED4ext::Vector4(p.x, p.y, p.z, 1.0f);
        }
    }
    if (out) *out = point;
}

void RED4EXT_CALL PostRegister()
{
    auto* rtti = RED4ext::CRTTISystem::Get();
    auto* start = RED4ext::CGlobalFunction::Create("DriveNoCollision_GpsRouteStart", "DriveNoCollision_GpsRouteStart", &Start);
    start->flags = {.isNative = true, .isStatic = true};
    start->AddParam("Vector4", "from");
    start->AddParam("Vector4", "to");
    start->SetReturnType("Int32");
    rtti->RegisterFunction(start);

    auto* poll = RED4ext::CGlobalFunction::Create("DriveNoCollision_GpsRoutePoll", "DriveNoCollision_GpsRoutePoll", &Poll);
    poll->flags = {.isNative = true, .isStatic = true};
    poll->AddParam("Int32", "id");
    poll->SetReturnType("Int32");
    rtti->RegisterFunction(poll);

    auto* point = RED4ext::CGlobalFunction::Create("DriveNoCollision_GpsRoutePoint", "DriveNoCollision_GpsRoutePoint", &RoutePoint);
    point->flags = {.isNative = true, .isStatic = true};
    point->AddParam("Int32", "index");
    point->SetReturnType("Vector4");
    rtti->RegisterFunction(point);
}
}

void GpsRoute::Register(RED4ext::v1::PluginHandle handle, const RED4ext::v1::Sdk* sdk)
{
    pluginHandle = handle;
    pluginSdk = sdk;
    RED4ext::CRTTISystem::Get()->AddPostRegisterCallback(&PostRegister);
}
