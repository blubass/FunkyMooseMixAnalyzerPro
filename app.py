import os, subprocess, re, wave, contextlib, uuid, glob, time, sqlite3, json, shutil, sys, base64

# Ensure common paths are in PATH so ffmpeg can be found from a .app bundle
os.environ['PATH'] = f"/opt/homebrew/bin:/usr/local/bin:{os.environ.get('PATH', '')}"

from flask import Flask, render_template, request, jsonify, send_from_directory, make_response
from werkzeug.utils import secure_filename
import numpy as np

APP_NAME = "Funky Moose Mix Analyzer"

def get_app_data_dir():
    override = os.environ.get("MIX_ANALYZER_DATA_DIR")
    if override:
        return os.path.abspath(os.path.expanduser(override))
    if sys.platform == "darwin":
        return os.path.expanduser(os.path.join("~/Library/Application Support", APP_NAME))
    if os.name == "nt":
        return os.path.join(os.environ.get("APPDATA", os.path.expanduser("~")), APP_NAME)
    return os.path.join(os.environ.get("XDG_DATA_HOME", os.path.expanduser("~/.local/share")), "funky-moose-mix-analyzer")

data_dir = get_app_data_dir()
mpl_config_dir = os.path.join(data_dir, "matplotlib")
os.makedirs(mpl_config_dir, exist_ok=True)
os.environ.setdefault("MPLCONFIGDIR", mpl_config_dir)

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

if getattr(sys, 'frozen', False):
    base_dir = sys._MEIPASS
else:
    base_dir = os.path.dirname(os.path.abspath(__file__))

DEFAULT_SR = 48000
SLICE_SEC = 30
MAX_UPLOAD_MB = 100
MEDIA_TTL_SECONDS = 6 * 60 * 60
PROCESS_TIMEOUT_SECONDS = 120
ALLOWED_AUDIO_EXTENSIONS = {".mp3", ".wav", ".flac", ".aiff", ".aif", ".m4a", ".ogg", ".aac"}
DEFAULT_GENRE = "Pop"
FALLBACK_GENRE = "Streaming / General"

GENRE_PROFILES = {
    "Pop": {"group": "Popular", "target_lufs": -10, "lufs_range": [-12, -8], "crest_range": [6, 12], "low_end_range": [10, 30], "presence_max": 38, "correlation_min": 0.35, "wide_expected": True},
    "K-Pop": {"group": "Popular", "target_lufs": -9, "lufs_range": [-11, -7], "crest_range": [5, 11], "low_end_range": [12, 32], "presence_max": 40, "correlation_min": 0.35, "wide_expected": True},
    "Schlager / Deutschpop": {"group": "Popular", "target_lufs": -10, "lufs_range": [-12, -8], "crest_range": [6, 12], "low_end_range": [9, 28], "presence_max": 38, "correlation_min": 0.4, "wide_expected": True},
    "Indie / Alternative": {"group": "Popular", "target_lufs": -11, "lufs_range": [-14, -8], "crest_range": [7, 14], "low_end_range": [8, 28], "presence_max": 38, "correlation_min": 0.3, "wide_expected": True},
    "Singer-Songwriter": {"group": "Popular", "target_lufs": -14, "lufs_range": [-17, -11], "crest_range": [9, 18], "low_end_range": [6, 24], "presence_max": 36, "correlation_min": 0.45, "wide_expected": False},
    "Country / Folk": {"group": "Popular", "target_lufs": -12, "lufs_range": [-15, -9], "crest_range": [8, 15], "low_end_range": [8, 26], "presence_max": 36, "correlation_min": 0.45, "wide_expected": False},
    "Rock / Metal": {"group": "Band", "target_lufs": -10, "lufs_range": [-12, -7], "crest_range": [5, 11], "low_end_range": [10, 32], "presence_max": 42, "correlation_min": 0.3, "wide_expected": True},
    "Punk / Hardcore": {"group": "Band", "target_lufs": -9, "lufs_range": [-11, -7], "crest_range": [5, 10], "low_end_range": [8, 28], "presence_max": 44, "correlation_min": 0.25, "wide_expected": True},
    "Blues": {"group": "Band", "target_lufs": -13, "lufs_range": [-16, -10], "crest_range": [8, 16], "low_end_range": [7, 25], "presence_max": 36, "correlation_min": 0.35, "wide_expected": False},
    "Funk / Disco": {"group": "Band", "target_lufs": -11, "lufs_range": [-13, -9], "crest_range": [7, 13], "low_end_range": [12, 32], "presence_max": 38, "correlation_min": 0.35, "wide_expected": True},
    "Acoustic / Jazz": {"group": "Band", "target_lufs": -16, "lufs_range": [-20, -12], "crest_range": [10, 22], "low_end_range": [5, 24], "presence_max": 34, "correlation_min": 0.25, "wide_expected": False},
    "R&B / Soul": {"group": "Urban", "target_lufs": -10, "lufs_range": [-13, -8], "crest_range": [6, 13], "low_end_range": [12, 34], "presence_max": 36, "correlation_min": 0.35, "wide_expected": True},
    "Hip Hop / Rap": {"group": "Urban", "target_lufs": -8, "lufs_range": [-10, -6], "crest_range": [5, 11], "low_end_range": [18, 42], "presence_max": 38, "correlation_min": 0.3, "wide_expected": True},
    "Trap": {"group": "Urban", "target_lufs": -8, "lufs_range": [-10, -6], "crest_range": [5, 10], "low_end_range": [20, 46], "presence_max": 38, "correlation_min": 0.25, "wide_expected": True},
    "Afrobeats": {"group": "Urban", "target_lufs": -10, "lufs_range": [-12, -8], "crest_range": [6, 12], "low_end_range": [12, 34], "presence_max": 38, "correlation_min": 0.35, "wide_expected": True},
    "Reggaeton / Latin": {"group": "Urban", "target_lufs": -9, "lufs_range": [-11, -7], "crest_range": [5, 11], "low_end_range": [14, 36], "presence_max": 40, "correlation_min": 0.35, "wide_expected": True},
    "EDM / Electronic": {"group": "Electronic", "target_lufs": -8, "lufs_range": [-10, -6], "crest_range": [5, 10], "low_end_range": [16, 40], "presence_max": 38, "correlation_min": 0.25, "wide_expected": True},
    "House / Tech House": {"group": "Electronic", "target_lufs": -8, "lufs_range": [-10, -6], "crest_range": [5, 10], "low_end_range": [18, 42], "presence_max": 36, "correlation_min": 0.25, "wide_expected": True},
    "Techno": {"group": "Electronic", "target_lufs": -8, "lufs_range": [-10, -6], "crest_range": [5, 10], "low_end_range": [18, 44], "presence_max": 36, "correlation_min": 0.2, "wide_expected": True},
    "Trance": {"group": "Electronic", "target_lufs": -8, "lufs_range": [-10, -6], "crest_range": [5, 10], "low_end_range": [14, 36], "presence_max": 40, "correlation_min": 0.25, "wide_expected": True},
    "Drum & Bass": {"group": "Electronic", "target_lufs": -7, "lufs_range": [-9, -5], "crest_range": [4, 9], "low_end_range": [22, 48], "presence_max": 40, "correlation_min": 0.2, "wide_expected": True},
    "Dubstep / Bass Music": {"group": "Electronic", "target_lufs": -7, "lufs_range": [-9, -5], "crest_range": [4, 9], "low_end_range": [22, 50], "presence_max": 40, "correlation_min": 0.2, "wide_expected": True},
    "Lo-Fi / Chillhop": {"group": "Electronic", "target_lufs": -12, "lufs_range": [-15, -10], "crest_range": [7, 15], "low_end_range": [10, 32], "presence_max": 34, "correlation_min": 0.25, "wide_expected": True},
    "Ambient / Downtempo": {"group": "Electronic", "target_lufs": -16, "lufs_range": [-22, -12], "crest_range": [9, 24], "low_end_range": [5, 30], "presence_max": 32, "correlation_min": 0.1, "wide_expected": True},
    "Cinematic / Trailer": {"group": "Cinematic", "target_lufs": -8, "lufs_range": [-11, -6], "crest_range": [7, 18], "low_end_range": [12, 42], "presence_max": 40, "correlation_min": 0.15, "wide_expected": True},
    "Film Score": {"group": "Cinematic", "target_lufs": -18, "lufs_range": [-24, -14], "crest_range": [12, 28], "low_end_range": [5, 30], "presence_max": 34, "correlation_min": 0.1, "wide_expected": True},
    "Classical / Orchestral": {"group": "Cinematic", "target_lufs": -20, "lufs_range": [-26, -15], "crest_range": [14, 30], "low_end_range": [4, 28], "presence_max": 32, "correlation_min": 0.05, "wide_expected": True},
    "Meditation / Wellness": {"group": "Spoken & Media", "target_lufs": -18, "lufs_range": [-24, -14], "crest_range": [10, 24], "low_end_range": [4, 24], "presence_max": 30, "correlation_min": 0.15, "wide_expected": True},
    "Podcast / Spoken Word": {"group": "Spoken & Media", "target_lufs": -16, "lufs_range": [-20, -14], "crest_range": [8, 18], "low_end_range": [3, 18], "presence_max": 42, "correlation_min": 0.55, "wide_expected": False},
    "Audiobook": {"group": "Spoken & Media", "target_lufs": -18, "lufs_range": [-21, -16], "crest_range": [8, 18], "low_end_range": [2, 16], "presence_max": 40, "correlation_min": 0.65, "wide_expected": False},
    "Broadcast / TV": {"group": "Spoken & Media", "target_lufs": -23, "lufs_range": [-24, -22], "crest_range": [8, 20], "low_end_range": [3, 20], "presence_max": 40, "correlation_min": 0.45, "wide_expected": False},
    "YouTube / Streaming": {"group": "Spoken & Media", "target_lufs": -14, "lufs_range": [-16, -12], "crest_range": [7, 16], "low_end_range": [6, 28], "presence_max": 38, "correlation_min": 0.35, "wide_expected": True},
    "Streaming / General": {"group": "General", "target_lufs": -14, "lufs_range": [-16, -12], "crest_range": [7, 16], "low_end_range": [6, 30], "presence_max": 38, "correlation_min": 0.3, "wide_expected": True},
}

