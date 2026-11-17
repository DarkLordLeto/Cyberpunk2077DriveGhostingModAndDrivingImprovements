import struct,sys,zlib,pickle
d=open(sys.argv[1],'rb').read()
def fnv(s):
    h=0xcbf29ce484222325
    for b in s.encode():
        h^=b; h=(h*0x100000001b3)&0xffffffffffffffff
    return h
cands=['String','array:String','CName','array:CName','TweakDBID','array:TweakDBID','Int32','array:Int32','Float','array:Float','Bool','array:Bool','gamedataLocKeyWrapper','array:gamedataLocKeyWrapper','raRef:CResource','array:raRef:CResource','Vector2','array:Vector2','Vector3','array:Vector3','Quaternion','array:Quaternion','EulerAngles','array:EulerAngles','Color','array:Color']
names={fnv(t):t for t in cands}
n=struct.unpack_from('<I',d,32)[0]
table=[]
for i in range(n):
    h,a,b=struct.unpack_from('<QII',d,36+i*20); table.append((names[h],a,b))
p=36+n*20
def cint(p):
    b=d[p]; v=b&0x3f; neg=(b&0x80)!=0; p+=1; sh=6; more=b&0x40
    while more:
        b=d[p]; p+=1; v|=(b&0x7f)<<sh; sh+=7; more=b&0x80
    return v,p
def rstr(p):
    l,p=cint(p); return d[p:p+l].decode('utf-8','replace'),p+l
fixed={'TweakDBID':8,'Int32':4,'Float':4,'Bool':1,'gamedataLocKeyWrapper':8,'raRef:CResource':8,'Vector2':8,'Vector3':12,'Quaternion':16,'EulerAngles':12,'Color':4}
fmt={'TweakDBID':'<Q','Int32':'<i','Float':'<f','Bool':'<?','gamedataLocKeyWrapper':'<Q','raRef:CResource':'<Q','Vector2':'<2f','Vector3':'<3f','Quaternion':'<4f','EulerAngles':'<3f','Color':'<4B'}
def rval(t,p):
    if t in ('String','CName'): return rstr(p)
    if t.startswith('array:'):
        e=t[6:]; c,p=cint(p); out=[]
        for _ in range(c):
            v,p=rval(e,p); out.append(v)
        return out,p
    v=struct.unpack_from(fmt[t],d,p); p+=fixed[t]
    return (v[0] if len(v)==1 else v),p
flats={}
for t,vc,kc in table:
    c=struct.unpack_from('<I',d,p)[0]; p+=4
    if c!=vc: print('note value count',t,c,'table',vc)
    vals=[]
    for _ in range(c):
        v,p=rval(t,p); vals.append(v)
    c=struct.unpack_from('<I',d,p)[0]; p+=4
    assert c==kc,(t,c,kc,p)
    for _ in range(c):
        k,ix=struct.unpack_from('<QI',d,p); p+=12
        flats[k]=(t,vals[ix])
    print('ok',t,vc,kc,'end',p)
print('flats end',p,'records',struct.unpack_from('<8I',d,0)[5])
pickle.dump(flats,open(sys.argv[2],'wb'))
