"""Bounded PhysX disassembly plus verified direct-call references; no game execution."""
import sys,pathlib,json,struct,bisect
root=pathlib.Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'dependencies/analysis-python'))
import pefile,capstone
p=pefile.PE(r'D:\SteamLibrary\steamapps\common\Cyberpunk 2077\bin\x64\PhysX3_x64.dll');base=p.OPTIONAL_HEADER.ImageBase
assert(p.FILE_HEADER.TimeDateStamp,p.OPTIONAL_HEADER.SizeOfImage)==(1690925236,2625536)
md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_64)
entries=sorted((e.struct.BeginAddress,e.struct.EndAddress) for e in p.DIRECTORY_ENTRY_EXCEPTION);starts=[a for a,b in entries]
targets=[int(x,0) for x in sys.argv[1:]];result={}
for target in targets:
 index=bisect.bisect_right(starts,target)-1
 end=entries[index][1] if index>=0 and entries[index][0]==target else target+256
 ins=list(md.disasm(p.get_data(target,end-target),base+target));refs=[]
 for sec in p.sections:
  if not sec.Characteristics & 0x20000000:continue
  data=sec.get_data();pos=0
  while True:
   pos=data.find(b'\xe8',pos)
   if pos<0 or pos+5>len(data):break
   rva=sec.VirtualAddress+pos;dest=rva+5+struct.unpack_from('<i',data,pos+1)[0];pos+=1
   if dest!=target:continue
   ix=bisect.bisect_right(starts,rva)-1
   if ix<0:continue
   a,b=entries[ix]
   if a<=rva<b and any(i.address==base+rva and i.mnemonic=='call' for i in md.disasm(p.get_data(a,b-a),base+a)):
    refs.append({'call':hex(rva),'entry':hex(a),'end':hex(b)})
 result[hex(target)]={'initialUnwindEnd':hex(end),'instructions':[f'{i.address-base:x} {i.mnemonic} {i.op_str}' for i in ins],'callers':refs}
 print(hex(target),'initialInstructions',len(ins),'callers',refs)
(root/'artifacts/native-research/physx-targets.json').write_text(json.dumps(result,indent=2))