GENRE_CURVES = {
    "Popular": [(20, -5), (100, 0), (1000, -10), (5000, -18), (20000, -35)],
    "Urban": [(20, 2), (100, 5), (1000, -12), (5000, -22), (20000, -40)],
    "Electronic": [(20, 4), (100, 6), (1000, -10), (5000, -16), (20000, -32)],
    "Band": [(20, -10), (100, -2), (1000, -8), (5000, -14), (20000, -38)],
    "Cinematic": [(20, 0), (100, 2), (1000, -14), (5000, -24), (20000, -45)],
    "Spoken & Media": [(20, -20), (100, -5), (1000, -10), (5000, -15), (20000, -50)],
    "General": [(20, -5), (100, 0), (1000, -12), (5000, -20), (20000, -40)],
}

BAND_DEFS = [
    ("Sub", 20, 60),
    ("Bass", 60, 250),
    ("Low-Mids", 250, 500),
    ("Mids", 500, 2000),
    ("Presence", 2000, 6000),
    ("Air", 6000, 20000),
]

audio_dir = os.path.join(data_dir, "audio")
image_dir = os.path.join(data_dir, "images")
snippet_dir = os.path.join(data_dir, "snippets")

app = Flask(__name__, static_folder=os.path.join(base_dir, "static"), template_folder=os.path.join(base_dir, "templates"))
app.config['UPLOAD_FOLDER'] = audio_dir
app.config['IMAGE_FOLDER'] = image_dir
app.config['SNIPPET_FOLDER'] = snippet_dir
app.config['MAX_CONTENT_LENGTH'] = MAX_UPLOAD_MB * 1024 * 1024

# Generate a persistent secret key for sessions (Public Readiness)
SECRET_PATH = os.path.join(data_dir, ".secret_key")
if os.path.exists(SECRET_PATH):
    with open(SECRET_PATH, "rb") as f:
        app.secret_key = f.read()
else:
    app.secret_key = os.urandom(24)
    with open(SECRET_PATH, "wb") as f:
        f.write(app.secret_key)

DB_PATH = os.path.join(data_dir, 'history.db')

for directory in (data_dir, audio_dir, image_dir, snippet_dir):
    os.makedirs(directory, exist_ok=True)

def init_db():
    with sqlite3.connect(DB_PATH) as conn:
        conn.execute('''
            CREATE TABLE IF NOT EXISTS analyses (
                id TEXT PRIMARY KEY,
                filename TEXT,
                genre TEXT,
                timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
                is_reference INTEGER DEFAULT 0,
                data TEXT
            )
        ''')

init_db()

def get_bin_path(cmd):
    names = [cmd]
    if os.name == "nt" and not cmd.lower().endswith(".exe"):
        names = [f"{cmd}.exe", cmd]

    # 1. Check if we are running in a PyInstaller bundle
    if getattr(sys, 'frozen', False):
        for name in names:
            bundle_bin = os.path.join(sys._MEIPASS, "bin", name)
            if os.path.exists(bundle_bin):
                return bundle_bin
            
    # 2. Check in local bin folder (relative to script)
    script_dir = os.path.dirname(os.path.abspath(__file__))
    for name in names:
        local_bin = os.path.join(script_dir, "bin", name)
        if os.path.exists(local_bin):
            return local_bin

    # 3. Check standard system paths
    for name in names:
        for p in [f"/opt/homebrew/bin/{name}", f"/usr/local/bin/{name}", f"/usr/bin/{name}"]:
            if os.path.exists(p):
                return p
            
    # 3. Fallback to shutil.which
    for name in names:
        found = shutil.which(name)
        if found:
            return found
    return names[0]

FFMPEG_CMD = get_bin_path("ffmpeg")
FFPROBE_CMD = get_bin_path("ffprobe")

def have_ffmpeg():
    try:
        subprocess.run([FFMPEG_CMD, "-version"], stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=True, text=True, timeout=10)
        return True
    except Exception:
        return False

def allowed_audio_file(filename):
    return os.path.splitext(filename or "")[1].lower() in ALLOWED_AUDIO_EXTENSIONS

def json_error(message, status=400):
    return jsonify({"error": message}), status

def safe_remove(path):
    try:
        if path and os.path.exists(path):
            os.remove(path)
    except OSError:
        pass

def build_slices_meta(total):
    slices_meta = [
        {"tag": "Intro", "start": 0.0, "duration": min(SLICE_SEC, total)}
    ]
    if total > SLICE_SEC:
        start = max(0.0, (total / 2.0) - (SLICE_SEC / 2.0))
        slices_meta.append({"tag": "Middle", "start": start, "duration": min(SLICE_SEC, total - start)})
    if total > SLICE_SEC * 1.5:
        start = max(0.0, total - SLICE_SEC)
        slices_meta.append({"tag": "Outro", "start": start, "duration": min(SLICE_SEC, total - start)})
    return slices_meta

