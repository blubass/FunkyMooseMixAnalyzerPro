const els = {
    dropZone: document.getElementById('dropZone'),
    fileInput: document.getElementById('fileInput'),
    uploadSection: document.getElementById('uploadSection'),
    loadingState: document.getElementById('loadingState'),
    resultsSection: document.getElementById('resultsSection'),
    newAnalysisBtn: document.getElementById('newAnalysisBtn'),
    filenameDisplay: document.getElementById('filenameDisplay'),
    resultsPrefix: document.getElementById('resultsPrefix'),
    tabsContainer: document.querySelector('.tabs'),
    tabContent: document.getElementById('tabContent'),
    statusMessage: document.getElementById('statusMessage'),
    sidebar: document.getElementById('historySidebar'),
    compareSelect: document.getElementById('compareSelect'),
    summaryStrip: document.getElementById('summaryStrip'),
    actionButtons: document.getElementById('actionButtons'),
    printHeader: document.getElementById('printHeader'),
    audioPlayer: document.getElementById('audioPlayer'),
    genreSelect: document.getElementById('genreSelect'),
    loadingTitle: document.getElementById('loadingTitle'),
    progressBar: document.getElementById('analysisProgressBar'),
    analysisSteps: document.getElementById('analysisSteps'),
    isInstrumental: document.getElementById('isInstrumental'),
    vocalToggleText: document.getElementById('vocalToggleText')
};

let analysisData = null;
let referenceData = null;
let currentLang = 'de';
let genreProfiles = new Map();
let defaultGenre = 'Pop';

const MAX_UPLOAD_BYTES = 100 * 1024 * 1024;

const i18n = {
    de: {
        subtitle: 'Audio Intelligence Dashboard - by Uwe Arthur Felchle',
        'genre-label': 'Genre Profil:',
        'drag-drop': 'Audiodatei hier ablegen',
        'or-click': 'oder klicken zum Auswählen',
        formats: 'MP3, WAV, FLAC, AIFF, M4A, OGG und AAC bis 100MB',
        analyzing: 'Mix wird analysiert...',
        'analyzing-sub': 'Audio wird dekodiert, gemessen und als Report aufbereitet',
        'results-for': 'Analyse-Ergebnisse für ',
        'new-analysis': 'Neue Analyse',
        export: 'PDF Export',
        'insights-title': 'Mix Insights & VST Empfehlungen',
        solution: 'Lösungsvorschlag:',
        vsts: 'Empfohlene VSTs:',
        dyn: 'Dynamik',
        rms: 'RMS Pegel',
        peak: 'Spitzenpegel',
        'left-peak': 'L Peak',
        'right-peak': 'R Peak',
        'mono-peak': 'Mono-Summe Peak',
        'stereo-balance': 'Balance L/R',
        'dc-offset': 'DC Offset',
        crest: 'Crest-Faktor',
        stereo: 'Stereo Bild',
        correlation: 'Phasenkorrelation',
        'mono-comp': 'Mono Kompatibilität',
        good: 'Gut',
        poor: 'Kritisch',
        'lufs-title': 'Lautheit (LUFS)',
        lra: 'Dynamikumfang (LRA)',
        'target-match': 'Ziel-Match',
        excellent: 'Exzellent',
        'needs-adj': 'Anpassung nötig',
        bands: 'Frequenzverteilung',
        visuals: 'Visualisierungen',
        library: 'Bibliothek',
        history: 'Historie & Referenzen',
        'compare-placeholder': 'Vergleichen mit...',
        'upload-ready': 'Bereit für die nächste Audiodatei.',
        'unsupported-file': 'Dieses Format wird nicht unterstützt. Bitte nutze MP3, WAV, FLAC, AIFF, M4A, OGG oder AAC.',
        'file-too-large': 'Die Datei ist größer als 100MB.',
        'upload-failed': 'Analyse fehlgeschlagen.',
        'history-empty': 'Noch keine Analysen gespeichert.',
        'history-error': 'Historie konnte nicht geladen werden.',
        reference: 'Referenz',
        'your-mix': 'Dein Mix',
        'summary-ready': 'Release-nah',
        'summary-polish': 'Feinschliff',
        'summary-attention': 'Bitte prüfen',
        'summary-limited': 'Messung begrenzt',
        'low-end': 'Low-End',
        reliability: 'Zuverlässigkeit',
        'quality-title': 'Qualitätschecks',
        'confidence-high': 'hoch',
        'confidence-medium': 'mittel',
        'confidence-low': 'niedrig',
        clipping: 'Clipping',
        silence: 'Stille',
        centroid: 'Spektraler Schwerpunkt',
        rolloff: 'Rolloff 85%',
        'loudness-method': 'LUFS-Messung',
        slices: 'Schnitte',
        target: 'Ziel',
        width: 'Stereo Breite',
        'ms-ratio': 'M/S Verhältnis',
        'copy-notes': 'DAW Notizen',
        copied: 'Kopiert!',
        'copy-wait': 'Bitte warte, bis der erste Schnitt analysiert wurde.',
        'ab-compare': 'A/B Vergleich',
        'priority-actions': 'Top Prioritäten',
        'resonance-detected': 'Resonanz bei',
        'full-track': 'Gesamttrack',
        'slice-average': 'Schnitt-Mittel',
        'mastering-title': 'Funky Moose Advice Engine',
        'mastering-sub': 'Smart Mix-Insights & Korrektur-Vorschläge',
        'vocal': 'Vocal',
        'instrumental': 'Instrumental',
        'toggle-hint': 'Enthält Vocals?',
        'headphone-title': 'Headphone & Speaker Score',
        'headphone-score': 'Kopfhörer',
        'speaker-score': 'Lautsprecher',
        'mono-compat': 'Mono-Tauglichkeit',
        'phase-quality': 'Phasenqualität',
        'track-type': 'Track-Typ',
        'overall-score': 'Mix Score',
        'transients-title': 'Transient Analyse',
        'transient-density': 'Einsätze / Sek.',
        'attack-time': 'Attack-Zeit',
        'perc-energy': 'Percussion-Energie',
        'spectrum-title': 'Spektrum Analyse (interaktiv)',
        'spectrum-mid': 'Mix (Mid)',
        'spectrum-side': 'Side-Kanal',
        'spectrum-target': 'Genre-Zielkurve',
        'spectrum-hint': 'Hover für Frequenz-Details',
        'spectrum-diff': 'Differenz (Mix - Ref)',
        'step-full': 'Gesamttrack Scan',
        'dep-title': 'FFmpeg fehlt',
        'dep-desc': 'FFmpeg wird für die Audio-Analyse benötigt, wurde aber nicht auf deinem System gefunden.',
        'dep-check': 'Erneut prüfen'
    },
    en: {
        subtitle: 'Audio Intelligence Dashboard - by Uwe Arthur Felchle',
        'genre-label': 'Genre Profile:',
        'drag-drop': 'Drag & drop your audio file',
        'or-click': 'or click to browse your files',
        formats: 'MP3, WAV, FLAC, AIFF, M4A, OGG and AAC up to 100MB',
        analyzing: 'Analyzing your mix...',
        'analyzing-sub': 'Decoding, measuring, and preparing a client-ready report',
        'results-for': 'Analysis results for ',
        'new-analysis': 'New Analysis',
        export: 'Export PDF',
        'insights-title': 'Mix Insights & VST Recommendations',
        solution: 'Suggested action:',
        vsts: 'Recommended VSTs:',
        dyn: 'Dynamics',
        rms: 'RMS Level',
        peak: 'Peak Level',
        'left-peak': 'L Peak',
        'right-peak': 'R Peak',
        'mono-peak': 'Mono Sum Peak',
        'stereo-balance': 'L/R Balance',
        'dc-offset': 'DC Offset',
        crest: 'Crest Factor',
        stereo: 'Stereo Field',
        correlation: 'Phase Correlation',
        'mono-comp': 'Mono Compatibility',
        good: 'Good',
        poor: 'Critical',
        'lufs-title': 'Loudness (LUFS)',
        lra: 'Loudness Range (LRA)',
        'target-match': 'Target Match',
        excellent: 'Excellent',
        'needs-adj': 'Needs Adjustment',
        bands: 'Band Distribution',
        visuals: 'Audio Visualizations',
        library: 'Library',
        history: 'History & References',
        'compare-placeholder': 'Compare with...',
        'upload-ready': 'Ready for the next audio file.',
        'unsupported-file': 'Unsupported format. Please use MP3, WAV, FLAC, AIFF, M4A, OGG, or AAC.',
        'file-too-large': 'The file is larger than 100MB.',
        'upload-failed': 'Analysis failed.',
        'history-empty': 'No analyses saved yet.',
        'history-error': 'History could not be loaded.',
        reference: 'Reference',
        'your-mix': 'Your Mix',
        'summary-ready': 'Release-ready',
        'summary-polish': 'Polish',
        'summary-attention': 'Review',
        'summary-limited': 'Limited',
        'low-end': 'Low-End',
        reliability: 'Reliability',
        'quality-title': 'Quality Checks',
        'confidence-high': 'high',
        'confidence-medium': 'medium',
        'confidence-low': 'low',
        clipping: 'Clipping',
        silence: 'Silence',
        centroid: 'Spectral Centroid',
        rolloff: 'Rolloff 85%',
        'loudness-method': 'LUFS method',
        slices: 'Slices',
        target: 'Target',
        width: 'Stereo Width',
        'ms-ratio': 'M/S Ratio',
        'copy-notes': 'DAW Notes',
        copied: 'Copied!',
        'copy-wait': 'Please wait for the first slice to be analyzed.',
        'ab-compare': 'A/B Compare',
        'priority-actions': 'Top Priorities',
        'resonance-detected': 'Resonance at',
        'full-track': 'Full track',
        'slice-average': 'Slice average',
        'mastering-title': 'Funky Moose Advice Engine',
        'mastering-sub': 'Smart Mix Insights & Tactical Advice',
        'vocal': 'Vocal',
        'instrumental': 'Instrumental',
        'toggle-hint': 'Contains vocals?',
        'headphone-title': 'Headphone & Speaker Score',
        'headphone-score': 'Headphones',
        'speaker-score': 'Speakers',
        'mono-compat': 'Mono Compatibility',
        'phase-quality': 'Phase Quality',
        'track-type': 'Track Type',
        'overall-score': 'Mix Score',
        'transients-title': 'Transient Analysis',
        'transient-density': 'Onsets / sec',
        'attack-time': 'Attack Time',
        'perc-energy': 'Percussion Energy',
        'spectrum-title': 'Spectrum Analysis (interactive)',
        'spectrum-mid': 'Mix (Mid)',
        'spectrum-side': 'Side Channel',
        'spectrum-target': 'Genre Target Curve',
        'spectrum-hint': 'Hover for frequency details',
        'spectrum-diff': 'Difference (Mix - Ref)',
        'step-full': 'Full-track Scan',
        'dep-title': 'FFmpeg Missing',
        'dep-desc': 'FFmpeg is required for audio analysis but was not found on your system.',
        'dep-check': 'Check Again'
    }
};

