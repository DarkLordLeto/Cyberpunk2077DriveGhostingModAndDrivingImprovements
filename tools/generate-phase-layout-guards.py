import pathlib,sys
root=pathlib.Path.cwd();sys.path.insert(0,str(root/'dependencies/analysis-python'))
import pefile
p=pefile.PE(r'D:\SteamLibrary\steamapps\common\Cyberpunk 2077\bin\x64\PhysX3_x64.dll')
assert(p.FILE_HEADER.TimeDateStamp,p.OPTIONAL_HEADER.SizeOfImage)==(1690925236,2625536)
regions=[('BoundsSizeLoop',0xf8610,0xaa),('ElementInit',0x148b70,0x110),('ActorShapes',0x12b2b0,0x62),('BoundsUpdate',0x128f60,0x139)]
lines=['#pragma once','#include <array>','namespace DncPhaseBuild {']
for name,rva,size in regions:
 data=p.get_data(rva,size);lines += [f'inline constexpr unsigned {name}Rva=0x{rva:x};',f'inline constexpr std::array<unsigned char,{size}> {name}Bytes={{'+','.join(f'0x{x:02x}' for x in data)+'};']
lines.append('}')
(root/'src/VerifiedPhaseLayout.hpp').write_text('\n'.join(lines)+'\n')
