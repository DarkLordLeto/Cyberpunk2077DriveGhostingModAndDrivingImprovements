import sys,pathlib,json,re,bisect
root=pathlib.Path(r'F:\Work\GameDev\2077Mod\DriveNoCollision');sys.path.insert(0,str(root/'dependencies/analysis-python'))
import pefile,capstone
pe=pefile.PE(r'D:\SteamLibrary\steamapps\common\Cyberpunk 2077\bin\x64\Cyberpunk2077.exe')
assert (pe.FILE_HEADER.TimeDateStamp,pe.OPTIONAL_HEADER.SizeOfImage)==(1792368047,92385280)
md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_64)
entries=sorted((e.struct.BeginAddress,e.struct.EndAddress) for e in pe.DIRECTORY_ENTRY_EXCEPTION); starts=[x[0] for x in entries]
sec=pe.sections[0];data=sec.get_data();found={}
for m in re.finditer(rb'\x0f\xba[\xe0-\xe7]\x3e',data):
 rva=sec.VirtualAddress+m.start();idx=bisect.bisect_right(starts,rva)-1
 if idx<0:continue
 start,end=entries[idx]
 if not start<=rva<end or end-start>5000:continue
 lines=[f'{i.address:x} {i.mnemonic} {i.op_str}' for i in md.disasm(pe.get_data(start,end-start),pe.OPTIONAL_HEADER.ImageBase+start)]
 found[hex(start)]=lines
(root/'artifacts/native-research/filter-pair-candidates.json').write_text(json.dumps(found,indent=2))
for key,lines in found.items():
 relevant=[l for l in lines if ' and ' in l or ' test ' in l or ' bt ' in l or ' shr ' in l]
 print(key,'instructions',len(lines),'\n'+'\n'.join(relevant[:12]))
print('Total',len(found))
