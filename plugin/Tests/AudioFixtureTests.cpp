#include "PluginProcessor.h"

#include <juce_audio_formats/juce_audio_formats.h>

#include <cmath>
#include <iostream>
#include <memory>

namespace
{
constexpr double sampleRate = 48000.0;
constexpr int blockSize = 512;
constexpr float pi = juce::MathConstants<float>::pi;

int failures = 0;

void expect(bool condition, const juce::String& message)
{
    if (condition)
        return;

    std::cerr << "FAIL: " << message.toRawUTF8() << '\n';
    ++failures;
}

void expectNear(float actual, float expected, float tolerance, const juce::String& message)
{
    if (std::isfinite(actual) && std::abs(actual - expected) <= tolerance)
        return;

    std::cerr << "FAIL: " << message.toRawUTF8() << " expected " << expected
              << " +/- " << tolerance << ", got " << actual << '\n';
    ++failures;
}

fmma::AnalyzerMetrics makeStoredMetrics(float seed)
{
    fmma::AnalyzerMetrics metrics;
    metrics.momentaryLufs = -12.0f - seed;
    metrics.shortTermLufs = -13.0f - seed;
    metrics.integratedLufs = -14.0f - seed;
    metrics.truePeakDb = -1.5f + (seed * 0.1f);
    metrics.truePeakHoldDb = -1.3f + (seed * 0.1f);
    metrics.worstTruePeakDb = -1.1f + (seed * 0.1f);
    metrics.lraLu = 5.0f + seed;
    metrics.rmsDb = -17.0f + seed;
    metrics.peakDb = -6.0f + seed;
    metrics.crestDb = 11.0f + seed;
    metrics.monoLossDb = -0.5f - seed;
    metrics.correlation = 0.85f - (seed * 0.05f);
    metrics.worstCorrelation = 0.65f - (seed * 0.05f);
    metrics.widthPct = 42.0f + seed;
    metrics.msRatioDb = -7.0f + seed;
    metrics.clippedPercent = 0.01f * seed;
    metrics.worstClippedPercent = 0.02f * seed;
    metrics.spectralCentroidHz = 1800.0f + (seed * 100.0f);
    metrics.spectralRolloffHz = 9000.0f + (seed * 200.0f);
    metrics.resonanceFreqHz = 3200.0f + (seed * 100.0f);
    metrics.resonanceGainDb = 7.0f + seed;
    metrics.worstResonanceFreqHz = 3600.0f + (seed * 100.0f);
    metrics.worstResonanceGainDb = 9.0f + seed;
    metrics.worstLowMidPercent = 18.0f + seed;
    metrics.analysisSeconds = 64.0f + seed;
    metrics.fullPassSeconds = 60.0f + seed;
    metrics.fullPassCompleted = true;
    metrics.analysisFrozen = true;

    for (auto band = 0; band < fmma::bandCount; ++band)
    {
        const auto index = static_cast<size_t>(band);
        metrics.bandPercents[index] = (static_cast<float>(band) * 3.0f) + seed;
        metrics.bandCorrelations[index] = 0.9f - (static_cast<float>(band) * 0.08f) - (seed * 0.01f);
        metrics.bandSideRatiosDb[index] = -18.0f + static_cast<float>(band) + seed;
    }

    return metrics;
}

void expectStoredMetricMatches(const fmma::AnalyzerMetrics& actual,
                               const fmma::AnalyzerMetrics& expected,
                               const char* label)
{
    const auto prefix = juce::String(label) + " should round-trip ";
    expectNear(actual.integratedLufs, expected.integratedLufs, 0.001f, prefix + "integrated LUFS");
    expectNear(actual.truePeakDb, expected.truePeakDb, 0.001f, prefix + "true peak");
    expectNear(actual.worstTruePeakDb, expected.worstTruePeakDb, 0.001f, prefix + "worst true peak");
    expectNear(actual.lraLu, expected.lraLu, 0.001f, prefix + "LRA");
    expectNear(actual.correlation, expected.correlation, 0.001f, prefix + "correlation");
    expectNear(actual.widthPct, expected.widthPct, 0.001f, prefix + "width");
    expectNear(actual.worstClippedPercent, expected.worstClippedPercent, 0.001f, prefix + "worst clipping");
    expectNear(actual.spectralCentroidHz, expected.spectralCentroidHz, 0.001f, prefix + "centroid");
    expectNear(actual.worstResonanceGainDb, expected.worstResonanceGainDb, 0.001f, prefix + "worst resonance");
    expectNear(actual.fullPassSeconds, expected.fullPassSeconds, 0.001f, prefix + "full-pass seconds");
    expect(actual.fullPassCompleted == expected.fullPassCompleted, prefix + "full-pass flag");
    expect(actual.analysisFrozen == expected.analysisFrozen, prefix + "frozen flag");

    for (auto band = 0; band < fmma::bandCount; ++band)
    {
        const auto index = static_cast<size_t>(band);
        const auto bandLabel = prefix + "band " + juce::String(band) + " ";
        expectNear(actual.bandPercents[index], expected.bandPercents[index], 0.001f, bandLabel + "percent");
        expectNear(actual.bandCorrelations[index], expected.bandCorrelations[index], 0.001f, bandLabel + "correlation");
        expectNear(actual.bandSideRatiosDb[index], expected.bandSideRatiosDb[index], 0.001f, bandLabel + "side ratio");
    }
}

float sineSample(int sample, float frequency, float peakGain, float phaseRadians = 0.0f)
{
    const auto phase = (2.0f * pi * frequency * static_cast<float>(sample) / static_cast<float>(sampleRate)) + phaseRadians;
    return peakGain * std::sin(phase);
}

juce::AudioBuffer<float> makeBuffer(float seconds)
{
    return juce::AudioBuffer<float> { 2, static_cast<int>(std::round(seconds * sampleRate)) };
}

juce::AudioBuffer<float> makeStereoSine(float seconds, float frequency, float peakGain)
{
    auto buffer = makeBuffer(seconds);
    for (auto sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        const auto value = sineSample(sample, frequency, peakGain);
        buffer.setSample(0, sample, value);
        buffer.setSample(1, sample, value);
    }

    return buffer;
}

juce::AudioBuffer<float> makeAntiPhaseSine(float seconds, float frequency, float peakGain)
{
    auto buffer = makeBuffer(seconds);
    for (auto sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        const auto value = sineSample(sample, frequency, peakGain);
        buffer.setSample(0, sample, value);
        buffer.setSample(1, sample, -value);
    }

    return buffer;
}

juce::AudioBuffer<float> makeClippedSine(float seconds, float frequency, float drive)
{
    auto buffer = makeBuffer(seconds);
    for (auto sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        const auto unclipped = sineSample(sample, frequency, drive);
        const auto clipped = juce::jlimit(-1.0f, 1.0f, unclipped);
        buffer.setSample(0, sample, clipped);
        buffer.setSample(1, sample, clipped);
    }

    return buffer;
}

juce::File fixtureDirectory()
{
    auto dir = juce::File::getCurrentWorkingDirectory()
        .getChildFile("FunkyMooseMixAnalyzerAudioFixtureTests-fixtures");
    expect(dir.createDirectory(), "fixture directory should be created");
    return dir;
}

juce::File writeWavFixture(const juce::AudioBuffer<float>& buffer, const juce::String& name)
{
    auto file = fixtureDirectory().getChildFile(name + ".wav");
    file.deleteFile();

    juce::WavAudioFormat format;
    std::unique_ptr<juce::OutputStream> stream { file.createOutputStream().release() };
    expect(stream != nullptr, "fixture WAV output stream should be created");
    if (stream == nullptr)
        return file;

    const auto writerOptions = juce::AudioFormatWriterOptions {}
        .withSampleRate(sampleRate)
        .withNumChannels(buffer.getNumChannels())
        .withBitsPerSample(24);
    auto writer = format.createWriterFor(stream, writerOptions);

    expect(writer != nullptr, "fixture WAV writer should be created");
    if (writer == nullptr)
        return file;

    expect(writer->writeFromAudioSampleBuffer(buffer, 0, buffer.getNumSamples()),
           "fixture WAV should be written");
    return file;
}

juce::AudioBuffer<float> readWavFixture(const juce::File& file)
{
    juce::WavAudioFormat format;
    auto stream = file.createInputStream();
    expect(stream != nullptr, "fixture WAV input stream should be created");
    if (stream == nullptr)
        return {};

    std::unique_ptr<juce::AudioFormatReader> reader { format.createReaderFor(stream.release(), true) };
    expect(reader != nullptr, "fixture WAV reader should be created");

    if (reader == nullptr)
        return {};

    juce::AudioBuffer<float> buffer {
        static_cast<int>(reader->numChannels),
        static_cast<int>(reader->lengthInSamples)
    };
    expect(reader->read(&buffer, 0, buffer.getNumSamples(), 0, true, true),
           "fixture WAV should be read");
    return buffer;
}

std::unique_ptr<FunkyMooseMixAnalyzerAudioProcessor> makePreparedProcessor()
{
    auto processor = std::make_unique<FunkyMooseMixAnalyzerAudioProcessor>();
    processor->setPlayConfigDetails(2, 2, sampleRate, blockSize);
    processor->prepareToPlay(sampleRate, blockSize);
    return processor;
}

fmma::AnalyzerMetrics analyseFixture(const juce::AudioBuffer<float>& source, bool asFullPass = false)
{
    auto processor = makePreparedProcessor();

    if (asFullPass)
        processor->requestFullPassStart();

    juce::MidiBuffer midi;
    auto offset = 0;

    while (offset < source.getNumSamples())
    {
        const auto samplesThisBlock = juce::jmin(blockSize, source.getNumSamples() - offset);
        juce::AudioBuffer<float> block { 2, samplesThisBlock };
        for (auto channel = 0; channel < 2; ++channel)
            block.copyFrom(channel, 0, source, juce::jmin(channel, source.getNumChannels() - 1), offset, samplesThisBlock);

        processor->processBlock(block, midi);
        offset += samplesThisBlock;
    }

    if (asFullPass)
    {
        processor->requestFullPassFinish();
        juce::AudioBuffer<float> flushBlock { 2, blockSize };
        flushBlock.clear();
        processor->processBlock(flushBlock, midi);
    }

    return processor->getMetrics();
}

fmma::AnalyzerMetrics analyseWavRoundTrip(const juce::AudioBuffer<float>& source,
                                          const juce::String& name,
                                          bool asFullPass = false)
{
    const auto file = writeWavFixture(source, name);
    expect(file.existsAsFile(), "fixture WAV should exist on disk");
    if (! file.existsAsFile())
        return {};

    const auto loaded = readWavFixture(file);
    expect(loaded.getNumSamples() == source.getNumSamples(), "fixture WAV should preserve sample length");
    expect(loaded.getNumChannels() == source.getNumChannels(), "fixture WAV should preserve channel count");
    return analyseFixture(loaded, asFullPass);
}

void sineFixtureMeasuresLevelAndStereo()
{
    const auto peakGain = juce::Decibels::decibelsToGain(-12.0f);
    const auto metrics = analyseWavRoundTrip(makeStereoSine(8.0f, 1000.0f, peakGain), "stereo_1k_minus12", true);

    expect(metrics.fullPassCompleted, "sine fixture full pass should complete");
    expectNear(metrics.analysisSeconds, 8.0f, 0.02f, "sine fixture analysis time should match fixture duration");
    expectNear(metrics.peakDb, -12.0f, 0.6f, "sine fixture sample peak should match known level");
    expectNear(metrics.rmsDb, -15.0f, 0.8f, "sine fixture RMS should match known sine RMS");
    expectNear(metrics.crestDb, 3.0f, 0.8f, "sine fixture crest should match known sine crest");
    expect(metrics.truePeakHoldDb > -13.0f && metrics.truePeakHoldDb < -11.0f,
           "sine fixture true peak hold should track known peak");
    expect(metrics.correlation > 0.98f, "dual-mono sine should be highly correlated");
    expect(metrics.widthPct < 2.0f, "dual-mono sine should have near-zero width");
    expect(metrics.clippedPercent < 0.001f, "clean sine should not report clipping");
    expect(metrics.integratedLufs > -18.0f && metrics.integratedLufs < -9.0f,
           "sine fixture integrated LUFS should be finite and plausible");
}

void antiPhaseFixtureFlagsMonoLoss()
{
    const auto metrics = analyseWavRoundTrip(makeAntiPhaseSine(6.0f, 250.0f, 0.35f), "antiphase_250hz", true);

    expect(metrics.fullPassCompleted, "anti-phase fixture full pass should complete");
    expect(metrics.correlation < -0.95f, "anti-phase fixture should show strong negative correlation");
    expect(metrics.worstCorrelation < -0.95f, "anti-phase fixture should preserve worst correlation");
    expect(metrics.monoLossDb < -20.0f, "anti-phase fixture should show severe mono cancellation");
    expect(metrics.widthPct > 1000.0f, "anti-phase fixture should show extreme side-to-mid width");
    expect(metrics.bandCorrelations[1] < -0.7f, "anti-phase 250 Hz fixture should flag low-band phase");
}

void clippedFixtureFlagsPeaks()
{
    const auto metrics = analyseWavRoundTrip(makeClippedSine(4.0f, 1000.0f, 1.45f), "clipped_1k", true);

    expect(metrics.fullPassCompleted, "clipped fixture full pass should complete");
    expect(metrics.peakDb > -0.5f, "clipped fixture sample peak should sit near 0 dBFS");
    expect(metrics.truePeakHoldDb > -0.5f, "clipped fixture true peak hold should sit near 0 dBTP");
    expect(metrics.worstTruePeakDb > -0.5f, "clipped fixture worst true peak should be retained");
    expect(metrics.clippedPercent > 10.0f, "clipped fixture should report substantial clipping");
    expect(metrics.worstClippedPercent > 10.0f, "clipped fixture should retain worst clipping");
}

void spectralFixtureFindsExpectedBand()
{
    const auto lowMetrics = analyseWavRoundTrip(makeStereoSine(5.0f, 100.0f, 0.35f), "low_band_100hz");
    const auto presenceMetrics = analyseWavRoundTrip(makeStereoSine(5.0f, 4000.0f, 0.35f), "presence_band_4khz");

    expect(lowMetrics.bandPercents[1] > 60.0f, "100 Hz fixture should dominate the bass band");
    expect(presenceMetrics.bandPercents[4] > 60.0f, "4 kHz fixture should dominate the presence band");
    expect(presenceMetrics.spectralCentroidHz > lowMetrics.spectralCentroidHz + 1500.0f,
           "presence fixture should have a much higher spectral centroid than low fixture");
}

void storedAnalysisStateRoundTrips()
{
    auto source = makePreparedProcessor();

    const auto reference = makeStoredMetrics(1.0f);
    const auto snapshotA = makeStoredMetrics(2.0f);
    const auto snapshotB = makeStoredMetrics(3.0f);

    source->storeReferenceMetrics(reference);
    source->storeSnapshotA(snapshotA);
    source->storeSnapshotB(snapshotB);

    juce::MemoryBlock state;
    source->getStateInformation(state);
    expect(state.getSize() > 0, "processor state should serialise stored analysis data");

    auto restored = makePreparedProcessor();
    restored->setStateInformation(state.getData(), static_cast<int>(state.getSize()));

    fmma::AnalyzerMetrics restoredReference;
    fmma::AnalyzerMetrics restoredSnapshotA;
    fmma::AnalyzerMetrics restoredSnapshotB;
    expect(restored->getStoredReferenceMetrics(restoredReference), "reference metrics should restore from state");
    expect(restored->getStoredSnapshotA(restoredSnapshotA), "snapshot A should restore from state");
    expect(restored->getStoredSnapshotB(restoredSnapshotB), "snapshot B should restore from state");

    expectStoredMetricMatches(restoredReference, reference, "reference");
    expectStoredMetricMatches(restoredSnapshotA, snapshotA, "snapshot A");
    expectStoredMetricMatches(restoredSnapshotB, snapshotB, "snapshot B");

    restored->clearStoredReferenceMetrics();
    restored->clearStoredSnapshots();
    juce::MemoryBlock clearedState;
    restored->getStateInformation(clearedState);

    auto cleared = makePreparedProcessor();
    cleared->setStateInformation(clearedState.getData(), static_cast<int>(clearedState.getSize()));
    expect(! cleared->getStoredReferenceMetrics(restoredReference), "cleared reference metrics should stay cleared after state load");
    expect(! cleared->getStoredSnapshotA(restoredSnapshotA), "cleared snapshot A should stay cleared after state load");
    expect(! cleared->getStoredSnapshotB(restoredSnapshotB), "cleared snapshot B should stay cleared after state load");
}

void phaseCorrelationFixtureTestsPhase()
{
    auto processor = makePreparedProcessor();

    juce::AudioBuffer<float> buffer(2, blockSize);
    buffer.clear();

    // Generate in-phase sine waves
    for (int i = 0; i < blockSize; ++i)
    {
        const auto sample = static_cast<float>(std::sin(2.0f * pi * 440.0f * i / sampleRate) * 0.1f);
        buffer.setSample(0, i, sample);
        buffer.setSample(1, i, sample);
    }

    // Process multiple blocks to fill FFT buffer
    juce::MidiBuffer midiBuffer;
    for (int block = 0; block < 10; ++block)
    {
        processor->processBlock(buffer, midiBuffer);
    }

    auto metrics = processor->getMetrics();
    expectNear(metrics.phaseCorrelation, 1.0f, 0.1f, "In-phase signals should have high phase correlation");

    // Generate out-of-phase
    for (int i = 0; i < blockSize; ++i)
    {
        const auto sample = static_cast<float>(std::sin(2.0f * pi * 440.0f * i / sampleRate) * 0.1f);
        buffer.setSample(0, i, sample);
        buffer.setSample(1, i, -sample);
    }

    juce::MidiBuffer midiBuffer2;
    for (int block = 0; block < 10; ++block)
    {
        processor->processBlock(buffer, midiBuffer2);
    }

    metrics = processor->getMetrics();
    expectNear(metrics.phaseCorrelation, -1.0f, 0.1f, "Out-of-phase signals should have negative phase correlation");
}

void runFixtureTest(const char* name, void (*test)())
{
    std::cerr << "RUN " << name << std::endl;
    test();
    std::cerr << "DONE " << name << std::endl;
}
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    runFixtureTest("sineFixtureMeasuresLevelAndStereo", sineFixtureMeasuresLevelAndStereo);
    runFixtureTest("antiPhaseFixtureFlagsMonoLoss", antiPhaseFixtureFlagsMonoLoss);
    runFixtureTest("clippedFixtureFlagsPeaks", clippedFixtureFlagsPeaks);
    runFixtureTest("spectralFixtureFindsExpectedBand", spectralFixtureFindsExpectedBand);
    runFixtureTest("storedAnalysisStateRoundTrips", storedAnalysisStateRoundTrips);
    runFixtureTest("phaseCorrelationFixtureTestsPhase", phaseCorrelationFixtureTestsPhase);
    if (failures == 0)
    {
        std::cout << "AudioFixtureTests OK\n";
        return 0;
    }

    std::cerr << failures << " AudioFixtureTests failed\n";
    return 1;
}
