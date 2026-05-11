import os
import subprocess
import re
import json
import numpy as np

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

from core.config import (
    DEFAULT_SR, PROCESS_TIMEOUT_SECONDS,
    BAND_DEFS, GENRE_CURVES, get_genre_profile
)

def get_mono(samples):
    if len(samples.shape) > 1 and samples.shape[1] == 2:
        return samples.mean(axis=1)
    return samples

def dbfs(value):
    return 20 * np.log10(max(float(value), 1e-12))

def signal_level_metrics(signal):
    signal = np.asarray(signal, dtype=np.float32)
    if signal.size == 0:
        return {"rms_db": -120.0, "peak_db": -120.0, "crest_db": 0.0}
    rms = float(np.sqrt(np.mean(signal ** 2)))
    peak = float(np.max(np.abs(signal)))
    rms_db = float(dbfs(rms))
    peak_db = float(dbfs(peak))
    return {
        "rms_db": rms_db,
        "peak_db": peak_db,
        "crest_db": peak_db - rms_db,
    }

def levels(samples):
    mono = get_mono(samples)
    if mono.size == 0:
        empty_details = {
            "mono": {"rms_db": -120.0, "peak_db": -120.0, "crest_db": 0.0},
            "left": None,
            "right": None,
            "max_channel_peak_db": -120.0,
            "combined_rms_db": -120.0,
        }
        return -120.0, -120.0, 0.0, {"width_pct": 0.0, "ms_ratio_db": 0.0}, empty_details

    mono_metrics = signal_level_metrics(mono)
    level_details = {
        "mono": {key: round(value, 3) for key, value in mono_metrics.items()},
        "left": None,
        "right": None,
        "max_channel_peak_db": round(mono_metrics["peak_db"], 3),
        "combined_rms_db": round(mono_metrics["rms_db"], 3),
    }

    # Mid/Side Analysis
    mid_side = {"width_pct": 0.0, "ms_ratio_db": 0.0}
    if len(samples.shape) > 1 and samples.shape[1] == 2:
        L, R = samples[:, 0], samples[:, 1]
        mid = (L + R) / 2.0
        side = (L - R) / 2.0
        mid_rms = np.sqrt(np.mean(mid**2)) + 1e-12
        side_rms = np.sqrt(np.mean(side**2)) + 1e-12
        ms_ratio = side_rms / mid_rms
        mid_side["width_pct"] = round(float(ms_ratio * 100.0), 1)
        mid_side["ms_ratio_db"] = round(float(20 * np.log10(ms_ratio)), 2)

        left_metrics = signal_level_metrics(L)
        right_metrics = signal_level_metrics(R)
        combined_rms = float(np.sqrt(np.mean(samples ** 2)))
        max_channel_peak_db = max(left_metrics["peak_db"], right_metrics["peak_db"])
        level_details.update({
            "left": {key: round(value, 3) for key, value in left_metrics.items()},
            "right": {key: round(value, 3) for key, value in right_metrics.items()},
            "max_channel_peak_db": round(max_channel_peak_db, 3),
            "combined_rms_db": round(float(dbfs(combined_rms)), 3),
        })

    rms_db = level_details["combined_rms_db"]
    peak_db = level_details["max_channel_peak_db"]
    crest = peak_db - rms_db
    return rms_db, peak_db, crest, mid_side, level_details

def parse_float(value):
    try:
        num = float(value)
        return num if np.isfinite(num) else None
    except (TypeError, ValueError):
        return None

def extract_ffmpeg_json(stderr):
    matches = re.findall(r"\{[\s\S]*?\}", stderr or "")
    for raw in reversed(matches):
        try:
            return json.loads(raw)
        except json.JSONDecodeError:
            continue
    return None

def loudnorm_snapshot(ffmpeg_cmd, src, start=0, duration=30):
    cmd = [ffmpeg_cmd, "-hide_banner", "-nostats"]
    if start > 0:
        cmd += ["-ss", str(start)]
    cmd += [
        "-t", str(max(duration, 0.1)),
        "-i", src,
        "-map", "0:a:0",
        "-vn",
        "-af", "loudnorm=I=-16:LRA=11:TP=-1.5:print_format=json",
        "-f", "null",
        "-"
    ]
    try:
        res = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, timeout=PROCESS_TIMEOUT_SECONDS)
    except Exception:
        return None
    payload = extract_ffmpeg_json(res.stderr)
    if not payload:
        return None
    return {
        "I": parse_float(payload.get("input_i")),
        "LRA": parse_float(payload.get("input_lra")),
        "TP": parse_float(payload.get("input_tp")),
        "threshold": parse_float(payload.get("input_thresh")),
        "method": "ffmpeg loudnorm",
    }

