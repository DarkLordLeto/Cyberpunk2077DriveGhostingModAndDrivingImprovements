import sys,pathlib,json
root=pathlib.Path(r'F:\Work\GameDev\2077Mod\DriveNoCollision')
sys.path.insert(0,str(root/'dependencies/analysis-python'))
import pefile
pe=pefile.PE(r'D:\SteamLibrary\steamapps\common\Cyberpunk 2077\bin\x64\Cyberpunk2077.exe',fast_load=True)
assert (pe.FILE_HEADER.TimeDateStamp, pe.OPTIONAL_HEADER.SizeOfImage) == (1792368047, 92385280), 'Build differs; manually review native bindings'
rows=json.loads((root/'artifacts/native-research/disassembly.json').read_text())
text=['#pragma once','#include <array>','#include <cstdint>','namespace DncBuild {',f'inline constexpr uint32_t Timestamp = {pe.FILE_HEADER.TimeDateStamp}u;',f'inline constexpr uint32_t ImageSize = {pe.OPTIONAL_HEADER.SizeOfImage}u;']
for row in rows:
 if row['name'] not in ['ChassisIsKinematic','ChassisLinearVelocity','ChassisAngularVelocity']: continue
 rva=int(row['rva'],16); code=pe.get_data(rva,24); name=row['name']
 text += [f'inline constexpr uint32_t {name}Rva = 0x{rva:x};',f'inline constexpr std::array<unsigned char,24> {name}Bytes = {{'+','.join(f'0x{x:02x}' for x in code)+'};']
# Read-only proxy state accessor traced from both kinematic and velocity getters.
rva=0x27d19c
code=pe.get_data(rva,24)
text += [f'inline constexpr uint32_t ProxyReadStateRva = 0x{rva:x};',
         'inline constexpr std::array<unsigned char,24> ProxyReadStateBytes = {'+','.join(f'0x{x:02x}' for x in code)+'};']
# Filter-resource getter and its singleton slot, traced through FilterLoadPreset.
rva=0x5bfa1c
code=pe.get_data(rva,12)
text += [f'inline constexpr uint32_t FilterResourceGetterRva = 0x{rva:x};',
         'inline constexpr std::array<unsigned char,12> FilterResourceGetterBytes = {'+','.join(f'0x{x:02x}' for x in code)+'};',
         'inline constexpr uint32_t PhysicsSingletonSlotRva = 0x3d32a80;']
# Full reviewed reader/wrapper ranges, including shape-count rejection branches.
for name,rva,size in [('ShapeStateReader',0x614668,2539),('PhysicalProxyReader',0x614568,176),('OtherProxyReader',0x614618,80)]:
 code=pe.get_data(rva,size)
 text += [f'inline constexpr uint32_t {name}Rva = 0x{rva:x};',f'inline constexpr std::array<unsigned char,{size}> {name}Bytes = {{'+','.join(f'0x{x:02x}' for x in code)+'};']
# PairFound body and registered callback table reviewed through scene creation.
rva=0xc0edc0
code=pe.get_data(rva,0x1a8)
text += [f'inline constexpr uint32_t PairFoundRva = 0x{rva:x};',
         'inline constexpr uint32_t PairCallbackTableRva = 0x35a1ba8;',
         f'inline constexpr std::array<unsigned char,{len(code)}> PairFoundBytes = {{'+','.join(f'0x{x:02x}' for x in code)+'};']
for name,rva,size in [('ProxyOwner',0x314a70,0x8d),('ResolveSharedProxy',0x27d430,0x132),('ReleaseSharedProxy',0x27d374,0x55)]:
 code=pe.get_data(rva,size)
 text += [f'inline constexpr uint32_t {name}Rva = 0x{rva:x};',f'inline constexpr std::array<unsigned char,{size}> {name}Bytes = {{'+','.join(f'0x{x:02x}' for x in code)+'};']
text += ['inline constexpr uint32_t ProxyManagerSlotRva = 0x3bfcdd0;', '}']
(root/'src/VerifiedBuild.hpp').write_text('\n'.join(text)+'\n')
print('Generated exact-build guards for 3 read-only functions')
