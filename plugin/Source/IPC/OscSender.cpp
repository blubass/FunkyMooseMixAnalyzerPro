#include "OscSender.h"

OscSender::OscSender()
{
    // Try to connect immediately on construction
    connect();
}

bool OscSender::connect(const juce::String& host, int port)
{
    connected = sender.connect(host, port);
    return connected;
}

void OscSender::disconnect()
{
    sender.disconnect();
    connected = false;
}

void OscSender::send(const fmma::AnalyzerMetrics& m,
                     const fmma::MixAssessment& a)
{
    if (! connected)
        return;

    // Build a single OSC message with all relevant fields.
    // Python side unpacks positionally — order must match plugin_bridge.py.
    juce::OSCMessage msg(kAddress);

    // --- Loudness ---
    msg.addFloat32(m.integratedLufs);
    msg.addFloat32(m.momentaryLufs);
    msg.addFloat32(m.shortTermLufs);
    msg.addFloat32(m.truePeakDb);
    msg.addFloat32(m.lraLu);
    msg.addFloat32(m.crestDb);

    // --- Stereo / Dynamics ---
    msg.addFloat32(m.correlation);
    msg.addFloat32(m.widthPct);
    msg.addFloat32(m.monoLossDb);

    // --- Clipping / Safety ---
    msg.addFloat32(m.clippedPercent);
    msg.addFloat32(m.worstTruePeakDb);

    // --- Spectral ---
    msg.addFloat32(m.spectralCentroidHz);
    msg.addFloat32(m.spectralRolloffHz);
    msg.addFloat32(m.resonanceFreqHz);
    msg.addFloat32(m.resonanceGainDb);

    // --- Band percents (6 bands) ---
    for (size_t i = 0; i < m.bandPercents.size(); ++i)
        msg.addFloat32(m.bandPercents[i]);

    // --- Score & verdict ---
    msg.addInt32(a.overallScore);
    msg.addString(a.verdictKey);
    msg.addString(a.verdictTitle);
    msg.addInt32(a.confidenceScore);

    // --- Analysis state ---
    msg.addFloat32(m.analysisSeconds);
    msg.addInt32(m.fullPassCompleted ? 1 : 0);
    msg.addFloat32(m.worstClippedPercent);
    msg.addInt32(a.loudnessConfidenceScore);
    msg.addInt32(a.dynamicsConfidenceScore);
    msg.addInt32(a.stereoConfidenceScore);
    msg.addInt32(a.toneConfidenceScore);
    msg.addInt32(a.deliveryConfidenceScore);
    msg.addInt32(a.releaseGateScore);
    msg.addInt32(a.releaseReady ? 1 : 0);
    msg.addInt32(m.autoMasterEnabled ? 1 : 0);
    msg.addFloat32(m.autoMasterStrength);
    msg.addFloat32(m.autoMasterTargetLufs);
    msg.addFloat32(m.autoMasterCeilingDbTp);
    msg.addFloat32(m.autoMasterGainDb);
    msg.addFloat32(m.autoMasterLowShelfDb);
    msg.addFloat32(m.autoMasterPresenceDb);
    msg.addFloat32(m.autoMasterAirShelfDb);
    msg.addFloat32(m.autoMasterWidthPercent);
    msg.addFloat32(m.autoMasterLimiterReductionDb);
    msg.addFloat32(m.autoMasterGlueReductionDb);

    sender.send(msg);
}