def ebur128_snapshot(ffmpeg_cmd, src, start=0, duration=30):
    cmd = [ffmpeg_cmd, "-hide_banner", "-nostats"]
    if start > 0:
        cmd += ["-ss", str(start)]
    cmd += ["-t", str(max(duration, 0.1)), "-i", src, "-map", "0:a:0", "-vn", "-af", "ebur128=peak=true", "-f", "null", "-"]
    try:
        res = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, timeout=PROCESS_TIMEOUT_SECONDS)
    except Exception:
        return None
    I = LRA = TP = SP = None
    for line in res.stderr.splitlines():
        if "I:" in line and "LUFS" in line:
            m = re.search(r"I:\s*(-?\d+\.?\d*)\s*LUFS", line);  I = float(m.group(1)) if m else I
        if "LRA:" in line and "LU" in line:
            m = re.search(r"LRA:\s*(-?\d+\.?\d*)\s*LU", line);  LRA = float(m.group(1)) if m else LRA
        if "True peak" in line and "dBTP" in line:
            m = re.search(r"True peak:\s*(-?\d+\.?\d*)\s*dBTP", line); TP = float(m.group(1)) if m else TP
        if "Sample peak" in line and "dBFS" in line:
            m = re.search(r"Sample peak:\s*(-?\d+\.?\d*)\s*dBFS", line); SP = float(m.group(1)) if m else SP
    return {"I": I, "LRA": LRA, "TP": TP, "SP": SP, "method": "ffmpeg ebur128"}

def loudness_snapshot(ffmpeg_cmd, src, start=0, duration=30):
    snapshot = loudnorm_snapshot(ffmpeg_cmd, src, start=start, duration=duration)
    if snapshot and snapshot.get("I") is not None:
        return snapshot
    snapshot = ebur128_snapshot(ffmpeg_cmd, src, start=start, duration=duration)
    if snapshot:
        return snapshot
    return {"I": None, "LRA": None, "TP": None, "SP": None, "method": "unavailable"}

def lufs_snapshot(ffmpeg_cmd, src, start=0, duration=30):
    snapshot = loudness_snapshot(ffmpeg_cmd, src, start=start, duration=duration)
    return snapshot.get("I"), snapshot.get("LRA"), snapshot.get("TP"), snapshot.get("SP")

def band_distribution(samples, sr, genre="Pop"):
    mono = get_mono(samples)
    
    # Mid/Side for Spectrum
    side = None
    if len(samples.shape) > 1 and samples.shape[1] == 2:
        side = (samples[:, 0] - samples[:, 1]) / 2.0
        
    if mono.size == 0:
        empty_features = {"spectral_centroid_hz": None, "spectral_rolloff_hz": None}
        return (
            [{"name": name, "range": f"{lo}-{hi}Hz", "percent": 0.0} for name, lo, hi in BAND_DEFS],
            np.array([20.0, 20000.0]),
            np.array([-120.0, -120.0]),
            None,
            empty_features,
            [],
            [],
            [],
        )
        
    mono = np.nan_to_num(mono)
    n = 1 << (len(mono) - 1).bit_length()
    win = np.hanning(len(mono))
    
    X = np.fft.rfft(mono * win, n=n)
    freqs = np.fft.rfftfreq(n, d=1.0/sr)
    mag_db = 20*np.log10(np.maximum(np.abs(X), 1e-12))
    
    side_mag_db = None
    if side is not None:
        X_side = np.fft.rfft(side * win, n=n)
        side_mag_db = 20*np.log10(np.maximum(np.abs(X_side), 1e-12))
        
    power = (np.abs(X) ** 2).astype(np.float64)
    analysis_mask = (freqs >= 20) & (freqs <= 20000)
    analysis_freqs = freqs[analysis_mask]
    analysis_power = power[analysis_mask]
    total = float(analysis_power.sum()) + 1e-12
    
    def pct(lo, hi):
        idx = np.where((freqs >= lo) & (freqs < hi))[0]
        return 100.0 * float(power[idx].sum()) / total if len(idx) else 0.0
        
    if total <= 1e-9 or analysis_freqs.size == 0:
        features = {"spectral_centroid_hz": None, "spectral_rolloff_hz": None}
    else:
        centroid = float(np.sum(analysis_freqs * analysis_power) / total)
        cumulative = np.cumsum(analysis_power)
        rolloff_idx = int(np.searchsorted(cumulative, total * 0.85))
        rolloff_idx = min(rolloff_idx, len(analysis_freqs) - 1)
        features = {
            "spectral_centroid_hz": round(centroid, 1),
            "spectral_rolloff_hz": round(float(analysis_freqs[rolloff_idx]), 1),
        }
    bands = [{"name": name, "range": f"{lo}-{hi}Hz", "percent": round(pct(lo, hi), 2)} for name, lo, hi in BAND_DEFS]
    
    # Resonance Detection
    all_res = []
    if mag_db.size > 100:
        smoothed = np.convolve(mag_db, np.ones(50)/50.0, mode='same')
        diff = mag_db - smoothed
        for i in range(25, len(diff)-25):
            if diff[i] > 7.5 and diff[i] == np.max(diff[i-25:i+25]):
                f = float(freqs[i])
                if 20 < f < 18000:
                    all_res.append({"freq": round(f, 0), "gain": round(float(diff[i]), 1)})

    resonances = []
    for name, lo, hi in BAND_DEFS:
        band_res = [r for r in all_res if lo <= r["freq"] < hi]
        if band_res:
            top = max(band_res, key=lambda x: x["gain"])
            top["band"] = name
            resonances.append(top)
            
    # Target Curve Generation
    profile = get_genre_profile(genre)
    curve_pts = GENRE_CURVES.get(profile["group"], GENRE_CURVES["General"])
    avg_level = np.mean(mag_db[analysis_mask]) if np.any(analysis_mask) else -40
    target_curve = []
    for f, v in curve_pts:
        target_curve.append({"f": f, "v": v + avg_level + 10})

    # Down-sampled FFT data for interactive frontend chart
    NUM_POINTS = 512
    log_freqs = np.logspace(np.log10(20), np.log10(20000), NUM_POINTS)
    fft_points_mid  = []
    fft_points_side = []
    for lf in log_freqs:
        idx = int(np.argmin(np.abs(freqs - lf)))
        fft_points_mid.append(round(float(mag_db[idx]), 2))
        if side_mag_db is not None:
            fft_points_side.append(round(float(side_mag_db[idx]), 2))
    fft_export = {
        "freqs": [round(float(f), 2) for f in log_freqs],
        "mid":   fft_points_mid,
        "side":  fft_points_side if fft_points_side else None,
        "target_curve": target_curve,
    }

    return bands, freqs, mag_db, side_mag_db, features, resonances, target_curve, fft_export

