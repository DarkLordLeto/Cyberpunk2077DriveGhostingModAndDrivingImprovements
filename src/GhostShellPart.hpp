#pragma once
#include <array>
#include <string_view>
namespace Dnc {
// Vehicle assets use underscore-delimited part names. Prefer hiding an unknown
// part to exposing seats, drivetrains or a second complete appearance mesh.
inline bool GhostShellPart(std::string_view name) {
    constexpr std::string_view hidden[]{"interior","inner","inside","liner","lining","trim","shadow","sticker","stickers","stripe","stripes","decal","decals","glass","window","windows","steering","spare","brake","caliper","engine","seat","dashboard","wiper","wipers","hinge","handle"};
    constexpr std::string_view shell[]{"body","bodywork","panel","panels","door","doors","hood","bonnet","trunk","boot","tailgate","bumper","fender","fenders","quarterpanel","roof","sidepanel","skirt","sideskirt","fairing","spoiler","fuelcap","mirror","wheel","wheels","tire","tyre","rim","rims","hubcap"};
    bool exterior=false;
    while(!name.empty()) {
        const auto end=name.find_first_of("_- .");
        auto token=name.substr(0,end);
        // Normalize ASCII asset tokens only. Avoid allocations in the game loop.
        std::array<char,32> lower{};
        if(token.size()<lower.size()) {
            for(size_t i=0;i<token.size();++i) lower[i]=token[i]>='A' && token[i]<='Z' ? char(token[i]-'A'+'a') : token[i];
            std::string_view word(lower.data(),token.size());
            for(auto excluded:hidden) if(word==excluded) return false;
            for(auto allowed:shell) if(word==allowed) exterior=true;
        }
        if(end==std::string_view::npos) break;
        name.remove_prefix(end+1);
    }
    return exterior;
}
}
