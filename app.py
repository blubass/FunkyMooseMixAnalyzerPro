import os, subprocess, re, wave, contextlib, uuid, glob, time, sqlite3, json, shutil

# Ensure common paths are in PATH so ffmpeg can be found from a .app bundle
os.environ['PATH'] = f"/opt/homebrew/bin:/usr/local/bin:{os.environ.get('PATH', '')}"

from flask import Flask, render_template, request, jsonify
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

import sys
if getattr(sys, 'frozen', False):
    base_dir = sys._MEIPASS
else:
    base_dir = os.path.dirname(os.path.abspath(__file__))

app = Flask(__name__, static_folder=os.path.join(base_dir, "static"), template_folder=os.path.join(base_dir, "templates"))
app.config['UPLOAD_FOLDER'] = os.path.join(base_dir, 'static', 'audio')
app.config['MAX_CONTENT_LENGTH'] = 100 * 1024 * 1024 # 100MB limit

DB_PATH = os.path.join(base_dir, 'history.db')

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

os.makedirs(app.config['UPLOAD_FOLDER'], exist_ok=True)
os.makedirs(os.path.join(base_dir, "static", "images"), exist_ok=True)

DEFAULT_SR = 48000
SLICE_SEC = 30

def get_bin_path(cmd):
    for p in [f"/opt/homebrew/bin/{cmd}", f"/usr/local/bin/{cmd}"]:
        if os.path.exists(p):
            return p
    return shutil.which(cmd) or cmd

FFMPEG_CMD = get_bin_path("ffmpeg")
FFPROBE_CMD = get_bin_path("ffprobe")

def have_ffmpeg():
    try:
        subprocess.run([FFMPEG_CMD, "-version"], stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=True, text=True)
        return True
    except Exception:
        return False

def cut_to_wav(src, out_wav, start=0, duration=30, sr=DEFAULT_SR):
    cmd = [FFMPEG_CMD, "-hide_banner", "-loglevel", "error"]
    if start > 0:
        cmd += ["-ss", str(start)]
    cmd += ["-t", str(duration), "-i", src, "-ac", "2", "-ar", str(sr), "-y", out_wav]
    subprocess.run(cmd, check=True)

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

def levels(samples):
    mono = get_mono(samples)
    rms = float(np.sqrt(np.mean(mono**2)))
    peak = float(np.max(np.abs(mono)))
    rms_db = 20*np.log10(max(rms, 1e-12))
    peak_db = 20*np.log10(max(peak, 1e-12))
    return rms_db, peak_db, peak_db - rms_db

def lufs_snapshot(src, start=0, duration=30):
    cmd = [FFMPEG_CMD, "-hide_banner"]
    if start > 0:
        cmd += ["-ss", str(start)]
    cmd += ["-t", str(duration), "-i", src, "-af", "ebur128=peak=true", "-f", "null", "-"]
    res = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
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
    return I, LRA, TP, SP

def band_distribution(samples, sr):
    mono = get_mono(samples)
    n = 1
    while n < len(mono):
        n *= 2
    win = np.hanning(len(mono))
    X = np.fft.rfft(mono * win, n=n)
    freqs = np.fft.rfftfreq(n, d=1.0/sr)
    power = (np.abs(X) ** 2).astype(np.float64)
    total = float(power.sum()) + 1e-12
    def pct(lo, hi):
        idx = np.where((freqs >= lo) & (freqs < hi))[0]
        return 100.0 * float(power[idx].sum()) / total if len(idx) else 0.0
    bands = [("Sub",0,60),("Bass",60,120),("Low-Mids",120,300),("Mids",300,1000),("Presence",1000,5000),("Air",5000,16000)]
    return [{"name": name, "range": f"{lo}-{hi}Hz", "percent": round(pct(lo,hi),2)} for name,lo,hi in bands], freqs, 20*np.log10(np.maximum(np.abs(X), 1e-12))

