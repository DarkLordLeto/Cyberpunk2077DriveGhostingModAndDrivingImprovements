#include "TryQueue.hpp"
#include <atomic>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <vector>
void Check(bool v,const char* m){if(!v){std::cerr<<m<<'\n';std::exit(1);}}
int main(){
    Dnc::TryQueue<unsigned,2> q;
    unsigned out=99;
    Check(!q.Pop(out) && out==99,"Empty queue modified output");
    for(unsigned i=0;i<10000;++i){
        Check(q.Push(i) && q.Push(i+1) && !q.Push(999),"Capacity behavior failed");
        Check(q.Pop(out) && out==i && q.Pop(out) && out==i+1,"FIFO/reuse failed");
    }
    Dnc::TryQueue<unsigned,32> concurrent;
    constexpr unsigned producers=4, each=12000;
    std::atomic<unsigned> done{0},accepted{0};
    std::vector<std::thread> threads;
    for(unsigned n=0;n<producers;++n) threads.emplace_back([&,n]{
        for(unsigned i=0;i<each;++i) if(concurrent.Push(n*each+i)) ++accepted;
        ++done;
    });
    std::vector<bool> seen(producers*each,false);
    unsigned consumed=0;
    auto take=[&](unsigned value){Check(value<seen.size() && !seen[value],"Invalid/duplicate queue item");seen[value]=true;++consumed;};
    while(done.load()!=producers){if(concurrent.Pop(out)) take(out);else std::this_thread::yield();}
    for(auto& t:threads)t.join();
    while(concurrent.Pop(out))take(out);
    Check(consumed==accepted.load(),"Accepted item lost");
    Check(concurrent.Push(7) && concurrent.Pop(out) && out==7,"Queue stopped accepting after drain");
    std::cout<<"Reusable FIFO and concurrent producer delivery passed: "<<consumed<<" items\n";
}
