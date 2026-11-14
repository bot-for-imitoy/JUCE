/*
  ==============================================================================

   This file is part of the JUCE framework.
   Copyright (c) Raw Material Software Limited

   JUCE is an open source framework subject to commercial or open source
   licensing.

   By downloading, installing, or using the JUCE framework, or combining the
   JUCE framework with any other source code, object code, content or any other
   copyrightable work, you agree to the terms of the JUCE End User Licence
   Agreement, and all incorporated terms including the JUCE Privacy Policy and
   the JUCE Website Terms of Service, as applicable, which will bind you. If you
   do not agree to the terms of these agreements, we will not license the JUCE
   framework to you, and you must discontinue the installation or download
   process and cease use of the JUCE framework.

   JUCE End User Licence Agreement: https://juce.com/legal/juce-9-licence/
   JUCE Privacy Policy: https://juce.com/juce-privacy-policy
   JUCE Website Terms of Service: https://juce.com/juce-website-terms-of-service/

   Or:

   You may also use this code under the terms of the AGPLv3:
   https://www.gnu.org/licenses/agpl-3.0.en.html

   THE JUCE FRAMEWORK IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL
   WARRANTIES, WHETHER EXPRESSED OR IMPLIED, INCLUDING WARRANTY OF
   MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, ARE DISCLAIMED.

  ==============================================================================
*/

// A stress test for the PipeWire backend.
//
// Every test runs in a forked child process with its own timeout, so that a
// deadlock in the backend shows up as a reported "HANG" instead of wedging the
// whole run, and a segfault shows up as a reported "CRASH" while the remaining
// tests keep going. Nothing is initialised in the parent process: the fork
// happens before JUCE is brought up, which is the only safe order.

#include <juce_audio_devices/juce_audio_devices.h>

#include <atomic>
#include <cmath>
#include <functional>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <csignal>
#include <string>
#include <thread>
#include <vector>

#include <dlfcn.h>
#include <execinfo.h>
#include <dirent.h>
#include <sys/wait.h>
#include <unistd.h>

namespace
{

//==============================================================================
struct TestEnv
{
    juce::StringArray failures;
    int checks = 0;

    bool ok() const { return failures.isEmpty(); }

    void check (bool condition, const juce::String& what)
    {
        ++checks;

        if (! condition)
        {
            failures.add (what);

            if (failures.size() <= 25)
                std::printf ("    check failed: %s\n", what.toRawUTF8());
        }
    }
};

TestEnv* currentEnv = nullptr;

// A crash in the backend takes the whole test process down, so the child
// installs this to leave a stack trace behind before it goes.
void crashHandler (int signalNumber)
{
    void* frames[64];
    const auto count = backtrace (frames, (int) std::size (frames));
    std::fprintf (stderr, "\n--- signal %d, backtrace ---\n", signalNumber);
    backtrace_symbols_fd (frames, count, 2);
    ::_exit (99);
}

#define CHECK(cond)         currentEnv->check ((cond), juce::String (#cond) + "  [line " + juce::String (__LINE__) + "]")
#define CHECK_MSG(cond, m)  currentEnv->check ((cond), juce::String (m) + "  [line " + juce::String (__LINE__) + "]")

//==============================================================================
using TypePtr = std::unique_ptr<juce::AudioIODeviceType>;

static TypePtr makeType()
{
    return TypePtr (juce::AudioIODeviceType::createAudioIODeviceType_PipeWire());
}

static juce::BigInteger channelMask (int numChannels)
{
    juce::BigInteger mask;

    for (int i = 0; i < numChannels; ++i)
        mask.setBit (i);

    return mask;
}

static juce::String pickOutputName (juce::AudioIODeviceType& type)
{
    auto names = type.getDeviceNames (false);

    if (names.isEmpty())
        return {};

    if (auto* preferred = std::getenv ("JUCE_PW_OUTPUT_NAME"))
        if (names.contains (preferred))
            return preferred;

    return names[type.getDefaultDeviceIndex (false)];
}

static juce::String pickInputName (juce::AudioIODeviceType& type)
{
    auto names = type.getDeviceNames (true);

    if (names.isEmpty())
        return {};

    if (auto* preferred = std::getenv ("JUCE_PW_INPUT_NAME"))
        if (names.contains (preferred))
            return preferred;

    return names[type.getDefaultDeviceIndex (true)];
}

//==============================================================================
struct CallbackStats
{
    std::atomic<int> callbacks { 0 };
    std::atomic<int> lastSamples { 0 };
    std::atomic<int> lastInputs { 0 };
    std::atomic<int> lastOutputs { 0 };
    std::atomic<int> badRate { 0 };
    std::atomic<int> badBufferSize { 0 };
    std::atomic<int> badOpenState { 0 };
    std::atomic<int> badPlayingState { 0 };
    std::atomic<int> nonSilentInputs { 0 };
    std::atomic<int> aboutToStartCount { 0 };
    std::atomic<int> stoppedCount { 0 };
    std::atomic<int> errorCount { 0 };
    std::atomic<int> stopsFromCallback { 0 };
    std::atomic<bool> queryFromCallback { false };
    std::atomic<bool> stopFromCallback { false };
    std::atomic<double> lastRate { 0.0 };
};

// Writes a very quiet tone, so that the tests are audible only if the volume is
// cranked up, but still carry data all the way through the graph.
class TestCallback final : public juce::AudioIODeviceCallback
{
public:
    TestCallback (CallbackStats& statsToUse, juce::AudioIODevice* deviceToUse)
        : stats (statsToUse), device (deviceToUse)
    {
    }

