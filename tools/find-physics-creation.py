import sys,pathlib,json,struct,re,bisect
root=pathlib.Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'dependencies/analysis-python'))
import pefile,capstone
pe=pefile.PE(r'D:\SteamLibrary\steamapps\common\Cyberpunk 2077\bin\x64\Cyberpunk2077.exe');md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_64)
assert (pe.FILE_HEADER.TimeDateStamp,pe.OPTIONAL_HEADER.SizeOfImage)==(1792368047,92385280)
entries=sorted((e.struct.BeginAddress,e.struct.EndAddress) for e in pe.DIRECTORY_ENTRY_EXCEPTION);starts=[x[0] for x in entries];sec=pe.sections[0];data=sec.get_data();out={}
for m in re.finditer(rb'\xff\x15',data):
 pos=m.start();rva=sec.VirtualAddress+pos;target=rva+6+struct.unpack_from('<i',data,pos+2)[0]
 if target not in [0x2ff38d8,0x2ff38e0]:continue
 idx=bisect.bisect_right(starts,rva)-1
 if idx<0 or not entries[idx][0]<=rva<entries[idx][1]:continue
 start,end=entries[idx];lines=[f'{i.address:x} {i.mnemonic} {i.op_str}' for i in md.disasm(pe.get_data(start,end-start),pe.OPTIONAL_HEADER.ImageBase+start)];out[hex(start)]=lines
 if target==0x2ff38d8 or any('call qword ptr' in l and '+ 0x90]' in l for l in lines):print(hex(start),'iat',hex(target),'instructions',len(lines))
(root/'artifacts/native-research/physics-creation-callers.json').write_text(json.dumps(out,indent=2));print('total',len(out))
