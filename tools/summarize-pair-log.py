"""Summarize observed callback correlation, preserving sample-limit uncertainty."""
import sys,re,json
from pathlib import Path
source=Path(sys.argv[1]);out=Path(sys.argv[2]);text=source.read_text(encoding='utf-8-sig');vehicles={};current=None;matches=[];counts=[]
for line in text.splitlines():
 m=re.search(r'DNC_LIVE_VEHICLE id=(\d+) class=(\S+) appearance=(\S+)',line)
 if m:current=dict(zip(('entity','class','appearance'),m.groups()))
 m=re.search(r'DNC_NATIVE_CHASSIS proxy=([0-9a-f]+)',line)
 if m and current:vehicles[m[1]]=current.copy()
 m=re.search(r'DNC_PAIR_PLAYER_MATCH pair=(\d+) actor0=([0-9a-f]+) actor1=([0-9a-f]+) observedPlayer=([0-9a-f]+).*filterFlags=([0-9a-f]+) pairFlags=([0-9a-f]+)',line)
 if m:
  assert m[4] in (m[2],m[3]),'Mismatched observer record'
  matches.append(dict(zip(('pair','actor0','actor1','player','filterFlags','pairFlags'),m.groups())))
 m=re.search(r'DNC_PAIR_COUNTS total=(\d+) playerMatches=(\d+)',line)
 if m:counts.append({'total':int(m[1]),'playerMatches':int(m[2])})
summary=[]
for proxy,vehicle in vehicles.items():
 saved=[m for m in matches if m['player']==proxy]
 summary.append(dict(proxy=proxy,**vehicle,saved_matches=len(saved),counterparts=sorted({m['actor1'] if m['actor0']==proxy else m['actor0'] for m in saved})))
report={'source':source.name,'attached':'DNC_PAIR_ATTACHED' in text,'detached':'DNC_PAIR_DETACHED' in text,'clean_plugin_unload':'DNC_UNLOADED' in text,'latest_periodic_counts':counts[-1] if counts else None,'saved_player_samples':len(matches),'sample_cap_reached':len(matches)>=64,'saved_pair_ids':sorted({int(m['pair']) for m in matches}),'vehicles':summary,'matches':matches,'warning_error_lines':[l for l in text.splitlines() if re.search(r'\[(?:warn\w*|error)\s*\]',l)]}
out.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({k:v for k,v in report.items() if k!='matches'},indent=2))
