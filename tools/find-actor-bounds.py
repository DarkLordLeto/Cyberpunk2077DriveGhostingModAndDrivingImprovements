import sys,pathlib,json,struct
root=pathlib.Path.cwd();sys.path.insert(0,str(root/'dependencies/analysis-python'))
import pefile,capstone
p=pefile.PE(r'D:\SteamLibrary\steamapps\common\Cyberpunk 2077\bin\x64\PhysX3_x64.dll');base=p.OPTIONAL_HEADER.ImageBase
assert(p.FILE_HEADER.TimeDateStamp,p.OPTIONAL_HEADER.SizeOfImage)==(1690925236,2625536)
md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_64); entries={e.struct.BeginAddress:e.struct.EndAddress for e in p.DIRECTORY_ENTRY_EXCEPTION}
# Getter returns a struct through RDX and uses XMM2 for its inflation argument.
rows=[]
for start,end in entries.items():
 if end-start>700 or end-start<80: continue
 ins=list(md.disasm(p.get_data(start,end-start),base+start))
 if not any('xmm2' in i.op_str and i.mnemonic in ('mulss','mulps','movaps','movss','shufps') for i in ins):continue
 if not any(i.mnemonic=='call' for i in ins):continue
 refs=[]
 for sec in p.sections:
  if sec.Characteristics & 0x20000000:continue
  data=sec.get_data();needle=struct.pack('<Q',base+start);pos=0
  while True:
   pos=data.find(needle,pos)
   if pos<0:break
   refs.append(hex(sec.VirtualAddress+pos));pos+=1
 if not refs:continue
 rows.append({'rva':hex(start),'references':refs,'instructions':[f'{i.address-base:x} {i.mnemonic} {i.op_str}' for i in ins]})
(root/'artifacts/native-research/actor-bounds-candidates.json').write_text(json.dumps(rows,indent=2))
print('candidates',len(rows))
for r in rows:print(r['rva'],r['references'],'instructions',len(r['instructions']))