def media_filename_from_url(url, prefix):
    if not isinstance(url, str) or not url.startswith(prefix):
        return None
    return os.path.basename(url.split("?", 1)[0])

def media_files_from_analysis(data):
    files = {"audio": set(), "images": set(), "snippets": set()}
    if not isinstance(data, dict):
        return files
    audio_name = media_filename_from_url(data.get("audio_url"), "/media/audio/")
    if audio_name:
        files["audio"].add(audio_name)
    for slice_data in data.get("slices", []):
        if not isinstance(slice_data, dict):
            continue
        for key in ("waveform_url", "spectrum_url"):
            image_name = media_filename_from_url(slice_data.get(key), "/media/images/")
            if image_name:
                files["images"].add(image_name)
    return files

def merge_media_files(target, source):
    for key, names in source.items():
        target.setdefault(key, set()).update(names)

def remove_analysis_media(data):
    files = media_files_from_analysis(data)
    for filename in files["audio"]:
        safe_remove(os.path.join(app.config['UPLOAD_FOLDER'], filename))
    for filename in files["images"]:
        safe_remove(os.path.join(app.config['IMAGE_FOLDER'], filename))
    for filename in files["snippets"]:
        safe_remove(os.path.join(app.config['SNIPPET_FOLDER'], filename))

def probe_duration(path):
    res = subprocess.run(
        [FFPROBE_CMD, "-v", "error", "-show_entries", "format=duration", "-of", "default=nw=1:nk=1", path],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        check=True,
        timeout=PROCESS_TIMEOUT_SECONDS,
    )
    total = float(res.stdout.strip())
    if not np.isfinite(total) or total <= 0:
        raise ValueError("Audio duration is empty or invalid.")
    return total

def get_genre_profile(genre):
    return GENRE_PROFILES.get(genre, GENRE_PROFILES[FALLBACK_GENRE])

def serialize_genre_profile(name):
    profile = dict(get_genre_profile(name))
    profile["name"] = name if name in GENRE_PROFILES else FALLBACK_GENRE
    return profile

def genre_profiles_payload():
    return [
        {"name": name, **profile}
        for name, profile in GENRE_PROFILES.items()
    ]

def get_target_lufs(genre):
    return get_genre_profile(genre)["target_lufs"]

def in_range(value, value_range):
    return value is not None and value_range[0] <= value <= value_range[1]

def mean_available(values):
    nums = [float(v) for v in values if v is not None and np.isfinite(float(v))]
    return float(np.mean(nums)) if nums else None

BAND_ALIASES = {
    "Bässe": {"Bässe", "Sub", "Bass"},
    "Mitten": {"Mitten", "Mids", "Presence"},
    "Höhen": {"Höhen", "Air"},
}

def band_percent(slice_data, names):
    expanded_names = set(names)
    for name in names:
        expanded_names.update(BAND_ALIASES.get(name, {name}))
    return sum(
        band.get("percent", 0)
        for band in slice_data.get("bands", [])
        if band.get("name") in expanded_names
    )

def cut_to_wav(src, out_wav, start=0, duration=30, sr=DEFAULT_SR):
    cmd = [FFMPEG_CMD, "-hide_banner", "-loglevel", "error"]
    if start > 0:
        cmd += ["-ss", str(start)]
    cmd += ["-t", str(max(duration, 0.1)), "-i", src, "-map", "0:a:0", "-vn", "-ac", "2", "-ar", str(sr), "-sample_fmt", "s16", "-y", out_wav]
    subprocess.run(cmd, check=True, timeout=PROCESS_TIMEOUT_SECONDS)

def read_wav(path):
    with contextlib.closing(wave.open(path, 'rb')) as wf:
        sr = wf.getframerate()
        channels = wf.getnchannels()
        data = wf.readframes(wf.getnframes())
    samples = np.frombuffer(data, dtype=np.int16).astype(np.float32) / 32768.0
    if channels == 2:
        samples = samples.reshape(-1, 2)
    return sr, samples

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

def loudnorm_snapshot(src, start=0, duration=30):
    cmd = [FFMPEG_CMD, "-hide_banner", "-nostats"]
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

def ebur128_snapshot(src, start=0, duration=30):
    cmd = [FFMPEG_CMD, "-hide_banner", "-nostats"]
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

def loudness_snapshot(src, start=0, duration=30):
    snapshot = loudnorm_snapshot(src, start=start, duration=duration)
    if snapshot and snapshot.get("I") is not None:
        return snapshot
    snapshot = ebur128_snapshot(src, start=start, duration=duration)
    if snapshot:
        return snapshot
    return {"I": None, "LRA": None, "TP": None, "SP": None, "method": "unavailable"}

def lufs_snapshot(src, start=0, duration=30):
    snapshot = loudness_snapshot(src, start=start, duration=duration)
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
    
    # Resonance Detection (peaks > 7.5dB above local average)
    all_res = []
    if mag_db.size > 100:
        smoothed = np.convolve(mag_db, np.ones(50)/50.0, mode='same')
        diff = mag_db - smoothed
        # Wider window (25 bins) to avoid redundant peaks in high-res FFT
        for i in range(25, len(diff)-25):
            if diff[i] > 7.5 and diff[i] == np.max(diff[i-25:i+25]):
                f = float(freqs[i])
                if 20 < f < 18000:
                    all_res.append({"freq": round(f, 0), "gain": round(float(diff[i]), 1)})

    # Limit to top resonance per band as requested
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
    # Shift target curve to match overall level roughly
    avg_level = np.mean(mag_db[analysis_mask]) if np.any(analysis_mask) else -40
    target_curve = []
    for f, v in curve_pts:
        target_curve.append({"f": f, "v": v + avg_level + 10})  # Offset for visualization

    # ── Down-sampled FFT data for interactive frontend chart (max 512 log-spaced points) ──
    NUM_POINTS = 512
    log_freqs = np.logspace(np.log10(20), np.log10(20000), NUM_POINTS)
    fft_points_mid  = []
    fft_points_side = []
    for lf in log_freqs:
        # Find nearest FFT bin
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

def plot_waveform(samples, sr, req_id, tag):
    """Render only the waveform PNG. Spectrum is now handled by Chart.js in the frontend."""
    plt.style.use('dark_background')
    mono = get_mono(samples)
    t = np.arange(len(mono)) / sr
    w_filename = f"{req_id}_{tag}_waveform.png"
    w_png = os.path.join(app.config['IMAGE_FOLDER'], w_filename)
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
    """Onset detection + attack time + percussion energy – pure numpy, no librosa."""
    mono = get_mono(samples)
    if mono.size == 0 or sr == 0:
        return {"transient_density": 0.0, "attack_time_ms": 0.0, "percussion_energy_pct": 0.0}

    frame_len = 1024
    hop = 512
    n_frames = (len(mono) - frame_len) // hop
    if n_frames < 2:
        return {"transient_density": 0.0, "attack_time_ms": 0.0, "percussion_energy_pct": 0.0}

    # Frame-wise RMS
    rms_frames = np.array([
        np.sqrt(np.mean(mono[i * hop: i * hop + frame_len] ** 2))
        for i in range(n_frames)
    ], dtype=np.float32)

    # Positive flux (onset function)
    flux = np.diff(rms_frames)
    flux = np.maximum(flux, 0)
    # Robust Thresholding: Mean + 1.5*Std, with an absolute floor (min_threshold)
    # to avoid ghost transients in near-silent or hyper-compressed material.
    min_threshold = 0.001 # Spectral Flux floor
    threshold = max(min_threshold, float(np.mean(flux) + 1.5 * np.std(flux)))
    onset_frames = np.where(flux > threshold)[0]

    duration_sec = len(mono) / sr
    transient_density = round(len(onset_frames) / duration_sec, 2) if duration_sec > 0 else 0.0

    # Attack time: frames from onset to local peak (within 80ms)
    lookahead = max(1, int(0.08 * sr / hop))
    attack_times = []
    for idx in onset_frames:
        end = min(idx + lookahead, len(rms_frames) - 1)
        if end > idx:
            peak_offset = int(np.argmax(rms_frames[idx:end + 1]))
            attack_times.append(peak_offset * hop / sr * 1000.0)
    attack_time_ms = round(float(np.mean(attack_times)), 1) if attack_times else 15.0

    # Percussion energy: spectral energy > 3 kHz relative to total
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

