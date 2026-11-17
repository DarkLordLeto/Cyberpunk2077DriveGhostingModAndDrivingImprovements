import sys,pathlib,json,re,bisect
root=pathlib.Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'dependencies/analysis-python'))
import pefile,capstone
pe=pefile.PE(r'D:\SteamLibrary\steamapps\common\Cyberpunk 2077\bin\x64\Cyberpunk2077.exe');md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_64)
entries=sorted((e.struct.BeginAddress,e.struct.EndAddress) for e in pe.DIRECTORY_ENTRY_EXCEPTION);starts=[x[0] for x in entries];sec=pe.sections[0];data=sec.get_data();ranges=set();result={}
assert (pe.FILE_HEADER.TimeDateStamp,pe.OPTIONAL_HEADER.SizeOfImage)==(1792368047,92385280)
for m in re.finditer(rb'[\x48-\x4f]\xc1[\xe8-\xff][\x3e\x3f]',data):
 rva=sec.VirtualAddress+m.start();idx=bisect.bisect_right(starts,rva)-1
 if idx>=0 and entries[idx][0]<=rva<entries[idx][1]:start,end=entries[idx]
 else:
  pad=data.rfind(b'\xcc',max(0,m.start()-1000),m.start());stop=data.find(b'\xcc',m.end(),m.end()+1500)
  if pad<0 or stop<0:continue
  start,end=sec.VirtualAddress+pad+1,sec.VirtualAddress+stop
 if end-start<6000:ranges.add((start,end))
for start,end in sorted(ranges):
 lines=[f'{i.address:x} {i.mnemonic} {i.op_str}' for i in md.disasm(pe.get_data(start,end-start),pe.OPTIONAL_HEADER.ImageBase+start)]
 result[hex(start)]=lines
print('Candidates',len(result), 'addresses',list(result)[:40]);(root/'artifacts/native-research/filter-shift-candidates.json').write_text(json.dumps(result,indent=2))
