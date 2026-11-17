import json, pathlib, re
p=pathlib.Path(r'F:\Work\GameDev\2077Mod\DriveNoCollision')
data=json.loads(pathlib.Path(r'D:\SteamLibrary\steamapps\common\Cyberpunk 2077\bin\x64\cyberpunk2077_addresses.json').read_text())
pattern=re.compile(r'(ChassisComponent::.*(Proxy|Body|Filter)|PhysicalBodyInterface::.*Filter|SimulationFilter|CollisionFilter|RenderProxy.*(Fade|Opacity)|Mesh.*(Fade|Opacity))',re.I)
rows=[a for a in data['Addresses'] if pattern.search(a.get('symbol',''))]
(p/'artifacts/native-research/physics-symbols.json').write_text(json.dumps(rows,indent=2))
for a in rows[:85]: print(a.get('hash'), a.get('offset'), a.get('symbol'))
print('Total matches:',len(rows))
