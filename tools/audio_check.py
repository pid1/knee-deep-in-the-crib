"""Per-window RMS and dominant frequency of a mono WAV (verifies the device is playing)."""
import sys, wave, numpy as np
w = wave.open(sys.argv[1]); sr = w.getframerate()
x = np.frombuffer(w.readframes(w.getnframes()), dtype=np.int16).astype(float)
win = int(sr * float(sys.argv[2]) if len(sys.argv) > 2 else sr * 0.5)
for i in range(0, len(x) - win, win):
    seg = x[i:i + win] * np.hanning(win)
    sp = np.abs(np.fft.rfft(seg)); f = np.fft.rfftfreq(win, 1 / sr)
    band = (f > 60) & (f < 4000)
    pk = f[band][np.argmax(sp[band])]
    ton = sp[band].max() / (np.median(sp[band]) + 1e-9)
    print(f"{i/sr:5.1f}s rms={np.sqrt(np.mean(x[i:i+win]**2)):7.1f} peak={pk:6.0f}Hz tonality={ton:6.1f}")
