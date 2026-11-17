import json,pathlib,re
p=pathlib.Path(r'F:\Work\GameDev\2077Mod\DriveNoCollision\artifacts\native-research');d=json.loads((p/'filter-abi-candidates.json').read_text());out={}
for k,a in d.items():
 if len(a)>220:continue
 if not any('[rdx + 8]' in l or '[rdx + 0xc]' in l for l in a):continue
 if sum(' and ' in l for l in a)<2:continue
 out[k]=a
 if len(out)<=20: print(k,len(a))
(p/'filter-abi-shortlist.json').write_text(json.dumps(out,indent=2));print('count',len(out))
