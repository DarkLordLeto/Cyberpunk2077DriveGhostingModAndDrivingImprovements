import sys,pathlib,json,struct,bisect
root=pathlib.Path.cwd();sys.path.insert(0,str(root/'dependencies/analysis-python'))
import pefile,capstone
p=pefile.PE(r'D:\SteamLibrary\steamapps\common\Cyberpunk 2077\bin\x64\PhysX3_x64.dll')
assert(p.FILE_HEADER.TimeDateStamp,p.OPTIONAL_HEADER.SizeOfImage)==(1690925236,2625536)
base=p.OPTIONAL_HEADER.ImageBase; raw=p.__data__; rows=[]
md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_64)
entries={e.struct.BeginAddress:e.struct.EndAddress for e in p.DIRECTORY_ENTRY_EXCEPTION}
for name in [b'.?AVNpRigidDynamic@physx@@',b'.?AVNpRigidStatic@physx@@']:
 at=raw.find(name)
 if at<0: continue
 td=p.get_rva_from_offset(at)-16
 for sec in p.sections:
  data=sec.get_data();start=0
  while True:
   hit=data.find(struct.pack('<I',td),start)
   if hit<0:break
   start=hit+1
   if hit<12 or hit+12>len(data):continue
   col=sec.VirtualAddress+hit-12
   signature,offset,cd,typ,hierarchy,selfrva=struct.unpack_from('<6I',data,hit-12)
   if signature!=1 or selfrva!=col:continue
   for tableSec in p.sections:
    tabledata=tableSec.get_data();pos=0
    while True:
     ref=tabledata.find(struct.pack('<Q',base+col),pos)
     if ref<0:break
     pos=ref+1;vt=tableSec.VirtualAddress+ref+8
     target=struct.unpack('<Q',p.get_data(vt+0x48,8))[0]-base
     end=entries.get(target,target+256)
     ins=list(md.disasm(p.get_data(target,end-target),base+target))
     rows.append({'type':name.decode(),'offset':offset,'vtable':hex(vt),'boundsSlotTarget':hex(target),'unwindBounded':target in entries,'instructions':[f'{i.address-base:x} {i.mnemonic} {i.op_str}' for i in ins]})
print(json.dumps(rows,indent=2))
(root/'artifacts/native-research/actor-bounds-vtables.json').write_text(json.dumps(rows,indent=2))
