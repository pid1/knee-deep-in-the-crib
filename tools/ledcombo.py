"""Light LED channel subsets and measure lamp/spill colour through the Photo Booth preview."""
import serial, time, subprocess, sys
from PIL import Image
s = serial.Serial(); s.port = '/dev/cu.usbmodem101'; s.timeout = 0.2; s.write_timeout = 2; s.dtr = True; s.rts = False; s.open()
def c(x): s.write((x + '\n').encode()); time.sleep(0.12); s.read(4096)
def avg(im, box):
    px = list(im.crop(box).getdata()); n = len(px); return tuple(round(sum(p[i] for p in px) / n) for i in range(3))
def look(name):
    subprocess.run(['tools/look.sh', f'captures/{name}.png'], capture_output=True)
    im = Image.open(f'captures/{name}.png').convert('RGB')
    return avg(im, (680, 380, 920, 720)), avg(im, (250, 500, 450, 700))
G = [4, 9, 10, 11, 12, 13, 46]
for spec in sys.argv[1:]:
    duty = {}
    for t in spec.split(','):
        g, _, d = t.partition(':'); duty[int(g)] = int(d or 1023)
    for g in G: c(f'led {g} {duty.get(g, 0)}')
    time.sleep(1.3)
    print(f'{spec:28s}', *look('combo_' + spec.replace(',', '_').replace(':', '-')))
c('ledall 0')
