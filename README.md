# 🦌 Funky Moose Mix Analyzer Pro

**Der intelligente Begleiter für dein Mixing & Mastering.** 

Der Funky Moose Mix Analyzer Pro ist ein leistungsstarkes Desktop-Tool, das deine Audio-Exporte gegen professionelle Genre-Standards prüft. Es liefert dir präzise Metriken zu Lautheit, Frequenzbalance und Phasenlage – komplett lokal, datenschutzfreundlich und mit konkreten Tipps zur Optimierung deines Masters.

---

## ✨ Key Features

*   **Interaktive FFT-Spektralanalyse**: Hochpräzise Echtzeit-Visualisierung via Chart.js. Inklusive Hover-Details, Zoom-Möglichkeit und Erkennung störender Resonanzen.
*   **Professioneller Track-Vergleich (A/B)**: Vergleiche deinen Mix direkt mit Referenztracks. Nutze die **Differenzkurve (Mix - Ref)**, um Maskierungseffekte und Balance-Unterschiede sofort visuell zu identifizieren.
*   **Genre-Referenzkurven**: Vergleiche deinen Mix mit über 30 dedizierten Profilen (Techno, K-Pop, Rock, Hip Hop, Podcast, etc.).
*   **Loudness & Dynamics**: Messung nach Industriestandards: LUFS (EBU R128), True Peak, Sample Peak und Crest-Faktor.
*   **Optimiert für große Dateien**: Asynchrone Analyse-Engine mit Fortschrittsanzeige – verarbeitet auch lange Mixe oder ganze DJ-Sets ohne Performance-Verlust.
*   **Mono-Kompatibilität**: Überprüfung der Phasenkorrelation und Stereo-Breite (Mid/Side-Verhältnis).
*   **KI-basierte Mastering-Insights**: Konkrete Vorschläge ("Low-End aufräumen", "Harte Resonanzen bei 4kHz bändigen"), um deinen Mix final zu polieren.
*   **Privacy First**: Alle Analysen laufen 100% lokal. Deine Musik verlässt niemals deine Festplatte.

---

## 🚀 Installation

### Voraussetzungen
Stelle sicher, dass **FFmpeg** auf deinem System installiert ist. Die App führt beim Start einen automatischen System-Check durch.

*   **macOS**: `brew install ffmpeg`
*   **Windows**: `choco install ffmpeg` oder manueller Download via [ffmpeg.org](https://ffmpeg.org/download.html).

### Starten (Entwickler-Modus)
1. Repository klonen:
   ```bash
   git clone https://github.com/blubass/FunkyMooseMixAnalyzerPro.git
   ```
2. Abhängigkeiten installieren:
   ```bash
   pip install -r requirements.txt
   ```
3. App starten:
   ```bash
   python app.py
   ```

---

## 🛠 Technologie-Stack

*   **Backend**: Python & Flask
*   **Audio-Engine**: NumPy (FFT & Transienten-Tuning mit Noise-Floor) & FFmpeg (Loudness Streaming)
*   **Frontend**: Modernes HTML5/JS/CSS (Vanilla) integriert via **PyWebView** für ein natives macOS/Windows App-Gefühl.
*   **Charts**: Chart.js für interaktive Spektren und Radar-Diagramme.
*   **Datenbank**: SQLite für eine persistente Analyse-Historie und lokale Bibliothek.

---

## 📈 Analyse-Metriken im Detail

### Spektrale Balance
Das Tool berechnet die Energieverteilung über sechs kritische Bänder (**Sub, Bass, Low-Mids, Mids, Presence, Air**) und vergleicht diese mit der Zielkurve deines gewählten Genres.

### Robuste Transienten-Erkennung
Dank eines intelligenten "Noise-Floor"-Thresholding werden Transienten auch bei extrem laut gemastertem Material oder in leisen Passagen präzise erkannt, ohne Geister-Onsets zu erzeugen.

### Phasenkorrelation
Ein Wert zwischen -1 und +1 gibt an, wie mono-kompatibel dein Mix ist. Ein Wert nahe 0 oder im negativen Bereich warnt vor Phasenauslöschungen im Bass-Bereich oder bei Lead-Synths.

---

## 🔬 Technische Spezifikationen & Mathematik

Der Funky Moose Mix Analyzer Pro setzt auf State-of-the-Art Algorithmen zur Audio-Analyse:

### 1. Frequenzanalyse (FFT)
Die Spektralanalyse basiert auf einer Fast Fourier Transformation (FFT). Um eine optimale Balance zwischen Frequenzauflösung (besonders im Bassbereich) und zeitlicher Genauigkeit zu erreichen, nutzt das Tool:
*   **Windowing**: Eine Hann-Fensterfunktion zur Minimierung von Spectral Leakage.
*   **Zero-Padding**: Automatische Auffüllung auf die nächste Zweierpotenz ($2^n$), um die Berechnungseffizienz zu maximieren.
*   **Logarithmische Skalierung**: Die Frequenzbänder werden für eine gehörrichtige Darstellung nach Oktaven gruppiert.

### 2. Dynamik & Transienten-Erkennung
Im Gegensatz zu einfachen Peak-Metern nutzt dieser Analyzer eine dedizierte Onset-Detection auf Basis von Spectral Flux:
*   **Transient Density**: Berechnet die Anzahl der signifikanten Pegelanstiege pro Sekunde.
*   **Attack-Time Estimation**: Misst die Zeitspanne vom Onset bis zum lokalen Peak innerhalb eines 80ms-Fensters, um festzustellen, ob Kompressoren den "Punch" unterdrücken.
*   **Crest-Faktor**: Das Verhältnis zwischen Peak- und RMS-Pegel in dB:
    $$C = 20 \log_{10} \left( \frac{|x_{peak}|}{x_{rms}} \right)$$

### 3. Loudness-Standards
Die Messung der wahrgenommenen Lautheit erfolgt streng nach **ITU-R BS.1770-4 / EBU R128**:
*   **Integrated Loudness (LUFS)**: Ein gewichteter Mittelwert über das gesamte Zeitfenster (K-Weighting).
*   **True Peak (dBTP)**: Erkennung von Inter-Sample-Peaks durch 4-faches Oversampling (via FFmpeg/ebur128).
*   **Loudness Range (LRA)**: Statistische Verteilung der Lautheit zur Bestimmung der makroskopischen Dynamik eines Tracks.

---

## 📄 Lizenz
Dieses Projekt steht unter der **MIT-Lizenz** – fühle dich frei, es zu nutzen, zu modifizieren und für deine eigenen Projekte zu teilen.

*Entwickelt von Uwe Arthur Felchle*
