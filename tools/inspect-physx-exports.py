import sys,pathlib,json
root=pathlib.Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'dependencies/analysis-python'))
import pefile
p=pefile.PE(r'D:\SteamLibrary\steamapps\common\Cyberpunk 2077\bin\x64\PhysX3_x64.dll')
rows=[]
for s in p.DIRECTORY_ENTRY_EXPORT.symbols:
 name=s.name.decode(errors='replace') if s.name else ''
 if any(x.lower() in name.lower() for x in ['SimulationFilter','NpShape','resetFiltering','filterShader']):rows.append({'name':name,'rva':hex(s.address)})
print(json.dumps(rows[:30],indent=2));print('matches',len(rows))
(root/'artifacts/native-research/physx-filter-exports.json').write_text(json.dumps(rows,indent=2))