def plot_waveform_spectrum(samples, sr, freqs, mag_db, req_id, tag):
    plt.style.use('dark_background')
    
    mono = get_mono(samples)
    t = np.arange(len(mono)) / sr
    w_filename = f"{req_id}_{tag}_waveform.png"
    w_png = os.path.join(base_dir, "static", "images", w_filename)
    fig, ax = plt.subplots(figsize=(10,3))
    ax.plot(t, mono, color="#00f2fe", alpha=0.8, linewidth=0.5)
    ax.set_title(f"{tag} Waveform", color="#ffffff")
    ax.axis('off')
    fig.patch.set_alpha(0.0)
    ax.patch.set_alpha(0.0)
    plt.tight_layout(); plt.savefig(w_png, dpi=120, transparent=True); plt.close()
    
    mask = (freqs>=20)&(freqs<=20000)
    s_filename = f"{req_id}_{tag}_spectrum.png"
    s_png = os.path.join(base_dir, "static", "images", s_filename)
    fig, ax = plt.subplots(figsize=(10,3))
    ax.semilogx(freqs[mask], mag_db[mask], color="#4facfe", linewidth=1.5)
    ax.fill_between(freqs[mask], mag_db[mask], -100, color="#4facfe", alpha=0.3)
    ax.set_ylim(-100, np.max(mag_db[mask]) + 10)
    ax.set_title(f"{tag} Magnitude Spectrum", color="#ffffff")
    ax.grid(True, which="both", color="#ffffff", alpha=0.1)
    ax.tick_params(colors="#ffffff")
    for spine in ax.spines.values():
        spine.set_color('#ffffff')
        spine.set_alpha(0.2)
    fig.patch.set_alpha(0.0)
    ax.patch.set_alpha(0.0)
    plt.tight_layout(); plt.savefig(s_png, dpi=120, transparent=True); plt.close()
    
    return f"/static/images/{w_filename}", f"/static/images/{s_filename}"

def analyze_slice(src, start, duration, tag, req_id):
    snippet = os.path.join(app.config['UPLOAD_FOLDER'], f"_snippet_{req_id}_{tag}.wav")
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
            
    rms_db, peak_db, crest = levels(samples)
    I, LRA, TP, SP = lufs_snapshot(src, start=start, duration=duration)
    bands, freqs, mag_db = band_distribution(samples, sr)
    w_url, s_url = plot_waveform_spectrum(samples, sr, freqs, mag_db, req_id, tag)
    try: os.remove(snippet)
    except: pass
    return dict(tag=tag, start=start, duration=duration, sr=sr,
                rms_db=rms_db, peak_db=peak_db, crest_db=crest,
                correlation=correlation,
                I=I, LRA=LRA, TP=TP, SP=SP, bands=bands,
                waveform_url=w_url, spectrum_url=s_url)