def analyze_slice(src, start, duration, tag, req_id, genre="Pop"):
    snippet = os.path.join(app.config['SNIPPET_FOLDER'], f"_snippet_{req_id}_{tag}.wav")
    try:
        cut_to_wav(src, snippet, start=start, duration=duration, sr=DEFAULT_SR)
        sr, samples = read_wav(snippet)
        
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
        loudness = loudness_snapshot(src, start=start, duration=duration)
        bands, freqs, mag_db, side_mag_db, spectral_features, resonances, target_curve, fft_data = band_distribution(samples, sr, genre)
        quality = audio_quality_metrics(samples, sr, spectral_features)
        transients = analyze_transients(samples, sr)
        w_url = plot_waveform(samples, sr, req_id, tag)
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

def get_insights(slices, genre, lang="de", track_loudness=None, is_instrumental=False):
    insights = []
    target_slice = next((s for s in slices if s['tag'] == 'Middle'), slices[0])
    profile = get_genre_profile(genre)
    
    # Helper to create priority actions
    priority_actions = []
    
    track_lufs = (track_loudness or {}).get("I")
    I = track_lufs if track_lufs is not None else target_slice['I'] if target_slice['I'] is not None else -14
    crest = target_slice['crest_db'] if target_slice['crest_db'] is not None else 10
    correlation = target_slice.get('correlation', 1.0)
    quality = target_slice.get("quality", {})
    low_end = band_percent(target_slice, ("Bässe",))
    presence_pct = band_percent(target_slice, ("Presence",))
    brilliance_pct = band_percent(target_slice, ("Höhen",))
    lufs_low, lufs_high = profile["lufs_range"]
    crest_low, crest_high = profile["crest_range"]
    low_end_min, low_end_max = profile["low_end_range"]
    # Vocal tracks need more presence energy; instrumentals are looser
    presence_max = profile["presence_max"] if is_instrumental else profile["presence_max"] - 4
    presence_vocal_min = 18 if not is_instrumental else 0  # vocals need mids
    
    if quality.get("clipped_percent", 0) > 0.05:
        insights.append({
            "title": "Clipping detected" if lang == "en" else "Clipping erkannt",
            "desc": f"{quality.get('clipped_percent', 0):.2f}% of samples are at or near digital full scale." if lang == "en" else f"{quality.get('clipped_percent', 0):.2f}% der Samples liegen am oder nahe am digitalen Maximum.",
            "vst": "Youlean Loudness Meter, FabFilter Pro-L 2",
            "action": "Lower the final limiter ceiling and inspect clipped buses before mastering." if lang == "en" else "Senke das Limiter-Ceiling und prüfe geclippte Busse vor dem Mastering."
        })
        priority_actions.append("Reduce limiter ceiling to avoid digital clipping." if lang == "en" else "Limiter-Ceiling senken, um Clipping zu vermeiden.")

    if correlation < profile["correlation_min"]:
        insights.append({
            "title": "Phase Issues (Poor Mono Compatibility)" if lang == "en" else "Phasenprobleme (Schlechte Mono-Kompatibilität)",
            "desc": f"Correlation is very low ({correlation:.2f}). Your mix will lose elements when played on mono speakers (phones, club PAs)." if lang == "en" else f"Die Korrelation ist sehr niedrig ({correlation:.2f}). Auf Mono-Lautsprechern (Handy, Club-PA) kommt es zu Auslöschungen.",
            "vst": "Brainworx bx_control V2, iZotope Imager",
            "action": "Check your stereo widening plugins. Use a correlation meter and mono-sum your master frequently." if lang == "en" else "Prüfe deine Stereo-Widener (Chorus/Imager). Schalte den Master oft auf Mono und reduziere extreme Panning-Effekte."
        })
        priority_actions.append("Fix phase correlation issues for mono compatibility." if lang == "en" else "Phasenkorrelation für Mono-Kompatibilität korrigieren.")
    elif correlation > 0.95 and profile.get("wide_expected", True):
        insights.append({
            "title": "Very narrow stereo field" if lang == "en" else "Sehr enges Stereobild",
            "desc": "The mix is almost entirely mono. It might lack width and depth." if lang == "en" else "Der Mix ist fast komplett Mono. Er könnte räumliche Tiefe und Breite vertragen.",
            "vst": "Polyverse Wider, Soundtoys MicroShift",
            "action": "Pan instruments wider or add subtle stereo delay/chorus to synths and backing vocals." if lang == "en" else "Zieh Begleitinstrumente (Gitarren, Synths) weiter nach außen (Hard-Panning) oder nutze Micro-Shifting."
        })

    # Mid/Side Insights
    ms_ratio = target_slice.get("mid_side", {}).get("ms_ratio_db", -100)
    if ms_ratio > -3:
        insights.append({
            "title": "Extreme Stereo Width" if lang == "en" else "Extreme Stereo-Breite",
            "desc": f"Side energy is very high relative to mid ({ms_ratio:.1f} dB). This can cause listener fatigue and phase issues." if lang == "en" else f"Die Seiten-Energie ist sehr hoch im Vergleich zur Mitte ({ms_ratio:.1f} dB). Das kann ermüdend wirken und zu Phasenproblemen führen.",
            "vst": "Brainworx bx_digital V3, FabFilter Pro-Q 3 (M/S Mode)",
            "action": "Use M/S EQ to pull down low frequencies in the side channel (high-pass) or reduce the side gain." if lang == "en" else "Nutze M/S EQ, um tiefe Frequenzen in den Seiten zu senken (High-Pass) oder reduziere den Side-Gain."
        })
        priority_actions.append("Reduce side-channel energy to stabilize stereo image." if lang == "en" else "Seiten-Energie reduzieren, um das Stereobild zu stabilisieren.")
    
    if I > lufs_high:
        insights.append({
            "title": "Too heavily compressed" if lang == "en" else "Zu stark komprimiert",
            "desc": f"The track is very loud at {I:.1f} LUFS for {genre}. Profile range is {lufs_low} to {lufs_high} LUFS." if lang == "en" else f"Der Track ist mit {I:.1f} LUFS sehr laut für {genre}. Der Profilbereich liegt bei {lufs_low} bis {lufs_high} LUFS.",
            "vst": "FabFilter Pro-L 2, iZotope Ozone Maximizer",
            "action": "Reduce limiter gain slightly or use longer attack times on your compressors." if lang == "en" else "Nimm etwas Gain beim Limiter heraus oder verlängere die Attack-Zeiten deiner Kompressoren."
        })
        priority_actions.append(f"Decrease loudness (current {I:.1f} LUFS) to match {genre} standards." if lang == "en" else f"Lautheit senken (aktuell {I:.1f} LUFS), um {genre}-Standards zu entsprechen.")
    elif I < lufs_low:
        insights.append({
            "title": "Too quiet for the genre" if lang == "en" else "Zu leise für das Genre",
            "desc": f"At {I:.1f} LUFS, the track sits below the {genre} profile range ({lufs_low} to {lufs_high} LUFS)." if lang == "en" else f"Mit {I:.1f} LUFS liegt der Track unter dem {genre}-Profilbereich ({lufs_low} bis {lufs_high} LUFS).",
            "vst": "Waves SSL G-Master Buss, Shadow Hills Mastering Compressor",
            "action": "Use more glue compression on busses and drive your mastering limiter a few dB hotter." if lang == "en" else "Verwende mehr Glue-Kompression auf Bussen und fahre deinen Mastering-Limiter ein paar dB heißer an."
        })
        priority_actions.append(f"Increase overall loudness to reach {lufs_low} LUFS minimum." if lang == "en" else f"Gesamtlautheit erhöhen, um mindestens {lufs_low} LUFS zu erreichen.")

    if crest < crest_low:
        insights.append({
            "title": "Flattened transients" if lang == "en" else "Transienten abgeflacht",
            "desc": f"Crest factor is {crest:.1f} dB; this is low for {genre}." if lang == "en" else f"Der Crest-Faktor liegt bei {crest:.1f} dB; das ist niedrig für {genre}.",
            "vst": "SPL Transient Designer, Oeksound Spiff",
            "action": "Reduce compression on the drum bus/snare or use a transient shaper to restore punch." if lang == "en" else "Reduziere Kompression auf Snare/Kick oder nutze einen Transient Shaper, um Punch zurückzuholen."
        })
    elif crest > crest_high:
        insights.append({
            "title": "Dynamics may feel uneven" if lang == "en" else "Dynamik wirkt eventuell ungleichmäßig",
            "desc": f"Crest factor is {crest:.1f} dB, above the expected range for {genre}." if lang == "en" else f"Der Crest-Faktor liegt bei {crest:.1f} dB und damit über dem erwarteten Bereich für {genre}.",
            "vst": "FabFilter Pro-C 2, Klanghelm MJUC",
            "action": "Use gentle bus compression or clip-gain automation to control jumpy peaks without flattening the song." if lang == "en" else "Nutze sanfte Bus-Kompression oder Clip-Gain-Automation, um Sprünge zu kontrollieren, ohne den Song platt zu machen."
        })
        
    if low_end > low_end_max:
        insights.append({
            "title": "Critical Low-End (Muddiness)" if lang == "en" else "Kritischer Low-End Bereich (Muddiness)",
            "desc": f"Sub and bass make up {low_end:.1f}% of the measured spectrum; the {genre} profile expects up to {low_end_max}%." if lang == "en" else f"Sub und Bass machen {low_end:.1f}% des gemessenen Spektrums aus; das {genre}-Profil erwartet bis {low_end_max}%.",
            "vst": "FabFilter Pro-Q 3, Pultec EQP-1A, Trackspacer",
            "action": "Apply clean low-cuts (high-pass) on guitars, keys, and vocals. Sidechain the bass to the kick." if lang == "en" else "Setze saubere Low-Cuts (High-Pass) auf Gitarren, Keys und Vocals. Sidechaine den Bass an die Kick."
        })
        priority_actions.append("Clean up low-end muddiness (too much energy below 300Hz)." if lang == "en" else "Low-End aufräumen (zu viel Energie unter 300Hz).")
    elif low_end < low_end_min:
        insights.append({
            "title": "Weak Low-End" if lang == "en" else "Schwaches Low-End",
            "desc": f"Sub and bass are at {low_end:.1f}%, below the profile floor of {low_end_min}%." if lang == "en" else f"Sub und Bass liegen bei {low_end:.1f}% und damit unter dem Profil-Minimum von {low_end_min}%.",
            "vst": "Waves MaxxBass, Soundtoys Decapitator",
            "action": "Saturate your sub-bass to generate upper harmonics or add subharmonics via plugins." if lang == "en" else "Sättige deinen Sub-Bass (um Obertöne hörbar zu machen) oder füge Subharmonische via Plugin hinzu."
        })

    # Resonances Insight
    resonances = target_slice.get("resonances", [])
    if resonances:
        top_res = sorted(resonances, key=lambda x: x["gain"], reverse=True)[0]
        insights.append({
            "title": "Harsh Resonances Detected" if lang == "en" else "Harte Resonanzen erkannt",
            "desc": f"Found sharp peaks, notably at {top_res['freq']} Hz (+{top_res['gain']} dB)." if lang == "en" else f"Scharfe Peaks gefunden, besonders bei {top_res['freq']} Hz (+{top_res['gain']} dB).",
            "vst": "Oeksound Soothe2, FabFilter Pro-Q 3",
            "action": "Apply a narrow notch filter or use dynamic EQ at the detected frequencies." if lang == "en" else "Nutze schmalbandige Notch-Filter oder dynamischen EQ bei den erkannten Frequenzen."
        })
        priority_actions.append(f"Tame harsh resonance at {top_res['freq']} Hz." if lang == "en" else f"Harte Resonanz bei {top_res['freq']} Hz bändigen.")

    if presence_pct > presence_max:
        insights.append({
            "title": "Harshness in upper mids" if lang == "en" else "Harshness in den oberen Mitten",
            "desc": f"Energy between 1-5 kHz is {presence_pct:.1f}%, above this profile's comfort zone." if lang == "en" else f"Die Energie zwischen 1-5 kHz liegt bei {presence_pct:.1f}% und damit über der Komfortzone dieses Profils.",
            "vst": "Oeksound Soothe2, FabFilter Pro-DS, Gullfoss",
            "action": "Check vocals, cymbals, and synths for resonances. Use a de-esser or a dynamic EQ." if lang == "en" else "Überprüfe Vocals, Cymbals und Synths auf Resonanzen. Nutze De-Esser oder einen dynamischen EQ."
        })

    if not is_instrumental and presence_pct < presence_vocal_min:
        insights.append({
            "title": "Vocals may lack presence" if lang == "en" else "Vocals könnten Präsenz fehlen",
            "desc": f"Mid-range energy (1-5 kHz) is only {presence_pct:.1f}% — vocals may sound distant or thin." if lang == "en" else f"Die Mitten-Energie (1-5 kHz) ist nur {presence_pct:.1f}% — Vocals könnten dünn oder weit entfernt klingen.",
            "vst": "FabFilter Pro-Q 3, Neve 1073, API 550",
            "action": "Boost a wide shelf around 2-4 kHz on the vocal bus or reduce competing mid instruments." if lang == "en" else "Hebe einen breiten Shelf bei 2-4 kHz auf dem Vocal-Bus an oder reduziere konkurrierende Mitten-Instrumente."
        })

    if brilliance_pct > 18 and genre not in ["Classical / Orchestral", "Film Score"]:
        insights.append({
            "title": "Very bright top end" if lang == "en" else "Sehr heller Höhenbereich",
            "desc": f"Brilliance above 12 kHz is unusually high at {brilliance_pct:.1f}%." if lang == "en" else f"Die Brillanz über 12 kHz ist mit {brilliance_pct:.1f}% ungewöhnlich hoch.",
            "vst": "FabFilter Pro-Q 3, TDR Nova",
            "action": "Check cymbals, exciters, and broad high-shelf boosts. Try a gentle dynamic high shelf." if lang == "en" else "Prüfe Cymbals, Exciter und breite High-Shelf-Boosts. Teste ein sanftes dynamisches High-Shelf."
        })

    # Transient Insights
    transients = target_slice.get("transients", {})
    t_density = transients.get("transient_density", 0.0) if transients else 0.0
    t_attack = transients.get("attack_time_ms", 15.0) if transients else 15.0
    crest_low_t, crest_high_t = profile["crest_range"]
    if t_density > 0 and t_density < 1.0 and t_attack > 30 and crest is not None and crest < crest_low_t:
        insights.append({
            "title": "Transients over-compressed" if lang == "en" else "Transienten überkomprimiert",
            "desc": f"Only {t_density:.1f} onsets/sec with slow average attack {t_attack:.0f} ms — punch has been squeezed out." if lang == "en" else f"Nur {t_density:.1f} Einsätze/Sek. mit langsamem Anschlag ({t_attack:.0f} ms) — der Punch wurde herausgepresst.",
            "vst": "SPL Transient Designer, Oeksound Spiff, Waves Smack Attack",
            "action": "Increase attack time on your bus compressor or use a transient shaper to restore punch." if lang == "en" else "Verlängere die Attack-Zeit deines Bus-Kompressors oder nutze einen Transient Shaper, um den Punch zurückzuholen."
        })
        priority_actions.append("Restore punch — transients over-compressed." if lang == "en" else "Punch zurückbringen — Transienten überkomprimiert.")
    elif t_density > 8 and t_attack < 4:
        insights.append({
            "title": "Very fast / hyper-transient mix" if lang == "en" else "Sehr schnelle Transienten",
            "desc": f"{t_density:.1f} onsets/sec with extremely fast attack ({t_attack:.0f} ms). May cause listening fatigue." if lang == "en" else f"{t_density:.1f} Einsätze/Sek. mit sehr schnellem Anschlag ({t_attack:.0f} ms). Kann zu Ermüdung führen.",
            "vst": "FabFilter Pro-C 2, Klanghelm MJUC",
            "action": "Add light parallel compression to smooth transient spikes without killing dynamics." if lang == "en" else "Füge leichte Parallelkompression hinzu, um Transientenspitzen zu glätten ohne die Dynamik zu zerstören."
        })

    if not insights:
        insights.append({
            "title": "Excellent Balance" if lang == "en" else "Exzellente Ausgewogenheit",
            "desc": "Your metrics sit inside the selected genre profile." if lang == "en" else "Die Metriken liegen sauber innerhalb des gewählten Genre-Profils.",
            "vst": "Ozone Vintage Tape, Black Box HG-2",
            "action": "Just add subtle analog warmth or saturation for the final 10% polish on the master bus." if lang == "en" else "Füge nur noch subtile analoge Wärme oder Sättigung für den letzten 10% \"Polish\" auf dem Master-Bus hinzu."
        })

    return {
        "insights": insights,
        "priority_actions": priority_actions[:3] if priority_actions else []
    }

