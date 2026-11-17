import sys,pathlib,json
root=pathlib.Path(r'F:\Work\GameDev\2077Mod\DriveNoCollision');sys.path.insert(0,str(root/'dependencies/analysis-python'))
import pefile,capstone
pe=pefile.PE(r'D:\SteamLibrary\steamapps\common\Cyberpunk 2077\bin\x64\Cyberpunk2077.exe')
md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_64)
result={}
for rva in [0x3c8764,0x3209a0,0x619a78,0x619af4]:
 end=next((e.struct.EndAddress for e in pe.DIRECTORY_ENTRY_EXCEPTION if e.struct.BeginAddress==rva),rva+256)
 lines=[f'{i.address:x} {i.mnemonic} {i.op_str}' for i in md.disasm(pe.get_data(rva,min(end-rva,1400)),0x140000000+rva)]
 result[hex(rva)]=lines
 print(hex(rva),'\n'+'\n'.join(lines[:100]))
(root/'artifacts/native-research/group-bit-disassembly.json').write_text(json.dumps(result,indent=2))
