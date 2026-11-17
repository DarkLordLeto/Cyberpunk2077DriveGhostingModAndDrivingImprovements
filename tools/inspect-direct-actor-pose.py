import sys,pathlib,json,struct
root=pathlib.Path.cwd();sys.path.insert(0,str(root/'dependencies/analysis-python'))
import pefile,capstone
p=pefile.PE(r'D:\SteamLibrary\steamapps\common\Cyberpunk 2077\bin\x64\PhysX3_x64.dll');base=p.OPTIONAL_HEADER.ImageBase
assert(p.FILE_HEADER.TimeDateStamp,p.OPTIONAL_HEADER.SizeOfImage)==(1690925236,2625536)
md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_64)
result={}
for vt in [0x20b2e8,0x20b980]:
 target=struct.unpack('<Q',p.get_data(vt+0xa0,8))[0]-base
 lines=[]
 for i in md.disasm(p.get_data(target,1200),base+target):
  lines.append(f'{i.address-base:x} {i.mnemonic} {i.op_str}')
  if i.mnemonic=='ret':break
 result[hex(vt)]={'poseGetter':hex(target),'instructions':lines}
print(json.dumps(result,indent=2));(root/'artifacts/native-research/direct-actor-pose.json').write_text(json.dumps(result,indent=2))