const bandI18n = {
    de: {
        Sub: 'Sub',
        Bass: 'Bass',
        'Low-Mids': 'Tiefmitten',
        Mids: 'Mitten',
        Presence: 'Präsenz',
        Air: 'Air'
    },
    en: {
        Sub: 'Sub',
        Bass: 'Bass',
        'Low-Mids': 'Low-Mids',
        Mids: 'Mids',
        Presence: 'Presence',
        Air: 'Air'
    }
};

function t(key) {
    return (i18n[currentLang] && i18n[currentLang][key]) || key;
}

function bandLabel(name) {
    return (bandI18n[currentLang] && bandI18n[currentLang][name]) || name;
}

function escapeHtml(value) {
    return String(value ?? '').replace(/[&<>"']/g, char => ({
        '&': '&amp;',
        '<': '&lt;',
        '>': '&gt;',
        '"': '&quot;',
        "'": '&#039;'
    }[char]));
}

function asNumber(value) {
    const num = Number(value);
    return Number.isFinite(num) ? num : null;
}

function fmt(value, suffix = '', decimals = 2) {
    const num = asNumber(value);
    return num === null ? 'N/A' : `${num.toFixed(decimals)}${suffix}`;
}

function currentProfile() {
    if (analysisData && analysisData.profile) return analysisData.profile;
    if (analysisData && genreProfiles.has(analysisData.genre)) return genreProfiles.get(analysisData.genre);
    const selected = els.genreSelect ? els.genreSelect.value : defaultGenre;
    return genreProfiles.get(selected) || genreProfiles.get('Streaming / General') || null;
}

function currentLufsRange() {
    const profile = currentProfile();
    return profile && profile.lufs_range ? profile.lufs_range : [-16, -12];
}

function clamp(value, min, max) {
    return Math.min(max, Math.max(min, value));
}

function formatTime(seconds) {
    const safeSeconds = Math.max(0, Math.round(Number(seconds) || 0));
    const mins = Math.floor(safeSeconds / 60);
    const secs = String(safeSeconds % 60).padStart(2, '0');
    return `${mins}:${secs}`;
}

function metricTone(value, kind) {
    const num = asNumber(value);
    if (num === null) return 'var(--text-muted)';
    if (kind === 'lufs') {
        const [low, high] = currentLufsRange();
        if (num > high) return 'var(--danger)';
        if (num < low) return 'var(--warning)';
        return 'var(--success)';
    }
    if (kind === 'correlation') {
        if (num < 0.3) return 'var(--danger)';
        if (num < 0.55) return 'var(--warning)';
        return 'var(--success)';
    }
    return '#fff';
}

function setStatus(message, type = 'info') {
    if (!message) {
        els.statusMessage.className = 'status-message hidden';
        els.statusMessage.textContent = '';
        return;
    }
    els.statusMessage.className = `status-message ${type}`;
    els.statusMessage.textContent = message;
}

function setButtonLabel(button, iconClass, label) {
    button.innerHTML = `<i class="${iconClass}"></i><span>${escapeHtml(label)}</span>`;
}

function getActiveSlice() {
    const active = document.querySelector('.tab-btn.active');
    const tag = active ? active.dataset.tag : null;
    return analysisData && analysisData.slices.find(slice => slice.tag === tag);
}

function getCurrentInsights() {
    if (!analysisData) return [];
    let payload = null;
    if (analysisData.insights_by_lang && analysisData.insights_by_lang[currentLang]) {
        payload = analysisData.insights_by_lang[currentLang];
    } else {
        payload = analysisData.insights;
    }
    
    if (payload && payload.insights) return payload.insights;
    return Array.isArray(payload) ? payload : [];
}

function updateVocalToggleLabel() {
    if (!els.vocalToggleText || !els.isInstrumental) return;
    els.vocalToggleText.textContent = els.isInstrumental.checked ? t('instrumental') : t('vocal');
}

function updateTexts() {
    document.documentElement.lang = currentLang;
    document.querySelector('header p').textContent = t('subtitle');
    document.querySelector('label[for="genreSelect"]').textContent = t('genre-label');
    document.querySelector('#dropZone h2').textContent = t('drag-drop');
    document.querySelector('#dropZone p').textContent = t('or-click');
    document.querySelector('.formats').textContent = t('formats');
    document.querySelector('#loadingState h3').textContent = t('analyzing');
    document.querySelector('#loadingState p').textContent = t('analyzing-sub');
    setButtonLabel(els.newAnalysisBtn, 'fa-solid fa-rotate-right', t('new-analysis'));
    document.getElementById('exportText').textContent = t('export');
    document.getElementById('copyNotesText').textContent = t('copy-notes');
    document.getElementById('libText').textContent = t('library');
    document.getElementById('historyText').textContent = t('history');
    els.resultsPrefix.textContent = t('results-for');
    
    // New Branding IDs
    const masteringTitle = document.getElementById('masteringTitleEl');
    if (masteringTitle) masteringTitle.textContent = t('mastering-title');
    const masteringSub = document.getElementById('masteringSubEl');
    if (masteringSub) masteringSub.textContent = t('mastering-sub');

    const compOpt = els.compareSelect.querySelector('option[value=""]');
    if (compOpt) compOpt.textContent = t('compare-placeholder');
    const abLabel = document.querySelector('.ab-label');
    if (abLabel) abLabel.textContent = t('ab-compare');
    const assistantTitle = document.querySelector('#masteringAssistant .assistant-header h3');
    if (assistantTitle) assistantTitle.innerHTML = `<i class="fa-solid fa-magic"></i> ${t('mastering-title')}`;
    const assistantSub = document.querySelector('#masteringAssistant .assistant-header p');
    if (assistantSub) assistantSub.textContent = t('mastering-sub');
    updateVocalToggleLabel();

    if (analysisData) {
        els.filenameDisplay.textContent = analysisData.filename;
        renderSummary(analysisData);
        const activeSlice = getActiveSlice();
        if (activeSlice) renderSlice(activeSlice);
    }
}

