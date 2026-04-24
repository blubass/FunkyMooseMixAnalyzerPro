const dropZone = document.getElementById('dropZone');
const fileInput = document.getElementById('fileInput');
const uploadSection = document.getElementById('uploadSection');
const loadingState = document.getElementById('loadingState');
const resultsSection = document.getElementById('resultsSection');
const newAnalysisBtn = document.getElementById('newAnalysisBtn');
const filenameDisplay = document.getElementById('filenameDisplay');
const tabsContainer = document.querySelector('.tabs');
const tabContent = document.getElementById('tabContent');

let analysisData = null;
let referenceData = null;
let currentLang = 'de';

const i18n = {
    'de': {
        'subtitle': 'KI Audio Analyse Dashboard • by Uwe Arthur Felchle',
        'genre-label': 'Genre Profil:',
        'drag-drop': 'Audiodatei hier ablegen',
        'or-click': 'oder klicken zum Auswählen',
        'formats': 'MP3, WAV, FLAC, AIFF unterstützt bis 100MB',
        'analyzing': 'Mix wird analysiert...',
        'analyzing-sub': 'Führe Audio-Modelle & VST-Matching aus',
        'results-for': 'Analyse-Ergebnisse für ',
        'new-analysis': 'Neue Analyse',
        'export': 'Export PDF',
        'insights-title': 'AI Mix Insights & VST Empfehlungen',
        'solution': 'Lösungsvorschlag:',
        'vsts': 'Empfohlene VSTs:',
        'dyn': 'Dynamik',
        'rms': 'RMS Pegel',
        'peak': 'Spitzenpegel',
        'crest': 'Crest-Faktor',
        'stereo': 'Stereo Bild',
        'correlation': 'Phasenkorrelation',
        'mono-comp': 'Mono Kompatibilität',
        'good': 'Gut',
        'poor': 'Schlecht (Gefahr)',
        'lufs-title': 'Lautheit (LUFS)',
        'lra': 'Dynamikumfang (LRA)',
        'target-match': 'Ziel-Match',
        'excellent': 'Exzellent',
        'needs-adj': 'Anpassung nötig',
        'bands': 'Frequenzverteilung',
        'visuals': 'Visualisierungen',
        'library': 'Bibliothek',
        'history': 'Historie & Referenzen',
        'compare-placeholder': 'Vergleichen mit...'
    },
    'en': {
        'subtitle': 'AI Audio Analysis Dashboard • by Uwe Arthur Felchle',
        'genre-label': 'Genre Profile:',
        'drag-drop': 'Drag & Drop your audio file',
        'or-click': 'or click to browse your files',
        'formats': 'MP3, WAV, FLAC, AIFF supported up to 100MB',
        'analyzing': 'Analyzing your mix...',
        'analyzing-sub': 'Running audio processing models & VST matching',
        'results-for': 'Analysis Results for ',
        'new-analysis': 'New Analysis',
        'export': 'Export PDF',
        'insights-title': 'AI Mix Insights & VST Recommendations',
        'solution': 'Suggested Action:',
        'vsts': 'Recommended VSTs:',
        'dyn': 'Dynamics',
        'rms': 'RMS Level',
        'peak': 'Peak Level',
        'crest': 'Crest Factor',
        'stereo': 'Stereo Field',
        'correlation': 'Phase Correlation',
        'mono-comp': 'Mono Compatibility',
        'good': 'Good',
        'poor': 'Poor (Risk)',
        'lufs-title': 'Loudness (LUFS)',
        'lra': 'Loudness Range (LRA)',
        'target-match': 'Target Match',
        'excellent': 'Excellent',
        'needs-adj': 'Needs Adjustment',
        'bands': 'Band Distribution',
        'visuals': 'Audio Visualizations',
        'library': 'Library',
        'history': 'History & References',
        'compare-placeholder': 'Compare with...'
    }
};

function t(key) {
    return i18n[currentLang][key] || key;
}

