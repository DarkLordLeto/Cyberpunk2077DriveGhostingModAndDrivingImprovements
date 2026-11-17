import sys,pathlib,struct,json,re,bisect
root=pathlib.Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'dependencies/analysis-python'))
import pefile,capstone
pe=pefile.PE(r'D:\SteamLibrary\steamapps\common\Cyberpunk 2077\bin\x64\Cyberpunk2077.exe');base=pe.OPTIONAL_HEADER.ImageBase;md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_64)
sec=pe.sections[0];data=sec.get_data();targets={0x3f2abc8,0x3f2abd0};entries=sorted((e.struct.BeginAddress,e.struct.EndAddress) for e in pe.DIRECTORY_ENTRY_EXCEPTION);starts=[x[0] for x in entries];out={}
# These runtime identifiers reside in zero-initialized data; trace initialization instead.
assert (pe.FILE_HEADER.TimeDateStamp,pe.OPTIONAL_HEADER.SizeOfImage)==(1792368047,92385280)
for m in re.finditer(rb'[\x48-\x4f][\x8b\x89\x8d\x39\x3b][\x05\x0d\x15\x1d\x25\x2d\x35\x3d]',data):
 pos=m.start();rva=sec.VirtualAddress+pos;target=rva+7+struct.unpack_from('<i',data,pos+3)[0]
 if target not in targets:continue
 idx=bisect.bisect_right(starts,rva)-1
 if idx<0 or not entries[idx][0]<=rva<entries[idx][1]:continue
 start,end=entries[idx];lines=[f'{i.address:x} {i.mnemonic} {i.op_str}' for i in md.disasm(pe.get_data(start,end-start),base+start)];out[hex(start)]=lines
 print('reference',hex(rva),'function',hex(start))
(root/'artifacts/native-research/filter-flag-name-refs.json').write_text(json.dumps(out,indent=2))