def aggregate_slices(slices):
    return {
        "I": mean_available([s.get("I") for s in slices]),
        "LRA": mean_available([s.get("LRA") for s in slices]),
        "crest_db": mean_available([s.get("crest_db") for s in slices]),
        "correlation": mean_available([s.get("correlation") for s in slices]),
        "low_end_percent": mean_available([band_percent(s, ("Bässe",)) for s in slices]),
        "presence_percent": mean_available([band_percent(s, ("Mitten",)) for s in slices]),
        "clipped_percent": mean_available([s.get("quality", {}).get("clipped_percent") for s in slices]),
        "silence_percent": mean_available([s.get("quality", {}).get("silence_percent") for s in slices]),
    }

def analysis_confidence(slices, total_duration):
    score = 100
    issues = []
    if total_duration < 20:
        score -= 12
        issues.append("short-file")
    if len(slices) == 1:
        score -= 10
        issues.append("single-slice")
    if any(s.get("I") is None for s in slices):
        score -= 20
        issues.append("loudness-fallback")
    if any(s.get("loudness_method") != "ffmpeg loudnorm" for s in slices):
        score -= 8
        issues.append("non-primary-loudness-method")
    max_clipping = max((s.get("quality", {}).get("clipped_percent", 0) for s in slices), default=0)
    if max_clipping > 0.05:
        score -= 18
        issues.append("clipping")
    avg_silence = mean_available([s.get("quality", {}).get("silence_percent") for s in slices]) or 0
    if avg_silence > 35:
        score -= 10
        issues.append("high-silence")
    score = max(0, min(100, score))
    if score >= 85:
        label = "high"
    elif score >= 65:
        label = "medium"
    else:
        label = "low"
    return {"score": score, "label": label, "issues": issues}

