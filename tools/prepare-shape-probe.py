from pathlib import Path
p=Path(r'F:\Work\GameDev\2077Mod\DriveNoCollision')
f=p/'tools/generate-native-guards.py';s=f.read_text();s=s.replace("rows=json.loads", "assert (pe.FILE_HEADER.TimeDateStamp, pe.OPTIONAL_HEADER.SizeOfImage) == (1792368047, 92385280), 'Build differs; manually review native bindings'\nrows=json.loads")
s=s.replace("text += ['}']", """# Full reviewed reader/wrapper ranges, including shape-count rejection branches.
for name,rva,size in [('ShapeStateReader',0x614668,2539),('PhysicalProxyReader',0x614568,176),('OtherProxyReader',0x614618,80)]:
 code=pe.get_data(rva,size)
 text += [f'inline constexpr uint32_t {name}Rva = 0x{rva:x};',f'inline constexpr std::array<unsigned char,{size}> {name}Bytes = {{'+','.join(f'0x{x:02x}' for x in code)+'};']
text += ['}']""");f.write_text(s)
f=p/'src/NativeProbe.cpp';s=f.read_text();s=s.replace('matches(DncBuild::FilterResourceGetterRva, DncBuild::FilterResourceGetterBytes);','matches(DncBuild::FilterResourceGetterRva, DncBuild::FilterResourceGetterBytes) &&\n        matches(DncBuild::ShapeStateReaderRva, DncBuild::ShapeStateReaderBytes) &&\n        matches(DncBuild::PhysicalProxyReaderRva, DncBuild::PhysicalProxyReaderBytes) &&\n        matches(DncBuild::OtherProxyReaderRva, DncBuild::OtherProxyReaderBytes);')
needle='''    } else {
        sdk->logger->Info(h, "DNC_LIVE_FILTER_UNSUPPORTED: engine rejected SimulationFilter read; output ignored");
    }
}'''
replacement='''    } else {
        sdk->logger->Info(h, "DNC_LIVE_FILTER_UNSUPPORTED: engine rejected SimulationFilter read; output ignored");
        return;
    }
    // The reviewed state-32 implementation checks getNbShapes() and returns false
    // for out-of-range indices. Stay on body 0; resource counts are not used.
    // A rejection ends the scan, but is not claimed to prove an exact shape count.
    uint32_t bottomMask = 0;
    std::memcpy(&bottomMask, static_cast<const unsigned char*>(chassis)+0x174, sizeof(bottomMask));
    sdk->logger->InfoF(h, "DNC_SHAPE_SCAN_BEGIN body=0 limit=64 bottomMask=%08x", bottomMask);
    uint32_t accepted = 0;
    constexpr uint32_t limit = 64;
    for (uint32_t shape = 0; shape < limit; ++shape) {
        RED4ext::physics::SimulationFilter current{};
        if (shape == 0) current = filter;
        else if (!readState(nullptr, proxyId, 0, shape,
            static_cast<uint32_t>(RED4ext::physics::StateValue::SimulationFilter), &current, sizeof(current))) {
            sdk->logger->InfoF(h, "DNC_SHAPE_SCAN_END body=0 accepted=%u firstRejected=%u capped=0", accepted, shape);
            return;
        }
        const bool bottom = shape < 32 && (bottomMask & (uint32_t{1} << shape)) != 0;
        sdk->logger->InfoF(h, "DNC_SHAPE_FILTER body=0 shape=%u bottom=%u mask=%016llx:%016llx",
            shape, unsigned(bottom), static_cast<unsigned long long>(current.mask1),
            static_cast<unsigned long long>(current.mask2));
        ++accepted;
    }
    sdk->logger->WarnF(h, "DNC_SHAPE_SCAN_END body=0 accepted=%u capped=1; inventory incomplete", accepted);
}'''
assert needle in s;s=s.replace(needle,replacement);f.write_text(s)
for name in ['CMakeLists.txt','src/Main.cpp']:
 f=p/name;s=f.read_text().replace('0.7.0','0.8.0').replace('SEMVER(0, 7, 0)','SEMVER(0, 8, 0)');f.write_text(s)
print('Prepared read-only 0.8.0 per-shape scan')