document.querySelectorAll('.lang-switcher .lang-btn').forEach(btn => {
    btn.addEventListener('click', (event) => {
        const nextLang = event.currentTarget.dataset.lang;
        if (!nextLang || nextLang === currentLang) return;
        document.querySelectorAll('.lang-switcher .lang-btn').forEach(item => item.classList.remove('active'));
        event.currentTarget.classList.add('active');
        currentLang = nextLang;
        updateTexts();
    });
});

if (els.isInstrumental) {
    els.isInstrumental.addEventListener('change', updateVocalToggleLabel);
}

updateTexts();

['dragenter', 'dragover', 'dragleave', 'drop'].forEach(eventName => {
    els.dropZone.addEventListener(eventName, preventDefaults, false);
});

function preventDefaults(event) {
    event.preventDefault();
    event.stopPropagation();
}

['dragenter', 'dragover'].forEach(eventName => {
    els.dropZone.addEventListener(eventName, () => els.dropZone.classList.add('dragover'), false);
});

['dragleave', 'drop'].forEach(eventName => {
    els.dropZone.addEventListener(eventName, () => els.dropZone.classList.remove('dragover'), false);
});

els.dropZone.addEventListener('drop', handleDrop, false);
els.dropZone.addEventListener('click', () => els.fileInput.click());
els.dropZone.addEventListener('keydown', (event) => {
    if (event.key === 'Enter' || event.key === ' ') {
        event.preventDefault();
        els.fileInput.click();
    }
});
els.fileInput.addEventListener('change', event => handleFiles(event.target.files));

els.newAnalysisBtn.addEventListener('click', () => {
    els.resultsSection.classList.add('hidden');
    els.uploadSection.classList.remove('hidden');
    els.dropZone.classList.remove('hidden');
    els.loadingState.classList.add('hidden');
    els.summaryStrip.classList.add('hidden');
    els.fileInput.value = '';
    referenceData = null;
    els.compareSelect.value = '';
    setStatus(t('upload-ready'), 'success');
});

document.getElementById('libraryBtn').addEventListener('click', () => {
    els.sidebar.classList.remove('hidden');
    fetchHistory();
});

document.getElementById('closeSidebarBtn').addEventListener('click', () => {
    els.sidebar.classList.add('hidden');
});

async function fetchJson(url, options = {}) {
    const response = await fetch(url, options);
    let payload = null;
    try {
        payload = await response.json();
    } catch (_error) {
        payload = {};
    }
    if (!response.ok) {
        throw new Error(payload.error || `${response.status} ${response.statusText}`);
    }
    return payload;
}

async function loadGenres() {
    try {
        const payload = await fetchJson('/genres');
        defaultGenre = payload.default || defaultGenre;
        genreProfiles = new Map(payload.genres.map(profile => [profile.name, profile]));
        const previousValue = els.genreSelect.value || defaultGenre;
        els.genreSelect.replaceChildren();

        const groups = new Map();
        payload.genres.forEach(profile => {
            const groupName = profile.group || 'Other';
            if (!groups.has(groupName)) {
                const optgroup = document.createElement('optgroup');
                optgroup.label = groupName;
                groups.set(groupName, optgroup);
                els.genreSelect.appendChild(optgroup);
            }
            groups.get(groupName).appendChild(new Option(profile.name, profile.name));
        });

        els.genreSelect.value = genreProfiles.has(previousValue) ? previousValue : defaultGenre;
    } catch (error) {
        setStatus(error.message, 'error');
    }
}

loadGenres();

async function fetchHistory() {
    const list = document.getElementById('historyList');
    list.replaceChildren();
    els.compareSelect.replaceChildren(new Option(t('compare-placeholder'), ''));

    try {
        const data = await fetchJson('/history');
        if (!data.length) {
            const empty = document.createElement('div');
            empty.className = 'empty-state';
            empty.textContent = t('history-empty');
            list.appendChild(empty);
            els.compareSelect.style.display = 'none';
            return;
        }

        let hasRefs = false;
        data.forEach(item => {
            if (item.is_reference) {
                hasRefs = true;
                els.compareSelect.appendChild(new Option(`${item.filename} (${item.genre})`, item.id));
            }

            const div = document.createElement('div');
            div.className = `history-item ${item.is_reference ? 'is-ref' : ''}`;

            const star = document.createElement('button');
            star.className = `star-btn ${item.is_reference ? 'active' : ''}`;
            star.dataset.id = item.id;
            star.title = 'Toggle Reference';
            star.innerHTML = '<i class="fa-solid fa-star"></i>';

            const title = document.createElement('h4');
            title.textContent = item.filename;

            const meta = document.createElement('p');
            const date = new Date(`${item.timestamp}Z`);
            meta.textContent = `${item.genre} - ${Number.isNaN(date.getTime()) ? item.timestamp : date.toLocaleString()}`;

            div.append(star, title, meta);
            div.addEventListener('click', event => {
                if (event.target.closest('.star-btn')) {
                    toggleReference(item.id, div);
                    return;
                }
                loadAnalysis(item.id);
            });

            list.appendChild(div);
        });

        els.compareSelect.style.display = hasRefs ? 'block' : 'none';
    } catch (error) {
        const empty = document.createElement('div');
        empty.className = 'empty-state error';
        empty.textContent = `${t('history-error')} ${error.message}`;
        list.appendChild(empty);
        els.compareSelect.style.display = 'none';
    }
}

async function toggleReference(id, element) {
    try {
        const data = await fetchJson(`/toggle_reference/${id}`, { method: 'POST' });
        if (data.is_reference) {
            element.classList.add('is-ref');
            element.querySelector('.star-btn').classList.add('active');
        } else {
            element.classList.remove('is-ref');
            element.querySelector('.star-btn').classList.remove('active');
        }
        fetchHistory();
    } catch (error) {
        setStatus(error.message, 'error');
    }
}

async function loadAnalysis(id) {
    els.loadingState.classList.remove('hidden');
    els.dropZone.classList.add('hidden');
    els.uploadSection.classList.remove('hidden');
    els.resultsSection.classList.add('hidden');
    els.sidebar.classList.add('hidden');
    setStatus('', 'info');

    try {
        const data = await fetchJson(`/analysis/${id}`);
        analysisData = data;
        referenceData = null;
        els.compareSelect.value = '';
        displayResults(data);
    } catch (error) {
        setStatus(error.message, 'error');
        els.loadingState.classList.add('hidden');
        els.dropZone.classList.remove('hidden');
    }
}

els.compareSelect.addEventListener('change', async event => {
    if (!event.target.value) {
        referenceData = null;
        const activeSlice = getActiveSlice();
        if (activeSlice) renderSlice(activeSlice);
        return;
    }

    try {
        referenceData = await fetchJson(`/analysis/${event.target.value}`);
        setupABComparison();
        const activeSlice = getActiveSlice();
        if (activeSlice) renderSlice(activeSlice);
    } catch (error) {
        setStatus(error.message, 'error');
        referenceData = null;
        setupABComparison();
    }
});

