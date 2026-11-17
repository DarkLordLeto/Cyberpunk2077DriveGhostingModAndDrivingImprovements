import sys,pathlib,json
root=pathlib.Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'dependencies/analysis-python'))
import pefile
pe=pefile.PE(r'D:\SteamLibrary\steamapps\common\Cyberpunk 2077\bin\x64\Cyberpunk2077.exe');out=[]
for lib in pe.DIRECTORY_ENTRY_IMPORT:
 for i in lib.imports:
  name=i.name.decode() if i.name else ''
  if 'physx' in lib.dll.decode().lower() or 'PxCreate' in name:
   out.append({'dll':lib.dll.decode(),'name':name,'iat_rva':hex(i.address-pe.OPTIONAL_HEADER.ImageBase)})
print(json.dumps(out[:45],indent=2));(root/'artifacts/native-research/physx-imports.json').write_text(json.dumps(out,indent=2))