function updateTexts() {
    document.querySelector('header p').textContent = t('subtitle');
    document.querySelector('label[for="genreSelect"]').textContent = t('genre-label');
    document.querySelector('#dropZone h2').textContent = t('drag-drop');
    document.querySelector('#dropZone p').textContent = t('or-click');
    document.querySelector('.formats').textContent = t('formats');
    document.querySelector('#loadingState h3').textContent = t('analyzing');
    document.querySelector('#loadingState p').textContent = t('analyzing-sub');
    document.getElementById('newAnalysisBtn').textContent = t('new-analysis');
    document.getElementById('exportText').textContent = t('export');
    document.getElementById('libText').textContent = t('library');
    document.getElementById('historyText').textContent = t('history');
    const compOpt = document.querySelector('#compareSelect option[value=""]');
    if (compOpt) compOpt.textContent = t('compare-placeholder');
    
    if (!resultsSection.classList.contains('hidden') && analysisData) {
        document.querySelector('.results-header h2').innerHTML = t('results-for') + '<span id="filenameDisplay">' + analysisData.filename + '</span>';
    }
}

document.querySelectorAll('.lang-btn').forEach(btn => {
    btn.addEventListener('click', (e) => {
        document.querySelectorAll('.lang-btn').forEach(b => b.classList.remove('active'));
        e.target.classList.add('active');
        currentLang = e.target.getAttribute('data-lang');
        updateTexts();
        if (analysisData) {
            // Re-render the active tab to update UI labels
            const activeSliceName = document.querySelector('.tab-btn.active').textContent;
            const slice = analysisData.slices.find(s => s.tag === activeSliceName);
            if (slice) renderSlice(slice);
        }
    });
});

updateTexts();

// Event Listeners for Drag & Drop
['dragenter', 'dragover', 'dragleave', 'drop'].forEach(eventName => {
    dropZone.addEventListener(eventName, preventDefaults, false);
});

function preventDefaults(e) {
    e.preventDefault();
    e.stopPropagation();
}

['dragenter', 'dragover'].forEach(eventName => {
    dropZone.addEventListener(eventName, () => dropZone.classList.add('dragover'), false);
});

['dragleave', 'drop'].forEach(eventName => {
    dropZone.addEventListener(eventName, () => dropZone.classList.remove('dragover'), false);
});

dropZone.addEventListener('drop', handleDrop, false);
dropZone.addEventListener('click', () => fileInput.click());
fileInput.addEventListener('change', (e) => handleFiles(e.target.files));

newAnalysisBtn.addEventListener('click', () => {
    resultsSection.classList.add('hidden');
    uploadSection.classList.remove('hidden');
    dropZone.classList.remove('hidden');
    loadingState.classList.add('hidden');
    fileInput.value = '';
    referenceData = null;
    document.getElementById('compareSelect').value = "";
});

// Library Sidebar Logic
const sidebar = document.getElementById('historySidebar');
const compareSelect = document.getElementById('compareSelect');

document.getElementById('libraryBtn').addEventListener('click', () => {
    sidebar.classList.remove('hidden');
    fetchHistory();
});

document.getElementById('closeSidebarBtn').addEventListener('click', () => {
    sidebar.classList.add('hidden');
});

function fetchHistory() {
    fetch('/history')
        .then(res => res.json())
        .then(data => {
            const list = document.getElementById('historyList');
            list.innerHTML = '';
            
            compareSelect.innerHTML = '<option value="">' + t('compare-placeholder') + '</option>';
            let hasRefs = false;

            data.forEach(item => {
                if (item.is_reference) {
                    hasRefs = true;
                    compareSelect.innerHTML += `<option value="${item.id}">${item.filename} (${item.genre})</option>`;
                }

                const div = document.createElement('div');
                div.className = `history-item ${item.is_reference ? 'is-ref' : ''}`;
                
                const date = new Date(item.timestamp).toLocaleString();
                
                div.innerHTML = `
                    <button class="star-btn ${item.is_reference ? 'active' : ''}" data-id="${item.id}" title="Toggle Reference">
                        <i class="fa-solid fa-star"></i>
                    </button>
                    <h4>${item.filename}</h4>
                    <p>${item.genre} • ${date}</p>
                `;
                
                div.addEventListener('click', (e) => {
                    if (e.target.closest('.star-btn')) {
                        toggleReference(item.id, div);
                        return;
                    }
                    loadAnalysis(item.id);
                });
                
                list.appendChild(div);
            });
            
            compareSelect.style.display = hasRefs ? 'block' : 'none';
        });
}

function toggleReference(id, element) {
    fetch(`/toggle_reference/${id}`, { method: 'POST' })
        .then(res => res.json())
        .then(data => {
            if (data.success) {
                if (data.is_reference) {
                    element.classList.add('is-ref');
                    element.querySelector('.star-btn').classList.add('active');
                } else {
                    element.classList.remove('is-ref');
                    element.querySelector('.star-btn').classList.remove('active');
                }
                fetchHistory(); // Refresh dropdown
            }
        });
}