document.getElementById('exportPdfBtn').addEventListener('click', () => {
    if (!analysisData) return;
    els.printHeader.classList.remove('hidden');
    els.actionButtons.style.display = 'none';

    const cleanFilename = (analysisData.filename || 'Report').replace(/\.[^/.]+$/, "");
    const pdfFilename = `Mix_Report_${cleanFilename}.pdf`;

    const restoreExportUi = () => {
        els.printHeader.classList.add('hidden');
        els.actionButtons.style.display = 'flex';
    };

    setStatus('PDF wird generiert...', 'success');

    html2pdf().set({
        margin: [10, 10, 10, 10],
        filename: pdfFilename,
        image: { type: 'jpeg', quality: 0.90 },
        html2canvas: { scale: 1.0, useCORS: true, logging: false, backgroundColor: '#101215' },
        jsPDF: { unit: 'mm', format: 'a4', orientation: 'portrait', compress: true }
    }).from(els.resultsSection).outputPdf('datauristring').then(dataUri => {
        setStatus('PDF wird gespeichert...', 'success');
        restoreExportUi();
        // Robust base64 extraction
        const base64 = dataUri.includes('base64,') ? dataUri.split('base64,')[1] : dataUri;

        // --- Native pywebview path (macOS app) ---
        if (window.pywebview && window.pywebview.api && window.pywebview.api.save_pdf) {
            window.pywebview.api.save_pdf(base64, pdfFilename).then(result => {
                if (result && result.success) {
                    setStatus(`✓ PDF gespeichert: ${result.path}`, 'success');
                } else if (result && result.cancelled) {
                    // user cancelled – do nothing
                } else {
                    setStatus(`PDF-Fehler: ${(result && result.error) || 'Unbekannter Fehler'}`, 'error');
                }
            }).catch(err => setStatus(`PDF-Fehler: ${err}`, 'error'));
            return;
        }

        // --- Browser fallback: send base64 to Flask and get a real download ---
        fetch('/export_pdf', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ data: base64, filename: pdfFilename })
        }).then(response => {
            if (!response.ok) throw new Error(`Server error ${response.status}`);
            return response.blob();
        }).then(blob => {
            const url = URL.createObjectURL(blob);
            const a = document.createElement('a');
            a.href = url;
            a.download = pdfFilename;
            document.body.appendChild(a);
            a.click();
            setTimeout(() => { URL.revokeObjectURL(url); a.remove(); }, 2000);
        }).catch(err => setStatus(`PDF Export fehlgeschlagen: ${err.message}`, 'error'));
    }).catch(error => {
        setStatus(error.message, 'error');
        restoreExportUi();
    });
});

function handleDrop(event) {
    handleFiles(event.dataTransfer.files);
}

function handleFiles(files) {
    if (!files || files.length === 0) return;
    const file = files[0];
    const validTypes = ['audio/mpeg', 'audio/wav', 'audio/flac', 'audio/x-aiff', 'audio/aiff', 'audio/ogg', 'audio/aac', 'audio/mp4'];
    const validExtension = /\.(mp3|wav|flac|aiff|aif|m4a|ogg|aac)$/i.test(file.name);

    if (file.size > MAX_UPLOAD_BYTES) {
        setStatus(t('file-too-large'), 'error');
        return;
    }

    if (file.type && !validTypes.includes(file.type) && !validExtension) {
        setStatus(t('unsupported-file'), 'error');
        return;
    }

    uploadFile(file);
}

async function uploadFile(file) {
    els.dropZone.classList.add('hidden');
    els.loadingState.classList.remove('hidden');
    setStatus('', 'info');
    referenceData = null;
    els.compareSelect.value = '';

    const isInstrumental = els.isInstrumental ? els.isInstrumental.checked : false;

    const formData = new FormData();
    formData.append('audio', file);
    formData.append('genre', els.genreSelect.value || defaultGenre);
    formData.append('lang', currentLang);
    formData.append('is_instrumental', isInstrumental ? '1' : '0');

    try {
        const startData = await fetchJson('/upload', {
            method: 'POST',
            body: formData
        });

        const reqId = startData.req_id;
        const totalSlices = startData.slices_meta.length;
        let completedSlices = 0;

        // Populate steps: Full track first, then slices
        els.analysisSteps.replaceChildren();
        
        const fullStepSpan = document.createElement('span');
        fullStepSpan.textContent = t('step-full');
        fullStepSpan.id = 'step-full-scan';
        els.analysisSteps.appendChild(fullStepSpan);

        startData.slices_meta.forEach(meta => {
            const span = document.createElement('span');
            span.textContent = meta.tag;
            span.id = `step-${meta.tag}`;
            els.analysisSteps.appendChild(span);
        });

        const totalSteps = totalSlices + 1; // +1 for full scan
        let completedSteps = 0;

        // Prepare the result shell while keeping the analysis progress visible.
        renderResults({
            filename: startData.filename,
            audio_url: startData.audio_url,
            slices: [],
            genre: els.genreSelect.value,
            summary: null
        }, { showResults: false, keepLoading: true, refreshHistory: false });

        // 1. Full Track Analysis Step
        updateLoadingProgress(t('step-full'), completedSteps, totalSteps, 'full-scan');
        const fullTrackData = await fetchJson(`/analyze_full_track/${reqId}?lang=${currentLang}`, {
            method: 'POST'
        });
        
        const fullStepEl = document.getElementById('step-full-scan');
        if (fullStepEl) {
            fullStepEl.classList.remove('active');
            fullStepEl.classList.add('done');
        }
        
        analysisData = fullTrackData.current_analysis;
        updateIncrementalResults(analysisData);
        completedSteps++;
        els.progressBar.style.width = `${(completedSteps / totalSteps) * 100}%`;

        // 2. Slices Analysis Step
        for (const meta of startData.slices_meta) {
            updateLoadingProgress(meta.tag, completedSteps, totalSteps, meta.tag);
            const sliceData = await fetchJson(`/analyze_slice/${reqId}/${meta.tag}?lang=${currentLang}`, {
                method: 'POST'
            });
            
            const stepEl = document.getElementById(`step-${meta.tag}`);
            if (stepEl) {
                stepEl.classList.remove('active');
                stepEl.classList.add('done');
            }

            analysisData = sliceData.current_analysis;
            updateIncrementalResults(analysisData);
            completedSteps++;
            
            // Update progress bar
            const pct = (completedSteps / totalSteps) * 100;
            els.progressBar.style.width = `${pct}%`;
        }

        els.loadingState.classList.add('hidden');
        els.uploadSection.classList.add('hidden');
        els.resultsSection.classList.remove('hidden');
        els.progressBar.style.width = '0%';
    } catch (error) {
        setStatus(`${t('upload-failed')} ${error.message}`, 'error');
        els.loadingState.classList.add('hidden');
        els.dropZone.classList.remove('hidden');
    }
}

function updateLoadingProgress(label, current, total, stepIdSuffix) {
    const stepId = stepIdSuffix ? `step-${stepIdSuffix}` : null;
    if (stepId) {
        const stepEl = document.getElementById(stepId);
        if (stepEl) stepEl.classList.add('active');
    }
    
    els.loadingTitle.textContent = `${t('analyzing')} (${label})`;
}

function updateIncrementalResults(data) {
    renderResults(data, { showResults: true, keepLoading: true, refreshHistory: false });
    els.resultsSection.classList.remove('hidden');
    els.uploadSection.classList.add('hidden');
}

function displayResults(data) {
    renderResults(data);
}

function renderResults(data, options = {}) {
    const {
        showResults = true,
        keepLoading = false,
        refreshHistory = true
    } = options;

    els.uploadSection.classList.add('hidden');
    if (!keepLoading) {
        els.loadingState.classList.add('hidden');
    }
    els.resultsSection.classList.toggle('hidden', !showResults);
    els.filenameDisplay.textContent = data.filename;
    els.resultsPrefix.textContent = t('results-for');
    els.audioPlayer.src = data.audio_url;
    els.audioPlayer.load();
    els.tabsContainer.replaceChildren();

    data.slices.forEach((slice, index) => {
        const btn = document.createElement('button');
        btn.className = `tab-btn ${index === 0 ? 'active' : ''}`;
        btn.dataset.tag = slice.tag;
        btn.innerHTML = `<span>${escapeHtml(slice.tag)}</span><small>${formatTime(slice.start)} - ${formatTime(slice.start + slice.duration)}</small>`;
        btn.addEventListener('click', () => {
            document.querySelectorAll('.tab-btn').forEach(item => item.classList.remove('active'));
            btn.classList.add('active');
            renderSlice(slice);
        });
        els.tabsContainer.appendChild(btn);
    });

    renderSummary(data);
    renderMasteringAssistant(data);
    if (data.slices.length > 0) renderSlice(data.slices[0]);
    if (refreshHistory) fetchHistory();
}

