#include "FadeContacts.hpp"
#include "GhostVisual.hpp"
#include "RenderDispatch.hpp"
#include "TryQueue.hpp"
#include "VerifiedBuild.hpp"
#include <array>
#include <chrono>
#include <algorithm>
#include <cstring>
namespace {
Dnc::TryQueue<FadeContact,128> contacts;
struct Target {
    FadeContact contact{};
    bool active{},overlapping{},inspected{};
    uint64_t nextSample{},lastValid{};
    GhostVisual visual;
    std::chrono::steady_clock::time_point retryAt{};
};
std::array<Target,8> targets{};

}
void QueueFadeContact(FadeContact contact) {contacts.Push(contact);}
void UpdateFadeContacts(uint64_t mount,uint64_t now,RED4ext::v1::PluginHandle h,const RED4ext::v1::Sdk* sdk) {
    auto retire=[&](Target& t){RestoreGhostVisual(t.visual,h,sdk);t={};};
    PrepareGhostDonor(static_cast<uint32_t>(mount)!=0xffffffff);
    FadeContact contact{};
    for(unsigned budget=0;budget<128 && contacts.Pop(contact);++budget) {
        if(contact.mount!=mount || now<contact.at || now-contact.at>500 || !contact.traffic.Valid() || contact.driver==contact.traffic) continue;
        Target* available=nullptr;
        for(auto& target:targets) {
            if(target.active && target.contact.mount==mount && target.contact.traffic==contact.traffic){available=&target;break;}
            if(!target.active && !available) available=&target;
        }
        if(available && !available->active) *available={contact,true,false,false,0,now};
        else if(available) available->contact.at=contact.at;
    }
    Dnc::WorldBounds player{};uint32_t n=0;bool attempted=false,valid=false;
    for(auto& target:targets) {
        if(!target.active) continue;
        auto& hit=target.contact;
        if(hit.mount!=mount){retire(target);continue;}
        if(now<target.nextSample) continue;
        target.nextSample=target.overlapping ? now : now+16;
        if(!attempted){valid=ReadVerifiedProxyBounds(hit.driver.proxy,player,n);attempted=true;}
        Dnc::WorldBounds traffic{};uint32_t bodies=0;
        if(!valid || !ReadVerifiedProxyBounds(hit.traffic.proxy,traffic,bodies)) {
            if(!target.visual.changed.empty() || now-target.lastValid>500) retire(target);
            continue;
        }
        target.lastValid=now;
        const bool overlapping=Dnc::CompareBounds(player,traffic,0.0)==Dnc::BoundsRelation::Overlapping;
        if(overlapping!=target.overlapping) {
            target.overlapping=overlapping;
            sdk->logger->InfoF(h,"DNC_FADE_TARGET entity=%llu overlap=%u desiredOpacity=%.2f bodies=%u:%u",static_cast<unsigned long long>(hit.traffic.entity),unsigned(overlapping),overlapping?0.5:1.0,n,bodies);
            if(!overlapping){retire(target);continue;}
        }
        if(!overlapping && now-hit.at>300) retire(target);
    }
}
void InspectFadeTarget(Dnc::VehicleIdentity id,RED4ext::ent::Entity* entity,RED4ext::v1::PluginHandle h,const RED4ext::v1::Sdk* sdk) {
    Target* found=nullptr;
    for(auto& t:targets) if(t.active && t.overlapping && !t.inspected && t.contact.traffic==id){found=&t;break;}
    if(!found || !entity || entity->components.size()>512) return;
    const auto now=std::chrono::steady_clock::now();
    if(now<found->retryAt) return;
    found->retryAt=now+std::chrono::milliseconds(300);
    found->inspected=ApplyGhostVisual(found->visual,entity,h,sdk);
}
