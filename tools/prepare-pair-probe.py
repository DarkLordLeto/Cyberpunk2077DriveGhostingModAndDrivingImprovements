from pathlib import Path
p=Path(r'F:\Work\GameDev\2077Mod\DriveNoCollision')
f=p/'tools/generate-native-guards.py';s=f.read_text();s=s.replace("text += ['}']", """# PairFound body and registered callback table reviewed through scene creation.
rva=0xc0edc0
code=pe.get_data(rva,0x1a8)
text += [f'inline constexpr uint32_t PairFoundRva = 0x{rva:x};',
         'inline constexpr uint32_t PairCallbackTableRva = 0x35a1ba8;',
         f'inline constexpr std::array<unsigned char,{len(code)}> PairFoundBytes = {{'+','.join(f'0x{x:02x}' for x in code)+'};']
text += ['}']""");f.write_text(s)
f=p/'src/Main.cpp';s=f.read_text().replace('#include "LiveProbe.hpp"','#include "LiveProbe.hpp"\n#include "PairProbe.hpp"');s=s.replace('PollLiveVehicle(pluginHandle, sdk);','PollLiveVehicle(pluginHandle, sdk);\n            DrainPairProbe(pluginHandle, sdk);');s=s.replace('sdk->logger->Info(handle, "DNC_LOADED:', 'StartPairProbe(handle, sdk);\n        sdk->logger->Info(handle, "DNC_LOADED:');s=s.replace('sdk->logger->Info(handle, "DNC_UNLOADED");','if (!StopPairProbe(handle, sdk)) return false;\n        sdk->logger->Info(handle, "DNC_UNLOADED");');s=s.replace('0.8.0','0.9.0').replace('SEMVER(0, 8, 0)','SEMVER(0, 9, 0)');f.write_text(s)
f=p/'src/NativeProbe.cpp';s=f.read_text().replace('#include "NativeProbe.hpp"','#include "NativeProbe.hpp"\n#include "PairProbe.hpp"');s=s.replace('    using IsKinematic =', '    ObservePlayerProxy(proxyId);\n    using IsKinematic =');f.write_text(s)
f=p/'src/LiveProbe.cpp';s=f.read_text();s='#include "PairProbe.hpp"\n'+s;s=s.replace('static std::string_view previous;', 'if (std::string_view(value)!="vehicle-attached") ObservePlayerProxy(0xffffffff);\n        static std::string_view previous;');f.write_text(s)
f=p/'CMakeLists.txt';s=f.read_text().replace('VERSION 0.8.0','VERSION 0.9.0').replace('src/NativeProbe.cpp)','src/NativeProbe.cpp src/PairProbe.cpp)');f.write_text(s)
print('Prepared pass-through pair observer 0.9.0')