function renderMasteringAssistant(data) {
    const container = document.getElementById('masteringAssistant');
    const priorityList = document.getElementById('priorityActions');
    
    if (!data.priority_actions || data.priority_actions.length === 0) {
        container.classList.add('hidden');
        return;
    }
    
    container.classList.remove('hidden');
    priorityList.innerHTML = data.priority_actions.map((action, idx) => `
        <div class="priority-card">
            <i class="fa-solid fa-circle-exclamation"></i>
            <span>${escapeHtml(action)}</span>
        </div>
    `).join('');
}

function setupABComparison() {
    const abContainer = document.getElementById('abToggleContainer');
    const btnA = document.getElementById('btnA');
    const btnB = document.getElementById('btnB');
    
    if (!referenceData) {
        abContainer.classList.add('hidden');
        window.currentAB = 'A';
        return;
    }
    
    abContainer.classList.remove('hidden');
    
    const originalSrc = analysisData.audio_url;
    const refSrc = referenceData.audio_url;
    
    const updateChartsHighlight = (active) => {
        window.currentAB = active;
        const activeSlice = getActiveSlice();
        if (activeSlice) renderSpectrumChart(activeSlice, referenceData);
    };

    btnA.onclick = () => {
        if (window.currentAB === 'A') return;
        btnA.classList.add('active');
        btnB.classList.remove('active');
        const currentTime = els.audioPlayer.currentTime;
        const wasPlaying = !els.audioPlayer.paused;
        els.audioPlayer.src = originalSrc;
        els.audioPlayer.currentTime = currentTime;
        if (wasPlaying) els.audioPlayer.play();
        updateChartsHighlight('A');
    };
    
    btnB.onclick = () => {
        if (window.currentAB === 'B') return;
        btnB.classList.add('active');
        btnA.classList.remove('active');
        const currentTime = els.audioPlayer.currentTime;
        const wasPlaying = !els.audioPlayer.paused;
        els.audioPlayer.src = refSrc;
        els.audioPlayer.currentTime = currentTime;
        if (wasPlaying) els.audioPlayer.play();
        updateChartsHighlight('B');
    };
}

function renderSummary(data) {
    const summary = data.summary;
    if (!summary) {
        els.summaryStrip.classList.add('hidden');
        return;
    }

    const verdictKey = {
        ready: 'summary-ready',
        polish: 'summary-polish',
        'needs-attention': 'summary-attention',
        'measurement-limited': 'summary-limited'
    }[summary.verdict] || 'summary-polish';
    const confidence = summary.confidence || { score: 0, label: 'low' };
    const confidenceLabel = t(`confidence-${confidence.label || 'low'}`);
    const loudnessScope = t(summary.loudness_scope === 'full-track' ? 'full-track' : 'slice-average');
    const overallScore = summary.overall_score != null ? summary.overall_score : null;
    const scoreColor = overallScore == null ? '#fff'
        : overallScore >= 80 ? 'var(--success)'
        : overallScore >= 60 ? 'var(--warning)'
        : 'var(--danger)';

    els.summaryStrip.className = `summary-strip ${summary.verdict || 'polish'}`;
    els.summaryStrip.innerHTML = `
        <div class="summary-chip primary"><span>${t(verdictKey)}</span><strong>${escapeHtml(data.genre)}</strong></div>
        ${overallScore != null ? `<div class="summary-chip" style="border-color: ${scoreColor}33; background: ${scoreColor}12;">
            <span>${t('overall-score')}</span>
            <strong style="color: ${scoreColor}; font-size: 1.6rem;">${overallScore}</strong>
            <small style="color: ${scoreColor};">/ 100</small>
        </div>` : ''}
        <div class="summary-chip"><span>LUFS</span><strong>${fmt(summary.measured_lufs, '', 1)}</strong><small>${t('target')} ${fmt(summary.target_lufs, '', 0)} / ${loudnessScope}</small></div>
        <div class="summary-chip"><span>${t('correlation')}</span><strong>${fmt(summary.correlation, '', 2)}</strong></div>
        <div class="summary-chip"><span>${t('low-end')}</span><strong>${fmt(summary.low_end_percent, '%', 1)}</strong></div>
        <div class="summary-chip"><span>${t('reliability')}</span><strong>${confidence.score}%</strong><small>${confidenceLabel}</small></div>
        <div class="summary-chip"><span>${t('slices')}</span><strong>${summary.slice_count}</strong></div>
    `;
}

