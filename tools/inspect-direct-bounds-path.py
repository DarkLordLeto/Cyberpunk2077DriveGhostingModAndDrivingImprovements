import sys,pathlib,json,struct
root=pathlib.Path.cwd();sys.path.insert(0,str(root/'dependencies/analysis-python'))
import pefile,capstone
p=pefile.PE(r'D:\SteamLibrary\steamapps\common\Cyberpunk 2077\bin\x64\PhysX3_x64.dll');base=p.OPTIONAL_HEADER.ImageBase
assert(p.FILE_HEADER.TimeDateStamp,p.OPTIONAL_HEADER.SizeOfImage)==(1690925236,2625536)
md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_64);md.detail=True
ends={e.struct.BeginAddress:e.struct.EndAddress for e in p.DIRECTORY_ENTRY_EXCEPTION}
result={'tables':[],'functions':{}}
for vt in [0x208000-0x48,0x208200-0x48,0x208400-0x48,0x20adf0-0x48,0x20b090-0x48,0x20b330-0x48,0x20b8b0-0x48,0x20b9c8-0x48]:
 row={'table':hex(vt),'slots':[]}
 for off in range(0,0x50,8):
  target=struct.unpack('<Q',p.get_data(vt+off,8))[0]-base
  if not 0<=target<p.OPTIONAL_HEADER.SizeOfImage:continue
  ins=list(md.disasm(p.get_data(target,min(ends.get(target,target+32)-target,48)),base+target))
  strings=[]
  for i in ins:
   for op in i.operands:
    if op.type==capstone.CS_OP_MEM and op.mem.base==capstone.x86.X86_REG_RIP:
     rva=i.address+i.size+op.mem.disp-base
     if 0<=rva<p.OPTIONAL_HEADER.SizeOfImage:
      raw=p.get_data(rva,100).split(b'\0')[0]
      if len(raw)>4 and all(32<=x<127 for x in raw): strings.append(raw.decode())
  row['slots'].append({'offset':hex(off),'target':hex(target),'strings':strings})
 result['tables'].append(row)
for target in [0x68710]:
 ins=list(md.disasm(p.get_data(target,ends[target]-target),base+target))
 result['functions'][hex(target)]=[f'{i.address-base:x} {i.mnemonic} {i.op_str}' for i in ins]
print(json.dumps(result,indent=2))
(root/'artifacts/native-research/direct-bounds-path.json').write_text(json.dumps(result,indent=2))
