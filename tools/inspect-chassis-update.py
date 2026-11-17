import sys,pathlib,json
root=pathlib.Path(r'F:\Work\GameDev\2077Mod\DriveNoCollision');sys.path.insert(0,str(root/'dependencies/analysis-python'))
import pefile,capstone
pe=pefile.PE(r'D:\SteamLibrary\steamapps\common\Cyberpunk 2077\bin\x64\Cyberpunk2077.exe')
assert pe.FILE_HEADER.TimeDateStamp == 1792368047 and pe.OPTIONAL_HEADER.SizeOfImage == 92385280, 'Executable build differs; addresses require new review'
md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_64)
result={}
for rva in [0x81afa8,0x613258,0x612eac,0x22ca300,0x613320,0x612f34]:
 end=next((e.struct.EndAddress for e in pe.DIRECTORY_ENTRY_EXCEPTION if e.struct.BeginAddress==rva),rva+96)
 lines=[f'{i.address:x} {i.mnemonic} {i.op_str}' for i in md.disasm(pe.get_data(rva,end-rva),pe.OPTIONAL_HEADER.ImageBase+rva)]
 result[hex(rva)]=lines
 if rva in [0x613320,0x612f34]: print(hex(rva),'\n'+'\n'.join(lines[:180]))
(root/'artifacts/native-research/chassis-filter-update.json').write_text(json.dumps(result,indent=2))
