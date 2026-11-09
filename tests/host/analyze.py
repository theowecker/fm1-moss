"""Per-scene checks on rendered WAVs: DC, clicks, loudness over time, brightness."""
import sys, glob, wave
import numpy as np

for path in sorted(glob.glob(sys.argv[1] + "/*.wav")):
    w = wave.open(path)
    x = np.frombuffer(w.readframes(w.getnframes()), dtype=np.int16).reshape(-1, 2).astype(np.float64) / 32768
    m = x.mean(axis=1)
    win = 44100
    rms = [20 * np.log10(np.sqrt(np.mean(m[i:i + win] ** 2)) + 1e-9) for i in range(0, len(m) - win, win)]
    jump = np.max(np.abs(np.diff(x, axis=0)))
    spec = np.abs(np.fft.rfft(m[: 1 << 20] * np.hanning(min(len(m), 1 << 20))))
    f = np.fft.rfftfreq(min(len(m), 1 << 20), 1 / 44100)
    centroid = np.sum(f * spec) / np.sum(spec)
    corr = np.corrcoef(x[:, 0], x[:, 1])[0, 1]
    print(f"{path.split('/')[-1]:24s} dc {m.mean():+.4f}  maxjump {jump:.3f}  rms/s min {min(rms):6.1f} max {max(rms):6.1f}"
          f"  centroid {centroid:6.0f} Hz  L/R corr {corr:+.2f}")