def build_summary(slices, genre, total_duration, track_loudness=None):
    if not slices:
        # Initial empty state or only full track data available
        target_slice = {}
        aggregate = {}
        confidence = {"score": 50, "label": "low", "issues": ["Awaiting slice analysis..."]}
    else:
        target_slice = next((s for s in slices if s['tag'] == 'Middle'), slices[0])
        aggregate = aggregate_slices(slices)
        confidence = analysis_confidence(slices, total_duration)

    profile = get_genre_profile(genre)
    target_lufs = profile["target_lufs"]
    track_lufs = (track_loudness or {}).get("I")
    measured_lufs = track_lufs if track_lufs is not None else aggregate.get("I") if aggregate.get("I") is not None else target_slice.get('I')
    loudness_scope = "full-track" if track_lufs is not None else "slice-average"
    lufs_delta = None if measured_lufs is None else round(measured_lufs - target_lufs, 1)
    correlation = aggregate.get("correlation") if aggregate.get("correlation") is not None else target_slice.get('correlation', 1.0)
    low_end = aggregate.get("low_end_percent") if aggregate.get("low_end_percent") is not None else band_percent(target_slice, ("Bässe",))
    crest = aggregate.get("crest_db")
    clipped = aggregate.get("clipped_percent") or 0
    if measured_lufs is None or confidence["score"] < 60:
        verdict = "measurement-limited"
    elif (
        in_range(measured_lufs, profile["lufs_range"])
        and correlation >= profile["correlation_min"]
        and in_range(low_end, profile["low_end_range"])
        and (crest is None or in_range(crest, profile["crest_range"]))
        and clipped <= 0.05
    ):
        verdict = "ready"
    elif correlation < profile["correlation_min"] or low_end > profile["low_end_range"][1] or clipped > 0.05:
        verdict = "needs-attention"
    else:
        verdict = "polish"
    verdict_scores = {"ready": 100, "polish": 72, "needs-attention": 45, "measurement-limited": 50}
    base = verdict_scores.get(verdict, 60)

    # Weighted overall mix score (0–100)
    def _score_lufs(delta):
        if delta is None: return 50
        return max(0, 100 - abs(delta) * 7)

    def _score_corr(corr, min_corr):
        if corr is None: return 50
        if corr >= min_corr: return min(100, int(corr * 100))
        return max(0, int(corr * 100) - 20)

    def _score_low(low, lo, hi):
        if low is None: return 70
        if lo <= low <= hi: return 100
        excess = max(abs(low - lo), abs(low - hi))
        return max(0, 100 - int(excess * 4))

    def _score_crest(cr, lo, hi):
        if cr is None: return 70
        if lo <= cr <= hi: return 100
        excess = max(abs(cr - lo), abs(cr - hi))
        return max(0, 100 - int(excess * 5))

    s_lufs  = _score_lufs(lufs_delta)
    s_corr  = _score_corr(correlation, profile["correlation_min"])
    s_low   = _score_low(low_end, profile["low_end_range"][0], profile["low_end_range"][1])
    s_crest = _score_crest(crest, profile["crest_range"][0], profile["crest_range"][1])
    s_clip  = 100 if clipped <= 0.01 else max(0, 100 - int(clipped * 200))

    overall_score = int(
        s_lufs  * 0.28 +
        s_corr  * 0.25 +
        s_low   * 0.20 +
        s_crest * 0.15 +
        s_clip  * 0.12
    )
    overall_score = max(0, min(100, overall_score))

    return {
        "slice": target_slice.get('tag'),
        "target_lufs": target_lufs,
        "measured_lufs": measured_lufs,
        "loudness_scope": loudness_scope,
        "lufs_delta": lufs_delta,
        "correlation": round(correlation, 2),
        "low_end_percent": round(low_end, 1),
        "crest_db": round(crest, 1) if crest is not None else None,
        "profile": serialize_genre_profile(genre),
        "confidence": confidence,
        "aggregate": aggregate,
        "verdict": verdict,
        "slice_count": len(slices),
        "overall_score": overall_score,
        "score_components": {
            "lufs": s_lufs, "correlation": s_corr,
            "low_end": s_low, "crest": s_crest, "clipping": s_clip
        },
    }

