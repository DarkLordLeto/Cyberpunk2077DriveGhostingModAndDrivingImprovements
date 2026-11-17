import sys,pathlib,json,re,bisect
root=pathlib.Path(r'F:\Work\GameDev\2077Mod\DriveNoCollision');sys.path.insert(0,str(root/'dependencies/analysis-python'))
import pefile,capstone
pe=pefile.PE(r'D:\SteamLibrary\steamapps\common\Cyberpunk 2077\bin\x64\Cyberpunk2077.exe');md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_64)
assert (pe.FILE_HEADER.TimeDateStamp,pe.OPTIONAL_HEADER.SizeOfImage)==(1792368047,92385280)
entries=sorted((e.struct.BeginAddress,e.struct.EndAddress) for e in pe.DIRECTORY_ENTRY_EXCEPTION); starts=[x[0] for x in entries]
sec=pe.sections[0];data=sec.get_data();ranges=set();out={}
for m in re.finditer(rb'\xff[\x90-\x97]\xa8\x00\x00\x00',data):
 rva=sec.VirtualAddress+m.start();idx=bisect.bisect_right(starts,rva)-1
 if idx>=0 and entries[idx][0]<=rva<entries[idx][1] and entries[idx][1]-entries[idx][0]<15000:ranges.add(entries[idx])
for start,end in sorted(ranges):
 b=pe.get_data(start,end-start)
 # Filter reader expected to branch by state and copy 16-byte filter data, with both sim/query getters.
 if b'\xb8\x00\x00\x00' not in b:continue
 lines=[f'{i.address:x} {i.mnemonic} {i.op_str}' for i in md.disasm(b,pe.OPTIONAL_HEADER.ImageBase+start)]
 if not any('call qword ptr' in l and '+ 0xb8]' in l for l in lines):continue
 out[hex(start)]=lines
 print(hex(start),'bytes',end-start,'instructions',len(lines))
(root/'artifacts/native-research/proxy-reader-candidates.json').write_text(json.dumps(out,indent=2));print('Total',len(out))
