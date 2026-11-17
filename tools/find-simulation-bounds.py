import sys,pathlib,json
root=pathlib.Path.cwd();sys.path.insert(0,str(root/'dependencies/analysis-python'))
import pefile,capstone
p=pefile.PE(r'D:\SteamLibrary\steamapps\common\Cyberpunk 2077\bin\x64\PhysX3_x64.dll');base=p.OPTIONAL_HEADER.ImageBase
assert(p.FILE_HEADER.TimeDateStamp,p.OPTIONAL_HEADER.SizeOfImage)==(1690925236,2625536)
md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_64);rows=[]
for e in p.DIRECTORY_ENTRY_EXCEPTION:
 a,b=e.struct.BeginAddress,e.struct.EndAddress
 if not 0x125000<=a<0x12c000:continue
 ins=list(md.disasm(p.get_data(a,b-a),base+a))
 if not any('0x1fffffff' in i.op_str for i in ins):continue
 lines=[f'{i.address-base:x} {i.mnemonic} {i.op_str}' for i in ins]
 rows.append({'rva':hex(a),'instructions':lines})
 print(hex(a),len(lines));print('\n'.join(x for x in lines if 'call ' in x or '0x1fffffff' in x or 'lea ' in x))
(root/'artifacts/native-research/shape-simulation-bounds-candidates.json').write_text(json.dumps(rows,indent=2))
