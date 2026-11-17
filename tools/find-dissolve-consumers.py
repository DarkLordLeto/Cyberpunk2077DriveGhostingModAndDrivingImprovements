import sys, pathlib,struct,bisect,re,json
root=pathlib.Path.cwd();sys.path.insert(0,str(root/'dependencies/analysis-python'));import pefile,capstone
p=pefile.PE(r'D:\SteamLibrary\steamapps\common\Cyberpunk 2077\bin\x64\Cyberpunk2077.exe',fast_load=True);b=p.OPTIONAL_HEADER.ImageBase;md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_64);t=next(x for x in p.sections if x.Name.startswith(b'.text'));pdata=next(x for x in p.sections if x.Name.startswith(b'.pdata')).get_data();entries=[x for x in struct.iter_unpack('<III',pdata[:len(pdata)//12*12]) if x[0]>0];entries.sort();starts=[x[0] for x in entries];raw=t.get_data();seen=set();out={}
for m in re.finditer(rb'\xf1\x00\x00\x00',raw):
 r=t.VirtualAddress+m.start();idx=bisect.bisect_right(starts,r)-1
 if idx<0:continue
 a,e,_=entries[idx]
 if not a<=r<e or a in seen:continue
 seen.add(a);ins=list(md.disasm(raw[a-t.VirtualAddress:e-t.VirtualAddress],b+a));lines=[f'{i.address-b:x} {i.mnemonic} {i.op_str}' for i in ins];matches=[n for n,s in enumerate(lines) if '+ 0xf1]' in s and 'rsp' not in s and 'rbp' not in s and ('byte ptr' in s)]
 for n in matches:
  if any(x in ' '.join(lines[n:n+6]) for x in [', 0x10',', 4',', 0xef','0x4000','0x8000']):out[hex(a)]=lines;print(hex(a),' '.join(lines[n:n+4])[:210]);break
(root/'artifacts/native-research/dissolve-consumers.json').write_text(json.dumps(out,indent=2))
