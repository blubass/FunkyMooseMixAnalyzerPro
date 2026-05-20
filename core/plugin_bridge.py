import threading
import time
from pythonosc.dispatcher import Dispatcher

try:
    from pythonosc.osc_server import ThreadingOSCUDPServer as OscServer
except ImportError:  # Older python-osc releases used this name.
    from pythonosc.osc_server import BlockingOSCUDPServer as OscServer

class PluginBridge:
    def __init__(self, host="127.0.0.1", port=9001):
        self.host = host
        self.port = port
        self.metrics = None
        self.last_update = 0
        self.lock = threading.Lock()
        self._server = None
        self._server_thread = None

    def start(self):
        if self._server_thread and self._server_thread.is_alive():
            return True

        dispatcher = Dispatcher()
        dispatcher.map("/fmma/metrics", self._handle_metrics)

        try:
            self._server = OscServer((self.host, self.port), dispatcher)
        except OSError as exc:
            print(f"OSC Bridge disabled on {self.host}:{self.port}: {exc}")
            self._server = None
            return False

        self._server_thread = threading.Thread(target=self._server.serve_forever, daemon=True)
        self._server_thread.start()
        print(f"OSC Bridge listening on {self.host}:{self.port}")
        return True

    def stop(self):
        if self._server:
            self._server.shutdown()
            self._server.server_close()
            self._server = None
        if self._server_thread and self._server_thread.is_alive():
            self._server_thread.join(timeout=1.0)
        self._server_thread = None

    def _handle_metrics(self, address, *args):
        # Order must match OscSender.cpp
        # msg.addFloat32(m.integratedLufs); // 0
        # msg.addFloat32(m.momentaryLufs);  // 1
        # msg.addFloat32(m.shortTermLufs);  // 2
        # msg.addFloat32(m.truePeakDb);     // 3
        # msg.addFloat32(m.lraLu);          // 4
        # msg.addFloat32(m.crestDb);        // 5
        # msg.addFloat32(m.correlation);    // 6
        # msg.addFloat32(m.widthPct);       // 7
        # msg.addFloat32(m.monoLossDb);     // 8
        # msg.addFloat32(m.clippedPercent); // 9
        # msg.addFloat32(m.worstTruePeakDb); // 10
        # msg.addFloat32(m.spectralCentroidHz); // 11
        # msg.addFloat32(m.spectralRolloffHz);  // 12
        # msg.addFloat32(m.resonanceFreqHz);     // 13
        # msg.addFloat32(m.resonanceGainDb);     // 14
        # bandPercents (6 fields) -> 15, 16, 17, 18, 19, 20
        # overallScore (int) -> 21
        # verdictKey (str) -> 22
        # verdictTitle (str) -> 23
        # confidenceScore (int) -> 24
        # analysisSeconds (float) -> 25
        # fullPassCompleted (int) -> 26
        # worstClippedPercent (float) -> 27
        # confidence domains (5 ints) -> 28, 29, 30, 31, 32
        # release gate score / ready -> 33, 34
        # auto master enabled, strength, target, ceiling, gain, low, presence, air, width, limiter GR -> 35..44
        # auto master glue GR -> 45
        # auto master projected LUFS, projected TP, loudness-match gain -> 46, 47, 48
        # auto master LUFS delta, TP margin, release score -> 49, 50, 51
        # auto master A/B loudness delta, matched TP, TP delta, dynamics delta, score -> 52..56
        # auto master audition enabled, gain, loudness delta, true peak -> 57..60

        if len(args) < 27:
            return

        worst_clipping = args[27] if len(args) > 27 else args[9]
        confidence_domains = None
        if len(args) >= 33:
            confidence_domains = {
                "loudness": int(args[28]),
                "dynamics": int(args[29]),
                "stereo": int(args[30]),
                "tone": int(args[31]),
                "delivery": int(args[32]),
            }
        release_gate = None
        if len(args) >= 35:
            release_gate = {
                "score": int(args[33]),
                "ready": bool(args[34]),
            }
        auto_master = None
        if len(args) >= 45:
            auto_master = {
                "enabled": bool(args[35]),
                "strengthPercent": float(args[36]),
                "targetLufs": float(args[37]),
                "ceilingDbTp": float(args[38]),
                "gainDb": float(args[39]),
                "lowShelfDb": float(args[40]),
                "presenceDb": float(args[41]),
                "airShelfDb": float(args[42]),
                "widthPercent": float(args[43]),
                "limiterReductionDb": float(args[44]),
                "glueReductionDb": float(args[45]) if len(args) >= 46 else 0.0,
                "projectedLufs": float(args[46]) if len(args) >= 47 else None,
                "projectedTruePeakDbTp": float(args[47]) if len(args) >= 48 else None,
                "loudnessMatchGainDb": float(args[48]) if len(args) >= 49 else 0.0,
                "lufsDeltaDb": float(args[49]) if len(args) >= 50 else 0.0,
                "truePeakMarginDb": float(args[50]) if len(args) >= 51 else 0.0,
                "releaseScore": float(args[51]) if len(args) >= 52 else 0.0,
                "abLoudnessDeltaDb": float(args[52]) if len(args) >= 53 else 0.0,
                "abTruePeakDbTp": float(args[53]) if len(args) >= 54 else None,
                "abTruePeakDeltaDb": float(args[54]) if len(args) >= 55 else 0.0,
                "abDynamicsDeltaDb": float(args[55]) if len(args) >= 56 else 0.0,
                "abScore": float(args[56]) if len(args) >= 57 else 0.0,
                "auditionMatch": bool(args[57]) if len(args) >= 58 else False,
                "auditionGainDb": float(args[58]) if len(args) >= 59 else 0.0,
                "auditionLoudnessDeltaDb": float(args[59]) if len(args) >= 60 else 0.0,
                "auditionTruePeakDbTp": float(args[60]) if len(args) >= 61 else None,
            }

        with self.lock:
            self.metrics = {
                "loudness": {
                    "integrated": args[0],
                    "momentary": args[1],
                    "shortTerm": args[2],
                    "truePeak": args[3],
                    "lra": args[4],
                    "crest": args[5]
                },
                "stereo": {
                    "correlation": args[6],
                    "width": args[7],
                    "monoLoss": args[8]
                },
                "safety": {
                    "clipping": args[9],
                    "worstTruePeak": args[10],
                    "worstClipping": worst_clipping
                },
                "spectral": {
                    "centroid": args[11],
                    "rolloff": args[12],
                    "resonanceFreq": args[13],
                    "resonanceGain": args[14],
                    "bands": list(args[15:21])
                },
                "assessment": {
                    "score": args[21],
                    "verdictKey": args[22],
                    "verdictTitle": args[23],
                    "confidence": args[24],
                    "confidenceDomains": confidence_domains,
                    "releaseGate": release_gate
                },
                "state": {
                    "duration": args[25],
                    "completed": bool(args[26])
                },
                "autoMaster": auto_master
            }
            self.last_update = time.time()

    def get_latest_metrics(self):
        with self.lock:
            if not self.metrics:
                return None
            
            # Check for timeout (2 seconds)
            if time.time() - self.last_update > 2.0:
                return None
                
            return self.metrics

# Global singleton
bridge = PluginBridge()
