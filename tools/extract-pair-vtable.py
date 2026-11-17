import sys,pathlib,struct,json
root=pathlib.Path(r'F:\Work\GameDev\2077Mod\DriveNoCollision');sys.path.insert(0,str(root/'dependencies/analysis-python'))
import pefile
pe=pefile.PE(r'D:\SteamLibrary\steamapps\common\Cyberpunk 2077\bin\x64\Cyberpunk2077.exe',fast_load=True);rva=0x72f8de+0x2e722ca
rows=[{'slot':i*8,'target_rva':hex(v-pe.OPTIONAL_HEADER.ImageBase)} for i,v in enumerate(struct.unpack('<QQQQ',pe.get_data(rva,32)))];print(hex(rva),rows);(root/'artifacts/native-research/pair-callback-vtable.json').write_text(json.dumps({'vtable':hex(rva),'entries':rows},indent=2))
