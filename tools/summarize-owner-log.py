"""Summarize sampled owner observations without treating traffic flags as identity."""
import sys, re, json
from pathlib import Path
from collections import Counter, defaultdict
source, output = map(Path, sys.argv[1:3])
text = source.read_text(encoding="utf-8-sig")
owners, drivers, counts = [], [], []
for line in text.splitlines():
    if "DNC_PAIR_OWNER " in line:
        owners.append(dict(re.findall(r"(\w+)=([^\s]+)", line.split("DNC_PAIR_OWNER ", 1)[1])))
    if "DNC_DRIVER " in line:
        drivers.append(dict(re.findall(r"(\w+)=([^\s]+)", line.split("DNC_DRIVER ", 1)[1])))
    m = re.search(r"DNC_PAIR_COUNTS total=(\d+) playerMatches=(\d+)", line)
    if m: counts.append(dict(total=int(m[1]), playerMatches=int(m[2])))
entities = defaultdict(list)
for owner in owners:
    if owner.get("vehicle") == "1": entities[owner["entity"]].append(owner)
report = dict(source=source.name, reader_ready="DNC_OWNER_READER ready=1" in text,
    attached="DNC_PAIR_ATTACHED" in text, detached="DNC_PAIR_DETACHED" in text,
    unloaded="DNC_UNLOADED" in text, drivers=drivers,
    latest_periodic_counts=counts[-1] if counts else None,
    owner_classes=dict(Counter(o.get("class", "unresolved") for o in owners)),
    nonplayer_traffic_proxies=sorted({o["proxy"] for o in owners if o.get("vehicle")=="1" and o.get("playerVehicle")=="0" and o.get("trafficSlot")=="1"}),
    entities_with_multiple_proxies={e: v for e,v in entities.items() if len({o["proxy"] for o in v})>1},
    warning_error_lines=[line for line in text.splitlines() if re.search(r"\[(?:warn\w*|error)\s*\]", line)],
    owners=owners,
    limitations=["Bounded samples; flags observed at drain time, not physics callback time.", "Traffic slot does not establish NPC occupancy.", "Periodic counters are not exact final totals."])
output.write_text(json.dumps(report, indent=2)+"\n")
print(json.dumps({k:v for k,v in report.items() if k!="owners"}, indent=2))