function renderSlice(slice) {
    const radarLabels = [];
    const radarData = [];
    slice.bands.forEach(band => {
        radarLabels.push(bandLabel(band.name));
        radarData.push(Number(band.percent) || 0);
    });

    const datasets = [{
        label: t('your-mix'),
        data: radarData,
        backgroundColor: 'rgba(25, 211, 197, 0.18)',
        borderColor: '#19d3c5',
        pointBackgroundColor: '#19d3c5',
        pointBorderColor: '#fff',
        pointHoverBackgroundColor: '#fff',
        pointHoverBorderColor: '#19d3c5',
        borderWidth: 2
    }];

    if (referenceData) {
        const refSlice = referenceData.slices.find(item => item.tag === slice.tag) || referenceData.slices[0];
        datasets.push({
            label: t('reference'),
            data: refSlice.bands.map(band => Number(band.percent) || 0),
            backgroundColor: 'rgba(244, 114, 182, 0.18)',
            borderColor: '#f472b6',
            pointBackgroundColor: '#f472b6',
            pointBorderColor: '#fff',
            borderWidth: 2
        });
    }

    const insightsHtml = getCurrentInsights().map(insight => `
        <div class="insight-item">
            <h4><i class="fa-solid fa-lightbulb"></i> ${escapeHtml(insight.title)}</h4>
            <p>${escapeHtml(insight.desc)}</p>
            <p><strong><i class="fa-solid fa-wrench"></i> ${t('solution')}</strong> ${escapeHtml(insight.action)}</p>
            <div class="vst-badge"><i class="fa-solid fa-plug"></i> ${t('vsts')} ${escapeHtml(insight.vst)}</div>
        </div>
    `).join('');

    const bandRows = slice.bands.map(band => `
        <div class="band-row">
            <div>
                <strong>${escapeHtml(bandLabel(band.name))}</strong>
                <small>${escapeHtml(band.range)}</small>
            </div>
            <div class="band-meter">
                <span style="width: ${clamp(Number(band.percent) || 0, 0, 100)}%"></span>
            </div>
            <em>${fmt(band.percent, '%', 1)}</em>
        </div>
    `).join('');

    const correlation = asNumber(slice.correlation) ?? 0;
    const marker = clamp(((correlation + 1) / 2) * 100, 0, 100);
    const lufs = asNumber(slice.I);
    const [lufsLow, lufsHigh] = currentLufsRange();
    const targetMatch = lufs !== null && lufs >= lufsLow && lufs <= lufsHigh;
    const quality = slice.quality || {};
    const levelDetails = slice.levels || {};
    const leftLevels = levelDetails.left || {};
    const rightLevels = levelDetails.right || {};
    const monoLevels = levelDetails.mono || {};
    
    const resonanceHtml = (slice.resonances || []).map(r => `
        <div class="resonance-pill">
            <i class="fa-solid fa-wave-square"></i>
            ${r.band ? `<strong>${escapeHtml(bandLabel(r.band))}</strong> ` : ''}${t('resonance-detected')} ${r.freq} Hz (+${r.gain} dB)
        </div>
    `).join('');

    els.tabContent.innerHTML = `
        <div class="dashboard-grid">
            <div class="card insights-card">
                <h3><i class="fa-solid fa-wand-magic-sparkles"></i> ${t('insights-title')}</h3>
                <div class="insights-container">${insightsHtml}</div>
            </div>

            <div class="card">
                <h3><i class="fa-solid fa-bolt"></i> ${t('dyn')}</h3>
                <div class="metric-row"><span class="metric-label">${t('rms')}</span><span class="metric-value">${fmt(slice.rms_db, ' dBFS')}</span></div>
                <div class="metric-row"><span class="metric-label">${t('peak')}</span><span class="metric-value">${fmt(slice.peak_db, ' dBFS')}</span></div>
                <div class="metric-row"><span class="metric-label">${t('left-peak')}</span><span class="metric-value">${fmt(leftLevels.peak_db, ' dBFS')}</span></div>
                <div class="metric-row"><span class="metric-label">${t('right-peak')}</span><span class="metric-value">${fmt(rightLevels.peak_db, ' dBFS')}</span></div>
                <div class="metric-row"><span class="metric-label">${t('mono-peak')}</span><span class="metric-value">${fmt(monoLevels.peak_db, ' dBFS')}</span></div>
                <div class="metric-row"><span class="metric-label">${t('crest')}</span><span class="metric-value">${fmt(slice.crest_db, ' dB')}</span></div>
                <div class="metric-row"><span class="metric-label">True Peak</span><span class="metric-value">${fmt(slice.TP, ' dBTP')}</span></div>
            </div>

            <div class="card">
                <h3><i class="fa-solid fa-arrows-left-right"></i> ${t('stereo')}</h3>
                <div class="metric-row"><span class="metric-label">${t('correlation')}</span><span class="metric-value" style="color: ${metricTone(slice.correlation, 'correlation')}">${fmt(slice.correlation, '')}</span></div>
                <div class="metric-row"><span class="metric-label">${t('width')}</span><span class="metric-value">${fmt(slice.mid_side ? slice.mid_side.width_pct : 0, '%', 1)}</span></div>
                <div class="metric-row"><span class="metric-label">${t('ms-ratio')}</span><span class="metric-value">${fmt(slice.mid_side ? slice.mid_side.ms_ratio_db : 0, ' dB', 1)}</span></div>
                <div class="metric-row"><span class="metric-label">${t('stereo-balance')}</span><span class="metric-value">${fmt(quality.stereo_balance_db, ' dB', 2)}</span></div>
                <div class="correlation-meter">
                    <span style="left: ${marker}%"></span>
                </div>
                <div class="meter-labels"><span>-1</span><span>0</span><span>+1</span></div>
            </div>

            <div class="card">
                <h3><i class="fa-solid fa-volume-high"></i> ${t('lufs-title')}</h3>
                <div class="metric-row"><span class="metric-label">Integrated (I)</span><span class="metric-value" style="color: ${metricTone(slice.I, 'lufs')}">${fmt(slice.I, ' LUFS')}</span></div>
                <div class="metric-row"><span class="metric-label">${t('lra')}</span><span class="metric-value">${fmt(slice.LRA, ' LU')}</span></div>
                <div class="metric-row"><span class="metric-label">Sample Peak</span><span class="metric-value">${fmt(slice.SP, ' dBFS')}</span></div>
                <div class="metric-row"><span class="metric-label">${t('target-match')}</span><span class="metric-value">${targetMatch ? t('excellent') : t('needs-adj')}</span></div>
            </div>

            <div class="card">
                <h3><i class="fa-solid fa-shield-halved"></i> ${t('quality-title')}</h3>
                <div class="metric-row"><span class="metric-label">${t('clipping')}</span><span class="metric-value">${fmt(quality.clipped_percent, '%', 3)}</span></div>
                <div class="metric-row"><span class="metric-label">${t('silence')}</span><span class="metric-value">${fmt(quality.silence_percent, '%', 1)}</span></div>
                <div class="metric-row"><span class="metric-label">${t('dc-offset')}</span><span class="metric-value">${fmt(quality.dc_offset, '', 5)}</span></div>
                <div class="metric-row"><span class="metric-label">${t('centroid')}</span><span class="metric-value">${fmt(quality.spectral_centroid_hz, ' Hz', 0)}</span></div>
                <div class="metric-row"><span class="metric-label">${t('rolloff')}</span><span class="metric-value">${fmt(quality.spectral_rolloff_hz, ' Hz', 0)}</span></div>
                <div class="metric-row"><span class="metric-label">${t('loudness-method')}</span><span class="metric-value compact">${escapeHtml(slice.loudness_method || 'N/A')}</span></div>
            </div>

            <div class="card">
                <h3><i class="fa-solid fa-bolt-lightning"></i> ${t('transients-title')}</h3>
                ${(() => {
                    const tr = slice.transients || {};
                    const density = typeof tr.transient_density === 'number' ? tr.transient_density : null;
                    const attack  = typeof tr.attack_time_ms === 'number' ? tr.attack_time_ms : null;
                    const perc    = typeof tr.percussion_energy_pct === 'number' ? tr.percussion_energy_pct : null;
                    const dc = density == null ? '#fff' : density < 1 ? 'var(--warning)' : density > 8 ? 'var(--danger)' : 'var(--success)';
                    const ac = attack  == null ? '#fff' : attack > 30  ? 'var(--warning)' : attack < 4  ? 'var(--danger)' : 'var(--success)';
                    const dpct = density == null ? 0 : Math.min(100, density / 12 * 100);
                    const apct  = attack  == null ? 0 : Math.min(100, (1 - Math.max(0, attack - 4) / 76) * 100);
                    const ppct  = perc    == null ? 0 : Math.min(100, perc);
                    return `
                        <div class="metric-row"><span class="metric-label">${t('transient-density')}</span><span class="metric-value" style="color:${dc}">${density != null ? density.toFixed(1) : 'N/A'}</span></div>
                        <div class="band-meter" style="margin:-6px 0 12px"><span style="width:${dpct}%;background:${dc}"></span></div>
                        <div class="metric-row"><span class="metric-label">${t('attack-time')}</span><span class="metric-value" style="color:${ac}">${attack != null ? attack.toFixed(0)+' ms' : 'N/A'}</span></div>
                        <div class="band-meter" style="margin:-6px 0 12px"><span style="width:${apct}%;background:${ac}"></span></div>
                        <div class="metric-row"><span class="metric-label">${t('perc-energy')}</span><span class="metric-value">${perc != null ? perc.toFixed(1)+'%' : 'N/A'}</span></div>
                        <div class="band-meter" style="margin:-6px 0 0"><span style="width:${ppct}%"></span></div>`;
                })()}
            </div>

            <div class="card band-card">
                <h3><i class="fa-solid fa-chart-simple"></i> ${t('bands')}</h3>
                <div class="bands-container">
                    <canvas id="radarChart"></canvas>
                </div>
                <div class="band-list">${bandRows}</div>
            </div>

            <div class="card visual-card">
                <h3><i class="fa-solid fa-wave-square"></i> ${t('visuals')}</h3>
                <div class="visual-grid">
                    <div class="visual-container">
                        <img src="${escapeHtml(slice.waveform_url)}?t=${Date.now()}" alt="Waveform">
                    </div>
                    <div class="visual-container spectrum-chart-container">
                        <div class="spectrum-chart-header">
                            <span class="spectrum-chart-title">
                                <i class="fa-solid fa-chart-line"></i> ${t('spectrum-title')}
                            </span>
                            <span class="spectrum-hint">${t('spectrum-hint')}</span>
                        </div>
                        <canvas id="spectrumChart" style="width:100%;height:180px;"></canvas>
                        <div class="resonance-list">${resonanceHtml}</div>
                    </div>
                </div>
            </div>

            <div class="card">
                <h3><i class="fa-solid fa-headphones"></i> ${t('headphone-title')}</h3>
                ${(() => {
                    const sc = computeHeadphoneScores(slice);
                    const isInstr = analysisData && analysisData.is_instrumental;
                    const badgeClass = isInstr ? 'instrumental' : 'vocal';
                    const badgeIcon = isInstr ? 'fa-guitar' : 'fa-microphone';
                    const badgeText = isInstr ? t('instrumental') : t('vocal');
                    return `
                        <div class="score-ring-wrap">
                            ${scoreRingHtml('hp', 'hp', sc.headphone, t('headphone-score'))}
                            ${scoreRingHtml('sp', 'sp', sc.speaker, t('speaker-score'))}
                            <div class="score-detail">
                                <div class="metric-row"><span class="metric-label">${t('mono-compat')}</span><span class="metric-value">${sc.monoCompat}%</span></div>
                                <div class="metric-row"><span class="metric-label">${t('phase-quality')}</span><span class="metric-value">${sc.phaseQuality}%</span></div>
                                <div class="metric-row"><span class="metric-label">${t('track-type')}</span><span class="metric-value"><span class="vocal-badge ${badgeClass}"><i class="fa-solid ${badgeIcon}"></i>${badgeText}</span></span></div>
                            </div>
                        </div>`;
                })()}
            </div>
        </div>
    `;

    const ctx = document.getElementById('radarChart').getContext('2d');
    if (window.radarChartInstance) {
        window.radarChartInstance.destroy();
    }
    window.radarChartInstance = new Chart(ctx, {
        type: 'radar',
        data: {
            labels: radarLabels,
            datasets
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            scales: {
                r: {
                    angleLines: { color: 'rgba(255, 255, 255, 0.12)' },
                    grid: { color: 'rgba(255, 255, 255, 0.1)' },
                    pointLabels: { color: '#f4f7fb', font: { family: 'Inter', size: 11 } },
                    ticks: { display: false },
                    suggestedMax: Math.max(25, ...radarData) + 5
                }
            },
            plugins: {
                legend: {
                    display: !!referenceData,
                    labels: { color: '#fff', font: { family: 'Inter' } }
                }
            }
        }
    });
    animateScoreRings();

    // Render interactive spectrum chart after DOM is ready
    renderSpectrumChart(slice, referenceData);
}

