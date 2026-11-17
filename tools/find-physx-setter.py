import sys,pathlib,json,re,struct,bisect
root=pathlib.Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'dependencies/analysis-python'))
import pefile,capstone
out=[]
for name in ['Cyberpunk2077.exe','PhysX3_x64.dll']:
 pe=pefile.PE(str(pathlib.Path(r'D:\SteamLibrary\steamapps\common\Cyberpunk 2077\bin\x64')/name)); targets={}
 for sec in pe.sections:
  data=sec.get_data()
  for m in re.finditer(b'setSimulationFilterData',data):
   begin=data.rfind(b'\0',max(0,m.start()-250),m.start())+1;end=data.find(b'\0',m.end())
   targets[sec.VirtualAddress+begin]=data[begin:end].decode(errors='replace')
 entries=sorted((e.struct.BeginAddress,e.struct.EndAddress) for e in pe.DIRECTORY_ENTRY_EXCEPTION);starts=[x[0] for x in entries]
 md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_64)
 for sec in pe.sections:
  if not sec.Characteristics&0x20000000:continue
  data=sec.get_data()
  for m in re.finditer(rb'[\x48-\x4f]\x8d[\x05\x0d\x15\x1d\x25\x2d\x35\x3d]',data):
   pos=m.start();rva=sec.VirtualAddress+pos;target=rva+7+struct.unpack_from('<i',data,pos+3)[0]
   if target not in targets:continue
   idx=bisect.bisect_right(starts,rva)-1
   if idx<0 or not entries[idx][0]<=rva<entries[idx][1]:continue
   start,end=entries[idx];lines=[f'{i.address:x} {i.mnemonic} {i.op_str}' for i in md.disasm(pe.get_data(start,end-start),pe.OPTIONAL_HEADER.ImageBase+start)]
   out.append({'module':name,'rva':hex(start),'string':targets[target],'instructions':lines})
   print(name,hex(start),targets[target])
 print(name,'string targets',len(targets))
(root/'artifacts/native-research/physx-setter-string-refs.json').write_text(json.dumps(out,indent=2))
