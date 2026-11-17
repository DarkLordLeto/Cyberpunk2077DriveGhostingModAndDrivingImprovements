import sys,pathlib,json,re,bisect
root=pathlib.Path(r'F:\Work\GameDev\2077Mod\DriveNoCollision');sys.path.insert(0,str(root/'dependencies/analysis-python'))
import pefile,capstone
pe=pefile.PE(r'D:\SteamLibrary\steamapps\common\Cyberpunk 2077\bin\x64\Cyberpunk2077.exe')
assert (pe.FILE_HEADER.TimeDateStamp,pe.OPTIONAL_HEADER.SizeOfImage)==(1792368047,92385280)
md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_64)
sec=pe.sections[0];data=sec.get_data();entries=sorted((e.struct.BeginAddress,e.struct.EndAddress) for e in pe.DIRECTORY_ENTRY_EXCEPTION);starts=[x[0] for x in entries]
found={};ranges=set()
for m in re.finditer(rb'[\x48-\x4f]\x23[\x40-\x7f]\x08',data):
 rva=sec.VirtualAddress+m.start();idx=bisect.bisect_right(starts,rva)-1
 if idx>=0 and entries[idx][0]<=rva<entries[idx][1]:start,end=entries[idx]
 else:
  # Leaf functions may have no unwind record; use padding only as a search window, not a verified entry.
  start=sec.VirtualAddress+data.rfind(b'\xcc',max(0,m.start()-600),m.start())+1
  stop=data.find(b'\xcc',m.end(),m.end()+1000);end=sec.VirtualAddress+stop if stop>=0 else rva+200
 if end-start>5000 or start<sec.VirtualAddress:continue
 ranges.add((start,end))
for start,end in sorted(ranges):
 lines=[f'{i.address:x} {i.mnemonic} {i.op_str}' for i in md.disasm(pe.get_data(start,end-start),pe.OPTIONAL_HEADER.ImageBase+start)]
 hits=[l for l in lines if ' and ' in l and 'qword ptr [' in l]
 if len(hits)<2:continue
 found[hex(start)]=lines
 print(hex(start),len(lines),' '.join(hits[:4]))
(root/'artifacts/native-research/cross-mask-candidates.json').write_text(json.dumps(found,indent=2))
print('Candidates',len(found),'windows',len(ranges))
