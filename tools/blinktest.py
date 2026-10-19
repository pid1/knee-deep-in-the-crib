"""Start a blink pattern on the device and sample lamp spill colour from the camera."""
import serial, time, subprocess, sys
from PIL import Image
s = serial.Serial(); s.port = '/dev/cu.usbmodem101'; s.timeout = 0.2; s.write_timeout = 2; s.dtr = True; s.rts = False; s.open()
cmd, samples = sys.argv[1], int(sys.argv[2])
s.write((cmd + '\n').encode())
out = []
t0 = time.time()
for i in range(samples):
    subprocess.run(['tools/look.sh', 'captures/bt.png'], capture_output=True)
    im = Image.open('captures/bt.png').convert('RGB'); px = list(im.crop((250, 500, 450, 700)).getdata())
    r, g, b = (round(sum(p[k] for p in px) / len(px)) for k in range(3))
    out.append(f'{time.time()-t0:4.1f}s:{"R" if r > g + 50 else "."}')
print(cmd, ' '.join(out))
