#include "TrySnapshot.hpp"
#include <array>
#include <atomic>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <vector>
struct Frame { uint64_t sequence{}; std::array<uint64_t,64> values{}; };
void Check(bool value,const char* message){if(!value){std::cerr<<message<<'\n';std::exit(1);}}
int main() {
    Dnc::TrySnapshot<Frame> snapshot(Frame{});
    bool nestedRead=true,nestedWrite=true;
    Check(snapshot.Read([&](const auto&) {
        nestedRead=snapshot.Read([](const auto&){});
        nestedWrite=snapshot.Publish(Frame{});
    }),"Initial read failed");
    Check(!nestedRead && !nestedWrite,"Contended operations did not skip");
    try { snapshot.Read([](const auto&) { throw 1; }); } catch(int) {}
    Check(snapshot.Publish(Frame{}),"Exception retained publication lock");
    std::atomic<bool> start{false},done{false},bad{false};
    std::atomic<uint64_t> reads{0};
    std::vector<std::thread> readers;
    for(int n=0;n<4;++n) readers.emplace_back([&] {
        while(!start.load()) std::this_thread::yield();
        do {
            snapshot.Read([&](const auto& frame) {
                for(auto value:frame.values) if(value!=frame.sequence) bad.store(true);
                reads.fetch_add(1);
            });
        } while(!done.load());
    });
    start.store(true);
    while(reads.load()==0) std::this_thread::yield();
    uint64_t published=0;
    for(uint64_t i=1;i<=30000;++i) {
        Frame frame; frame.sequence=i; frame.values.fill(i);
        if(snapshot.Publish(frame)) ++published;
    }
    done.store(true);
    for(auto& reader:readers) reader.join();
    Check(!bad.load() && reads.load()>0,"Reader observed torn snapshot");
    Frame final;final.sequence=30001;final.values.fill(30001);
    Check(snapshot.Publish(final),"Final publication failed");
    Check(snapshot.Read([&](const auto& frame){Check(frame.sequence==30001,"Final snapshot stale");}),"Final read failed");
    std::cout<<"Concurrent coherent reads="<<reads.load()<<" publications="<<published<<"; contention skips passed\n";
}
