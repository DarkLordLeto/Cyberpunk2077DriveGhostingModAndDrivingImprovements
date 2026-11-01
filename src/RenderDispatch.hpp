#pragma once
#include <cstdint>
namespace Dnc {
struct RenderDispatch { uintptr_t table{}, visibility{}, dissolve{}; };
// Diagnostic only. Compare relative addresses with native visibility callers off-line.
template<class Read> bool ReadRenderDispatch(uintptr_t object,uintptr_t base,uintptr_t size,Read read,RenderDispatch& result) {
    if(object<65536 || (object&7) || !base || size<=0x38 || size>UINTPTR_MAX-base) return false;
    uintptr_t table=0,visible=0,dissolve=0;
    if(!read(object,table) || table<base || table>base+size-0x38 || (table&7)) return false;
    if(!read(table+0x28,visible) || !read(table+0x30,dissolve)) return false;
    if(visible<base || visible>=base+size || dissolve<base || dissolve>=base+size) return false;
    result={table-base,visible-base,dissolve-base};return true;
}
}
