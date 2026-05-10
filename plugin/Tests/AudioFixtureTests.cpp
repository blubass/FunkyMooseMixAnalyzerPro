#include "PluginProcessor.h"

#include <juce_audio_formats/juce_audio_formats.h>

#include <cmath>
#include <iostream>

namespace
{
constexpr double sampleRate = 48000.0;
constexpr int blockSize = 512;
constexpr float pi = juce::MathConstants<float>::pi;

int failures = 0;

void expect(bool condition, const char* message)
{
    if (condition)
        return;

    std::cerr << "FAIL: " << message << '\n';
    ++failures;
}

void expectNear(float actual, float expected, float tolerance, const char* message)
{
    if (std::isfinite(actual) && std::abs(actual - expected) <= tolerance)
        return;

    std::cerr << "FAIL: " << message << " expected " << expected
              << " +/- " << tolerance << ", got " << actual << '\n';
    ++failures;
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

fmma::AnalyzerMetrics analyseFixture(const juce::AudioBuffer<float>& source, bool asFullPass = false)
{
    FunkyMooseMixAnalyzerAudioProcessor processor;
    processor.setPlayConfigDetails(2, 2, sampleRate, blockSize);
    processor.prepareToPlay(sampleRate, blockSize);

    if (asFullPass)
        processor.requestFullPassStart();

    juce::MidiBuffer midi;
    auto offset = 0;

    while (offset < source.getNumSamples())
    {
        const auto samplesThisBlock = juce::jmin(blockSize, source.getNumSamples() - offset);
        juce::AudioBuffer<float> block { 2, samplesThisBlock };
        for (auto channel = 0; channel < 2; ++channel)
            block.copyFrom(channel, 0, source, juce::jmin(channel, source.getNumChannels() - 1), offset, samplesThisBlock);

        processor.processBlock(block, midi);
        offset += samplesThisBlock;
    }

    if (asFullPass)
    {
        processor.requestFullPassFinish();
        juce::AudioBuffer<float> flushBlock { 2, blockSize };
        flushBlock.clear();
        processor.processBlock(flushBlock, midi);
    }

    return processor.getMetrics();
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
}

int main()
{
    sineFixtureMeasuresLevelAndStereo();
    antiPhaseFixtureFlagsMonoLoss();
    clippedFixtureFlagsPeaks();
    spectralFixtureFindsExpectedBand();

    if (failures == 0)
    {
        std::cout << "AudioFixtureTests OK\n";
        return 0;
    }

    std::cerr << failures << " AudioFixtureTests failed\n";
    return 1;
}