def get_insights(slices, genre, lang="de"):
    insights = []
    target_slice = next((s for s in slices if s['tag'] == 'Middle'), slices[0])
    
    I = target_slice['I'] if target_slice['I'] is not None else -14
    crest = target_slice['crest_db'] if target_slice['crest_db'] is not None else 10
    bands = target_slice['bands']
    correlation = target_slice.get('correlation', 1.0)
    
    if correlation < 0.3:
        insights.append({
            "title": "Phase Issues (Poor Mono Compatibility)" if lang == "en" else "Phasenprobleme (Schlechte Mono-Kompatibilität)",
            "desc": f"Correlation is very low ({correlation:.2f}). Your mix will lose elements when played on mono speakers (phones, club PAs)." if lang == "en" else f"Die Korrelation ist sehr niedrig ({correlation:.2f}). Auf Mono-Lautsprechern (Handy, Club-PA) kommt es zu Auslöschungen.",
            "vst": "Brainworx bx_control V2, iZotope Imager",
            "action": "Check your stereo widening plugins. Use a correlation meter and mono-sum your master frequently." if lang == "en" else "Prüfe deine Stereo-Widener (Chorus/Imager). Schalte den Master oft auf Mono und reduziere extreme Panning-Effekte."
        })
    elif correlation > 0.95 and genre not in ['Acoustic / Jazz']:
        insights.append({
            "title": "Very narrow stereo field" if lang == "en" else "Sehr enges Stereobild",
            "desc": "The mix is almost entirely mono. It might lack width and depth." if lang == "en" else "Der Mix ist fast komplett Mono. Er könnte räumliche Tiefe und Breite vertragen.",
            "vst": "Polyverse Wider, Soundtoys MicroShift",
            "action": "Pan instruments wider or add subtle stereo delay/chorus to synths and backing vocals." if lang == "en" else "Zieh Begleitinstrumente (Gitarren, Synths) weiter nach außen (Hard-Panning) oder nutze Micro-Shifting."
        })
    
    if genre in ['EDM / Electronic', 'Hip Hop / Rap', 'Cinematic / Trailer']:
        target_I = -8
    elif genre in ['Pop', 'Rock / Metal', 'R&B / Soul']:
        target_I = -10
    elif genre in ['Lo-Fi / Chillhop']:
        target_I = -12
    elif genre in ['Podcast / Spoken Word']:
        target_I = -16
    else:
        target_I = -14

    if I > target_I + 2:
        insights.append({
            "title": "Too heavily compressed" if lang == "en" else "Zu stark komprimiert",
            "desc": f"The track is very loud at {I:.1f} LUFS for {genre}. Dynamics are suffering." if lang == "en" else f"Der Track ist mit {I:.1f} LUFS sehr laut für {genre}. Die Dynamik leidet.",
            "vst": "FabFilter Pro-L 2, iZotope Ozone Maximizer",
            "action": "Reduce limiter gain slightly or use longer attack times on your compressors." if lang == "en" else "Nimm etwas Gain beim Limiter heraus oder verlängere die Attack-Zeiten deiner Kompressoren."
        })
    elif I < target_I - 3:
        insights.append({
            "title": "Too quiet for the genre" if lang == "en" else "Zu leise für das Genre",
            "desc": f"At {I:.1f} LUFS, the track might lack energy compared to modern {genre} tracks." if lang == "en" else f"Mit {I:.1f} LUFS könnte der Track im Vergleich zu aktuellen {genre}-Tracks untergehen.",
            "vst": "Waves SSL G-Master Buss, Shadow Hills Mastering Compressor",
            "action": "Use more glue compression on busses and drive your mastering limiter a few dB hotter." if lang == "en" else "Verwende mehr Glue-Kompression auf Bussen und fahre deinen Mastering-Limiter ein paar dB heißer an."
        })

    if crest < 5.0:
        insights.append({
            "title": "Flattened transients" if lang == "en" else "Transienten abgeflacht",
            "desc": "The crest factor is very low. The mix feels squashed and drums lack punch." if lang == "en" else "Der Crest-Faktor ist sehr niedrig. Der Mix wirkt flach (Squashed), Drums haben keinen Punch.",
            "vst": "SPL Transient Designer, Oeksound Spiff",
            "action": "Reduce compression on the drum bus/snare or use a transient shaper to restore punch." if lang == "en" else "Reduziere Kompression auf Snare/Kick oder nutze einen Transient Shaper, um Punch zurückzuholen."
        })
        
    bass_pct = next((b['percent'] for b in bands if b['name'] == 'Bass'), 0)
    sub_pct = next((b['percent'] for b in bands if b['name'] == 'Sub'), 0)
    presence_pct = next((b['percent'] for b in bands if b['name'] == 'Presence'), 0)
    
    if (sub_pct + bass_pct) > 35:
        insights.append({
            "title": "Critical Low-End (Muddiness)" if lang == "en" else "Kritischer Low-End Bereich (Muddiness)",
            "desc": "The sub/bass region absorbs too much energy and masks the midrange." if lang == "en" else "Der Sub/Bass-Bereich schluckt enorm viel Energie und maskiert die Mitten.",
            "vst": "FabFilter Pro-Q 3, Pultec EQP-1A, Trackspacer",
            "action": "Apply clean low-cuts (high-pass) on guitars, keys, and vocals. Sidechain the bass to the kick." if lang == "en" else "Setze saubere Low-Cuts (High-Pass) auf Gitarren, Keys und Vocals. Sidechaine den Bass an die Kick."
        })
    elif (sub_pct + bass_pct) < 10 and genre in ['EDM / Electronic', 'Hip Hop / Rap']:
        insights.append({
            "title": "Weak Low-End" if lang == "en" else "Schwaches Low-End",
            "desc": "Lacks solid bass foundation for the selected genre." if lang == "en" else "Für das gewählte Genre fehlt es an solidem Bass-Fundament.",
            "vst": "Waves MaxxBass, Soundtoys Decapitator",
            "action": "Saturate your sub-bass to generate upper harmonics or add subharmonics via plugins." if lang == "en" else "Sättige deinen Sub-Bass (um Obertöne hörbar zu machen) oder füge Subharmonische via Plugin hinzu."
        })

    if presence_pct > 40:
        insights.append({
            "title": "Harshness in upper mids" if lang == "en" else "Harshness in den oberen Mitten",
            "desc": "High energy between 1-5 kHz. This can cause ear fatigue quickly." if lang == "en" else "Viel Energie zwischen 1-5 kHz. Das ist oft schnell ermüdend für das Ohr.",
            "vst": "Oeksound Soothe2, FabFilter Pro-DS, Gullfoss",
            "action": "Check vocals, cymbals, and synths for resonances. Use a de-esser or a dynamic EQ." if lang == "en" else "Überprüfe Vocals, Cymbals und Synths auf Resonanzen. Nutze De-Esser oder einen dynamischen EQ."
        })

    if not insights:
        insights.append({
            "title": "Excellent Balance" if lang == "en" else "Exzellente Ausgewogenheit",
            "desc": "Your metrics look perfectly balanced for this genre!" if lang == "en" else "Die Metriken sehen für dieses Genre absolut super und balanciert aus!",
            "vst": "Ozone Vintage Tape, Black Box HG-2",
            "action": "Just add subtle analog warmth or saturation for the final 10% polish on the master bus." if lang == "en" else "Füge nur noch subtile analoge Wärme oder Sättigung für den letzten 10% \"Polish\" auf dem Master-Bus hinzu."
        })

    return insights

