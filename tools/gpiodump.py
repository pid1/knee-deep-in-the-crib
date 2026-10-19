"""Decode a live GPIO-matrix snapshot from the target via OpenOCD."""
import re, sys, os, json
sys.path.insert(0, os.path.dirname(__file__))
from ocd import OCD, snapshot

hdr = os.path.join(os.environ["IDF_PATH"], "components/soc/esp32s3/include/soc/gpio_sig_map.h")
OUT, IN = {}, {}
for name, idx in re.findall(r"#define\s+(\w+)_IDX\s+(\d+)", open(hdr).read()):
    (IN if name.endswith("_IN") or "_IN_" in name or name.endswith("IN") else OUT).setdefault(int(idx), name)
    if name.endswith("_OUT") or "OUT" in name:
        OUT[int(idx)] = name

def decode(snap):
    lines = []
    for g in range(49):
        sel = snap["out"][g] & 0x1FF
        oen = (snap["en"][g // 32] >> (g % 32)) & 1
        mux = snap["mux"][g]
        fun = (mux >> 12) & 7
        ie = (mux >> 9) & 1
        pu, pd = (mux >> 8) & 1, (mux >> 7) & 1
        lvl = (snap["lvl"][g // 32] >> (g % 32)) & 1
        sig = "GPIO" if sel == 256 else OUT.get(sel, f"sig{sel}")
        lines.append((g, f"GPIO{g:2d} mux_fun={fun} ie={ie} pu={pu} pd={pd} out_en={oen} level={lvl} out_sel={sig}"))
    ins = []
    for m, v in enumerate(snap["insel"]):
        if v & 0x40:  # SIG_IN_SEL: routed through matrix
            pin = v & 0x3F
            ins.append(f"  in signal {m:3d} {IN.get(m, '?'):28s} <- GPIO{pin}" + (" (inv)" if v & 0x80 else ""))
    return lines, ins

if __name__ == "__main__":
    o = OCD()
    s = snapshot(o)
    if len(sys.argv) > 1:
        json.dump(s, open(sys.argv[1], "w"))
    lines, ins = decode(s)
    for g, l in lines:
        if g in (19, 20) or (26 <= g <= 37):
            continue
        print(l)
    print("matrix inputs:")
    print("\n".join(ins))
