import pathlib,os,struct
from collections import namedtuple
reports=sorted((pathlib.Path(os.environ['LOCALAPPDATA'])/'REDEngine/ReportQueue').glob('Cyberpunk*'),key=lambda p:p.stat().st_mtime,reverse=True)[:2]
for r in reports:
 data=(r/'Cyberpunk2077.dmp').read_bytes();u=lambda f,o:struct.unpack_from('<'+f,data,o); n,start=u('II',8); streams={t:(s,a) for t,s,a in (u('III',start+i*12) for i in range(n))};mo=streams[4][1];mods=[]
 for i in range(u('I',mo)[0]):
  a=mo+4+108*i;base,size=u('QI',a);name=u('I',a+20)[0];length=u('I',name)[0];mods.append((base,base+size,data[name+4:name+4+length].decode('utf-16le').split('\\')[-1]))
 def fmt(v):return next((f'{name}+{v-base:#x}' for base,end,name in mods if base<=v<end),hex(v))
 ex=streams[6][1];rip=u('Q',ex+24)[0];ctx=u('I',ex+164)[0];print(r.name,fmt(rip));print('registers',[(k,fmt(u('Q',ctx+o)[0])) for k,o in [('rcx',128),('rdx',136),('rax',120),('rip',248)] ]);tid=u('I',ex)[0];th=streams[3][1];
 for i in range(u('I',th)[0]):
  a=th+4+i*48
  if u('I',a)[0]!=tid:continue
  sp,sz,off=u('QII',a+24);rsp=u('Q',ctx+152)[0];out=[]
  for delta in range(max(0,rsp-sp),min(sz,max(0,rsp-sp)+2048),8):
   loc=fmt(u('Q',off+delta)[0]);
   if '+' in loc:out.append((hex(sp+delta-rsp),loc))
  print('stack candidates',out[:30])
