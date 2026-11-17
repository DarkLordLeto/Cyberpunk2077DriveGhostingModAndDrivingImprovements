import sys,pathlib,json,bisect,struct
root=pathlib.Path(r'F:\Work\GameDev\2077Mod\DriveNoCollision');sys.path.insert(0,str(root/'dependencies/analysis-python'))
import pefile,capstone
pe=pefile.PE(r'D:\SteamLibrary\steamapps\common\Cyberpunk 2077\bin\x64\Cyberpunk2077.exe')
md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_64)
result={}
for entry in pe.DIRECTORY_ENTRY_EXCEPTION:
 start,end=entry.struct.BeginAddress,entry.struct.EndAddress
 if end-start>12000: continue
 data=pe.get_data(start,end-start)
 if b'\x60\x01\x00\x00' not in data or b'\x74\x01\x00\x00' not in data: continue
 lines=[f'{i.address:x} {i.mnemonic} {i.op_str}' for i in md.disasm(data,pe.OPTIONAL_HEADER.ImageBase+start)]
 if not any(' + 0x160]' in x for x in lines) or not any(' + 0x174]' in x for x in lines): continue
 result[hex(start)]=lines
 print(hex(start),end-start)
(root/'artifacts/native-research/filter-update-refined.json').write_text(json.dumps(result,indent=2))
print('Preserved flag masks:',[hex(v) for v in struct.unpack('<QQ',pe.get_data(0x36f5038,16))])
print('Candidates',len(result))
