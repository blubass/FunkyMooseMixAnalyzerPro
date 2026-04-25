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
    analysisSteps: document.getElementById('analysisSteps')
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
        'mastering-title': 'Mastering Assistant',
        'mastering-sub': 'Top-Prioritäten für Balance und Master'
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
        'mastering-title': 'Mastering Assistant',
        'mastering-sub': 'Top priority actions for your mix balance'
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
    const compOpt = els.compareSelect.querySelector('option[value=""]');
    if (compOpt) compOpt.textContent = t('compare-placeholder');
    const abLabel = document.querySelector('.ab-label');
    if (abLabel) abLabel.textContent = t('ab-compare');
    const assistantTitle = document.querySelector('#masteringAssistant .assistant-header h3');
    if (assistantTitle) assistantTitle.innerHTML = `<i class="fa-solid fa-magic"></i> ${t('mastering-title')}`;
    const assistantSub = document.querySelector('#masteringAssistant .assistant-header p');
    if (assistantSub) assistantSub.textContent = t('mastering-sub');

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

    const pdfFilename = `Mix_Analysis_Report_${analysisData.filename}.pdf`;

    const restoreExportUi = () => {
        els.printHeader.classList.add('hidden');
        els.actionButtons.style.display = 'flex';
    };

    html2pdf().set({
        margin: [15, 15, 15, 15],
        filename: pdfFilename,
        image: { type: 'jpeg', quality: 0.98 },
        html2canvas: { scale: 2, useCORS: true, logging: false, backgroundColor: '#101215' },
        jsPDF: { unit: 'mm', format: 'a4', orientation: 'portrait' }
    }).from(els.resultsSection).outputPdf('datauristring').then(dataUri => {
        restoreExportUi();
        // Strip the data URI prefix to get raw base64
        const base64 = dataUri.replace(/^data:application\/pdf;base64,/, '');

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

    const formData = new FormData();
    formData.append('audio', file);
    formData.append('genre', els.genreSelect.value || defaultGenre);
    formData.append('lang', currentLang);

    try {
        const startData = await fetchJson('/upload', {
            method: 'POST',
            body: formData
        });

        const reqId = startData.req_id;
        const totalSlices = startData.slices_meta.length;
        let completedSlices = 0;

        // Populate steps
        els.analysisSteps.replaceChildren();
        startData.slices_meta.forEach(meta => {
            const span = document.createElement('span');
            span.textContent = meta.tag;
            span.id = `step-${meta.tag}`;
            els.analysisSteps.appendChild(span);
        });

        // Prepare the result shell while keeping the analysis progress visible.
        renderResults({
            filename: startData.filename,
            audio_url: startData.audio_url,
            slices: [],
            genre: els.genreSelect.value,
            summary: null
        }, { showResults: false, keepLoading: true, refreshHistory: false });

        for (const meta of startData.slices_meta) {
            updateLoadingProgress(meta.tag, completedSlices, totalSlices);
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
            completedSlices++;
            
            // Update progress bar
            const pct = (completedSlices / totalSlices) * 100;
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

function updateLoadingProgress(tag, current, total) {
    const stepEl = document.getElementById(`step-${tag}`);
    if (stepEl) stepEl.classList.add('active');
    
    els.loadingTitle.textContent = `${t('analyzing')} (${tag})`;
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
        return;
    }
    
    abContainer.classList.remove('hidden');
    
    const originalSrc = analysisData.audio_url;
    const refSrc = referenceData.audio_url;
    
    btnA.onclick = () => {
        btnA.classList.add('active');
        btnB.classList.remove('active');
        const currentTime = els.audioPlayer.currentTime;
        const wasPlaying = !els.audioPlayer.paused;
        els.audioPlayer.src = originalSrc;
        els.audioPlayer.currentTime = currentTime;
        if (wasPlaying) els.audioPlayer.play();
    };
    
    btnB.onclick = () => {
        btnB.classList.add('active');
        btnA.classList.remove('active');
        const currentTime = els.audioPlayer.currentTime;
        const wasPlaying = !els.audioPlayer.paused;
        els.audioPlayer.src = refSrc;
        els.audioPlayer.currentTime = currentTime;
        if (wasPlaying) els.audioPlayer.play();
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

    els.summaryStrip.className = `summary-strip ${summary.verdict || 'polish'}`;
    els.summaryStrip.innerHTML = `
        <div class="summary-chip primary"><span>${t(verdictKey)}</span><strong>${escapeHtml(data.genre)}</strong></div>
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
                    <div class="visual-container">
                        <img src="${escapeHtml(slice.spectrum_url)}?t=${Date.now()}" alt="Spectrum">
                        <div class="resonance-list">${resonanceHtml}</div>
                    </div>
                </div>
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
}

const bgCanvas = document.getElementById('bgCanvas');
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
