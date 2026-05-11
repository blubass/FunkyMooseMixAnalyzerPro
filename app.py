import os, subprocess, re, uuid, glob, time, sqlite3, json, shutil, sys, base64

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

from core.config import (
    DEFAULT_SR, SLICE_SEC, MAX_UPLOAD_MB, MEDIA_TTL_SECONDS, PROCESS_TIMEOUT_SECONDS,
    ALLOWED_AUDIO_EXTENSIONS, DEFAULT_GENRE, FALLBACK_GENRE,
    GENRE_PROFILES, GENRE_CURVES, BAND_DEFS, BAND_ALIASES,
    get_genre_profile, serialize_genre_profile, genre_profiles_payload, get_target_lufs
)

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

PLUGIN_REPORT_SCHEMA = "funky-moose.mix-analyzer.report"

def finite_number(value, default=None):
    try:
        number = float(value)
        return number if np.isfinite(number) else default
    except (TypeError, ValueError):
        return default

def rounded_number(value, digits=3):
    number = finite_number(value)
    return round(number, digits) if number is not None else None

def plugin_range_to_list(value, fallback):
    if not isinstance(value, dict):
        return list(fallback)
    low = finite_number(value.get("low"))
    high = finite_number(value.get("high"))
    return [low, high] if low is not None and high is not None else list(fallback)

def plugin_confidence_label(value):
    text = str(value or "").strip().lower()
    if "high" in text or "hoch" in text:
        return "high"
    if "medium" in text or "mittel" in text:
        return "medium"
    return "low"

def plugin_report_filename(report):
    explicit_name = report.get("filename") or report.get("trackName") or report.get("title")
    if explicit_name:
        return os.path.basename(str(explicit_name).replace("\\", "/"))
    generated = str(report.get("generatedAt") or time.strftime("%Y-%m-%dT%H:%M:%S"))
    safe_generated = re.sub(r"[^0-9A-Za-z._-]+", "-", generated[:19].replace("T", "-")).strip("-")
    return f"Plugin-Report-{safe_generated or uuid.uuid4().hex[:8]}.json"

def plugin_band_range(name):
    for band_name, low, high in BAND_DEFS:
        if band_name == name:
            return f"{low}-{high}Hz"
    return ""

def plugin_band_for_frequency(freq):
    number = finite_number(freq)
    if number is None:
        return None
    for band_name, low, high in BAND_DEFS:
        if low <= number < high:
            return band_name
    return None

def plugin_profile_payload(genre, plugin_profile):
    profile = dict(serialize_genre_profile(genre))
    if not isinstance(plugin_profile, dict):
        return profile

    profile["target_lufs"] = finite_number(plugin_profile.get("targetLufs"), profile["target_lufs"])
    profile["lufs_range"] = plugin_range_to_list(plugin_profile.get("lufsRange"), profile["lufs_range"])
    profile["crest_range"] = plugin_range_to_list(plugin_profile.get("crestRange"), profile["crest_range"])
    profile["low_end_range"] = plugin_range_to_list(plugin_profile.get("lowEndRange"), profile["low_end_range"])
    profile["presence_max"] = finite_number(plugin_profile.get("presenceMax"), profile["presence_max"])
    profile["correlation_min"] = finite_number(plugin_profile.get("correlationMin"), profile["correlation_min"])
    profile["wide_expected"] = bool(plugin_profile.get("wideExpected", profile["wide_expected"]))
    return profile

def plugin_report_insights(assessment):
    actions = assessment.get("priorityActions") if isinstance(assessment.get("priorityActions"), list) else []
    actions = [str(action) for action in actions if str(action).strip()]
    status_line = str(assessment.get("statusLine") or assessment.get("summary") or assessment.get("verdictTitle") or "").strip()
    first_action = actions[0] if actions else "Review the imported plugin pass and compare it against the selected genre target."
    insight = {
        "title": str(assessment.get("verdictTitle") or "Imported plugin judgement"),
        "desc": status_line or "This pass was imported from the live plugin JSON report.",
        "action": first_action,
        "vst": "Funky Moose Mix Analyzer Plugin",
    }
    return {
        "insights": [insight],
        "priority_actions": actions[:3],
    }

