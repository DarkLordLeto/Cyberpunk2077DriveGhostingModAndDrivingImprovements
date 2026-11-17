import sys,pathlib,json,re,bisect
root=pathlib.Path(r'F:\Work\GameDev\2077Mod\DriveNoCollision');sys.path.insert(0,str(root/'dependencies/analysis-python'))
import pefile,capstone
pe=pefile.PE(r'D:\SteamLibrary\steamapps\common\Cyberpunk 2077\bin\x64\Cyberpunk2077.exe')
section=pe.sections[0];data=section.get_data();md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_64)
entries=sorted((x.struct.BeginAddress,x.struct.EndAddress) for x in pe.DIRECTORY_ENTRY_EXCEPTION);starts=[x[0] for x in entries]
patterns={'chassis_filter':rb'\x8b\x81\x60\x01\x00\x00.{0,20}\x4c\x8d\x81\x6c\x01\x00\x00','bottom_mask':rb'[\x8b\x85].{0,2}\x74\x01\x00\x00'}
found={}
for name,pat in patterns.items():
 hits=[]
 for m in re.finditer(pat,data,re.DOTALL):
  rva=section.VirtualAddress+m.start(); idx=bisect.bisect_right(starts,rva)-1
  if idx<0: continue
  start,end=entries[idx]
  if start<=rva<end and end-start<3000:
   hits.append(hex(start));found[start]=end
 print(name,len(hits),hits[:25])
result={}
for start,end in list(found.items())[:45]:
 lines=[f'{i.address:x} {i.mnemonic} {i.op_str}' for i in md.disasm(pe.get_data(start,end-start),0x140000000+start)]
 result[hex(start)]=lines
(root/'artifacts/native-research/filter-update-candidates.json').write_text(json.dumps(result,indent=2))
print('Saved',len(result),'candidate functions')
