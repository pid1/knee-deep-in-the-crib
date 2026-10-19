"""Send console commands to the doom firmware over USB-Serial-JTAG without resetting it."""
import serial, sys, time
s = serial.Serial()
s.port = sys.argv[1] if sys.argv[1].startswith("/dev/") else "/dev/cu.usbmodem101"
cmds = [a for a in sys.argv[1:] if not a.startswith("/dev/")]
s.timeout = 0.2; s.write_timeout = 2
s.dtr = True; s.rts = False   # keep EN released, no reset
s.open()
time.sleep(0.3); s.read(65536)
for c in cmds:
    s.write((c + "\n").encode())
    t = time.time(); out = b""
    wait = 4.0 if c.startswith(("tone", "i2c")) else 1.0
    while time.time() - t < wait:
        out += s.read(4096)
    print(f"> {c}\n{out.decode(errors='replace').strip()}")