def plugin_report_to_analysis(report, req_id):
    if not isinstance(report, dict) or report.get("schema") != PLUGIN_REPORT_SCHEMA:
        raise ValueError("This is not a Funky Moose plugin JSON report.")

    plugin_profile = report.get("genreProfile") if isinstance(report.get("genreProfile"), dict) else {}
    measurements = report.get("measurements") if isinstance(report.get("measurements"), dict) else {}
    scores = report.get("scores") if isinstance(report.get("scores"), dict) else {}
    assessment = report.get("assessment") if isinstance(report.get("assessment"), dict) else {}
    worst_case = report.get("worstCase") if isinstance(report.get("worstCase"), dict) else {}

    genre = plugin_profile.get("name") if isinstance(plugin_profile.get("name"), str) else FALLBACK_GENRE
    if genre not in GENRE_PROFILES:
        genre = FALLBACK_GENRE
    profile = plugin_profile_payload(genre, plugin_profile)
    analysis_seconds = finite_number(report.get("analysisSeconds"), 0.0) or 0.0
    mode = str(report.get("mode") or "")

    bands = []
    imported_bands = measurements.get("bands") if isinstance(measurements.get("bands"), list) else []
    for item in imported_bands:
        if not isinstance(item, dict):
            continue
        name = str(item.get("name") or "")
        if not name:
            continue
        bands.append({
            "name": name,
            "range": plugin_band_range(name),
            "percent": rounded_number(item.get("percent"), 2),
            "correlation": rounded_number(item.get("correlation"), 3),
            "side_ratio_db": rounded_number(item.get("sideRatioDb"), 3),
        })
    if not bands:
        bands = [{"name": name, "range": f"{low}-{high}Hz", "percent": 0.0} for name, low, high in BAND_DEFS]

    resonance_freq = finite_number(measurements.get("resonanceFreqHz"))
    resonance_gain = finite_number(measurements.get("resonanceGainDb"))
    resonances = []
    if resonance_freq is not None and resonance_gain is not None and resonance_gain > 0:
        resonances.append({
            "freq": round(resonance_freq, 0),
            "gain": round(resonance_gain, 1),
            "band": plugin_band_for_frequency(resonance_freq),
        })

    slice_data = {
        "tag": "Plugin Pass",
        "start": 0.0,
        "duration": analysis_seconds,
        "sr": None,
        "rms_db": rounded_number(measurements.get("rmsDb"), 3),
        "peak_db": rounded_number(measurements.get("samplePeakDb"), 3),
        "crest_db": rounded_number(measurements.get("crestDb"), 3),
        "correlation": rounded_number(measurements.get("correlation"), 3),
        "mid_side": {
            "width_pct": rounded_number(measurements.get("widthPercent"), 2),
            "ms_ratio_db": rounded_number(measurements.get("msRatioDb"), 3),
        },
        "levels": {
            "mono": {
                "rms_db": rounded_number(measurements.get("monoRmsDb"), 3),
                "peak_db": rounded_number(measurements.get("samplePeakDb"), 3),
                "crest_db": rounded_number(measurements.get("crestDb"), 3),
            },
            "left": None,
            "right": None,
            "max_channel_peak_db": rounded_number(measurements.get("samplePeakDb"), 3),
            "combined_rms_db": rounded_number(measurements.get("rmsDb"), 3),
        },
        "I": rounded_number(measurements.get("integratedLufs"), 3),
        "LRA": rounded_number(measurements.get("lraLu"), 3),
        "TP": rounded_number(measurements.get("truePeakDbTp"), 3),
        "SP": rounded_number(measurements.get("samplePeakDb"), 3),
        "loudness_method": "Funky Moose plugin live analyzer",
        "bands": bands,
        "quality": {
            "clipped_percent": rounded_number(measurements.get("clippedPercent"), 4),
            "silence_percent": rounded_number(measurements.get("silencePercent"), 3),
            "dc_offset": rounded_number(measurements.get("dcOffset"), 6),
            "stereo_balance_db": rounded_number(measurements.get("stereoBalanceDb"), 3),
            "spectral_centroid_hz": rounded_number(measurements.get("spectralCentroidHz"), 1),
            "spectral_rolloff_hz": rounded_number(measurements.get("spectralRolloffHz"), 1),
        },
        "resonances": resonances,
        "target_curve": [],
        "transients": {
            "transient_density": rounded_number(measurements.get("transientDensityPerSecond"), 3),
            "attack_time_ms": rounded_number(measurements.get("attackTimeMs"), 2),
            "percussion_energy_pct": rounded_number(measurements.get("percussionEnergyPercent"), 2),
        },
        "imported_report": True,
    }

    confidence_score = int(max(0, min(100, finite_number(scores.get("confidence"), 0) or 0)))
    measured_lufs = slice_data["I"]
    target_lufs = profile["target_lufs"]
    lufs_delta = round(measured_lufs - target_lufs, 1) if measured_lufs is not None and target_lufs is not None else None
    verdict = assessment.get("verdictKey") if assessment.get("verdictKey") in {"ready", "polish", "needs-attention", "measurement-limited"} else "measurement-limited"
    aggregate = {
        "I": measured_lufs,
        "LRA": slice_data["LRA"],
        "crest_db": slice_data["crest_db"],
        "correlation": slice_data["correlation"],
        "low_end_percent": rounded_number(measurements.get("lowEndPercent"), 2),
        "presence_percent": rounded_number(measurements.get("presencePercent"), 2),
        "clipped_percent": slice_data["quality"]["clipped_percent"],
        "silence_percent": slice_data["quality"]["silence_percent"],
    }
    summary = {
        "slice": slice_data["tag"],
        "target_lufs": target_lufs,
        "measured_lufs": measured_lufs,
        "loudness_scope": "plugin-pass",
        "lufs_delta": lufs_delta,
        "correlation": slice_data["correlation"],
        "low_end_percent": aggregate["low_end_percent"],
        "crest_db": slice_data["crest_db"],
        "profile": profile,
        "confidence": {
            "score": confidence_score,
            "label": plugin_confidence_label(assessment.get("confidenceLabel")),
            "issues": [] if assessment.get("measurementReady") else ["plugin-measurement-limited"],
        },
        "aggregate": aggregate,
        "verdict": verdict,
        "slice_count": 1,
        "overall_score": int(max(0, min(100, finite_number(scores.get("overall"), 0) or 0))),
        "score_components": {
            "lufs": int(max(0, min(100, finite_number(scores.get("lufs"), 0) or 0))),
            "correlation": int(max(0, min(100, finite_number(scores.get("correlation"), 0) or 0))),
            "low_end": int(max(0, min(100, finite_number(scores.get("lowEnd"), 0) or 0))),
            "crest": int(max(0, min(100, finite_number(scores.get("crest"), 0) or 0))),
            "clipping": int(max(0, min(100, finite_number(scores.get("clipping"), 0) or 0))),
        },
    }
    insights_payload = plugin_report_insights(assessment)

    return {
        "id": req_id,
        "filename": plugin_report_filename(report),
        "genre": genre,
        "profile": profile,
        "is_instrumental": "instrumental" in mode.lower(),
        "total_duration": analysis_seconds,
        "slices": [slice_data],
        "slices_meta": [{"tag": slice_data["tag"], "start": 0.0, "duration": analysis_seconds}],
        "track_loudness": {
            "I": measured_lufs,
            "LRA": slice_data["LRA"],
            "TP": slice_data["TP"],
            "SP": slice_data["SP"],
            "method": slice_data["loudness_method"],
        },
        "summary": summary,
        "insights": insights_payload["insights"],
        "insights_by_lang": {"de": insights_payload, "en": insights_payload},
        "priority_actions": insights_payload["priority_actions"],
        "audio_url": None,
        "source": report.get("source") or "Funky Moose Mix Analyzer Plugin",
        "imported_plugin_report": {
            "schemaVersion": report.get("schemaVersion"),
            "measurementStandard": report.get("measurementStandard"),
            "generatedAt": report.get("generatedAt"),
            "analysisScope": report.get("analysisScope"),
            "plainTextReport": report.get("plainTextReport"),
            "deliveryPreview": report.get("deliveryPreview"),
            "worstCase": worst_case,
            "reference": report.get("reference"),
            "snapshots": report.get("snapshots"),
        },
    }

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



