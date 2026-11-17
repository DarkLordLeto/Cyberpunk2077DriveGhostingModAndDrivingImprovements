from pathlib import Path
p=Path(r'F:\Work\GameDev\2077Mod\DriveNoCollision')
f=p/'tools/summarize-shape-log.py';s=f.read_text();old="scan['shapes'].append({'index':index,'bottom':bottom,'mask1':m[4],'mask2':m[5]})";new="""scan['shapes'].append({'index':index,'bottom':bottom,'mask1':m[4],'mask2':m[5],
   'registerForContacts':bool(int(m[5],16)&(1<<63)),
   'registerForPersistentContacts':bool(int(m[5],16)&(1<<62))})""";assert old in s;s=s.replace(old,new);f.write_text(s)
# Keep future search output bounded; full evidence remains in JSON artifacts.
f=p/'tools/find-filter-abi.py';s=f.read_text();s=s.replace(" print(hex(start),len(lines),' | '.join(l for l in lines if 'r9 + ' in l)[:250])", " if len(result)<=20: print(hex(start),len(lines))");f.write_text(s)
f=p/'tools/shortlist-filter-abi.py';s=f.read_text();s=s.replace(" print(k,len(a),' | '.join(l for l in a if ' and ' in l)[:350])", " if len(out)<=20: print(k,len(a))");f.write_text(s)
for name in ['find-filter-flags.py','find-filter-shifts.py','find-filter-flag-names.py']:
 f=p/'tools'/name;s=f.read_text();marker="sec=pe.sections[0]";s=s.replace(marker,"assert (pe.FILE_HEADER.TimeDateStamp,pe.OPTIONAL_HEADER.SizeOfImage)==(1792368047,92385280)\n"+marker,1) if s.startswith('x') else s
 # Some files compose assignments on one line; inject the guard before first executable search.
 if 'assert (pe.FILE_HEADER.TimeDateStamp' not in s:
  at=s.index('for m in re.finditer')
  s=s[:at]+"assert (pe.FILE_HEADER.TimeDateStamp,pe.OPTIONAL_HEADER.SizeOfImage)==(1792368047,92385280)\n"+s[at:]
 f.write_text(s)
print('Flag annotations added; search output bounded')