function loadAnalysis(id) {
    loadingState.classList.remove('hidden');
    dropZone.classList.add('hidden');
    uploadSection.classList.remove('hidden');
    resultsSection.classList.add('hidden');
    sidebar.classList.add('hidden');
    
    fetch(`/analysis/${id}`)
        .then(res => res.json())
        .then(data => {
            analysisData = data;
            referenceData = null;
            compareSelect.value = "";
            displayResults(data);
        });
}

compareSelect.addEventListener('change', (e) => {
    if (!e.target.value) {
        referenceData = null;
        const activeSliceName = document.querySelector('.tab-btn.active').textContent;
        const slice = analysisData.slices.find(s => s.tag === activeSliceName);
        if (slice) renderSlice(slice);
        return;
    }
    
    fetch(`/analysis/${e.target.value}`)
        .then(res => res.json())
        .then(data => {
            referenceData = data;
            const activeSliceName = document.querySelector('.tab-btn.active').textContent;
            const slice = analysisData.slices.find(s => s.tag === activeSliceName);
            if (slice) renderSlice(slice);
        });
});

document.getElementById('exportPdfBtn').addEventListener('click', () => {
    const element = document.getElementById('resultsSection');
    const printHeader = document.getElementById('printHeader');
    const actionButtons = document.getElementById('actionButtons');
    
    // Show print header, hide buttons
    printHeader.classList.remove('hidden');
    actionButtons.style.display = 'none';
    
    html2pdf().set({
        margin: [15, 15, 15, 15],
        filename: 'Mix_Analysis_Report_' + analysisData.filename + '.pdf',
        image: { type: 'jpeg', quality: 0.98 },
        html2canvas: { scale: 2, useCORS: true, logging: false, backgroundColor: '#0f1115' },
        jsPDF: { unit: 'mm', format: 'a4', orientation: 'portrait' }
    }).from(element).save().then(() => {
        // Restore UI
        printHeader.classList.add('hidden');
        actionButtons.style.display = 'flex';
    });
});

function handleDrop(e) {
    const dt = e.dataTransfer;
    const files = dt.files;
    handleFiles(files);
}

function handleFiles(files) {
    if (files.length === 0) return;
    const file = files[0];
    
    // Validate file type
    const validTypes = ['audio/mpeg', 'audio/wav', 'audio/flac', 'audio/x-aiff', 'audio/aiff', 'audio/ogg', 'audio/aac', 'audio/mp4'];
    if (file.type && !validTypes.includes(file.type) && !file.name.match(/\.(mp3|wav|flac|aiff|aif|m4a|ogg|aac)$/i)) {
        alert('Unsupported file format. Please upload an audio file.');
        return;
    }
    
    uploadFile(file);
}

function uploadFile(file) {
    dropZone.classList.add('hidden');
    loadingState.classList.remove('hidden');
    referenceData = null;
    compareSelect.value = "";
    
    const formData = new FormData();
    formData.append('audio', file);
    const genre = document.getElementById('genreSelect').value;
    formData.append('genre', genre);
    formData.append('lang', currentLang);
    
    fetch('/upload', {
        method: 'POST',
        body: formData
    })
    .then(response => {
        if (!response.ok) {
            return response.json().then(err => { throw new Error(err.error || 'Upload failed') });
        }
        return response.json();
    })
    .then(data => {
        analysisData = data;
        displayResults(data);
    })
    .catch(error => {
        alert(error.message);
        loadingState.classList.add('hidden');
        dropZone.classList.remove('hidden');
    });
}

function displayResults(data) {
    uploadSection.classList.add('hidden');
    resultsSection.classList.remove('hidden');
    
    filenameDisplay.textContent = data.filename;
    
    const audioPlayer = document.getElementById('audioPlayer');
    audioPlayer.src = data.audio_url;
    audioPlayer.load();
    
    tabsContainer.innerHTML = '';
    
    data.slices.forEach((slice, index) => {
        const btn = document.createElement('button');
        btn.className = `tab-btn ${index === 0 ? 'active' : ''}`;
        btn.textContent = slice.tag;
        btn.onclick = () => {
            document.querySelectorAll('.tab-btn').forEach(b => b.classList.remove('active'));
            btn.classList.add('active');
            renderSlice(slice);
        };
        tabsContainer.appendChild(btn);
    });
    
    if (data.slices.length > 0) {
        renderSlice(data.slices[0]);
    }
}