def audio_quality_metrics(samples, sr, spectral_features):
    mono = get_mono(samples)
    if mono.size == 0:
        return {
            "clipped_percent": 0.0,
            "silence_percent": 100.0,
            "dc_offset": 0.0,
            "stereo_balance_db": None,
            **spectral_features,
        }
    all_samples = np.asarray(samples).reshape(-1)
    clipped_percent = 100.0 * float(np.mean(np.abs(all_samples) >= 0.999))
    silence_percent = 100.0 * float(np.mean(np.abs(mono) < (10 ** (-60 / 20))))
    dc_offset = float(np.mean(mono))
    stereo_balance_db = None
    if len(samples.shape) > 1 and samples.shape[1] == 2:
        left_rms = np.sqrt(np.mean(samples[:, 0] ** 2))
        right_rms = np.sqrt(np.mean(samples[:, 1] ** 2))
        stereo_balance_db = 20 * np.log10(max(left_rms, 1e-12) / max(right_rms, 1e-12))
    return {
        "clipped_percent": round(clipped_percent, 3),
        "silence_percent": round(silence_percent, 2),
        "dc_offset": round(dc_offset, 6),
        "stereo_balance_db": round(float(stereo_balance_db), 2) if stereo_balance_db is not None else None,
        **spectral_features,
    }

def plot_waveform(samples, sr, req_id, tag, image_dir):
    plt.style.use('dark_background')
    mono = get_mono(samples)
    t = np.arange(len(mono)) / sr
    w_filename = f"{req_id}_{tag}_waveform.png"
    w_png = os.path.join(image_dir, w_filename)
    fig, ax = plt.subplots(figsize=(10, 3))
    ax.plot(t, mono, color="#19d3c5", alpha=0.9, linewidth=0.55)
    ax.set_title(f"{tag} Waveform", color="#ffffff", fontsize=10)
    ax.axis('off')
    fig.patch.set_alpha(0.0)
    ax.patch.set_alpha(0.0)
    plt.tight_layout()
    plt.savefig(w_png, dpi=120, transparent=True)
    plt.close()
    return f"/media/images/{w_filename}"