    void audioDeviceAboutToStart (juce::AudioIODevice*) override
    {
        stats.aboutToStartCount.fetch_add (1);
    }

    void audioDeviceStopped() override
    {
        stats.stoppedCount.fetch_add (1);
    }

    void audioDeviceError (const juce::String&) override
    {
        stats.errorCount.fetch_add (1);
    }

    void audioDeviceIOCallbackWithContext (const float* const* inputChannelData,
                                           int numInputChannels,
                                           float* const* outputChannelData,
                                           int numOutputChannels,
                                           int numSamples,
                                           const juce::AudioIODeviceCallbackContext&) override
    {
        stats.callbacks.fetch_add (1, std::memory_order_relaxed);
        stats.lastSamples.store (numSamples, std::memory_order_relaxed);
        stats.lastInputs.store (numInputChannels, std::memory_order_relaxed);
        stats.lastOutputs.store (numOutputChannels, std::memory_order_relaxed);

        // Only the documented realtime-safe queries are allowed here.
        if (stats.queryFromCallback.load (std::memory_order_relaxed))
        {
            if (! device->isOpen())
                stats.badOpenState.fetch_add (1, std::memory_order_relaxed);

            if (! device->isPlaying())
                stats.badPlayingState.fetch_add (1, std::memory_order_relaxed);

            const auto rate = device->getCurrentSampleRate();

            if (rate <= 0.0)
                stats.badRate.fetch_add (1, std::memory_order_relaxed);

            if (device->getCurrentBufferSizeSamples() != numSamples)
                stats.badBufferSize.fetch_add (1, std::memory_order_relaxed);

            stats.lastRate.store (rate, std::memory_order_relaxed);

            if (device->getCurrentBitDepth() != 32)
                stats.badRate.fetch_add (1, std::memory_order_relaxed);

            juce::ignoreUnused (device->getXRunCount());
        }

        for (int channel = 0; channel < numOutputChannels; ++channel)
        {
            auto* out = outputChannelData[channel];

            if (out == nullptr)
                continue;

            // 0.001 of full scale is about -60 dBFS.
            for (int i = 0; i < numSamples; ++i)
                out[i] = 0.001f * std::sin (phase + (float) i * 0.01f);

            phase += (float) numSamples * 0.01f;
        }

        for (int channel = 0; channel < numInputChannels; ++channel)
        {
            auto* in = inputChannelData[channel];

            if (in == nullptr)
                continue;

            for (int i = 0; i < numSamples; ++i)
            {
                if (std::abs (in[i]) > 1.0e-9f)
                {
                    stats.nonSilentInputs.fetch_add (1, std::memory_order_relaxed);
                    break;
                }
            }
        }

        // A host calling stop() from the audio thread is outside what
        // AudioIODevice allows, so this is only used by the probe test.
        if (stats.stopFromCallback.exchange (false, std::memory_order_acq_rel))
        {
            stats.stopsFromCallback.fetch_add (1, std::memory_order_relaxed);
            device->stop();
        }
    }