function renderSlice(slice) {
    // Helper to format values safely
    const fmt = (val, suffix='') => val !== null ? `${parseFloat(val).toFixed(2)}${suffix}` : 'N/A';
    
    // Create Data for Radar Chart instead of HTML bars
    const radarLabels = [];
    const radarData = [];
    slice.bands.forEach(band => {
        radarLabels.push(band.name);
        radarData.push(band.percent);
    });

    const datasets = [{
        label: 'Your Mix',
        data: radarData,
        backgroundColor: 'rgba(0, 242, 254, 0.2)',
        borderColor: '#00f2fe',
        pointBackgroundColor: '#4facfe',
        pointBorderColor: '#fff',
        pointHoverBackgroundColor: '#fff',
        pointHoverBorderColor: '#00f2fe',
        borderWidth: 2
    }];

    if (referenceData) {
        const refSlice = referenceData.slices.find(s => s.tag === slice.tag) || referenceData.slices[0];
        const refRadarData = refSlice.bands.map(b => b.percent);
        datasets.push({
            label: 'Reference',
            data: refRadarData,
            backgroundColor: 'rgba(244, 114, 182, 0.2)',
            borderColor: '#f472b6',
            pointBackgroundColor: '#f472b6',
            pointBorderColor: '#fff',
            borderWidth: 2
        });
    }

    let insightsHtml = '';
    if (analysisData && analysisData.insights) {
        analysisData.insights.forEach(insight => {
            insightsHtml += `
                <div class="insight-item">
                    <h4><i class="fa-solid fa-lightbulb"></i> ${insight.title}</h4>
                    <p>${insight.desc}</p>
                    <p><strong style="color: #fff;"><i class="fa-solid fa-wrench"></i> ${t('solution')}</strong> ${insight.action}</p>
                    <div class="vst-badge"><i class="fa-solid fa-plug"></i> ${t('vsts')} ${insight.vst}</div>
                </div>
            `;
        });
    }

    const html = `
        <div class="dashboard-grid">
            <div class="card insights-card">
                <h3><i class="fa-solid fa-wand-magic-sparkles"></i> ${t('insights-title')}</h3>
                <div class="insights-container">
                    ${insightsHtml}
                </div>
            </div>

            <div class="card">
                <h3><i class="fa-solid fa-bolt"></i> ${t('dyn')}</h3>
                <div class="metric-row">
                    <span class="metric-label">${t('rms')}</span>
                    <span class="metric-value">${fmt(slice.rms_db, ' dBFS')}</span>
                </div>
                <div class="metric-row">
                    <span class="metric-label">${t('peak')}</span>
                    <span class="metric-value">${fmt(slice.peak_db, ' dBFS')}</span>
                </div>
                <div class="metric-row">
                    <span class="metric-label">${t('crest')}</span>
                    <span class="metric-value">${fmt(slice.crest_db, ' dB')}</span>
                </div>
                <div class="metric-row">
                    <span class="metric-label">True Peak</span>
                    <span class="metric-value">${fmt(slice.TP, ' dBTP')}</span>
                </div>
            </div>
            
            <div class="card">
                <h3><i class="fa-solid fa-arrows-left-right"></i> ${t('stereo')}</h3>
                <div class="metric-row">
                    <span class="metric-label">${t('correlation')}</span>
                    <span class="metric-value">${fmt(slice.correlation, '')}</span>
                </div>
                <div class="metric-row">
                    <span class="metric-label">${t('mono-comp')}</span>
                    <span class="metric-value" style="color: ${slice.correlation > 0.5 ? 'var(--success)' : 'var(--danger)'}">${slice.correlation > 0.5 ? t('good') : t('poor')}</span>
                </div>
                <div class="progress-bar" style="margin-top: 20px; height: 12px; background: linear-gradient(90deg, #ef4444 0%, #f59e0b 40%, #10b981 100%);">
                    <div style="width: 4px; height: 100%; background: #fff; border-radius: 2px; box-shadow: 0 0 5px rgba(0,0,0,0.5); margin-left: ${((slice.correlation + 1) / 2) * 100}%"></div>
                </div>
                <div style="display: flex; justify-content: space-between; font-size: 0.75rem; color: var(--text-muted); margin-top: 8px;">
                    <span>-1 (Anti-Phase)</span>
                    <span>0 (Wide)</span>
                    <span>+1 (Mono)</span>
                </div>
            </div>

            <div class="card">
                <h3><i class="fa-solid fa-volume-high"></i> ${t('lufs-title')}</h3>
                <div class="metric-row">
                    <span class="metric-label">Integrated (I)</span>
                    <span class="metric-value" style="color: ${slice.I > -9 ? 'var(--danger)' : slice.I < -16 ? 'var(--warning)' : 'var(--success)'}">${fmt(slice.I, ' LUFS')}</span>
                </div>
                <div class="metric-row">
                    <span class="metric-label">${t('lra')}</span>
                    <span class="metric-value">${fmt(slice.LRA, ' LU')}</span>
                </div>
                <div class="metric-row">
                    <span class="metric-label">Sample Peak</span>
                    <span class="metric-value">${fmt(slice.SP, ' dBFS')}</span>
                </div>
                <div class="metric-row">
                    <span class="metric-label">${t('target-match')}</span>
                    <span class="metric-value">${slice.I >= -14 && slice.I <= -9 ? t('excellent') : t('needs-adj')}</span>
                </div>
            </div>

            <div class="card" style="grid-column: span 1;">
                <h3><i class="fa-solid fa-chart-simple"></i> ${t('bands')}</h3>
                <div class="bands-container" style="position: relative; height: 250px; width: 100%; display: flex; justify-content: center; align-items: center;">
                    <canvas id="radarChart"></canvas>
                </div>
            </div>
            
            <div class="card visual-card">
                <h3><i class="fa-solid fa-wave-square"></i> ${t('visuals')}</h3>
                <div class="visual-container">
                    <img src="${slice.waveform_url}?t=${new Date().getTime()}" alt="Waveform">
                </div>
                <div class="visual-container">
                    <img src="${slice.spectrum_url}?t=${new Date().getTime()}" alt="Spectrum">
                </div>
            </div>
        </div>
    `;
    
    tabContent.innerHTML = html;
    
    // Render Chart.js Radar
    const ctx = document.getElementById('radarChart').getContext('2d');
    if (window.radarChartInstance) {
        window.radarChartInstance.destroy();
    }
    window.radarChartInstance = new Chart(ctx, {
        type: 'radar',
        data: {
            labels: radarLabels,
            datasets: datasets
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            scales: {
                r: {
                    angleLines: { color: 'rgba(255, 255, 255, 0.1)' },
                    grid: { color: 'rgba(255, 255, 255, 0.1)' },
                    pointLabels: { color: '#f0f2f5', font: { family: 'Inter', size: 11 } },
                    ticks: { display: false, max: Math.max(...radarData) + 5 }
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

// Background Waveform Animation
const bgCanvas = document.getElementById('bgCanvas');
if (bgCanvas) {
    const bgCtx = bgCanvas.getContext('2d');
    
    function resizeBg() {
        bgCanvas.width = window.innerWidth;
        bgCanvas.height = window.innerHeight;
    }
    window.addEventListener('resize', resizeBg);
    resizeBg();
    
    let time = 0;
    function drawBg() {
        bgCtx.clearRect(0, 0, bgCanvas.width, bgCanvas.height);
        
        // Draw first wave
        bgCtx.beginPath();
        for (let i = 0; i <= bgCanvas.width; i += 5) {
            const y = Math.sin(i * 0.003 + time) * 60 + Math.sin(i * 0.008 - time * 0.5) * 40;
            const baseY = bgCanvas.height * 0.75;
            if (i === 0) bgCtx.moveTo(i, baseY + y);
            else bgCtx.lineTo(i, baseY + y);
        }
        bgCtx.strokeStyle = 'rgba(0, 242, 254, 0.4)';
        bgCtx.lineWidth = 2;
        bgCtx.stroke();
        
        // Draw second wave
        bgCtx.beginPath();
        for (let i = 0; i <= bgCanvas.width; i += 5) {
            const y = Math.cos(i * 0.002 - time) * 80 + Math.sin(i * 0.005 + time * 0.8) * 50;
            const baseY = bgCanvas.height * 0.75;
            if (i === 0) bgCtx.moveTo(i, baseY + y);
            else bgCtx.lineTo(i, baseY + y);
        }
        bgCtx.strokeStyle = 'rgba(79, 172, 254, 0.2)';
        bgCtx.lineWidth = 3;
        bgCtx.stroke();
    
        time += 0.015;
        requestAnimationFrame(drawBg);
    }
    drawBg();
}
