"""Summarize bounded shape-probe sessions without conflating resource and live shapes."""
import json,re,sys
from pathlib import Path
source=Path(sys.argv[1]); output=Path(sys.argv[2]);text=source.read_text(encoding='utf-8-sig')
vehicle=None;scan=None;scans=[]
for line in text.splitlines():
 m=re.search(r'DNC_LIVE_VEHICLE id=(\d+) class=(\S+) appearance=(\S+)',line)
 if m: vehicle=dict(zip(('id','class','appearance'),m.groups()))
 m=re.search(r'DNC_SHAPE_SCAN_BEGIN body=(\d+) limit=(\d+) bottomMask=([0-9a-fA-F]+)',line)
 if m:
  scan={'vehicle':vehicle,'body':int(m[1]),'limit':int(m[2]),'bottom_mask':int(m[3],16),'shapes':[],'complete':False};scans.append(scan)
 m=re.search(r'DNC_SHAPE_FILTER body=(\d+) shape=(\d+) bottom=(\d+) mask=([0-9a-fA-F]+):([0-9a-fA-F]+)',line)
 if m and scan:
  assert int(m[1])==scan['body']
  index=int(m[2]);assert index==len(scan['shapes']), 'Missing or repeated shape index'
  bottom=bool(int(m[3]));assert bottom==(index<32 and bool(scan['bottom_mask']&(1<<index)))
  scan['shapes'].append({'index':index,'bottom':bottom,'mask1':m[4],'mask2':m[5],
   'registerForContacts':bool(int(m[5],16)&(1<<63)),
   'registerForPersistentContacts':bool(int(m[5],16)&(1<<62))})
 m=re.search(r'DNC_SHAPE_SCAN_END body=(\d+) accepted=(\d+)(?: firstRejected=(\d+))? capped=(\d+)',line)
 if m and scan:
  assert int(m[2])==len(scan['shapes'])
  scan.update(complete=True,accepted=int(m[2]),first_rejected=int(m[3]) if m[3] else None,capped=bool(int(m[4])))
  scan=None
report={'source':source.name,'clean_plugin_unload':'DNC_UNLOADED' in text,'scans':scans}
assert scans and all(s['complete'] for s in scans),'Missing or unfinished scans'
output.write_text(json.dumps(report,indent=2)+'\n')
for s in scans:
 print(s['vehicle']['appearance'], 'accepted',s['accepted'],'bottom indices',[v['index'] for v in s['shapes'] if v['bottom']], 'capped',s['capped'])
print('Clean plugin unload:',report['clean_plugin_unload'])
