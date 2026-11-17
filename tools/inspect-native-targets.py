"""Bounded offline disassembly and direct-call references for the verified game build."""
import sys,pathlib,json,bisect,struct
root=pathlib.Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'dependencies/analysis-python'))
import pefile,capstone
pe=pefile.PE(r'D:\SteamLibrary\steamapps\common\Cyberpunk 2077\bin\x64\Cyberpunk2077.exe')
assert (pe.FILE_HEADER.TimeDateStamp,pe.OPTIONAL_HEADER.SizeOfImage)==(1792368047,92385280)
md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_64)
entries=sorted((e.struct.BeginAddress,e.struct.EndAddress) for e in pe.DIRECTORY_ENTRY_EXCEPTION);starts=[x[0] for x in entries]
sec=pe.sections[0];data=sec.get_data();targets={int(x,0) for x in sys.argv[1:]};refs={x:[] for x in targets}
pos=0
while True:
 pos=data.find(b'\xe8',pos)
 if pos<0 or pos+5>len(data):break
 rva=sec.VirtualAddress+pos;dest=rva+5+struct.unpack_from('<i',data,pos+1)[0]
 if dest in targets:
  idx=bisect.bisect_right(starts,rva)-1
  if idx>=0 and entries[idx][0]<=rva<entries[idx][1]:
   start,end=entries[idx]
   # Reject opcode-like bytes inside another instruction.
   if any(i.address==pe.OPTIONAL_HEADER.ImageBase+rva and i.mnemonic=='call' for i in md.disasm(pe.get_data(start,end-start),pe.OPTIONAL_HEADER.ImageBase+start)):
    refs[dest].append({'call':hex(rva),'function':hex(start)})
 pos+=1
result={}
for target in sorted(targets):
 idx=bisect.bisect_right(starts,target)-1
 bounded=idx>=0 and entries[idx][0]==target
 end=entries[idx][1] if bounded else target+256
 lines=[f'{i.address:x} {i.mnemonic} {i.op_str}' for i in md.disasm(pe.get_data(target,min(end-target,16000)),pe.OPTIONAL_HEADER.ImageBase+target)]
 result[hex(target)]={'unwind_entry':bounded,'instructions':lines,'callers':refs[target]}
 print(hex(target),'instructions',len(lines),'callers',refs[target][:25])
(root/'artifacts/native-research/target-analysis.json').write_text(json.dumps(result,indent=2))
