import sys,pathlib,json
root=pathlib.Path(r'F:\Work\GameDev\2077Mod\DriveNoCollision');sys.path.insert(0,str(root/'dependencies/analysis-python'))
import pefile
pe=pefile.PE(r'D:\SteamLibrary\steamapps\common\Cyberpunk 2077\bin\x64\Cyberpunk2077.exe',fast_load=True)
rows=[]
for bit,rva in [(63,0x26cdaf4+0x961454),(62,0x26cda28+0xed3bb8)]:
 name=pe.get_data(rva,100).split(b'\0')[0].decode();rows.append({'bit':bit,'string_rva':hex(rva),'property':name});print(bit,hex(rva),name)
(root/'artifacts/native-research/filter-flag-names.json').write_text(json.dumps(rows,indent=2))