def cleanup_old_files():
    now = time.time()
    for directory in [app.config['UPLOAD_FOLDER'], os.path.join(base_dir, 'static', 'images')]:
        for f in glob.glob(os.path.join(directory, '*')):
            if os.path.isfile(f) and now - os.path.getmtime(f) > 3600:
                try: os.remove(f)
                except: pass

@app.route("/")
def index():
    return render_template("index.html")

@app.route("/upload", methods=["POST"])
def upload():
    cleanup_old_files()
    if not have_ffmpeg():
        return jsonify({"error": "ffmpeg not found on server."}), 500
        
    if 'audio' not in request.files:
        return jsonify({"error": "No audio file provided."}), 400
        
    file = request.files['audio']
    genre = request.form.get('genre', 'Pop')
    lang = request.form.get('lang', 'de')
    
    if file.filename == '':
        return jsonify({"error": "Empty file."}), 400
        
    req_id = str(uuid.uuid4())[:8]
    ext = os.path.splitext(file.filename)[1]
    
    save_filename = f"upload_{req_id}{ext}"
    save_path = os.path.join(app.config['UPLOAD_FOLDER'], save_filename)
    file.save(save_path)
    
    try:
        res = subprocess.run([FFPROBE_CMD,"-v","error","-show_entries","format=duration","-of","default=nw=1:nk=1", save_path],
                              stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, check=True)
        total = float(res.stdout.strip())
    except Exception:
        return jsonify({"error": "Could not determine duration of the audio."}), 400
        
    intro_start = 0.0
    middle_start = max(0.0, (total/2.0)-(SLICE_SEC/2.0))
    outro_start = max(0.0, total - SLICE_SEC)
    
    results = []
    try:
        results.append(analyze_slice(save_path, intro_start, min(SLICE_SEC, total), "Intro", req_id))
        if total > SLICE_SEC:
            results.append(analyze_slice(save_path, middle_start, min(SLICE_SEC, total-middle_start), "Middle", req_id))
        if total > SLICE_SEC*1.5:
            results.append(analyze_slice(save_path, outro_start, min(SLICE_SEC, SLICE_SEC), "Outro", req_id))
    except Exception as e:
        return jsonify({"error": str(e)}), 500
        
    insights = get_insights(results, genre, lang)
    
    analysis_data = {
        "id": req_id,
        "filename": file.filename,
        "genre": genre,
        "slices": results,
        "insights": insights,
        "audio_url": f"/static/audio/{save_filename}"
    }
    
    with sqlite3.connect(DB_PATH) as conn:
        conn.execute(
            'INSERT INTO analyses (id, filename, genre, data) VALUES (?, ?, ?, ?)',
            (req_id, file.filename, genre, json.dumps(analysis_data))
        )
        
    return jsonify(analysis_data)

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
        return jsonify({"error": "Not found"}), 404

@app.route('/toggle_reference/<req_id>', methods=['POST'])
def toggle_reference(req_id):
    with sqlite3.connect(DB_PATH) as conn:
        cursor = conn.execute('SELECT is_reference FROM analyses WHERE id = ?', (req_id,))
        row = cursor.fetchone()
        if row:
            new_val = 0 if row[0] == 1 else 1
            conn.execute('UPDATE analyses SET is_reference = ? WHERE id = ?', (new_val, req_id))
            return jsonify({"success": True, "is_reference": new_val})
        return jsonify({"error": "Not found"}), 404

if __name__ == "__main__":
    import threading
    import webview
    
    def start_server():
        app.run(host="127.0.0.1", port=5050, debug=False)
        
    t = threading.Thread(target=start_server)
    t.daemon = True
    t.start()
    
    # Create the native app window without browser-like features
    webview.create_window('Mix Analyzer', 'http://127.0.0.1:5050/', width=1280, height=800, text_select=False)
    webview.start(gui='cocoa', debug=False)
