"""String/literal cross-referencing for the stock nanit-lite-fw image (Xtensa l32r literals).

Works on YOUR OWN dump; nothing vendor-derived ships in this repo. Expected layout
(gitignored):
  analysis/parts/ota_0.bin           the app partition (0x430000, 0x580000 bytes) from your dump
  analysis/disasm/seg_<vaddr>.asm    per-segment objdump, e.g.
      xtensa-esp32s3-elf-objdump -D -b binary -m xtensa --adjust-vma=0x42000020 seg_42000020.bin
Usage: python tools/xref.py "some string" ...
"""
import re, struct, sys, os, bisect
ROOT = os.path.join(os.path.dirname(__file__), "..", "analysis")
IMG = os.path.join(ROOT, "parts", "ota_0.bin")
ASM = os.path.join(ROOT, "disasm", "seg_42000020.asm")
IRAM_ASM = os.path.join(ROOT, "disasm", "seg_4037985c.asm")

def segments():
    d = open(IMG, "rb").read(); off = 24; out = []
    for _ in range(d[1]):
        a, l = struct.unpack("<II", d[off:off + 8]); out.append((a, d[off + 8:off + 8 + l])); off += 8 + l
    return out

SEGS = segments()

def find_bytes(b):
    hits = []
    for a, blob in SEGS:
        for m in re.finditer(re.escape(b), blob):
            hits.append(a + m.start())
    return hits

def literal_slots(value):
    """Addresses of 4-byte literal-pool slots holding `value`."""
    pat = struct.pack("<I", value); out = []
    for a, blob in SEGS:
        if not (0x40370000 <= a < 0x42800000): continue
        i = blob.find(pat)
        while i >= 0:
            if (a + i) % 4 == 0: out.append(a + i)
            i = blob.find(pat, i + 1)
    return out

_lines = None
def asm_lines():
    global _lines
    if _lines is None:
        _lines = []
        for f in (ASM, IRAM_ASM):
            for ln in open(f):
                m = re.match(r"([0-9a-f]{8}):\s", ln)
                if m: _lines.append((int(m.group(1), 16), ln.rstrip()))
        _lines.sort()
    return _lines

def users_of_slot(slot):
    s = f"0x{slot:08x}"
    return [(a, l) for a, l in asm_lines() if "l32r" in l and s in l]

def func_start(addr):
    """Walk back to the nearest `entry` instruction."""
    L = asm_lines(); i = bisect.bisect_right([a for a, _ in L], addr) - 1
    while i > 0 and "\tentry\t" not in L[i][1]: i -= 1
    return L[i][0]

def xref_string(s):
    for sa in find_bytes(s.encode()):
        for slot in literal_slots(sa):
            for a, l in users_of_slot(slot):
                yield sa, slot, a, func_start(a)

def callers(fn):
    t = f"0x{fn:08x}"
    return [a for a, l in asm_lines() if re.search(r"\tcall(0|4|8|12)\t" + t + r"\b", l) or (("callx" not in l) and ("call" in l) and l.endswith(t))]

def dis(start, n=80):
    L = asm_lines(); keys = [a for a, _ in L]; i = bisect.bisect_left(keys, start)
    return "\n".join(l for _, l in L[i:i + n])

if __name__ == "__main__":
    for s in sys.argv[1:]:
        print(f"== {s!r}")
        for sa, slot, a, f in xref_string(s):
            print(f"  str@{sa:08x} slot@{slot:08x} used@{a:08x} in func {f:08x}")

def rust_str_users(s):
    """Rust &str: code -> literal -> (ptr,len) pair in rodata -> bytes. Also direct literal to ptr."""
    out = []
    for sa in find_bytes(s.encode()):
        refs = [sa]
        pat = struct.pack("<II", sa, len(s.encode()))
        for a, blob in SEGS:
            i = blob.find(struct.pack("<I", sa))
            while i >= 0:
                if (a + i) % 4 == 0 and a < 0x40000000:
                    refs.append(a + i)
                i = blob.find(struct.pack("<I", sa), i + 1)
        for r in refs:
            for slot in literal_slots(r):
                for ua, _ in users_of_slot(slot):
                    out.append((sa, r, ua))
    return out
