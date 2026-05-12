#pragma once

#include <JuceHeader.h>
#include "../AnalysisModel.h"

/**
 * OscSender — fires live AnalyzerMetrics as OSC bundles to the
 * Funky Moose Python companion app on 127.0.0.1:9001.
 *
 * Usage:
 *   Call send(metrics) from the PluginProcessor timer / processBlock.
 *   The sender is fire-and-forget UDP — no handshake required.
 */
class OscSender
{
public:
    static constexpr int kDefaultPort = 9001;
    static constexpr const char* kAddress = "/fmma/metrics";

    OscSender();
    ~OscSender() = default;

    /** Enable / disable sending. Disabled by default until connect() succeeds. */
    bool connect(const juce::String& host = "127.0.0.1", int port = kDefaultPort);
    void disconnect();
    bool isConnected() const noexcept { return connected; }

    /** Send all relevant fields of metrics as a single OSC message. */
    void send(const fmma::AnalyzerMetrics& metrics,
              const fmma::MixAssessment& assessment);

private:
    juce::OSCSender sender;
    bool connected = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OscSender)
};
