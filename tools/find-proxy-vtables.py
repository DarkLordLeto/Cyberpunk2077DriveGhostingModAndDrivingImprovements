import sys,pathlib,struct,json
root=pathlib.Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'dependencies/analysis-python'))
import pefile
pe=pefile.PE(r'D:\SteamLibrary\steamapps\common\Cyberpunk 2077\bin\x64\Cyberpunk2077.exe',fast_load=True)
assert (pe.FILE_HEADER.TimeDateStamp,pe.OPTIONAL_HEADER.SizeOfImage)==(1792368047,92385280)
base=pe.OPTIONAL_HEADER.ImageBase;out=[]
for sec in pe.sections:
 if sec.Characteristics & 0x20000000:continue
 data=sec.get_data()
 for reader in [0x614568,0x614618,0x612dcc]:
  needle=struct.pack('<Q',base+reader);pos=0
  while True:
   pos=data.find(needle,pos)
   if pos<0:break
   rva=sec.VirtualAddress+pos
   if rva%8==0 and pos+16<=len(data):
    setter=struct.unpack_from('<Q',data,pos+8)[0]-base
    row={'reader':hex(reader),'slot':hex(rva),'candidate_table':hex(rva-0x50),'next_slot_target':hex(setter)};out.append(row);print(row)
   pos+=1
(root/'artifacts/native-research/proxy-vtable-candidates.json').write_text(json.dumps(out,indent=2))