def analyze_transients(samples, sr):
    mono = get_mono(samples)
    if mono.size == 0 or sr == 0:
        return {"transient_density": 0.0, "attack_time_ms": 0.0, "percussion_energy_pct": 0.0}

    frame_len = 1024
    hop = 512
    n_frames = (len(mono) - frame_len) // hop
    if n_frames < 2:
        return {"transient_density": 0.0, "attack_time_ms": 0.0, "percussion_energy_pct": 0.0}

    rms_frames = np.array([
        np.sqrt(np.mean(mono[i * hop: i * hop + frame_len] ** 2))
        for i in range(n_frames)
    ], dtype=np.float32)

    flux = np.diff(rms_frames)
    flux = np.maximum(flux, 0)
    min_threshold = 0.001
    threshold = max(min_threshold, float(np.mean(flux) + 1.5 * np.std(flux)))
    onset_frames = np.where(flux > threshold)[0]

    duration_sec = len(mono) / sr
    transient_density = round(len(onset_frames) / duration_sec, 2) if duration_sec > 0 else 0.0

    lookahead = max(1, int(0.08 * sr / hop))
    attack_times = []
    for idx in onset_frames:
        end = min(idx + lookahead, len(rms_frames) - 1)
        if end > idx:
            peak_offset = int(np.argmax(rms_frames[idx:end + 1]))
            attack_times.append(peak_offset * hop / sr * 1000.0)
    attack_time_ms = round(float(np.mean(attack_times)), 1) if attack_times else 15.0

    n = 1 << max((len(mono) - 1).bit_length(), 10)
    X = np.fft.rfft(mono, n=n)
    freqs = np.fft.rfftfreq(n, d=1.0 / sr)
    power = np.abs(X) ** 2
    total_power = float(power.sum()) + 1e-12
    perc_power = float(power[freqs >= 3000].sum())
    percussion_energy_pct = round(perc_power / total_power * 100.0, 1)

    return {
        "transient_density": transient_density,
        "attack_time_ms": attack_time_ms,
        "percussion_energy_pct": percussion_energy_pct,
    }

def cut_to_f32le(ffmpeg_cmd, src, out_raw, start=0, duration=30, sr=DEFAULT_SR):
    cmd = [ffmpeg_cmd, "-hide_banner", "-loglevel", "error"]
    if start > 0:
        cmd += ["-ss", str(start)]
    cmd += [
        "-t", str(max(duration, 0.1)),
        "-i", src,
        "-map", "0:a:0",
        "-vn",
        "-ac", "2",
        "-ar", str(sr),
        "-f", "f32le",
        "-acodec", "pcm_f32le",
        "-y", out_raw,
    ]
    subprocess.run(cmd, check=True, timeout=PROCESS_TIMEOUT_SECONDS)

def read_f32le(path, sr=DEFAULT_SR, channels=2):
    samples = np.fromfile(path, dtype="<f4")
    if channels > 1:
        usable = (samples.size // channels) * channels
        samples = samples[:usable].reshape(-1, channels)
    samples = np.nan_to_num(samples.astype(np.float32, copy=False))
    return sr, samples

def safe_remove(path):
    try:
        if path and os.path.exists(path):
            os.remove(path)
    except OSError:
        pass

def analyze_slice(ffmpeg_cmd, src, start, duration, tag, req_id, genre, snippet_dir, image_dir):
    snippet = os.path.join(snippet_dir, f"_snippet_{req_id}_{tag}.f32le")
    try:
        cut_to_f32le(ffmpeg_cmd, src, snippet, start=start, duration=duration, sr=DEFAULT_SR)
        sr, samples = read_f32le(snippet, sr=DEFAULT_SR, channels=2)
        
        correlation = 1.0
        if len(samples.shape) > 1 and samples.shape[1] == 2:
            L = samples[:, 0]
            R = samples[:, 1]
            std_l = np.std(L)
            std_r = np.std(R)
            if std_l > 1e-6 and std_r > 1e-6:
                correlation = float(np.mean((L - np.mean(L)) * (R - np.mean(R))) / (std_l * std_r))
                correlation = max(-1.0, min(1.0, correlation))
                
        rms_db, peak_db, crest, mid_side, level_details = levels(samples)
        loudness = loudness_snapshot(ffmpeg_cmd, src, start=start, duration=duration)
        bands, freqs, mag_db, side_mag_db, spectral_features, resonances, target_curve, fft_data = band_distribution(samples, sr, genre)
        quality = audio_quality_metrics(samples, sr, spectral_features)
        transients = analyze_transients(samples, sr)
        w_url = plot_waveform(samples, sr, req_id, tag, image_dir)
        return dict(tag=tag, start=start, duration=duration, sr=sr,
                    rms_db=rms_db, peak_db=peak_db, crest_db=crest,
                    correlation=correlation, mid_side=mid_side, levels=level_details,
                    I=loudness.get("I"), LRA=loudness.get("LRA"), TP=loudness.get("TP"), SP=peak_db if loudness.get("SP") is None else loudness.get("SP"),
                    loudness_method=loudness.get("method"),
                    bands=bands, quality=quality, resonances=resonances, target_curve=target_curve,
                    transients=transients,
                    fft_data=fft_data,
                    waveform_url=w_url)
    finally:
        safe_remove(snippet)