def cleanup_old_files():
    now = time.time()
    referenced = {"audio": set(), "images": set(), "snippets": set()}
    stale_ids = []

    with sqlite3.connect(DB_PATH) as conn:
        conn.row_factory = sqlite3.Row
        ttl_arg = f"-{MEDIA_TTL_SECONDS} seconds"
        stale_rows = conn.execute(
            "SELECT id, data FROM analyses WHERE is_reference = 0 AND timestamp < datetime('now', ?)",
            (ttl_arg,),
        ).fetchall()

        for row in stale_rows:
            try:
                remove_analysis_media(json.loads(row["data"]))
                stale_ids.append(row["id"])
            except (TypeError, json.JSONDecodeError):
                stale_ids.append(row["id"])

        if stale_ids:
            conn.executemany("DELETE FROM analyses WHERE id = ?", [(item_id,) for item_id in stale_ids])

        remaining_rows = conn.execute("SELECT data FROM analyses").fetchall()

    for row in remaining_rows:
        try:
            merge_media_files(referenced, media_files_from_analysis(json.loads(row["data"])))
        except (TypeError, json.JSONDecodeError):
            continue

    cleanup_targets = [
        (app.config['UPLOAD_FOLDER'], "audio"),
        (app.config['IMAGE_FOLDER'], "images"),
        (app.config['SNIPPET_FOLDER'], "snippets"),
    ]
    for directory, media_type in cleanup_targets:
        for f in glob.glob(os.path.join(directory, '*')):
            filename = os.path.basename(f)
            if os.path.isfile(f) and filename not in referenced[media_type] and now - os.path.getmtime(f) > MEDIA_TTL_SECONDS:
                safe_remove(f)

@app.route("/")
def index():
    return render_template("index.html")

@app.route("/health")
def health():
    return jsonify({
        "status": "ok",
        "ffmpeg": have_ffmpeg(),
        "data_dir": data_dir,
    })

@app.route("/genres")
def genres():
    return jsonify({
        "default": DEFAULT_GENRE,
        "fallback": FALLBACK_GENRE,
        "genres": genre_profiles_payload(),
    })

@app.route("/init_check")
def init_check():
    """System check for FFmpeg and environment status."""
    ffmpeg_found = have_ffmpeg()
    return jsonify({
        "success": True,
        "ffmpeg": ffmpeg_found,
        "ffmpeg_path": FFMPEG_CMD if ffmpeg_found else None,
        "os": sys.platform,
        "version": "1.1.0-pro",
        "data_dir": data_dir
    })

@app.route("/media/audio/<path:filename>")
def media_audio(filename):
    return send_from_directory(app.config['UPLOAD_FOLDER'], filename)

@app.route("/media/images/<path:filename>")
def media_images(filename):
    return send_from_directory(app.config['IMAGE_FOLDER'], filename)

@app.errorhandler(413)
def file_too_large(_error):
    return json_error(f"File is larger than the {MAX_UPLOAD_MB}MB upload limit.", 413)

@app.route("/upload", methods=["POST"])
def upload():
    cleanup_old_files()
    if not have_ffmpeg():
        return json_error("ffmpeg was not found. Please install ffmpeg or contact support.", 500)
        
    if 'audio' not in request.files:
        return json_error("No audio file provided.", 400)
        
    file = request.files['audio']
    genre = request.form.get('genre', DEFAULT_GENRE)
    if genre not in GENRE_PROFILES:
        genre = FALLBACK_GENRE

    if file.filename == '':
        return json_error("Empty file.", 400)
    if not allowed_audio_file(file.filename):
        return json_error("Unsupported file format.", 400)
        
    req_id = uuid.uuid4().hex[:10]
    ext = os.path.splitext(secure_filename(file.filename))[1].lower() or os.path.splitext(file.filename)[1].lower()
    display_filename = os.path.basename(file.filename.replace("\\", "/"))
    is_instrumental = request.form.get('is_instrumental', '0') == '1'
    
    save_filename = f"upload_{req_id}{ext}"
    save_path = os.path.join(app.config['UPLOAD_FOLDER'], save_filename)
    file.save(save_path)
    
    try:
        total = probe_duration(save_path)
    except subprocess.TimeoutExpired:
        safe_remove(save_path)
        return json_error("FFmpeg process timed out. The file might be too complex or corrupt.", 504)
    except Exception as e:
        safe_remove(save_path)
        # Specific error messaging for public readiness
        err_msg = str(e)
        if "Invalid data found" in err_msg:
            return json_error("Corrupt or invalid audio file format.", 400)
        return json_error(f"Could not analyze file: {err_msg}", 400)

    slices_meta = build_slices_meta(total)
    
    # Initial analysis record - full track loudness moved to separate step
    initial_data = {
        "id": req_id,
        "filename": display_filename,
        "genre": genre,
        "is_instrumental": is_instrumental,
        "total_duration": total,
        "slices": [],
        "slices_meta": slices_meta,
        "track_loudness": None, # Will be filled by /analyze_full_track
        "audio_url": f"/media/audio/{save_filename}"
    }
    
    with sqlite3.connect(DB_PATH) as conn:
        conn.execute(
            'INSERT INTO analyses (id, filename, genre, data) VALUES (?, ?, ?, ?)',
            (req_id, display_filename, genre, json.dumps(initial_data))
        )
        
    return jsonify({
        "success": True, 
        "req_id": req_id, 
        "filename": display_filename, 
        "audio_url": initial_data["audio_url"],
        "slices_meta": slices_meta,
        "total_duration": total
    })


@app.route("/analyze_full_track/<req_id>", methods=["POST"])
def analyze_full_track(req_id):
    """Perform full-track loudness analysis asynchronously (called from frontend)."""
    with sqlite3.connect(DB_PATH) as conn:
        cursor = conn.execute('SELECT data FROM analyses WHERE id = ?', (req_id,))
        row = cursor.fetchone()
        if not row:
            return json_error("Analysis not found.", 404)
        data = json.loads(row[0])
    
    filename = os.path.basename(data["audio_url"])
    save_path = os.path.join(app.config['UPLOAD_FOLDER'], filename)
    
    if not os.path.exists(save_path):
        return json_error("Audio file no longer exists.", 404)
        
    # Large file optimization: loudness_snapshot uses ffmpeg streaming
    track_loudness = loudness_snapshot(save_path, start=0, duration=data["total_duration"])
    
    data["track_loudness"] = track_loudness
    data["summary"] = build_summary(data["slices"], data["genre"], data["total_duration"], track_loudness=track_loudness)
    
    with sqlite3.connect(DB_PATH) as conn:
        conn.execute('UPDATE analyses SET data = ? WHERE id = ?', (json.dumps(data), req_id))
        
    return jsonify({"success": True, "current_analysis": data})

