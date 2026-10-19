"""Run console command groups and crop the display region from the camera after each."""
import serial, time, subprocess, sys
from PIL import Image
s = serial.Serial(); s.port = '/dev/cu.usbmodem101'; s.timeout = 0.2; s.write_timeout = 2; s.dtr = True; s.rts = False; s.open()
def c(x): s.write((x + '\n').encode()); time.sleep(0.2); return s.read(4096)
crops = []
for i, group in enumerate(sys.argv[1:]):
    for cmd in group.split(';'): c(cmd.strip())
    time.sleep(1.0)
    subprocess.run(['tools/look.sh', f'captures/dt_{i}.png'], capture_output=True)
    crops.append(Image.open(f'captures/dt_{i}.png').convert('RGB').crop((640, 760, 920, 880)))
W = Image.new('RGB', (280, 120 * len(crops)))
for i, im in enumerate(crops): W.paste(im, (0, i * 120))
W.save('captures/dt_grid.png'); print('captures/dt_grid.png')
