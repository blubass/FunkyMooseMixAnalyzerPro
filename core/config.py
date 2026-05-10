# core/config.py
import os

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

BAND_ALIASES = {
    "Bässe": {"Bässe", "Sub", "Bass"},
    "Mitten": {"Mitten", "Mids", "Presence"},
    "Höhen": {"Höhen", "Air"},
}

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