@app.route("/analyze_slice/<req_id>/<tag>", methods=["POST"])
def analyze_slice_endpoint(req_id, tag):
    with sqlite3.connect(DB_PATH) as conn:
        cursor = conn.execute('SELECT data, genre FROM analyses WHERE id = ?', (req_id,))
        row = cursor.fetchone()
        if not row:
            return json_error("Analysis session not found.", 404)
        
        current_data = json.loads(row[0])
        genre = row[1]
    
    save_filename = os.path.basename(current_data["audio_url"])
    save_path = os.path.join(app.config['UPLOAD_FOLDER'], save_filename)
    
    if not os.path.exists(save_path):
        return json_error("Source file no longer exists.", 410)
    
    total = current_data["total_duration"]
    slices_meta = current_data.get("slices_meta") or build_slices_meta(total)
    requested_meta = next((item for item in slices_meta if item["tag"] == tag), None)
    if not requested_meta:
        return json_error("Unknown analysis slice.", 400)
    start = requested_meta["start"]
    duration = requested_meta["duration"]
        
    try:
        slice_result = analyze_slice(save_path, start, duration, tag, req_id, genre)
    except Exception as e:
        return json_error(f"Slice analysis failed: {e}", 500)
    
    # Update DB
    with sqlite3.connect(DB_PATH) as conn:
        # Get latest data again to avoid race conditions
        cursor = conn.execute('SELECT data FROM analyses WHERE id = ?', (req_id,))
        latest_data = json.loads(cursor.fetchone()[0])
        latest_data["slices"] = [
            item for item in latest_data.get("slices", [])
            if item.get("tag") != tag
        ]
        latest_data["slices"].append(slice_result)
        
        # Sort slices by start time to maintain consistent UI order
        latest_data["slices"].sort(key=lambda x: x["start"])
        
        # If this was the last slice (or if we have enough to summarize), update summary/insights
        # For simplicity, we recalculate summary and insights whenever a slice is added
        lang = request.args.get('lang', 'de')
        latest_data.setdefault("slices_meta", slices_meta)
        track_loudness = latest_data.get("track_loudness")
        is_instrumental = latest_data.get("is_instrumental", False)
        latest_data["summary"] = build_summary(latest_data["slices"], latest_data["genre"], total, track_loudness)
        
        # New Insights Structure
        insights_payload = get_insights(latest_data["slices"], latest_data["genre"], "de", track_loudness, is_instrumental)
        latest_data["insights_by_lang"] = {
            "de": insights_payload,
            "en": get_insights(latest_data["slices"], latest_data["genre"], "en", track_loudness, is_instrumental),
        }
        latest_data["priority_actions"] = insights_payload.get("priority_actions", [])
        latest_data["insights"] = latest_data["insights_by_lang"].get(lang, latest_data["insights_by_lang"]["de"]).get("insights", [])
        
        conn.execute('UPDATE analyses SET data = ? WHERE id = ?', (json.dumps(latest_data), req_id))
        
    return jsonify({
        "slice": slice_result,
        "is_complete": {item["tag"] for item in slices_meta}.issubset({item.get("tag") for item in latest_data["slices"]}),
        "current_analysis": latest_data
    })

@app.route('/history', methods=['GET'])
def get_history():
    with sqlite3.connect(DB_PATH) as conn:
        conn.row_factory = sqlite3.Row
        cursor = conn.execute('SELECT id, filename, genre, timestamp, is_reference FROM analyses ORDER BY timestamp DESC')
        rows = cursor.fetchall()
        return jsonify([dict(r) for r in rows])

@app.route('/analysis/<req_id>', methods=['GET'])
def get_analysis(req_id):
    with sqlite3.connect(DB_PATH) as conn:
        cursor = conn.execute('SELECT data FROM analyses WHERE id = ?', (req_id,))
        row = cursor.fetchone()
        if row:
            return jsonify(json.loads(row[0]))
        return json_error("Analysis not found.", 404)

@app.route('/toggle_reference/<req_id>', methods=['POST'])
def toggle_reference(req_id):
    with sqlite3.connect(DB_PATH) as conn:
        cursor = conn.execute('SELECT is_reference FROM analyses WHERE id = ?', (req_id,))
        row = cursor.fetchone()
        if row:
            new_val = 0 if row[0] == 1 else 1
            conn.execute('UPDATE analyses SET is_reference = ? WHERE id = ?', (new_val, req_id))
            return jsonify({"success": True, "is_reference": new_val})
        return json_error("Analysis not found.", 404)

@app.route('/fft_data/<req_id>/<tag>', methods=['GET'])
def get_fft_data(req_id, tag):
    """Return pre-computed FFT data (512 log-spaced points) for a given analysis slice."""
    with sqlite3.connect(DB_PATH) as conn:
        cursor = conn.execute('SELECT data FROM analyses WHERE id = ?', (req_id,))
        row = cursor.fetchone()
        if not row:
            return json_error("Analysis not found.", 404)
        data = json.loads(row[0])
    slice_data = next((s for s in data.get("slices", []) if s.get("tag") == tag), None)
    if not slice_data:
        return json_error("Slice not found.", 404)
    fft = slice_data.get("fft_data")
    if not fft:
        return json_error("FFT data not available for this slice.", 404)
    return jsonify(fft)


@app.route('/export_pdf', methods=['POST'])
def export_pdf():
    """Browser-mode fallback: receives base64-encoded PDF and sends it as a download."""
    try:
        payload = request.get_json(force=True)
        b64 = payload.get('data', '')
        filename = secure_filename(payload.get('filename', 'Mix_Analysis_Report.pdf'))
        if not filename.lower().endswith('.pdf'):
            filename += '.pdf'
        pdf_bytes = base64.b64decode(b64)
        response = make_response(pdf_bytes)
        response.headers['Content-Type'] = 'application/pdf'
        response.headers['Content-Disposition'] = f'attachment; filename="{filename}"'
        return response
    except Exception as exc:
        return json_error(f'PDF export failed: {exc}', 400)


if __name__ == "__main__":
    import threading
    import webview

    class PyWebViewApi:
        """Exposes Python methods to the JS layer via window.pywebview.api."""

        def save_pdf(self, base64_data, filename):
            """Open a native macOS Save dialog and write the PDF."""
            import base64 as _b64
            try:
                win = webview.windows[0]
                save_path = win.create_file_dialog(
                    webview.SAVE_DIALOG,
                    directory=os.path.expanduser('~/Desktop'),
                    save_filename=filename,
                    file_types=('PDF Files (*.pdf)',),
                )
                if save_path:
                    # pywebview returns a tuple on some platforms
                    path = save_path[0] if isinstance(save_path, (list, tuple)) else save_path
                    if not path.lower().endswith('.pdf'):
                        path += '.pdf'
                    pdf_bytes = _b64.b64decode(base64_data)
                    print(f"DEBUG: Saving PDF to {path}, size: {len(pdf_bytes)} bytes")
                    with open(path, 'wb') as fh:
                        fh.write(pdf_bytes)
                        fh.flush()
                        os.fsync(fh.fileno())
                    return {'success': True, 'path': path}
                return {'success': False, 'cancelled': True}
            except Exception as exc:
                return {'success': False, 'error': str(exc)}

    api = PyWebViewApi()

    def start_server():
        app.run(host="127.0.0.1", port=5050, debug=False)

    t = threading.Thread(target=start_server)
    t.daemon = True
    t.start()

    # Create the native app window and expose the Python API bridge
    webview.create_window(
        'Mix Analyzer',
        'http://127.0.0.1:5050/',
        width=1280,
        height=800,
        text_select=False,
        js_api=api,
    )
    webview.start(gui='cocoa', debug=False)
