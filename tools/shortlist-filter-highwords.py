import json,pathlib,re
p=pathlib.Path(r'F:\Work\GameDev\2077Mod\DriveNoCollision\artifacts\native-research');d=json.loads((p/'filter-abi-candidates.json').read_text());out={}
for k,a in d.items():
 if len(a)>250:continue
 if not any(re.search(r' (test|and) .*0x(3fffffff|40000000|80000000|c0000000)\b',l) for l in a):continue
 out[k]=a
 print(k,len(a))
(p/'filter-highword-shortlist.json').write_text(json.dumps(out,indent=2));print('count',len(out))
