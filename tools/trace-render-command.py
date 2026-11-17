import pathlib,sys,struct,bisect,re
r=pathlib.Path(__file__).resolve().parents[1];sys.path.insert(0,str(r/'dependencies/analysis-python'));import pefile,capstone
p=pefile.PE(r'D:\SteamLibrary\steamapps\common\Cyberpunk 2077\bin\x64\Cyberpunk2077.exe');base=p.OPTIONAL_HEADER.ImageBase;md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_64); ends={e.struct.BeginAddress:e.struct.EndAddress for e in p.DIRECTORY_ENTRY_EXCEPTION}; starts=sorted(ends);target=int(sys.argv[1],16);print('target',hex(target));sec=next(s for s in p.sections if s.Name.startswith(b'.text'));buf=sec.get_data()
for s in p.sections:
 d=s.get_data();needle=struct.pack('<Q',base+target);pos=0
 while (hit:=d.find(needle,pos))!=-1:
  pos=hit+1;off=s.VirtualAddress+hit;print('pointer',hex(off),[hex(x-base) for x in struct.unpack('<4Q',p.get_data(off,32))]);
for m in re.finditer(rb'[\xe8\xe9]',buf):
 pos=m.start();ref=sec.VirtualAddress+pos
 if ref+5+struct.unpack_from('<i',buf,pos+1)[0]==target:print('branch',hex(ref),'function',hex(starts[bisect.bisect_right(starts,ref)-1]))
if len(sys.argv)>2:
 for i in md.disasm(p.get_data(target,int(sys.argv[2],16)),base+target):print(f'{i.address-base:x} {i.mnemonic} {i.op_str}')
