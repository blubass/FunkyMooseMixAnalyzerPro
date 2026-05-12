import threading
import time
from pythonosc.dispatcher import Dispatcher
from pythonosc.osc_server import BlockingOSCServer

class PluginBridge:
    def __init__(self, host="127.0.0.1", port=9001):
        self.host = host
        self.port = port
        self.metrics = None
        self.last_update = 0
        self.lock = threading.Lock()
        self._stop_event = threading.Event()
        self._server_thread = None

    def start(self):
        dispatcher = Dispatcher()
        dispatcher.map("/fmma/metrics", self._handle_metrics)

        self._server = BlockingOSCServer((self.host, self.port), dispatcher)
        self._server_thread = threading.Thread(target=self._server.serve_forever, daemon=True)
        self._server_thread.start()
        print(f"OSC Bridge listening on {self.host}:{self.port}")

    def stop(self):
        if self._server:
            self._server.shutdown()
        if self._server_thread:
            self._server_thread.join()

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
        # msg.addFloat32(m.truePeakDb);     // 10 (worst TP)
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

        if len(args) < 27:
            return

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
                    "worstTruePeak": args[10]
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
                    "confidence": args[24]
                },
                "state": {
                    "duration": args[25],
                    "completed": bool(args[26])
                }
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