function computeHeadphoneScores(slice) {
    const correlation = typeof slice.correlation === 'number' ? slice.correlation : 1.0;
    const widthPct = (slice.mid_side && typeof slice.mid_side.width_pct === 'number') ? slice.mid_side.width_pct : 0;
    const msRatioDb = (slice.mid_side && typeof slice.mid_side.ms_ratio_db === 'number') ? slice.mid_side.ms_ratio_db : 0;
    const clipped = (slice.quality && typeof slice.quality.clipped_percent === 'number') ? slice.quality.clipped_percent : 0;

    // Mono compatibility: high correlation = good mono compat
    const monoCompat = Math.round(Math.max(0, Math.min(100, ((correlation + 1) / 2) * 100)));

    // Phase quality: penalise extreme M/S ratio (>0dB side dominant)
    const phaseQuality = Math.round(Math.max(0, Math.min(100, 100 - Math.max(0, msRatioDb) * 6)));

    // Headphone score: needs good phase + some stereo width
    const widthBonus = Math.min(20, widthPct * 0.4);
    const headphone = Math.round(Math.max(0, Math.min(100,
        monoCompat * 0.45 + phaseQuality * 0.35 + widthBonus + (clipped < 0.01 ? 10 : 0)
    )));

    // Speaker score: needs strong mono compat (low correlation penalty for stereo ok)
    const speaker = Math.round(Math.max(0, Math.min(100,
        monoCompat * 0.6 + phaseQuality * 0.3 + (clipped < 0.01 ? 10 : 0)
    )));

    return { headphone, speaker, monoCompat, phaseQuality };
}

function scoreRingHtml(id, cls, score, label) {
    const circ = 264;
    const offset = circ - (circ * score / 100);
    return `
        <div class="score-ring ${cls}" data-score="${score}" data-circ="${circ}">
            <svg viewBox="0 0 96 96">
                <circle class="ring-bg" cx="48" cy="48" r="42"/>
                <circle class="ring-fill" id="ring-${id}" cx="48" cy="48" r="42"
                    stroke-dasharray="${circ}" stroke-dashoffset="${circ}"/>
            </svg>
            <div class="ring-val">
                <strong>${score}</strong>
                <small>${escapeHtml(label)}</small>
            </div>
        </div>`;
}

function animateScoreRings() {
    document.querySelectorAll('.score-ring[data-score]').forEach(ring => {
        const fill = ring.querySelector('.ring-fill');
        if (!fill) return;
        const circ = parseFloat(ring.dataset.circ) || 264;
        const score = parseFloat(ring.dataset.score) || 0;
        requestAnimationFrame(() => {
            fill.style.strokeDashoffset = String(circ - (circ * score / 100));
        });
    });
}

const bgCanvas = document.getElementById('bgCanvas');

// ─────────────────────────────────────────────────────────────
// Interactive Spectrum Chart (Chart.js)
// ─────────────────────────────────────────────────────────────
async function renderSpectrumChart(slice, refData) {
    const canvas = document.getElementById('spectrumChart');
    if (!canvas) return;

    if (window.spectrumChartInstance) {
        window.spectrumChartInstance.destroy();
        window.spectrumChartInstance = null;
    }

    // Show loading skeleton
    canvas.style.opacity = '0.4';

    const fft = slice.fft_data;
    if (!fft || !fft.freqs || !fft.mid) {
        canvas.style.opacity = '1';
        return;
    }

    const labels = fft.freqs.map(f => {
        if (f >= 1000) return `${(f / 1000).toFixed(1)}k`;
        return `${Math.round(f)}`;
    });

    const datasets = [];

    const isA = (window.currentAB || 'A') === 'A';

    // Mid channel
    datasets.push({
        label: t('spectrum-mid'),
        data: fft.mid,
        borderColor: isA ? '#19d3c5' : 'rgba(25, 211, 197, 0.4)',
        backgroundColor: isA ? 'rgba(25, 211, 197, 0.08)' : 'transparent',
        borderWidth: isA ? 2 : 1,
        pointRadius: 0,
        tension: 0.25,
        fill: isA,
        order: 2
    });

    // Side channel
    if (fft.side) {
        datasets.push({
            label: t('spectrum-side'),
            data: fft.side,
            borderColor: '#f472b6',
            backgroundColor: 'rgba(244, 114, 182, 0.05)',
            borderWidth: 1,
            borderDash: [4, 3],
            pointRadius: 0,
            tension: 0.25,
            fill: false,
            order: 4,
            hidden: !isA
        });
    }

    // Reference track overlay
    if (refData) {
        const refSlice = refData.slices.find(s => s.tag === slice.tag) || refData.slices[0];
        if (refSlice && refSlice.fft_data && refSlice.fft_data.mid) {
            const isB = !isA;
            datasets.push({
                label: `${t('reference')} (Mid)`,
                data: refSlice.fft_data.mid,
                borderColor: isB ? '#f59e0b' : 'rgba(245, 158, 11, 0.4)',
                backgroundColor: isB ? 'rgba(245, 158, 11, 0.08)' : 'transparent',
                borderWidth: isB ? 2 : 1,
                borderDash: isB ? [] : [6, 3],
                pointRadius: 0,
                tension: 0.25,
                fill: isB,
                order: 1
            });

            // Diff curve: Mix - Reference
            const diffData = fft.mid.map((val, i) => val - refSlice.fft_data.mid[i]);
            datasets.push({
                label: t('spectrum-diff'),
                data: diffData,
                borderColor: 'rgba(139, 92, 246, 0.6)',
                backgroundColor: 'transparent',
                borderWidth: 1.5,
                pointRadius: 0,
                tension: 0.25,
                fill: false,
                order: 3,
                yAxisID: 'yDiff',
                hidden: true // Hidden by default, user can toggle via legend
            });
        }
    }

    // Genre target curve (interpolated to match our 512-point grid)
    if (fft.target_curve && fft.target_curve.length >= 2) {
        const tcFreqs = fft.target_curve.map(p => p.f);
        const tcVals  = fft.target_curve.map(p => p.v);
        // Simple linear interpolation for each of our label frequencies
        const targetData = fft.freqs.map(f => {
            if (f <= tcFreqs[0]) return tcVals[0];
            if (f >= tcFreqs[tcFreqs.length - 1]) return tcVals[tcFreqs.length - 1];
            let i = 0;
            while (i < tcFreqs.length - 1 && tcFreqs[i + 1] < f) i++;
            const t0 = tcFreqs[i], t1 = tcFreqs[i + 1];
            const ratio = (f - t0) / (t1 - t0);
            return tcVals[i] + ratio * (tcVals[i + 1] - tcVals[i]);
        });
        datasets.push({
            label: t('spectrum-target'),
            data: targetData,
            borderColor: 'rgba(255,255,255,0.22)',
            backgroundColor: 'transparent',
            borderWidth: 1,
            borderDash: [3, 4],
            pointRadius: 0,
            tension: 0.3,
            fill: false,
            order: 4
        });
    }

    const ctx = canvas.getContext('2d');
    window.spectrumChartInstance = new Chart(ctx, {
        type: 'line',
        data: { labels, datasets },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            animation: { duration: 400, easing: 'easeOutQuart' },
            interaction: {
                mode: 'index',
                intersect: false
            },
            scales: {
                x: {
                    ticks: {
                        color: 'rgba(255,255,255,0.45)',
                        font: { family: 'Inter', size: 9 },
                        maxTicksLimit: 12,
                        maxRotation: 0
                    },
                    grid: { color: 'rgba(255,255,255,0.05)' }
                },
                y: {
                    position: 'left',
                    ticks: {
                        color: 'rgba(255,255,255,0.45)',
                        font: { family: 'Inter', size: 9 },
                        callback: v => `${v} dB`
                    },
                    grid: { color: 'rgba(255,255,255,0.06)' },
                    title: {
                        display: true,
                        text: 'dB',
                        color: 'rgba(255,255,255,0.3)',
                        font: { size: 9, family: 'Inter' }
                    }
                },
                yDiff: {
                    position: 'right',
                    display: false, // Only show if Diff dataset is active
                    min: -20,
                    max: 20,
                    ticks: {
                        color: 'rgba(139, 92, 246, 0.6)',
                        font: { family: 'Inter', size: 8 },
                        callback: v => `${v > 0 ? '+' : ''}${v} dB`
                    },
                    grid: { drawOnChartArea: false },
                    title: {
                        display: true,
                        text: 'Diff',
                        color: 'rgba(139, 92, 246, 0.5)',
                        font: { size: 9, family: 'Inter' }
                    }
                }
            },
            plugins: {
                legend: {
                    display: true,
                    position: 'top',
                    align: 'end',
                    labels: {
                        color: 'rgba(255,255,255,0.7)',
                        font: { family: 'Inter', size: 10 },
                        boxWidth: 12,
                        padding: 8
                    },
                    onClick: function(e, legendItem, legend) {
                        const index = legendItem.datasetIndex;
                        const ci = legend.chart;
                        const meta = ci.getDatasetMeta(index);
                        
                        meta.hidden = meta.hidden === null ? !ci.data.datasets[index].hidden : null;
                        
                        // Toggle secondary Y-axis for Diff
                        if (ci.data.datasets[index].label === t('spectrum-diff')) {
                            ci.options.scales.yDiff.display = !meta.hidden;
                        }
                        
                        ci.update();
                    }
                },
                tooltip: {
                    backgroundColor: 'rgba(16,18,21,0.95)',
                    borderColor: 'rgba(25,211,197,0.3)',
                    borderWidth: 1,
                    titleColor: '#19d3c5',
                    bodyColor: 'rgba(255,255,255,0.8)',
                    titleFont: { family: 'Inter', size: 11 },
                    bodyFont: { family: 'Inter', size: 10 },
                    callbacks: {
                        title(items) {
                            const idx = items[0].dataIndex;
                            const hz = fft.freqs[idx];
                            return hz >= 1000 ? `${(hz/1000).toFixed(2)} kHz` : `${Math.round(hz)} Hz`;
                        },
                        label(item) {
                            return ` ${item.dataset.label}: ${item.parsed.y.toFixed(1)} dB`;
                        }
                    }
                }
            }
        }
    });

    canvas.style.opacity = '1';
}
if (bgCanvas) {
    const bgCtx = bgCanvas.getContext('2d');
    const reduceMotion = window.matchMedia('(prefers-reduced-motion: reduce)').matches;

    function resizeBg() {
        bgCanvas.width = window.innerWidth;
        bgCanvas.height = window.innerHeight;
    }

    window.addEventListener('resize', resizeBg);
    resizeBg();

    let time = 0;
    function drawBg() {
        bgCtx.clearRect(0, 0, bgCanvas.width, bgCanvas.height);
        const baseY = bgCanvas.height * 0.72;
        const gradients = [
            ['rgba(25, 211, 197, 0.38)', 2, 0.003, 55],
            ['rgba(139, 92, 246, 0.24)', 3, 0.002, 78],
            ['rgba(245, 158, 11, 0.16)', 1, 0.006, 34]
        ];

        gradients.forEach(([color, width, speed, amp], index) => {
            bgCtx.beginPath();
            for (let x = 0; x <= bgCanvas.width; x += 5) {
                const y = Math.sin(x * speed + time + index) * amp + Math.sin(x * 0.008 - time * 0.45) * 24;
                if (x === 0) bgCtx.moveTo(x, baseY + y);
                else bgCtx.lineTo(x, baseY + y);
            }
            bgCtx.strokeStyle = color;
            bgCtx.lineWidth = width;
            bgCtx.stroke();
        });

        if (!reduceMotion) {
            time += 0.015;
            requestAnimationFrame(drawBg);
        }
    }

    drawBg();
}

