import numpy as np
from core.config import get_genre_profile, serialize_genre_profile, BAND_ALIASES

def in_range(value, value_range):
    return value is not None and value_range[0] <= value <= value_range[1]

def mean_available(values):
    nums = [float(v) for v in values if v is not None and np.isfinite(float(v))]
    return float(np.mean(nums)) if nums else None

def band_percent(slice_data, names):
    expanded_names = set(names)
    for name in names:
        expanded_names.update(BAND_ALIASES.get(name, {name}))
    return sum(
        band.get("percent", 0)
        for band in slice_data.get("bands", [])
        if band.get("name") in expanded_names
    )

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

    # Streaming Penalty Check
    track_tp = (track_loudness or {}).get("TP")
    TP = track_tp if track_tp is not None else target_slice.get('TP') if target_slice.get('TP') is not None else -1.0
    
    if I > -14:
        penalty = round(I - (-14), 1)
        insights.append({
            "title": "Streaming Penalty Risk" if lang == "en" else "Streaming Penalty Risiko",
            "desc": f"Your track ({I:.1f} LUFS) will be turned down by ~{penalty} dB on Spotify/Apple Music to hit -14 LUFS." if lang == "en" else f"Dein Track ({I:.1f} LUFS) wird auf Spotify/Apple Music um ca. {penalty} dB leiser gemacht, um -14 LUFS zu erreichen.",
            "vst": "Check your Limiter / Loudness Meter",
            "action": f"Ensure your True Peak is below -1.0 dBTP (currently {TP:.1f} dBTP) to avoid inter-sample clipping after encoding." if lang == "en" else f"Stelle sicher, dass dein True Peak unter -1.0 dBTP liegt (aktuell {TP:.1f} dBTP), um Intersample-Clipping nach dem Encoding zu vermeiden."
        })
        if TP > -1.0:
            priority_actions.append("Lower True Peak to -1.0 dBTP for streaming." if lang == "en" else "True Peak für Streaming auf -1.0 dBTP senken.")

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