from core.advice import (
    in_range, mean_available, band_percent,
    get_insights, aggregate_slices, analysis_confidence, build_summary
)

from core.audio import (
    analyze_slice, loudness_snapshot, ebur128_snapshot, loudnorm_snapshot,
    levels, band_distribution
)



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

@app.route("/import_plugin_report", methods=["POST"])
def import_plugin_report():
    cleanup_old_files()
    payload = request.get_json(silent=True)
    if isinstance(payload, dict) and isinstance(payload.get("report"), dict):
        payload = payload["report"]
    if not isinstance(payload, dict):
        return json_error("No plugin JSON report provided.", 400)

    req_id = uuid.uuid4().hex[:10]
    try:
        imported_data = plugin_report_to_analysis(payload, req_id)
    except ValueError as exc:
        return json_error(str(exc), 400)

    with sqlite3.connect(DB_PATH) as conn:
        conn.execute(
            'INSERT INTO analyses (id, filename, genre, data) VALUES (?, ?, ?, ?)',
            (req_id, imported_data["filename"], imported_data["genre"], json.dumps(imported_data))
        )

    return jsonify({
        "success": True,
        "req_id": req_id,
        "filename": imported_data["filename"],
        "current_analysis": imported_data,
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
    track_loudness = loudness_snapshot(FFMPEG_CMD, save_path, start=0, duration=data["total_duration"])
    
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
        slice_result = analyze_slice(FFMPEG_CMD, save_path, start, duration, tag, req_id, genre, snippet_dir, image_dir)
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

@app.route('/export_eq_preset/<req_id>', methods=['GET'])
def export_eq_preset(req_id):
    """Generates a text file with EQ suggestions based on detected resonances."""
    with sqlite3.connect(DB_PATH) as conn:
        cursor = conn.execute('SELECT data FROM analyses WHERE id = ?', (req_id,))
        row = cursor.fetchone()
        if not row:
            return json_error("Analysis not found.", 404)
        data = json.loads(row[0])

    # Extract resonances from the Middle slice (or first available)
    slices = data.get("slices", [])
    target_slice = next((s for s in slices if s.get('tag') == 'Middle'), None)
    if not target_slice and slices:
        target_slice = slices[0]

    resonances = target_slice.get("resonances", []) if target_slice else []

    lines = [
        "========================================",
        f" Funky Moose Pro - EQ Suggestions",
        f" Track: {data.get('filename', 'Unknown')}",
        "========================================",
        "",
        "The following sharp resonances were detected and might need taming:",
        ""
    ]

    if not resonances:
        lines.append("No harsh resonances detected. Your mix looks clean!")
    else:
        # Sort by gain descending
        sorted_res = sorted(resonances, key=lambda x: x.get("gain", 0), reverse=True)
        for i, res in enumerate(sorted_res, 1):
            freq = res.get('freq', 0)
            gain = res.get('gain', 0)
            band = res.get('band', 'Mid')
            lines.append(f"Band {i}: {freq} Hz")
            lines.append(f"  - Action: Dynamic Cut / Notch")
            lines.append(f"  - Gain: -{gain/2:.1f} dB to -{gain:.1f} dB")
            lines.append(f"  - Q-Factor: Narrow (Q > 4.0)")
            lines.append(f"  - Region: {band}")
            lines.append("")

    lines.append("========================================")
    lines.append("Note: Use a dynamic EQ (like FabFilter Pro-Q 3 or Soothe2)")
    lines.append("to only cut when the resonance is active, preserving the")
    lines.append("natural tone of your mix.")

    text_content = "\\n".join(lines)

    response = make_response(text_content)
    response.headers['Content-Type'] = 'text/plain; charset=utf-8'
    clean_name = os.path.splitext(data.get('filename', 'Mix'))[0]
    response.headers['Content-Disposition'] = f'attachment; filename="EQ_Suggestions_{clean_name}.txt"'
    return response


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
