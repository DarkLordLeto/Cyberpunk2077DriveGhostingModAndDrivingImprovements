import sys,json,pathlib
root=pathlib.Path(r'F:\Work\GameDev\2077Mod\DriveNoCollision')
sys.path.insert(0,str(root/'dependencies/analysis-python'))
import pefile,capstone
base=pathlib.Path(r'D:\SteamLibrary\steamapps\common\Cyberpunk 2077\bin\x64')
data=json.loads((base/'cyberpunk2077_addresses.json').read_text())
labels={3094614336:'FilterLoadPreset',2725251676:'FilterInit'}
pe=pefile.PE(str(base/'Cyberpunk2077.exe'),fast_load=True)
md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_64)
results=[]
for a in data['Addresses']:
    h=int(a['hash'])
    if h not in labels: continue
    sec,off=(int(x,16) for x in a['offset'].split(':'))
    section=pe.sections[sec-1]
    rva=section.VirtualAddress+off
    code=pe.get_data(rva,400)
    lines=[f'{i.address:016x} {i.mnemonic} {i.op_str}' for i in md.disasm(code,pe.OPTIONAL_HEADER.ImageBase+rva)]
    out={'name':labels[h],'hash':h,'rva':hex(rva),'instructions':lines}
    results.append(out)
    print(labels[h],a['offset'],'rva',hex(rva))
    print('\n'.join(lines[:80]))
(root/'artifacts/native-research/preset-disassembly.json').write_text(json.dumps(results,indent=2))
print('Resolved',len(results),'of',len(labels))
