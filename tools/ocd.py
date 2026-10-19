"""Tiny client for OpenOCD's Tcl RPC port (6666)."""
import socket

class OCD:
    def __init__(self, host="127.0.0.1", port=6666):
        self.s = socket.create_connection((host, port), timeout=10)

    def cmd(self, c):
        self.s.sendall(c.encode() + b"\x1a")
        buf = b""
        while not buf.endswith(b"\x1a"):
            buf += self.s.recv(65536)
        return buf[:-1].decode(errors="replace")

    def read32(self, addr, n=1):
        return [int(x, 0) for x in self.cmd(f"read_memory 0x{addr:x} 32 {n}").split()]

    def write32(self, addr, val):
        return self.cmd(f"write_memory 0x{addr:x} 32 {{0x{val:x}}}")

GPIO_OUT_SEL = 0x60004554   # GPIO_FUNCn_OUT_SEL_CFG, n = 0..48
GPIO_IN_SEL = 0x60004154    # GPIO_FUNCm_IN_SEL_CFG, m = signal 0..255
IO_MUX = 0x60009004         # IO_MUX_GPIOn_REG

def snapshot(o, halt=True):
    if halt:
        o.cmd("halt")
    out = o.read32(GPIO_OUT_SEL, 49)
    en = o.read32(0x60004020) + o.read32(0x6000402C)
    lvl = o.read32(0x60004004) + o.read32(0x60004010)
    mux = o.read32(IO_MUX, 49)
    insel = o.read32(GPIO_IN_SEL, 256)
    if halt:
        o.cmd("resume")
    return dict(out=out, en=en, lvl=lvl, mux=mux, insel=insel)