function copyDAWNotes() {
    if (!analysisData || !analysisData.slices || analysisData.slices.length === 0) {
        setStatus(t('copy-wait'), "info");
        return;
    }
    
    const summary = analysisData.summary;
    const activeSlice = getActiveSlice() || analysisData.slices[0];
    if (!activeSlice) return;
    const levelDetails = activeSlice.levels || {};
    const leftLevels = levelDetails.left || {};
    const rightLevels = levelDetails.right || {};
    const monoLevels = levelDetails.mono || {};
    const quality = activeSlice.quality || {};
    
    let text = `MIX ANALYSIS REPORT: ${analysisData.filename}\n`;
    text += `Genre Profile: ${analysisData.genre}\n`;
    text += `------------------------------------------\n\n`;
    
    if (analysisData.priority_actions && analysisData.priority_actions.length > 0) {
        text += `TOP PRIORITY ACTIONS:\n`;
        analysisData.priority_actions.forEach((a, i) => text += `${i+1}. ${a}\n`);
        text += `\n`;
    }
    
    text += `KEY METRICS (${activeSlice.tag}):\n`;
    if (summary && summary.measured_lufs !== null && summary.measured_lufs !== undefined) {
        const scope = summary.loudness_scope === 'full-track' ? 'Full Track Loudness' : 'Slice Average Loudness';
        text += `- ${scope}: ${fmt(summary.measured_lufs, ' LUFS', 1)}\n`;
    } else {
        text += `- Loudness: ${fmt(activeSlice.I, ' LUFS', 1)}\n`;
    }
    text += `- Dynamics (Crest): ${fmt(activeSlice.crest_db, ' dB', 1)}\n`;
    text += `- Channel Peak L/R: ${fmt(leftLevels.peak_db, ' dBFS', 1)} / ${fmt(rightLevels.peak_db, ' dBFS', 1)}\n`;
    text += `- Mono Sum Peak: ${fmt(monoLevels.peak_db, ' dBFS', 1)}\n`;
    text += `- Correlation: ${fmt(activeSlice.correlation, '', 2)}\n`;
    text += `- L/R Balance: ${fmt(quality.stereo_balance_db, ' dB', 2)}\n`;
    text += `- Stereo Width: ${fmt(activeSlice.mid_side ? activeSlice.mid_side.width_pct : 0, '%', 1)}\n\n`;
    
    if (activeSlice.resonances && activeSlice.resonances.length > 0) {
        text += `DETECTED RESONANCES:\n`;
        activeSlice.resonances.forEach(r => text += `- ${r.band ? r.band + ': ' : ''}${r.freq} Hz (+${r.gain} dB)\n`);
        text += `\n`;
    }
    
    text += `Generated by Funky Moose Mix Analyzer Pro`;

    navigator.clipboard.writeText(text).then(() => {
        const btn = document.querySelector('.btn-secondary[onclick="copyDAWNotes()"]');
        if (btn) {
            const originalHtml = btn.innerHTML;
            btn.innerHTML = `<i class="fa-solid fa-check"></i> <span>${t('copied')}</span>`;
            setTimeout(() => btn.innerHTML = originalHtml, 2000);
        }
    }).catch(err => {
        console.error('Could not copy text: ', err);
    });
}

// ─────────────────────────────────────────────────────────────
// System Check (Public Readiness)
// ─────────────────────────────────────────────────────────────
async function checkSystem() {
    try {
        const status = await fetchJson('/init_check');
        if (status.success && !status.ffmpeg) {
            document.getElementById('dependencyModal').classList.remove('hidden');
            document.getElementById('depTitle').textContent = t('dep-title');
            document.getElementById('depDesc').textContent = t('dep-desc');
            const checkBtn = document.querySelector('#dependencyModal button');
            if (checkBtn) checkBtn.innerHTML = `<i class="fa-solid fa-rotate"></i> ${t('dep-check')}`;
        }
    } catch (err) {
        console.error('System check failed:', err);
    }
}

// Initial System Check
checkSystem();
