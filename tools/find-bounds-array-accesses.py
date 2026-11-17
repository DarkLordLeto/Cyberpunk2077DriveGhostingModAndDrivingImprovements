import sys,pathlib,json
root=pathlib.Path.cwd();sys.path.insert(0,str(root/'dependencies/analysis-python'))
import pefile,capstone
p=pefile.PE(r'D:\SteamLibrary\steamapps\common\Cyberpunk 2077\bin\x64\PhysX3_x64.dll');base=p.OPTIONAL_HEADER.ImageBase
assert(p.FILE_HEADER.TimeDateStamp,p.OPTIONAL_HEADER.SizeOfImage)==(1690925236,2625536)
md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_64);rows=[]
for e in p.DIRECTORY_ENTRY_EXCEPTION:
 a,b=e.struct.BeginAddress,e.struct.EndAddress;data=p.get_data(a,b-a)
 if b'\x18\x08\x00\x00' not in data:continue
 ins=list(md.disasm(data,base+a))
 if not any('0x818]' in i.op_str for i in ins):continue
 rows.append({'rva':hex(a),'instructions':[f'{i.address-base:x} {i.mnemonic} {i.op_str}' for i in ins]})
 print(hex(a),len(ins));print('\n'.join(f'{i.address-base:x} {i.mnemonic} {i.op_str}' for i in ins if '0x818]' in i.op_str or (any(x in i.op_str for x in ['+ 0x10]','+ 0x14]']) and i.mnemonic in ['cmp','mov','and'])))
(root/'artifacts/native-research/bounds-array-accesses.json').write_text(json.dumps(rows,indent=2))