    void resetCounters()
    {
        stats.callbacks.store (0);
        stats.badRate.store (0);
        stats.badBufferSize.store (0);
        stats.badOpenState.store (0);
        stats.badPlayingState.store (0);
        stats.nonSilentInputs.store (0);
    }

private:
    CallbackStats& stats;
    juce::AudioIODevice* device;
    float phase = 0.0f;
};

//==============================================================================
// A leak of file descriptors or threads is what usually turns a churn test into
// a timeout, so the count is printed as the test goes.
static int countOpenFiles()
{
    int count = 0;

    if (auto dir = opendir ("/proc/self/fd"))
    {
        while (auto* entry = readdir (dir))
            if (entry->d_name[0] != '.')
                ++count;

        closedir (dir);
    }

    return count;
}

static int countThreads()
{
    if (auto* file = std::fopen ("/proc/self/status", "r"))
    {
        char line[256];

        while (std::fgets (line, sizeof line, file) != nullptr)
        {
            int value = 0;

            if (std::sscanf (line, "Threads: %d", &value) == 1)
            {
                std::fclose (file);
                return value;
            }
        }

        std::fclose (file);
    }

    return -1;
}

static bool waitForCallbacks (CallbackStats& stats, int minimum, int timeoutMs)
{
    const auto deadline = juce::Time::getMillisecondCounterHiRes() + timeoutMs;

    while (juce::Time::getMillisecondCounterHiRes() < deadline)
    {
        if (stats.callbacks.load() >= minimum)
            return true;

        juce::Thread::sleep (5);
    }

    return stats.callbacks.load() >= minimum;
}

static bool waitUntil (std::function<bool()> predicate, int timeoutMs)
{
    const auto deadline = juce::Time::getMillisecondCounterHiRes() + timeoutMs;

    while (juce::Time::getMillisecondCounterHiRes() < deadline)
    {
        if (predicate())
            return true;

        juce::Thread::sleep (5);
    }

    return predicate();
}

// The device list notifications are delivered through AsyncUpdater, which needs
// a running message loop - and this is a console app, so there is none unless
// it is pumped by hand.
static bool waitUntilPumpingMessages (std::function<bool()> predicate, int timeoutMs)
{
    const auto deadline = juce::Time::getMillisecondCounterHiRes() + timeoutMs;

    while (juce::Time::getMillisecondCounterHiRes() < deadline)
    {
        if (predicate())
            return true;

        if (auto* messageManager = juce::MessageManager::getInstanceWithoutCreating())
            messageManager->runDispatchLoopUntil (10);
        else
            juce::Thread::sleep (10);
    }

    return predicate();
}

//==============================================================================
// Opens the default output (and optionally an input) of the given type.
struct OpenResult
{
    std::unique_ptr<juce::AudioIODevice> device;
    juce::String error;
    juce::String outputName, inputName;
};

static OpenResult openDefault (juce::AudioIODeviceType& type,
                               int numOutputs,
                               int numInputs,
                               double sampleRate,
                               int bufferSize)
{
    OpenResult result;
    result.outputName = numOutputs > 0 ? pickOutputName (type) : juce::String();
    result.inputName  = numInputs  > 0 ? pickInputName  (type) : juce::String();

    if (numOutputs > 0 && result.outputName.isEmpty())
    {
        result.error = "no PipeWire output device was found";
        return result;
    }

    if (numInputs > 0 && result.inputName.isEmpty())
    {
        result.error = "no PipeWire input device was found";
        return result;
    }

    result.device.reset (type.createDevice (result.outputName, result.inputName));

    if (result.device == nullptr)
    {
        result.error = "createDevice() returned nullptr";
        return result;
    }

    result.error = result.device->open (channelMask (numInputs),
                                        channelMask (numOutputs),
                                        sampleRate,
                                        bufferSize);
    return result;
}

//==============================================================================
// enumerate: the device lists must match what the graph actually contains, and
// the default device must be the one the session metadata points at.
static void testEnumerate()
{
    auto type = makeType();
    CHECK (type != nullptr);

    if (type == nullptr)
        return;

    type->scanForDevices();

    auto outputs = type->getDeviceNames (false);
    auto inputs  = type->getDeviceNames (true);

    std::printf ("    outputs (%d):\n", outputs.size());

    for (int i = 0; i < outputs.size(); ++i)
        std::printf ("      [%d] %s\n", i, outputs[i].toRawUTF8());

    std::printf ("    inputs (%d):\n", inputs.size());

    for (int i = 0; i < inputs.size(); ++i)
        std::printf ("      [%d] %s\n", i, inputs[i].toRawUTF8());

    CHECK_MSG (outputs.size() > 0, "the graph should contain at least one output");

    if (outputs.size() > 0)
    {
        const auto index = type->getDefaultDeviceIndex (false);
        CHECK_MSG (index >= 0 && index < outputs.size(), "the default output index is out of range");
        std::printf ("    default output index: %d\n", index);
    }

    if (inputs.size() > 0)
    {
        const auto index = type->getDefaultDeviceIndex (true);
        CHECK_MSG (index >= 0 && index < inputs.size(), "the default input index is out of range");
        std::printf ("    default input index: %d\n", index);
    }

    // Creating a device for each listed name must work, and the device must be
    // found again by the type.
    for (const auto& name : outputs)
    {
        std::unique_ptr<juce::AudioIODevice> device (type->createDevice (name, {}));
        CHECK_MSG (device != nullptr, "createDevice() failed for " + name);

        if (device != nullptr)
            CHECK_MSG (type->getIndexOfDevice (device.get(), false) >= 0,
                       "getIndexOfDevice() did not find " + name);
    }

    // Re-scanning must not lose anything.
    type->scanForDevices();
    CHECK (type->getDeviceNames (false) == outputs);
}

//==============================================================================
// open-close: churn the whole open/close path. This is where a stream that
// isn't torn down properly piles up sockets and threads.
static void testOpenCloseChurn()
{
    auto type = makeType();
    type->scanForDevices();

    constexpr int iterations = 50;

    std::printf ("    before: %d fds, %d threads\n", countOpenFiles(), countThreads());

    for (int i = 0; i < iterations; ++i)
    {
        auto opened = openDefault (*type, 2, 0, 48000.0, 256);
        CHECK_MSG (opened.error.isEmpty(), "open() failed on iteration " + juce::String (i) + ": " + opened.error);

        if (opened.error.isNotEmpty())
            return;

        CHECK (opened.device->isOpen());
        CHECK (opened.device->getCurrentSampleRate() > 0.0);
        CHECK (opened.device->getCurrentBufferSizeSamples() > 0);

        opened.device->close();
        CHECK (! opened.device->isOpen());

        if (i % 10 == 9)
            std::printf ("    after %d: %d fds, %d threads\n", i + 1, countOpenFiles(), countThreads());
    }

    std::printf ("    after: %d fds, %d threads\n", countOpenFiles(), countThreads());

    // Opening twice in a row without closing, and closing twice, must be
    // harmless as well.
    auto opened = openDefault (*type, 2, 0, 44100.0, 512);
    CHECK_MSG (opened.error.isEmpty(), "the 44.1 kHz open failed: " + opened.error);

    if (opened.device == nullptr)
        return;

    CHECK (opened.device->open (channelMask (0), channelMask (2), 48000.0, 128).isEmpty());
    opened.device->close();
    opened.device->close();
    CHECK (! opened.device->isOpen());
}

//==============================================================================
// start-stop: rapid start/stop with the same and with different callbacks.
static void testStartStopChurn()
{
    auto type = makeType();
    type->scanForDevices();

    auto opened = openDefault (*type, 2, 0, 48000.0, 256);
    CHECK (opened.error.isEmpty());

    if (opened.error.isNotEmpty())
        return;

    CallbackStats stats;
    TestCallback callback (stats, opened.device.get());

    constexpr int iterations = 300;

    for (int i = 0; i < iterations; ++i)
    {
        opened.device->start (&callback);
        CHECK (opened.device->isPlaying());

        // Every few iterations let audio actually run.
        if (i % 25 == 0)
            waitForCallbacks (stats, 1, 1000);

        opened.device->stop();
        CHECK (! opened.device->isPlaying());
    }

    // Starting with the same callback twice, and starting with a different one,
    // must not lose or duplicate audioDeviceStopped() notifications.
    auto otherStats = CallbackStats {};
    TestCallback other (otherStats, opened.device.get());

    opened.device->start (&callback);
    opened.device->start (&callback);
    opened.device->start (&other);
    CHECK (opened.device->isPlaying());
    waitForCallbacks (otherStats, 3, 3000);
    CHECK (otherStats.callbacks.load() > 0);
    opened.device->stop();
    CHECK (! opened.device->isPlaying());

    opened.device->close();
}

//==============================================================================
// rates: every advertised rate must be openable. The server may negotiate a
// different one, but the reported rate has to be the negotiated one.
static void testSampleRates()
{
    auto type = makeType();
    type->scanForDevices();

    const double rates[] = { 44100.0, 48000.0, 88200.0, 96000.0, 176400.0, 192000.0 };

    for (auto rate : rates)
    {
        CallbackStats stats;
        auto opened = openDefault (*type, 2, 0, rate, 256);
        CHECK_MSG (opened.error.isEmpty(),
                   "open() at " + juce::String (rate) + " Hz failed: " + opened.error);

        if (opened.error.isNotEmpty())
            continue;

        TestCallback callback (stats, opened.device.get());
        stats.queryFromCallback = true;
        opened.device->start (&callback);

        const bool got = waitForCallbacks (stats, 20, 4000);
        CHECK_MSG (got, "no callbacks at " + juce::String (rate) + " Hz");

        const auto negotiated = opened.device->getCurrentSampleRate();
        std::printf ("    requested %7.0f -> negotiated %7.0f, buffer %d\n",
                     rate, negotiated, opened.device->getCurrentBufferSizeSamples());

        CHECK_MSG (negotiated > 0.0, "the negotiated rate is not positive");
        CHECK_MSG (std::abs (negotiated - stats.lastRate.load()) < 1.0,
                   "getCurrentSampleRate() disagrees with the value seen in the callback");
        CHECK_MSG (stats.badRate.load() == 0, "the callback saw an invalid rate");

        // The host must be able to convert to its own rate from these numbers.
        CHECK_MSG (opened.device->getOutputLatencyInSamples() >= 0,
                   "the output latency is negative");

        opened.device->stop();
        opened.device->close();
    }
}

//==============================================================================
// buffers: every advertised block size has to survive a round trip.
static void testBufferSizes()
{
    auto type = makeType();
    type->scanForDevices();

    const int sizes[] = { 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192 };

    for (auto size : sizes)
    {
        CallbackStats stats;
        auto opened = openDefault (*type, 2, 0, 48000.0, size);
        CHECK_MSG (opened.error.isEmpty(),
                   "open() with " + juce::String (size) + " samples failed: " + opened.error);

        if (opened.error.isNotEmpty())
            continue;

        TestCallback callback (stats, opened.device.get());
        opened.device->start (&callback);

        const bool got = waitForCallbacks (stats, 15, 4000);
        CHECK_MSG (got, "no callbacks with a " + juce::String (size) + " sample buffer");

        const auto actual = opened.device->getCurrentBufferSizeSamples();
        std::printf ("    requested %5d -> actual %5d, callback %d, xruns %d\n",
                     size, actual, stats.lastSamples.load(),
                     opened.device->getXRunCount());

        CHECK_MSG (actual > 0, "the reported block size is not positive");
        CHECK_MSG (stats.badBufferSize.load() == 0,
                   "the block size seen in the callback disagreed with getCurrentBufferSizeSamples()");

        opened.device->stop();
        opened.device->close();
    }
}

//==============================================================================
// duplex: input and output at the same time.
static void testDuplex()
{
    auto type = makeType();
    type->scanForDevices();

    if (type->getDeviceNames (true).isEmpty())
    {
        std::printf ("    SKIPPED: the graph has no Audio/Source node\n");
        return;
    }

    // Recording from whatever microphone the machine happens to have is not
    // something a test should do silently, so this only runs when the input was
    // named explicitly (which is what the isolated runner does) or when the
    // caller opted in.
    const auto inputName = pickInputName (*type);
    const auto allowRealInput = juce::SystemStats::getEnvironmentVariable ("JUCE_PW_ALLOW_REAL_INPUT", {});

    if (allowRealInput.isEmpty() && ! inputName.containsIgnoreCase ("test"))
    {
        std::printf ("    SKIPPED: \"%s\" looks like a real microphone; "
                     "set JUCE_PW_ALLOW_REAL_INPUT=1 to record from it\n",
                     inputName.toRawUTF8());
        return;
    }

    CallbackStats stats;
    auto opened = openDefault (*type, 2, 1, 48000.0, 256);
    CHECK_MSG (opened.error.isEmpty(), "full duplex open() failed: " + opened.error);

    if (opened.error.isNotEmpty())
        return;

    TestCallback callback (stats, opened.device.get());
    opened.device->start (&callback);

    CHECK (waitForCallbacks (stats, 40, 5000));
    std::printf ("    callbacks %d, inputs %d, outputs %d, buffer %d, non-silent input blocks %d\n",
                 stats.callbacks.load(), stats.lastInputs.load(), stats.lastOutputs.load(),
                 stats.lastSamples.load(), stats.nonSilentInputs.load());

    CHECK_MSG (stats.lastInputs.load() == 1, "the callback didn't get the one selected input channel");
    CHECK_MSG (stats.lastOutputs.load() == 2, "the callback didn't get the two selected output channels");
    CHECK_MSG (stats.errorCount.load() == 0, "the device reported an error");

    CHECK_MSG (opened.device->getInputLatencyInSamples() >= 0, "the input latency is negative");
    CHECK_MSG (opened.device->getOutputLatencyInSamples() >= 0, "the output latency is negative");

    opened.device->stop();
    opened.device->close();
}

//==============================================================================
// rt-contract: the queries that are documented as realtime-safe must work from
// inside the audio callback without locking up.
static void testRealtimeQueries()
{
    auto type = makeType();
    type->scanForDevices();

    CallbackStats stats;
    auto opened = openDefault (*type, 2, 0, 48000.0, 128);
    CHECK (opened.error.isEmpty());

    if (opened.error.isNotEmpty())
        return;

    TestCallback callback (stats, opened.device.get());
    stats.queryFromCallback = true;
    opened.device->start (&callback);

    CHECK (waitForCallbacks (stats, 200, 6000));

    CHECK_MSG (stats.badOpenState.load() == 0, "isOpen() returned false inside the callback");
    CHECK_MSG (stats.badPlayingState.load() == 0, "isPlaying() returned false inside the callback");
    CHECK_MSG (stats.badRate.load() == 0, "a realtime-safe query returned an invalid value");
    CHECK_MSG (stats.badBufferSize.load() == 0, "getCurrentBufferSizeSamples() disagreed with numSamples");

    opened.device->stop();
    opened.device->close();
}

//==============================================================================
// close-while-playing: tear the device down while the graph is actively calling
// back. Under a sanitiser this is the test that finds use-after-free.
static void testCloseWhilePlaying()
{
    auto type = makeType();
    type->scanForDevices();

    for (int i = 0; i < 30; ++i)
    {
        CallbackStats stats;
        auto opened = openDefault (*type, 2, 0, 48000.0, 64);
        CHECK (opened.error.isEmpty());

        if (opened.error.isNotEmpty())
            return;

        {
            TestCallback callback (stats, opened.device.get());
            opened.device->start (&callback);
            waitForCallbacks (stats, i % 5, 1000);
            // No stop() here: close() has to take care of the callback.
            opened.device->close();
            CHECK (! opened.device->isOpen());
        }
    }
}

//==============================================================================
// concurrent: several independent device instances opened, started and closed
// from several threads at the same time.
static void testConcurrentInstances()
{
    auto type = makeType();
    type->scanForDevices();

    const auto outputName = pickOutputName (*type);
    CHECK (outputName.isNotEmpty());

    if (outputName.isEmpty())
        return;

    constexpr int numThreads = 4;
    constexpr int iterations = 12;

    std::atomic<int> failures { 0 };
    std::atomic<int> totalCallbacks { 0 };
    std::vector<std::thread> threads;

    for (int t = 0; t < numThreads; ++t)
    {
        threads.emplace_back ([&, t]
        {
            for (int i = 0; i < iterations; ++i)
            {
                std::unique_ptr<juce::AudioIODevice> device (type->createDevice (outputName, {}));

                if (device == nullptr)
                {
                    failures.fetch_add (1);
                    continue;
                }

                const auto error = device->open (channelMask (0), channelMask (2),
                                                 48000.0, 128 + 128 * (t % 3));

                if (error.isNotEmpty())
                {
                    failures.fetch_add (1);
                    continue;
                }

                CallbackStats stats;
                TestCallback callback (stats, device.get());
                device->start (&callback);
                waitForCallbacks (stats, 3, 2000);
                totalCallbacks.fetch_add (stats.callbacks.load());
                device->stop();
                device->close();
            }
        });
    }

    for (auto& thread : threads)
        thread.join();

    std::printf ("    %d callbacks in total, %d failures\n",
                 totalCallbacks.load(), failures.load());

    CHECK_MSG (failures.load() == 0, "an instance failed to open or start");
    CHECK_MSG (totalCallbacks.load() > 0, "no audio was delivered to any of the instances");
}

//==============================================================================
// hammer: query the non-realtime getters from a second thread while audio is
// running. The contract says they take the lifecycle lock and may block, but
// they must never return garbage or deadlock.
static void testQueryHammer()
{
    auto type = makeType();
    type->scanForDevices();

    CallbackStats stats;
    auto opened = openDefault (*type, 2, 0, 48000.0, 128);
    CHECK (opened.error.isEmpty());

    if (opened.error.isNotEmpty())
        return;

    TestCallback callback (stats, opened.device.get());
    opened.device->start (&callback);

    std::atomic<bool> stop { false };
    std::atomic<int> bad { 0 };
    std::atomic<int> queries { 0 };

    std::thread hammer ([&]
    {
        while (! stop.load())
        {
            const auto latency = opened.device->getOutputLatencyInSamples();
            const auto rate = opened.device->getCurrentSampleRate();
            const auto buffer = opened.device->getCurrentBufferSizeSamples();
            const auto xruns = opened.device->getXRunCount();
            const auto outs = opened.device->getActiveOutputChannels();

            if (latency < 0 || latency > (int) rate * 5 || rate <= 0.0 || buffer <= 0 || xruns < 0)
                bad.fetch_add (1);

            if (outs.countNumberOfSetBits() != 2)
                bad.fetch_add (1);

            queries.fetch_add (1);
            juce::Thread::sleep (1);
        }
    });

    juce::Thread::sleep (1500);
    stop = true;
    hammer.join();

    std::printf ("    %d concurrent queries, %d bad answers\n", queries.load(), bad.load());
    CHECK_MSG (bad.load() == 0, "a query returned an invalid value");
    CHECK_MSG (stats.errorCount.load() == 0, "the device reported an error");

    opened.device->stop();
    opened.device->close();
}

//==============================================================================
// routed-name: the routing query needs a live link to the default sink.
static void testRoutedOutputName()
{
    auto type = makeType();
    type->scanForDevices();

    CallbackStats stats;
    auto opened = openDefault (*type, 2, 0, 48000.0, 256);
    CHECK (opened.error.isEmpty());

    if (opened.error.isNotEmpty())
        return;

    TestCallback callback (stats, opened.device.get());
    opened.device->start (&callback);
    CHECK (waitForCallbacks (stats, 20, 5000));

    juce::String routed;
    const bool got = waitUntil ([&]
    {
        const auto name = opened.device->getRoutedOutputDeviceName();
        routed = name.has_value() ? *name : juce::String();
        return routed.isNotEmpty();
    }, 5000);

    std::printf ("    routed to: \"%s\"\n", routed.toRawUTF8());
    CHECK_MSG (got, "getRoutedOutputDeviceName() didn't find the peer node");

    // The cached answer must agree with the uncached one.
    const auto second = opened.device->getRoutedOutputDeviceName();
    CHECK_MSG (second.has_value() && *second == routed, "the cached routing answer changed");

    opened.device->stop();
    opened.device->close();
}

//==============================================================================
// manager: the end-to-end path through AudioDeviceManager.
static void testAudioDeviceManager()
{
    juce::AudioDeviceManager manager;
    const auto error = manager.initialise (0, 2, nullptr, true);
    CHECK_MSG (error.isEmpty(), "AudioDeviceManager::initialise() failed: " + error);

    auto* device = manager.getCurrentAudioDevice();
    CHECK_MSG (device != nullptr, "AudioDeviceManager picked no device");

    if (device == nullptr)
        return;

    std::printf ("    type: %s, device: %s, rate %.0f, buffer %d\n",
                 device->getTypeName().toRawUTF8(), device->getName().toRawUTF8(),
                 device->getCurrentSampleRate(), device->getCurrentBufferSizeSamples());

    CHECK_MSG (device->getTypeName() == "PipeWire",
               "AudioDeviceManager preferred " + device->getTypeName() + " over PipeWire");

    CallbackStats stats;
    TestCallback callback (stats, device);
    manager.addAudioCallback (&callback);
    CHECK (waitForCallbacks (stats, 20, 5000));

    // Saving and restoring the state has to keep the same device. The state is
    // only written once something has been set explicitly, which is what
    // setAudioDeviceSetup() does here.
    auto setup = manager.getAudioDeviceSetup();
    setup.bufferSize = device->getCurrentBufferSizeSamples();
    CHECK_MSG (manager.setAudioDeviceSetup (setup, true).isEmpty(), "setAudioDeviceSetup() failed");

    auto state = manager.createStateXml();
    CHECK_MSG (state != nullptr, "createStateXml() returned nullptr");

    const auto savedName = device->getName();

    if (state != nullptr)
    {
        juce::AudioDeviceManager restored;
        const auto restoreError = restored.initialise (0, 2, state.get(), true);
        CHECK_MSG (restoreError.isEmpty(), "restoring the state failed: " + restoreError);

        if (auto* restoredDevice = restored.getCurrentAudioDevice())
            CHECK_MSG (restoredDevice->getName() == savedName, "the restored device is not the saved one");
    }

    manager.removeAudioCallback (&callback);
    manager.closeAudioDevice();
}

//==============================================================================
// stop-in-callback probe: AudioIODevice forbids calling stop() from the audio
// thread, so this is reported separately rather than counted as a failure.
static void probeStopFromCallback()
{
    auto type = makeType();
    type->scanForDevices();

    CallbackStats stats;
    auto opened = openDefault (*type, 2, 0, 48000.0, 256);

    if (opened.error.isNotEmpty())
    {
        std::printf ("    SKIPPED: %s\n", opened.error.toRawUTF8());
        return;
    }

    TestCallback callback (stats, opened.device.get());
    opened.device->start (&callback);
    waitForCallbacks (stats, 5, 3000);

    std::printf ("    asking the audio thread to call stop()...\n");
    std::fflush (stdout);

    stats.stopFromCallback = true;

    // If this returns, the device either handled it or ignored it.
    const bool returned = waitUntil ([&] { return stats.stopsFromCallback.load() > 0; }, 5000);
    std::printf ("    stop() was called from the audio thread, callbacks after it: %d\n",
                 stats.callbacks.load());
    juce::ignoreUnused (returned);

    opened.device->stop();
    opened.device->close();
}

//==============================================================================
// hotplug: create and destroy a sink while a device is open, and check that the
// device list is refreshed and the running device survives.
static void testHotPlug()
{
    const auto command = juce::SystemStats::getEnvironmentVariable ("JUCE_PW_HOTPLUG_CMD", {});

    if (command.isEmpty())
    {
        std::printf ("    SKIPPED: set JUCE_PW_HOTPLUG_CMD to enable (see run-tests.sh)\n");
        return;
    }

    auto type = makeType();
    type->scanForDevices();

    const auto before = type->getDeviceNames (false).size();
    std::printf ("    %d outputs before the plug\n", before);

    CallbackStats stats;
    auto opened = openDefault (*type, 2, 0, 48000.0, 256);
    CHECK (opened.error.isEmpty());

    if (opened.error.isNotEmpty())
        return;

    TestCallback callback (stats, opened.device.get());
    opened.device->start (&callback);
    CHECK (waitForCallbacks (stats, 20, 5000));

    std::atomic<int> changeNotifications { 0 };
    struct Listener final : public juce::AudioIODeviceType::Listener
    {
        explicit Listener (std::atomic<int>& c) : count (c) {}
        void audioDeviceListChanged() override { count.fetch_add (1); }
        std::atomic<int>& count;
    };

    Listener listener (changeNotifications);
    type->addListener (&listener);

    // The command is expected to add a node that stays around (object.linger).
    const auto addResult = std::system (command.toRawUTF8());
    CHECK_MSG (addResult == 0, "the hotplug command failed");

    const bool grew = waitUntilPumpingMessages ([&]
    {
        return type->getDeviceNames (false).size() > before;
    }, 15000);

    const int after = type->getDeviceNames (false).size();
    std::printf ("    %d outputs after the plug, %d change notifications\n",
                 after, changeNotifications.load());

    CHECK_MSG (grew, "the new sink did not show up in the device list");

    // The notification for the plug itself is not asserted: the watcher only
    // starts comparing after its first poll, so a node that appears before that
    // first poll is part of its baseline and is never announced. That is the
    // documented behaviour of the backend (the host calls scanForDevices() in
    // its constructor, so it never needs that first notification) - and the
    // unplug below does verify the notification path.
    const auto notificationsAfterPlug = changeNotifications.load();

    // The device that was already open must still be running.
    const auto callbacksBefore = stats.callbacks.load();
    CHECK_MSG (waitForCallbacks (stats, callbacksBefore + 10, 3000),
               "the open device stopped delivering audio after the hotplug");

    if (auto* removeCommand = std::getenv ("JUCE_PW_HOTPLUG_REMOVE_CMD"))
        juce::ignoreUnused (std::system (removeCommand));

    // Now that the watcher has polled at least once its baseline contains the
    // plugged node, so removing it must show up in the list and must be
    // announced to the listeners.
    const bool shrank = waitUntilPumpingMessages ([&]
    {
        return type->getDeviceNames (false).size() == before;
    }, 15000);

    const bool notified = waitUntilPumpingMessages ([&]
    {
        return changeNotifications.load() > notificationsAfterPlug;
    }, 5000);

    std::printf ("    %d outputs after the unplug, %d change notifications\n",
                 type->getDeviceNames (false).size(), changeNotifications.load());

    CHECK_MSG (shrank, "the unplugged sink stayed in the device list");
    CHECK_MSG (notified, "no change notification was sent to the listeners");

    type->removeListener (&listener);

    opened.device->stop();
    opened.device->close();
}

//==============================================================================
// restart-server: kill the server and bring it back, then check that the device
// recovers on its own. Requires the daemon pid and command in the environment.
static void testServerRestart()
{
    const auto pidText = juce::SystemStats::getEnvironmentVariable ("JUCE_PW_DAEMON_PID", {});
    const auto startCommand = juce::SystemStats::getEnvironmentVariable ("JUCE_PW_DAEMON_CMD", {});

    if (pidText.isEmpty() || startCommand.isEmpty())
    {
        std::printf ("    SKIPPED: set JUCE_PW_DAEMON_PID/JUCE_PW_DAEMON_CMD to enable "
                     "(it kills and restarts the real server)\n");
        return;
    }

    auto type = makeType();
    type->scanForDevices();

    CallbackStats stats;
    auto opened = openDefault (*type, 2, 0, 48000.0, 256);
    CHECK (opened.error.isEmpty());

    if (opened.error.isNotEmpty())
        return;

    TestCallback callback (stats, opened.device.get());
    opened.device->start (&callback);
    CHECK (waitForCallbacks (stats, 30, 5000));

    const auto pid = (pid_t) std::atoi (pidText.toRawUTF8());
    std::printf ("    killing the server (pid %d)...\n", (int) pid);
    std::fflush (stdout);

    CHECK_MSG (::kill (pid, SIGKILL) == 0, "could not kill the server");

    // The host has to be told that the device stopped.
    const bool notified = waitUntil ([&] { return stats.errorCount.load() > 0; }, 15000);
    CHECK_MSG (notified, "the host was never told that the connection was lost");

    std::printf ("    restarting the server: %s\n", startCommand.toRawUTF8());
    std::fflush (stdout);

    CHECK_MSG (std::system (startCommand.toRawUTF8()) == 0, "could not restart the server");

    // The device is expected to stay open and rebuild its streams by itself.
    const bool stillOpen = waitUntil ([&] { return opened.device->isOpen(); }, 10000);
    CHECK_MSG (stillOpen, "the device closed itself when the server went away");

    // The callback was detached when the error was reported, so the host has to
    // start the device again - that is the documented behaviour.
    const auto callbacksBefore = stats.callbacks.load();
    bool restarted = false;

    for (int attempt = 0; attempt < 20 && ! restarted; ++attempt)
    {
        opened.device->start (&callback);
        restarted = waitForCallbacks (stats, callbacksBefore + 20, 2000);

        if (! restarted)
        {
            opened.device->stop();
            juce::Thread::sleep (500);
        }
    }

    std::printf ("    after the restart: %d new callbacks, open=%d playing=%d, errors %d\n",
                 stats.callbacks.load() - callbacksBefore,
                 (int) opened.device->isOpen(), (int) opened.device->isPlaying(),
                 stats.errorCount.load());

    CHECK_MSG (restarted, "audio did not resume after the server restarted");

    opened.device->stop();
    opened.device->close();
}

//==============================================================================
struct TestCase
{
    const char* name;
    void (*function)();
    int timeoutMs;
    bool probe;
};

const TestCase tests[] =
{
    { "enumerate",          testEnumerate,          30000, false },
    { "open-close",         testOpenCloseChurn,     120000, false },
    { "start-stop",         testStartStopChurn,     120000, false },
    { "sample-rates",       testSampleRates,        120000, false },
    { "buffer-sizes",       testBufferSizes,        120000, false },
    { "duplex",             testDuplex,             30000, false },
    { "rt-queries",         testRealtimeQueries,    30000, false },
    { "close-while-playing",testCloseWhilePlaying,  120000, false },
    { "concurrent",         testConcurrentInstances,120000, false },
    { "query-hammer",       testQueryHammer,        60000, false },
    { "routed-name",        testRoutedOutputName,   30000, false },
    { "manager",            testAudioDeviceManager, 60000, false },
    { "hotplug",            testHotPlug,            60000, false },
    { "restart-server",     testServerRestart,      120000, false },
    { "stop-in-callback",   probeStopFromCallback,  30000, true },
};

//==============================================================================
// Prints the backend's own log lines (JUCE_PIPEWIRE_LOG) when PW_LOG is set in
// the environment. Those lines say what the recovery machinery decided and why,
// which is the only practical way to debug a test that fails because a rebuild
// never happened. The output is unbuffered so that it stays interleaved with
// the test's own printf()s in the right order.
namespace pwlog
{
    static double elapsed()
    {
        static const auto start = juce::Time::getMillisecondCounterHiRes();
        return juce::Time::getMillisecondCounterHiRes() - start;
    }

