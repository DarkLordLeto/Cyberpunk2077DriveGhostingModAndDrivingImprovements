import sys,pathlib,json
root=pathlib.Path.cwd();sys.path.insert(0,str(root/'dependencies/analysis-python'))
import pefile,capstone
p=pefile.PE(r'D:\SteamLibrary\steamapps\common\Cyberpunk 2077\bin\x64\PhysX3_x64.dll');base=p.OPTIONAL_HEADER.ImageBase
assert(p.FILE_HEADER.TimeDateStamp,p.OPTIONAL_HEADER.SizeOfImage)==(1690925236,2625536)
md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_64)
result={}
for target,limit in [(0x68710,0x400),(0x4b650,0x200)]:
 lines=[]
 for i in md.disasm(p.get_data(target,limit),base+target):
  lines.append(f'{i.address-base:x} {i.mnemonic} {i.op_str}')
  if i.mnemonic=='ret':break
 result[hex(target)]=lines
(root/'artifacts/native-research/direct-bounds-full.json').write_text(json.dumps(result,indent=2))
for k,v in result.items():
 print(k,'instructions',len(v))
 print('\n'.join(x for x in v if 'call ' in x or 'jmp ' in x or 'ret ' in x))
