import sys,pathlib,json
root=pathlib.Path.cwd();sys.path.insert(0,str(root/'dependencies/analysis-python'))
import pefile
p=pefile.PE(r'D:\SteamLibrary\steamapps\common\Cyberpunk 2077\bin\x64\PhysX3_x64.dll')
assert(p.FILE_HEADER.TimeDateStamp,p.OPTIONAL_HEADER.SizeOfImage)==(1690925236,2625536)
rows=[]
for s in p.DIRECTORY_ENTRY_EXPORT.symbols:
 name=s.name.decode(errors='replace') if s.name else ''
 if any(x in name for x in ['getWorldBounds','getScene@','acquireReference','registerDeletionListener']): rows.append({'name':name,'rva':hex(s.address)})
print(json.dumps(rows,indent=2))
(root/'artifacts/native-research/actor-lifetime-exports.json').write_text(json.dumps(rows,indent=2))