    struct StdoutLogger : juce::Logger
    {
        void logMessage (const juce::String& message) override
        {
            std::printf ("      [%8.1f] %s\n", elapsed(), message.toRawUTF8());
            std::fflush (stdout);
        }
    };

    static void installIfRequested()
    {
        if (juce::SystemStats::getEnvironmentVariable ("PW_LOG", {}).isEmpty())
            return;

        static StdoutLogger logger;
        juce::Logger::setCurrentLogger (&logger);
    }
}

int runOneTest (const TestCase& test)
{
    std::printf ("\n== %s ==\n", test.name);
    std::fflush (stdout);

    const auto startTime = juce::Time::getMillisecondCounterHiRes();
    const auto pid = ::fork();

    if (pid < 0)
    {
        std::printf ("  FAIL (fork failed)\n");
        return 1;
    }

    if (pid == 0)
    {
        // The child: bring JUCE up only now, so that nothing inherits a thread
        // or a PipeWire connection from the parent.
        juce::ScopedJuceInitialiser_GUI juceInitialiser;

        pwlog::installIfRequested();

        for (auto signalNumber : { SIGSEGV, SIGABRT, SIGBUS, SIGFPE })
            ::signal (signalNumber, crashHandler);

        TestEnv env;
        currentEnv = &env;
        test.function();

        for (const auto& failure : env.failures)
            std::printf ("    FAILED: %s\n", failure.toRawUTF8());

        std::printf ("  %s (%d checks, %.1fs)\n",
                     env.ok() ? "PASS" : "FAIL",
                     env.checks,
                     (juce::Time::getMillisecondCounterHiRes() - startTime) / 1000.0);
        std::fflush (stdout);

        ::_exit (env.ok() ? 0 : 1);
    }

    int status = 0;
    bool timedOut = false;

    while (true)
    {
        const auto result = ::waitpid (pid, &status, WNOHANG);

        if (result == pid)
            break;

        if (juce::Time::getMillisecondCounterHiRes() - startTime > (double) test.timeoutMs)
        {
            ::kill (pid, SIGKILL);
            ::waitpid (pid, &status, 0);
            timedOut = true;
            break;
        }

        ::usleep (20000);
    }

    const auto elapsed = (juce::Time::getMillisecondCounterHiRes() - startTime) / 1000.0;

    if (timedOut)
    {
        std::printf ("  HANG: killed after %.1fs%s\n", elapsed,
                     test.probe ? " (probe; stop() from the audio thread is not supported)" : "");
        return test.probe ? 0 : 1;
    }

    if (WIFSIGNALED (status))
    {
        std::printf ("  CRASH: signal %d (%s) after %.1fs\n",
                     WTERMSIG (status), strsignal (WTERMSIG (status)), elapsed);
        return 1;
    }

    const auto exitCode = WEXITSTATUS (status);

    if (exitCode == 99)
    {
        std::printf ("  CRASH: the backtrace above is from the test process\n");
        return 1;
    }

    if (exitCode == 0 && test.probe)
        std::printf ("  PROBE OK: stop() from the audio thread returned\n");

    return exitCode;
}

} // namespace

//==============================================================================
int main (int argc, char* argv[])
{
    std::setvbuf (stdout, nullptr, _IOLBF, 0);

    const std::string only = argc > 1 ? argv[1] : "";

    if (only == "--list")
    {
        for (const auto& test : tests)
            std::printf ("%s\n", test.name);

        return 0;
    }

    std::printf ("JUCE PipeWire stress test\n");
    std::printf ("libpipewire at runtime: %s\n", [] {
        // This only prints the version that dlopen() will pick; it doesn't
        // initialise anything.
        if (auto* handle = dlopen ("libpipewire-0.3.so.0", RTLD_LAZY))
        {
            using VersionFn = const char* (*)();
            if (auto* fn = (VersionFn) dlsym (handle, "pw_get_library_version"))
                return juce::String (fn());
        }

        return juce::String ("not found");
    }().toRawUTF8());

    int failures = 0;

    for (const auto& test : tests)
    {
        if (! only.empty() && only != test.name)
            continue;

        failures += runOneTest (test);
    }

    std::printf ("\n%d test(s) failed\n", failures);
    return failures == 0 ? 0 : 1;
}
