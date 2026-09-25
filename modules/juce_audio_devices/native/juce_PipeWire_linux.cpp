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

namespace juce
{

// This file implements a native PipeWire audio device type. PipeWire is loaded
// dynamically at run-time (like the JACK backend does), so apps which enable
// JUCE_PIPEWIRE don't need to link against libpipewire.

#ifndef JUCE_PIPEWIRE_LOGGING
 #define JUCE_PIPEWIRE_LOGGING JUCE_DEBUG
#endif

#if JUCE_PIPEWIRE_LOGGING
 #define JUCE_PIPEWIRE_LOG(dbgtext)  { juce::String tempDbgBuf ("PipeWire: "); tempDbgBuf << dbgtext; Logger::writeToLog (tempDbgBuf); DBG (tempDbgBuf); }
#else
 #define JUCE_PIPEWIRE_LOG(dbgtext)
#endif

//==============================================================================
static void* juce_libpipewireHandle = nullptr;

static void* juce_loadPipeWireFunction (const char* const name)
{
    if (juce_libpipewireHandle == nullptr)
        return nullptr;

    return dlsym (juce_libpipewireHandle, name);
}

static bool loadPipeWireLibrary()
{
    if (juce_libpipewireHandle == nullptr)
    {
        juce_libpipewireHandle = dlopen ("libpipewire-0.3.so.0", RTLD_LAZY);

        if (juce_libpipewireHandle == nullptr)
            juce_libpipewireHandle = dlopen ("libpipewire-0.3.so", RTLD_LAZY);
    }

    return juce_libpipewireHandle != nullptr;
}

#define JUCE_DECL_PIPEWIRE_FUNCTION(return_type, fn_name, argument_types, arguments)  \
  static inline return_type fn_name argument_types                                    \
  {                                                                                   \
      using ReturnType = return_type;                                                 \
      typedef return_type (*fn_type) argument_types;                                  \
      static fn_type fn = (fn_type) juce_loadPipeWireFunction (#fn_name);             \
      jassert (fn != nullptr);                                                        \
      return (fn != nullptr) ? ((*fn) arguments) : ReturnType();                      \
  }

#define JUCE_DECL_VOID_PIPEWIRE_FUNCTION(fn_name, argument_types, arguments)          \
  static inline void fn_name argument_types                                           \
  {                                                                                   \
      typedef void (*fn_type) argument_types;                                         \
      static fn_type fn = (fn_type) juce_loadPipeWireFunction (#fn_name);             \
      jassert (fn != nullptr);                                                        \
      if (fn != nullptr) (*fn) arguments;                                             \
  }

//==============================================================================
// Core/context/loop
JUCE_DECL_VOID_PIPEWIRE_FUNCTION (pw_init, (int* argc, char** argv), (argc, argv))
JUCE_DECL_PIPEWIRE_FUNCTION (struct pw_main_loop*, pw_main_loop_new, (const struct spa_dict* props), (props))
JUCE_DECL_VOID_PIPEWIRE_FUNCTION (pw_main_loop_destroy, (struct pw_main_loop* loop), (loop))
JUCE_DECL_PIPEWIRE_FUNCTION (struct pw_loop*, pw_main_loop_get_loop, (struct pw_main_loop* loop), (loop))
JUCE_DECL_VOID_PIPEWIRE_FUNCTION (pw_main_loop_run, (struct pw_main_loop* loop), (loop))
JUCE_DECL_VOID_PIPEWIRE_FUNCTION (pw_main_loop_quit, (struct pw_main_loop* loop), (loop))
JUCE_DECL_PIPEWIRE_FUNCTION (struct pw_loop*, pw_loop_new, (const struct spa_dict* props), (props))
JUCE_DECL_VOID_PIPEWIRE_FUNCTION (pw_loop_destroy, (struct pw_loop* loop), (loop))
JUCE_DECL_PIPEWIRE_FUNCTION (int, pw_loop_iterate, (struct pw_loop* loop, int timeout), (loop, timeout))
JUCE_DECL_PIPEWIRE_FUNCTION (int, pw_loop_invoke, (struct pw_loop* loop, spa_invoke_func_t func, uint32_t seq, const void* data, size_t size, bool block, void* user_data), (loop, func, seq, data, size, block, user_data))
JUCE_DECL_PIPEWIRE_FUNCTION (struct pw_context*, pw_context_new, (struct pw_loop* loop, struct pw_properties* props, size_t user_data_size), (loop, props, user_data_size))
JUCE_DECL_VOID_PIPEWIRE_FUNCTION (pw_context_destroy, (struct pw_context* context), (context))
JUCE_DECL_PIPEWIRE_FUNCTION (struct pw_core*, pw_context_connect, (struct pw_context* context, struct pw_properties* properties, size_t user_data_size), (context, properties, user_data_size))
JUCE_DECL_VOID_PIPEWIRE_FUNCTION (pw_core_disconnect, (struct pw_core* core), (core))
JUCE_DECL_PIPEWIRE_FUNCTION (int, pw_core_sync, (struct pw_core* core, uint32_t id, int seq), (core, id, seq))
JUCE_DECL_VOID_PIPEWIRE_FUNCTION (pw_core_add_listener, (struct pw_core* core, struct spa_hook* listener, const struct pw_core_events* events, void* data), (core, listener, events, data))

// Registry / proxies
JUCE_DECL_PIPEWIRE_FUNCTION (struct pw_registry*, pw_core_get_registry, (struct pw_core* core, uint32_t version, size_t user_data_size), (core, version, user_data_size))
JUCE_DECL_VOID_PIPEWIRE_FUNCTION (pw_registry_add_listener, (struct pw_registry* registry, struct spa_hook* listener, const struct pw_registry_events* events, void* data), (registry, listener, events, data))
JUCE_DECL_PIPEWIRE_FUNCTION (struct pw_proxy*, pw_registry_bind, (struct pw_registry* registry, uint32_t id, const char* type, uint32_t version, size_t user_data_size), (registry, id, type, version, user_data_size))
JUCE_DECL_VOID_PIPEWIRE_FUNCTION (pw_proxy_destroy, (struct pw_proxy* proxy), (proxy))
JUCE_DECL_VOID_PIPEWIRE_FUNCTION (pw_proxy_add_object_listener, (struct pw_proxy* proxy, struct spa_hook* listener, const void* funcs, void* data), (proxy, listener, funcs, data))

// Streams
JUCE_DECL_PIPEWIRE_FUNCTION (struct pw_stream*, pw_stream_new, (struct pw_core* core, const char* name, struct pw_properties* props), (core, name, props))
JUCE_DECL_VOID_PIPEWIRE_FUNCTION (pw_stream_destroy, (struct pw_stream* stream), (stream))
JUCE_DECL_VOID_PIPEWIRE_FUNCTION (pw_stream_add_listener, (struct pw_stream* stream, struct spa_hook* listener, const struct pw_stream_events* events, void* data), (stream, listener, events, data))
JUCE_DECL_PIPEWIRE_FUNCTION (int, pw_stream_connect, (struct pw_stream* stream, enum pw_direction direction, uint32_t target_id, enum pw_stream_flags flags, const struct spa_pod** params, uint32_t n_params), (stream, direction, target_id, flags, params, n_params))
JUCE_DECL_PIPEWIRE_FUNCTION (uint32_t, pw_stream_get_node_id, (struct pw_stream* stream), (stream))
JUCE_DECL_VOID_PIPEWIRE_FUNCTION (pw_stream_disconnect, (struct pw_stream* stream), (stream))
JUCE_DECL_PIPEWIRE_FUNCTION (const char*, pw_stream_state_as_string, (enum pw_stream_state state), (state))
JUCE_DECL_PIPEWIRE_FUNCTION (struct pw_buffer*, pw_stream_dequeue_buffer, (struct pw_stream* stream), (stream))
JUCE_DECL_VOID_PIPEWIRE_FUNCTION (pw_stream_queue_buffer, (struct pw_stream* stream, struct pw_buffer* buffer), (stream, buffer))
JUCE_DECL_PIPEWIRE_FUNCTION (int, pw_stream_set_active, (struct pw_stream* stream, bool active), (stream, active))
JUCE_DECL_PIPEWIRE_FUNCTION (int, pw_stream_get_time, (struct pw_stream* stream, struct pw_time* time), (stream, time))

// Properties
JUCE_DECL_PIPEWIRE_FUNCTION (int, pw_properties_set, (struct pw_properties* properties, const char* key, const char* value), (properties, key, value))
JUCE_DECL_VOID_PIPEWIRE_FUNCTION (pw_properties_free, (struct pw_properties* properties), (properties))

// pw_properties_new() is a variadic function, so it can't be declared with the
// macro above. We only ever need an empty property bag which we fill in with
// pw_properties_set() afterwards.
static inline struct pw_properties* createEmptyPipeWireProperties()
{
    typedef struct pw_properties* (*fn_type) (const char*, ...);
    static fn_type fn = (fn_type) juce_loadPipeWireFunction ("pw_properties_new");
    jassert (fn != nullptr);

    if (fn == nullptr)
        return nullptr;

    return (*fn) ((const char*) nullptr);
}

//==============================================================================
static const char* getProp (const struct spa_dict* props, const char* key)
{
    if (props == nullptr)
        return nullptr;

    return spa_dict_lookup (props, key);
}

static bool mediaClassIsSink (const char* mediaClass)
{
    return mediaClass != nullptr && String (mediaClass).startsWith ("Audio/Sink");
}

static bool mediaClassIsSource (const char* mediaClass)
{
    return mediaClass != nullptr && String (mediaClass).startsWith ("Audio/Source");
}

//==============================================================================
// A description of a single sink or source node in the PipeWire graph,
// collected during scanForDevices().
struct PipeWireEndpoint
{
    uint32_t globalId = SPA_ID_INVALID; // id of the node, valid for the server session
    String name;                        // a stable identifier (usually the node.name)
    String displayName;                 // a human readable name for the JUCE device lists
    String objectPath;                  // optional object.path, may match the default metadata
    StringArray channels;               // the names of the audio channels, e.g. "FL", "FR"
};

struct PipeWireScanResult
{
    Array<PipeWireEndpoint> sinks, sources;
    String defaultSinkKey, defaultSourceKey;
};

//==============================================================================
class PipeWireRegistryScanner
{
public:
    // Synchronously queries the PipeWire registry. Returns false when the
    // PipeWire server could not be reached.
    static bool scan (PipeWireScanResult& result)
    {
        if (! loadPipeWireLibrary())
            return false;

        PipeWireRegistryScanner scanner;
        return scanner.scanInternal (result);
    }

    // Queries the graph to find out which node the given stream node is
    // currently linked to on the other side of the graph. Returns an empty
    // string when the peer cannot be determined (for example when the stream
    // isn't linked, or the graph has changed since the scan).
    static String findRoutedPeerName (uint32_t nodeId, bool nodeIsOutput)
    {
        if (! loadPipeWireLibrary() || nodeId == SPA_ID_INVALID)
            return {};

        PipeWireRegistryScanner scanner;
        PipeWireScanResult result;

        if (! scanner.scanInternal (result))
            return {};

        uint32_t peerId = SPA_ID_INVALID;

        for (const auto& link : scanner.links)
        {
            if (nodeIsOutput && link.outputNode == nodeId)
            {
                peerId = link.inputNode;
                break;
            }

            if (! nodeIsOutput && link.inputNode == nodeId)
            {
                peerId = link.outputNode;
                break;
            }
        }

        if (peerId == SPA_ID_INVALID)
            return {};

        for (const auto& endpoint : (nodeIsOutput ? result.sinks : result.sources))
            if (endpoint.globalId == peerId)
                return endpoint.displayName.isNotEmpty() ? endpoint.displayName : endpoint.name;

        return {};
    }

    //==============================================================================
    void handleCoreDone (uint32_t id, int seq)
    {
        // The seq of a sync reply always has the 1 << 30 bit set.
        if (id == PW_ID_CORE && (seq & (1 << 30)) != 0)
            connection.quit();
    }

    void handleCoreError()
    {
        failed = true;
        connection.quit();
    }

    void handleGlobal (uint32_t id, const char* type, const struct spa_dict* props)
    {
        if (String (type) == PW_TYPE_INTERFACE_Node)
        {
            const auto* mediaClass = getProp (props, PW_KEY_MEDIA_CLASS);

            if (mediaClassIsSink (mediaClass) || mediaClassIsSource (mediaClass))
            {
                NodeRecord node;
                node.id = id;
                node.isSink = mediaClassIsSink (mediaClass);
                node.name = getNodeKey (getProp (props, PW_KEY_NODE_NAME), getProp (props, PW_KEY_OBJECT_PATH));
                node.displayName = String::fromUTF8 (getProp (props, PW_KEY_NODE_DESCRIPTION));
                node.objectPath = String::fromUTF8 (getProp (props, PW_KEY_OBJECT_PATH));

                if (node.name.isNotEmpty())
                    nodes.add (node);
            }
        }
        else if (String (type) == PW_TYPE_INTERFACE_Port)
        {
            const auto* nodeIdStr = getProp (props, "node.id");

            if (nodeIdStr != nullptr)
            {
                PortRecord port;
                port.id = id;
                port.nodeId = (uint32_t) atoi (nodeIdStr);
                port.channel = String::fromUTF8 (getProp (props, PW_KEY_AUDIO_CHANNEL));
                port.isInput = String::fromUTF8 (getProp (props, PW_KEY_PORT_DIRECTION)) == "in";
                ports.add (port);
            }
        }
        else if (String (type) == PW_TYPE_INTERFACE_Metadata)
        {
            metadataIds.addIfNotAlreadyThere (id);
        }
        else if (String (type) == PW_TYPE_INTERFACE_Link)
        {
            const auto* outputNode = getProp (props, PW_KEY_LINK_OUTPUT_NODE);
            const auto* inputNode  = getProp (props, PW_KEY_LINK_INPUT_NODE);

            if (outputNode != nullptr && inputNode != nullptr)
            {
                LinkRecord link;
                link.outputNode = (uint32_t) atoi (outputNode);
                link.inputNode  = (uint32_t) atoi (inputNode);
                links.add (link);
            }
        }
    }

    void handleMetadataProperty (const char* key, const char* value)
    {
        if (key == nullptr || value == nullptr)
            return;

        if (String (key) == "default.audio.sink")
            result.defaultSinkKey = parseDefaultValue (value);

        if (String (key) == "default.audio.source")
            result.defaultSourceKey = parseDefaultValue (value);
    }

    //==============================================================================
    struct NodeRecord
    {
        uint32_t id = SPA_ID_INVALID;
        bool isSink = false;
        String name, displayName, objectPath;
    };

    struct PortRecord
    {
        uint32_t id = SPA_ID_INVALID;
        uint32_t nodeId = SPA_ID_INVALID;
        String channel;
        bool isInput = false;
    };

    struct LinkRecord
    {
        uint32_t outputNode = SPA_ID_INVALID;
        uint32_t inputNode = SPA_ID_INVALID;
    };

private:
    //==============================================================================
    PipeWireRegistryScanner()
    {
        // This struct will be destroyed when the scan is complete, so its
        // listener hooks always outlive the proxies they are attached to.
    }

    ~PipeWireRegistryScanner()
    {
        // The connection (and everything bound to it, including the metadata
        // proxies) is cleaned up when the PipeWireConnection member is
        // destroyed, so we only need to detach our listeners here.
        spa_hook_remove (&coreHook);
        spa_hook_remove (&registryHook);

        for (auto& binding : metadataBindings)
            spa_hook_remove (&binding->objectHook);
    }

    //==============================================================================
    bool scanInternal (PipeWireScanResult& resultToFill)
    {
        result = resultToFill;

        if (! connection.connect())
            return false;

        juce::pw_core_add_listener (connection.core, &coreHook, &getCoreEvents(), this);

        connection.registry = juce::pw_core_get_registry (connection.core, PW_VERSION_REGISTRY, 0);

        if (connection.registry == nullptr)
            return false;

        juce::pw_registry_add_listener (connection.registry, &registryHook, &getRegistryEvents(), this);

        // Wait for the initial burst of globals to arrive.
        if (juce::pw_core_sync (connection.core, PW_ID_CORE, ++syncCounter) < 0)
            return false;

        if (! connection.runUntilQuit (scanTimeoutMs) || failed)
            return false;

        // The "default" metadata contains the names of the default sink and
        // source. We bind to all metadata objects and read their properties to
        // find those names.
        for (auto metadataId : metadataIds)
            bindMetadata (metadataId);

        if (metadataBindings.size() > 0)
        {
            // A final sync ensures that all the property events have arrived
            // before we proceed.
            if (juce::pw_core_sync (connection.core, PW_ID_CORE, ++syncCounter) < 0)
                return false;

            if (! connection.runUntilQuit (scanTimeoutMs) || failed)
                return false;
        }

        assembleResult();
        resultToFill = result;
        return true;
    }

    //==============================================================================
    void bindMetadata (uint32_t id)
    {
        auto* metadata = (struct pw_metadata*) juce::pw_registry_bind (connection.registry, id,
                                                                 PW_TYPE_INTERFACE_Metadata,
                                                                 PW_VERSION_METADATA, 0);

        if (metadata == nullptr)
            return;

        auto binding = std::make_unique<MetadataBinding>();
        binding->metadata = metadata;
        juce::pw_proxy_add_object_listener ((struct pw_proxy*) metadata, &binding->objectHook,
                                            &getMetadataEvents(), this);
        metadataBindings.add (std::move (binding));
    }

    //==============================================================================
    static const struct pw_core_events& getCoreEvents()
    {
        static struct pw_core_events events {};

        if (events.version == 0)
        {
            events.version = PW_VERSION_CORE_EVENTS;
            events.done = [] (void* data, uint32_t id, int seq)
            {
                static_cast<PipeWireRegistryScanner*> (data)->handleCoreDone (id, seq);
            };
            events.error = [] (void* data, uint32_t, int, int, const char*)
            {
                static_cast<PipeWireRegistryScanner*> (data)->handleCoreError();
            };
        }

        return events;
    }

    static const struct pw_registry_events& getRegistryEvents()
    {
        static struct pw_registry_events events {};

        if (events.version == 0)
        {
            events.version = PW_VERSION_REGISTRY_EVENTS;
            events.global = [] (void* data, uint32_t id, uint32_t, const char* type,
                                uint32_t, const struct spa_dict* props)
            {
                static_cast<PipeWireRegistryScanner*> (data)->handleGlobal (id, type, props);
            };
            events.global_remove = [] (void*, uint32_t) {};
        }

        return events;
    }

    static const struct pw_metadata_events& getMetadataEvents()
    {
        static struct pw_metadata_events events {};

        if (events.version == 0)
        {
            events.version = PW_VERSION_METADATA_EVENTS;
            events.property = [] (void* data, uint32_t, const char* key, const char*, const char* value)
            {
                static_cast<PipeWireRegistryScanner*> (data)->handleMetadataProperty (key, value);
                return 0;
            };
        }

        return events;
    }

    //==============================================================================
    void assembleResult()
    {
        result.sinks.clear();
        result.sources.clear();

        // Put the ports in a deterministic order (their creation order) so the
        // channels of each device always appear in the same order.
        std::sort (ports.begin(), ports.end(),
                   [] (const PortRecord& a, const PortRecord& b) { return a.id < b.id; });

        for (auto& node : nodes)
        {
            // A sink exposes its channels via its input ports and a source via
            // its output ports.
            auto& endpointList = node.isSink ? result.sinks : result.sources;

            PipeWireEndpoint endpoint;
            endpoint.globalId = node.id;
            endpoint.name = node.name;
            endpoint.displayName = node.displayName.isNotEmpty() ? node.displayName : node.name;
            endpoint.objectPath = node.objectPath;

            for (auto& port : ports)
            {
                if (port.nodeId != node.id || port.isInput != node.isSink || port.channel.isEmpty())
                    continue;

                endpoint.channels.add (port.channel);
            }

            endpointList.add (endpoint);
        }
    }

    static String getNodeKey (const char* nodeName, const char* objectPath)
    {
        if (nodeName != nullptr && String::fromUTF8 (nodeName).isNotEmpty())
            return String::fromUTF8 (nodeName);

        if (objectPath != nullptr && String::fromUTF8 (objectPath).isNotEmpty())
            return String::fromUTF8 (objectPath);

        return {};
    }

    static String parseDefaultValue (const char* value)
    {
        // The default metadata stores values as JSON, e.g. {"name": "..."}.
        if (auto* namePos = strstr (value, "\"name\""))
        {
            if (auto* quote = strchr (namePos, ':'))
            {
                if (auto* start = strchr (quote, '"'))
                {
                    if (auto* end = strchr (start + 1, '"'))
                        return String::fromUTF8 (start + 1, (int) (end - start - 1));
                }
            }
        }

        return String::fromUTF8 (value).trim();
    }

    //==============================================================================
    struct MetadataBinding
    {
        struct pw_metadata* metadata = nullptr;
        struct spa_hook objectHook {};
    };

    struct PipeWireConnection
    {
        PipeWireConnection()
        {
            int fakeArgc = 1;
            char fakeArgv[] = "juce";
            char* fakeArgvPtr = fakeArgv;
            juce::pw_init (&fakeArgc, &fakeArgvPtr);
        }

        ~PipeWireConnection()
        {
            if (core != nullptr)        juce::pw_core_disconnect (core);
            if (context != nullptr)     juce::pw_context_destroy (context);
            if (mainLoop != nullptr)    juce::pw_main_loop_destroy (mainLoop);
        }

        bool connect()
        {
            mainLoop = juce::pw_main_loop_new (nullptr);

            if (mainLoop == nullptr)
                return false;

            context = juce::pw_context_new (juce::pw_main_loop_get_loop (mainLoop), nullptr, 0);

            if (context == nullptr)
                return false;

            core = juce::pw_context_connect (context, nullptr, 0);
            return core != nullptr;
        }

        // Runs the loop until quit() is called, the connection fails or the
        // timeout expires. Returns true if the operation completed normally.
        bool runUntilQuit (int timeoutMs)
        {
            quitRequested = false;

            const auto deadline = Time::getMillisecondCounterHiRes() + timeoutMs;

            while (! quitRequested)
            {
                if (Time::getMillisecondCounterHiRes() >= deadline)
                    return false;

                juce::pw_loop_iterate (juce::pw_main_loop_get_loop (mainLoop), 20);
            }

            return true;
        }

        void quit()
        {
            quitRequested = true;
            juce::pw_main_loop_quit (mainLoop);
        }

        struct pw_main_loop* mainLoop = nullptr;
        struct pw_context* context = nullptr;
        struct pw_core* core = nullptr;
        struct pw_registry* registry = nullptr;
        bool quitRequested = false;

        JUCE_DECLARE_NON_COPYABLE (PipeWireConnection)
    };

    PipeWireConnection connection;

    Array<NodeRecord> nodes;
    Array<PortRecord> ports;
    Array<LinkRecord> links;
    Array<uint32_t> metadataIds;
    Array<std::unique_ptr<MetadataBinding>> metadataBindings;
    struct spa_hook coreHook {}, registryHook {};

    PipeWireScanResult result;
    int syncCounter = 0;
    bool failed = false;

    static constexpr int scanTimeoutMs = 5000;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PipeWireRegistryScanner)
};

//==============================================================================
class PipeWireAudioIODevice final : public AudioIODevice
{
public:
    PipeWireAudioIODevice (const String& typeNameToUse,
                           const String& sinkKeyToUse,
                           const String& sinkDisplayName,
                           const StringArray& sinkChannelNames,
                           const String& sourceKeyToUse,
                           const String& sourceDisplayName,
                           const StringArray& sourceChannelNames)
        : AudioIODevice (sinkDisplayName.isNotEmpty() ? sinkDisplayName : sourceDisplayName,
                         typeNameToUse),
          sinkKey (sinkKeyToUse),
          sourceKey (sourceKeyToUse),
          outputChannelNames (sinkChannelNames),
          inputChannelNames (sourceChannelNames)
    {
        jassert (sinkKey.isNotEmpty() || sourceKey.isNotEmpty());
    }

    ~PipeWireAudioIODevice() override
    {
        close();
    }

    //==============================================================================
    StringArray getOutputChannelNames() override    { return outputChannelNames; }
    StringArray getInputChannelNames() override     { return inputChannelNames; }

    Array<double> getAvailableSampleRates() override
    {
        Array<double> rates;

        for (auto rate : SampleRateHelpers::getCommonSampleRates())
            rates.add (rate);

        return rates;
    }

    Array<int> getAvailableBufferSizes() override
    {
        // PipeWire decides the actual block size (its "quantum") at run-time,
        // so like the JACK backend we advertise a range of sensible sizes and
        // report the real one via getCurrentBufferSizeSamples().
        Array<int> sizes;

        for (int n = 16; n <= 8192; n += n < 64 ? 16 : (n < 512 ? 32 : (n < 2048 ? 64 : 128)))
            sizes.add (n);

        return sizes;
    }

    int getDefaultBufferSize() override                 { return 512; }

    //==============================================================================
    String open (const BigInteger& inputChannels,
                 const BigInteger& outputChannels,
                 double sampleRate,
                 int bufferSizeSamples) override
    {
        const ScopedLock sl (lifecycleLock);
        return openInternal (inputChannels, outputChannels, sampleRate, bufferSizeSamples);
    }

    void close() override
    {
        const ScopedLock sl (lifecycleLock);
        closeInternal();
    }

    bool isOpen() override                              { return isOpen_.load(); }
    bool isPlaying() override                           { return isPlaying_.load(); }
    String getLastError() override                      { return getLastErrorCopy(); }

    //==============================================================================
    void start (AudioIODeviceCallback* newCallback) override
    {
        const ScopedLock sl (lifecycleLock);

        if (! isOpen_.load())
            newCallback = nullptr;

        if (newCallback != callback.load())
        {
            if (newCallback != nullptr)
                newCallback->audioDeviceAboutToStart (this);

            setCallback (newCallback);

            if (newCallback != nullptr)
                xruns = 0;
        }

        setActive (callback.load() != nullptr);
    }

    void stop() override
    {
        start (nullptr);
    }

    //==============================================================================
    int getCurrentBufferSizeSamples() override          { return currentBufferSize.load(); }
    double getCurrentSampleRate() override              { return currentSampleRate.load(); }
    int getCurrentBitDepth() override                   { return 32; }

    BigInteger getActiveOutputChannels() const override { return enabledOutputChannels; }
    BigInteger getActiveInputChannels()  const override { return enabledInputChannels; }

    int getOutputLatencyInSamples() override
    {
        const ScopedLock sl (lifecycleLock);

        auto latency = playbackStream != nullptr ? getLatencyInSamples (playbackStream->stream) : 0;

        // Input data is handed to the callback one block after capture, so add
        // an extra block of latency when doing full-duplex I/O.
        if (playbackStream != nullptr && captureStream != nullptr)
            latency += jmax (1, getCurrentBufferSizeSamples());

        return latency;
    }

    int getInputLatencyInSamples() override
    {
        const ScopedLock sl (lifecycleLock);

        auto latency = captureStream != nullptr ? getLatencyInSamples (captureStream->stream) : 0;

        if (playbackStream != nullptr && captureStream != nullptr)
            latency += jmax (1, getCurrentBufferSizeSamples());

        return latency;
    }

    int getXRunCount() const noexcept override          { return xruns.load(); }

    std::optional<String> getRoutedOutputDeviceName() const override
    {
        const auto nodeId = playbackNodeId.load();

        if (nodeId == SPA_ID_INVALID)
            return {};

        // This performs a registry round-trip, which is why the method isn't
        // allowed to be called from a realtime thread.
        const auto peerName = PipeWireRegistryScanner::findRoutedPeerName (nodeId, true);

        if (peerName.isEmpty())
            return {};

        return peerName;
    }

    //==============================================================================
    String sinkKey, sourceKey;

private:
    //==============================================================================
    struct StreamData
    {
        PipeWireAudioIODevice* owner = nullptr;
        struct pw_stream* stream = nullptr;
        struct spa_hook listener {};
        bool isCapture = false;
        int numChannels = 0;
        uint32_t nodeId = SPA_ID_INVALID;
        std::atomic<bool> isReady { false };
    };

    //==============================================================================
    static BigInteger restrictToKnownChannels (const BigInteger& requested, int maxChannels)
    {
        auto result = requested;

        if (maxChannels > 0)
            for (int i = maxChannels; i <= result.getHighestBit(); ++i)
                result.clearBit (i);

        return result;
    }

    static uint32_t findNodeId (const Array<PipeWireEndpoint>& endpoints, const String& key)
    {
        for (auto& endpoint : endpoints)
        {
            if (endpoint.name == key || (endpoint.objectPath.isNotEmpty() && endpoint.objectPath == key))
                return endpoint.globalId;
        }

        return SPA_ID_INVALID;
    }

    //==============================================================================
    void setLastError (const String& message)
    {
        const ScopedLock sl (errorLock);
        lastError = message;
    }

    String getLastErrorCopy() const
    {
        const ScopedLock sl (errorLock);
        return lastError;
    }

    //==============================================================================
    // Swaps the callback and makes sure that the previous callback isn't being
    // invoked anymore before the swap completes.
    void setCallback (AudioIODeviceCallback* newCallback)
    {
        callbackGeneration.fetch_add (1, std::memory_order_acq_rel);
        auto* const oldCallback = callback.exchange (nullptr, std::memory_order_acq_rel);

        // No new invocations can start now, so wait for the running ones.
        while (activeCallbacks.load (std::memory_order_acquire) != 0)
            Thread::sleep (1);

        if (oldCallback != nullptr)
            oldCallback->audioDeviceStopped();

        callback.store (newCallback, std::memory_order_release);
        callbackGeneration.fetch_add (1, std::memory_order_acq_rel);
        isPlaying_ = (newCallback != nullptr);
    }

    // Invokes the JUCE callback without allocating or locking. If the callback
    // is swapped while this invocation is in flight, the invocation is
    // abandoned and the outputs are silenced.
    void invokeCallback (const float* const* inputData, int numInputs,
                         float* const* outputData, int numOutputs, int numSamples)
    {
        const auto generation = callbackGeneration.load (std::memory_order_acquire);
        auto* const cb = callback.load (std::memory_order_acquire);

        activeCallbacks.fetch_add (1, std::memory_order_acq_rel);

        if (cb == nullptr
             || callbackGeneration.load (std::memory_order_acquire) != generation
             || callback.load (std::memory_order_acquire) != cb)
        {
            activeCallbacks.fetch_sub (1, std::memory_order_release);

            for (int i = 0; i < numOutputs; ++i)
                if (outputData[i] != nullptr)
                    zeromem (outputData[i], (size_t) numSamples * sizeof (float));

            return;
        }

        cb->audioDeviceIOCallbackWithContext (inputData, numInputs, outputData, numOutputs, numSamples, {});
        activeCallbacks.fetch_sub (1, std::memory_order_release);
    }

    bool isFollowingDefaultSink() const
    {
        const ScopedLock sl (lifecycleLock);
        return followsDefaultSink;
    }

    bool isFollowingDefaultSource() const
    {
        const ScopedLock sl (lifecycleLock);
        return followsDefaultSource;
    }

    String getCurrentDefaultSinkKey() const
    {
        const ScopedLock sl (lifecycleLock);
        return currentDefaultSinkKey;
    }

    String getCurrentDefaultSourceKey() const
    {
        const ScopedLock sl (lifecycleLock);
        return currentDefaultSourceKey;
    }

    void setCurrentDefaultSinkKey (const String& key)
    {
        const ScopedLock sl (lifecycleLock);
        currentDefaultSinkKey = key;
    }

    void setCurrentDefaultSourceKey (const String& key)
    {
        const ScopedLock sl (lifecycleLock);
        currentDefaultSourceKey = key;
    }

    //==============================================================================
    String openInternal (const BigInteger& inputChannels,
                         const BigInteger& outputChannels,
                         double sampleRate,
                         int bufferSizeSamples)
    {
        closeInternal();

        requestedSampleRate = sampleRate > 0 ? sampleRate : 48000.0;
        requestedBufferSize = bufferSizeSamples > 0 ? bufferSizeSamples : getDefaultBufferSize();

        enabledInputChannels  = restrictToKnownChannels (inputChannels,  inputChannelNames.size());
        enabledOutputChannels = restrictToKnownChannels (outputChannels, outputChannelNames.size());

        if (enabledInputChannels.isZero() && enabledOutputChannels.isZero())
        {
            setLastError ("No channels were selected for the PipeWire device");
            return getLastErrorCopy();
        }

        setLastError ({});

        if (! loadPipeWireLibrary())
        {
            setLastError ("PipeWire is not available on this system");
            return getLastErrorCopy();
        }

        // Resolve the ids of the requested nodes. Node ids can change when the
        // server restarts or devices are unplugged, so we always re-scan before
        // opening the device.
        PipeWireScanResult scanResult;

        if (! PipeWireRegistryScanner::scan (scanResult))
        {
            setLastError ("Could not connect to the PipeWire server");
            return getLastErrorCopy();
        }

        if (! resolveTargets (scanResult))
            return getLastErrorCopy();

        // Enough room for any quantum the graph is likely to hand us. Larger
        // blocks are clipped defensively in the process callbacks.
        maxBlockFrames = jmax (8192, requestedBufferSize);

        streamErrorNotified = false;
        pendingRestart = false;
        serverLost = false;
        resumeAfterRecovery = false;
        pendingLostNotify = false;
        needsRebuildAfterLoss = false;
        pendingErrorNotify = false;

        if (! setupServerObjects())
        {
            closeInternal();
            return getLastErrorCopy();
        }

        if (! createStreamObjects())
        {
            teardownServerObjects();
            return getLastErrorCopy();
        }

        // The format negotiation events can arrive asynchronously, so until
        // the first process callback reports the real values, fall back to the
        // values that were requested in open().
        if (currentSampleRate.load() <= 0)
            currentSampleRate = requestedSampleRate;

        if (currentBufferSize.load() <= 0)
            currentBufferSize = requestedBufferSize;

        isOpen_ = true;
        isPlaying_ = false;
        xruns = 0;

        startMonitorThread();
        return {};
    }

    void closeInternal()
    {
        shuttingDown = true;

        stopMonitorThread();
        setCallback (nullptr);
        setActive (false);

        teardownServerObjects();

        outputPtrs.free();
        inputPtrs.free();
        outputPtrCapacity = 0;
        inputPtrCapacity = 0;
        captureBuffer.setSize (0, 0);
        captureSamples = 0;

        sinkNodeId = SPA_ID_INVALID;
        sourceNodeId = SPA_ID_INVALID;
        followsDefaultSink = false;
        followsDefaultSource = false;
        currentDefaultSinkKey.clear();
        currentDefaultSourceKey.clear();

        enabledInputChannels.clear();
        enabledOutputChannels.clear();

        currentSampleRate = 0.0;
        currentBufferSize = 0;
        serverLost = false;
        pendingRestart = false;
        resumeAfterRecovery = false;
        pendingLostNotify = false;
        needsRebuildAfterLoss = false;
        pendingErrorNotify = false;

        isOpen_ = false;
        isPlaying_ = false;
        shuttingDown = false;
    }

    //==============================================================================
    bool resolveTargets (const PipeWireScanResult& scanResult)
    {
        followsDefaultSink = false;
        followsDefaultSource = false;
        currentDefaultSinkKey = scanResult.defaultSinkKey;
        currentDefaultSourceKey = scanResult.defaultSourceKey;

        if (sinkKey.isNotEmpty())
        {
            sinkNodeId = findNodeId (scanResult.sinks, sinkKey);

            if (sinkNodeId == SPA_ID_INVALID)
            {
                setLastError ("The PipeWire output device \"" + sinkKey + "\" is not available");
                return false;
            }

            followsDefaultSink = scanResult.defaultSinkKey.isNotEmpty()
                                   && scanResult.defaultSinkKey == sinkKey;
        }

        if (sourceKey.isNotEmpty())
        {
            sourceNodeId = findNodeId (scanResult.sources, sourceKey);

            if (sourceNodeId == SPA_ID_INVALID)
            {
                setLastError ("The PipeWire input device \"" + sourceKey + "\" is not available");
                return false;
            }

            followsDefaultSource = scanResult.defaultSourceKey.isNotEmpty()
                                     && scanResult.defaultSourceKey == sourceKey;
        }

        return true;
    }

    //==============================================================================
    bool setupServerObjects()
    {
        pwLoop = juce::pw_loop_new (nullptr);

        if (pwLoop == nullptr)
        {
            setLastError ("Could not create a PipeWire loop");
            return false;
        }

        pwContext = juce::pw_context_new (pwLoop, nullptr, 0);

        if (pwContext == nullptr)
        {
            setLastError ("Could not create a PipeWire context");
            return false;
        }

        pwCore = juce::pw_context_connect (pwContext, nullptr, 0);

        if (pwCore == nullptr)
        {
            setLastError ("Could not connect to the PipeWire server");
            return false;
        }

        juce::pw_core_add_listener (pwCore, &coreListener, &getDeviceCoreEvents(), this);

        // The loop thread dispatches PipeWire's messages, including the
        // replies that pw_stream_connect() and pw_loop_invoke() wait for.
        audioThread = std::make_unique<AudioThread> (*this);
        audioThread->startThread (Thread::Priority::high);
        return true;
    }

    void teardownServerObjects()
    {
        if (audioThread != nullptr)
        {
            // Stream destruction must happen on the loop thread.
            destroyStreamObjectsOnLoopThread();

            audioThread->stopThread (2000);
            audioThread.reset();
        }
        else
        {
            destroyStreamObjectsOnLoop();
        }

        if (pwCore != nullptr)
        {
            juce::pw_core_disconnect (pwCore);
            pwCore = nullptr;
        }

        if (pwContext != nullptr)
        {
            juce::pw_context_destroy (pwContext);
            pwContext = nullptr;
        }

        if (pwLoop != nullptr)
        {
            juce::pw_loop_destroy (pwLoop);
            pwLoop = nullptr;
        }
    }

    //==============================================================================
    bool createStreamObjects()
    {
        streamsStartedEvent.reset();

        if (! enabledOutputChannels.isZero())
        {
            playbackStream = std::make_unique<StreamData>();
            playbackStream->owner = this;
            playbackStream->isCapture = false;
            playbackStream->numChannels = enabledOutputChannels.countNumberOfSetBits();
        }

        if (! enabledInputChannels.isZero())
        {
            captureStream = std::make_unique<StreamData>();
            captureStream->owner = this;
            captureStream->isCapture = true;
            captureStream->numChannels = enabledInputChannels.countNumberOfSetBits();
        }

        // Create the streams before opening the connections: the connect calls
        // block until the server replies, which requires the running loop.
        if (playbackStream != nullptr && ! createStream (*playbackStream))
            return false;

        if (captureStream != nullptr && ! createStream (*captureStream))
            return false;

        if (playbackStream != nullptr && ! connectStream (*playbackStream, sinkNodeId))
            return false;

        if (captureStream != nullptr && ! connectStream (*captureStream, sourceNodeId))
            return false;

        if (! waitForStreamsToStart())
            return false;

        // Preallocate everything the process callbacks need.
        const int outChans = enabledOutputChannels.countNumberOfSetBits();
        const int inChans  = enabledInputChannels.countNumberOfSetBits();

        outputPtrs.calloc ((size_t) jmax (1, outChans) + 2);
        outputPtrCapacity = jmax (1, outChans) + 2;
        inputPtrs.calloc ((size_t) jmax (1, inChans) + 2);
        inputPtrCapacity = jmax (1, inChans) + 2;

        if (inChans > 0)
            captureBuffer.setSize (inChans, maxBlockFrames, false, false, true);

        return true;
    }

    bool createStream (StreamData& data)
    {
        const auto clientName = "JUCE-PipeWire-" + (data.isCapture ? String ("Input") : String ("Output"))
                                  + "-" + String ((int) ++streamIdCounter);

        auto* props = createEmptyPipeWireProperties();

        if (props == nullptr)
        {
            setLastError ("Could not create a PipeWire stream");
            return false;
        }

        juce::pw_properties_set (props, PW_KEY_MEDIA_TYPE, "Audio");
        juce::pw_properties_set (props, PW_KEY_MEDIA_CATEGORY, data.isCapture ? "Capture" : "Playback");
        juce::pw_properties_set (props, PW_KEY_MEDIA_ROLE, "Music");
        juce::pw_properties_set (props, PW_KEY_NODE_NAME, clientName.toRawUTF8());
        juce::pw_properties_set (props, PW_KEY_NODE_DESCRIPTION, clientName.toRawUTF8());

        data.stream = juce::pw_stream_new (pwCore, clientName.toRawUTF8(), props);

        if (data.stream == nullptr)
        {
            juce::pw_properties_free (props);
            setLastError ("Could not create a PipeWire stream");
            return false;
        }

        juce::pw_stream_add_listener (data.stream, &data.listener, &getStreamEvents(), &data);
        return true;
    }

    bool connectStream (StreamData& data, uint32_t targetId)
    {
        // Build a format pod requesting floating point, non-interleaved audio.
        uint8_t podBuffer[512];
        auto builder = SPA_POD_BUILDER_INIT (podBuffer, sizeof podBuffer);

        struct spa_audio_info_raw info {};
        info.format = SPA_AUDIO_FORMAT_F32P;
        info.channels = (uint32_t) data.numChannels;
        info.rate = (uint32_t) requestedSampleRate;

        // Preserve the channel positions selected by the caller.
        const auto& channelNames = data.isCapture ? inputChannelNames : outputChannelNames;
        const auto& enabledChannels = data.isCapture ? enabledInputChannels : enabledOutputChannels;
        int position = 0;

        for (int channel = 0; channel < channelNames.size() && position < SPA_AUDIO_MAX_CHANNELS; ++channel)
            if (enabledChannels[channel])
                info.position[position++] = spa_type_audio_channel_from_short_name (channelNames[channel].toRawUTF8());

        const struct spa_pod* params[1];
        params[0] = spa_format_audio_raw_build (&builder, SPA_PARAM_EnumFormat, &info);

        const auto direction = data.isCapture ? PW_DIRECTION_INPUT : PW_DIRECTION_OUTPUT;
        const auto flags = (enum pw_stream_flags) (PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS);
        const auto result = juce::pw_stream_connect (data.stream, direction, targetId, flags, params, 1);

        if (result < 0)
        {
            setLastError ("Could not connect to the PipeWire server (error " + String (-result) + ")");
            return false;
        }

        // The node id may not be assigned yet: pw_stream_connect() is
        // asynchronous, so it is also refreshed once the stream is ready.
        refreshStreamNodeId (data);
        return true;
    }

    bool waitForStreamsToStart()
    {
        if (! streamsStartedEvent.wait (10000))
            setLastError ("Timed out waiting for the PipeWire server to start the streams");

        return getLastErrorCopy().isEmpty();
    }

    //==============================================================================
    static int destroyStreamsCallback (struct spa_loop*, bool, uint32_t, const void*, size_t, void* userData)
    {
        static_cast<PipeWireAudioIODevice*> (userData)->destroyStreamObjectsOnLoop();
        return 0;
    }

    void destroyStreamObjectsOnLoopThread()
    {
        if (pwLoop == nullptr)
        {
            destroyStreamObjectsOnLoop();
            return;
        }

        juce::pw_loop_invoke (pwLoop, &destroyStreamsCallback, 1, nullptr, 0, true, this);
    }

    void destroyStreamObjectsOnLoop()
    {
        if (playbackStream != nullptr)
        {
            if (playbackStream->stream != nullptr)
                juce::pw_stream_destroy (playbackStream->stream);

            playbackStream.reset();
        }

        if (captureStream != nullptr)
        {
            if (captureStream->stream != nullptr)
                juce::pw_stream_destroy (captureStream->stream);

            captureStream.reset();
        }

        playbackNodeId = SPA_ID_INVALID;
        captureNodeId = SPA_ID_INVALID;
    }

    void setActive (bool shouldBeActive)
    {
        const ScopedLock sl (lifecycleLock);

        if (playbackStream != nullptr && playbackStream->stream != nullptr)
            juce::pw_stream_set_active (playbackStream->stream, shouldBeActive);

        if (captureStream != nullptr && captureStream->stream != nullptr)
            juce::pw_stream_set_active (captureStream->stream, shouldBeActive);
    }

    //==============================================================================
    static const struct pw_stream_events& getStreamEvents()
    {
        static struct pw_stream_events events {};

        if (events.version == 0)
        {
            events.version = PW_VERSION_STREAM_EVENTS;
            events.state_changed = [] (void* data, enum pw_stream_state /* oldState */,
                                       enum pw_stream_state state, const char* error)
            {
                auto& streamData = *static_cast<StreamData*> (data);
                streamData.owner->handleStreamStateChanged (streamData, state, error);
            };
            events.param_changed = [] (void* data, uint32_t, const struct spa_pod* param)
            {
                auto& streamData = *static_cast<StreamData*> (data);
                streamData.owner->handleStreamParamChanged (param);
            };
            events.process = [] (void* data)
            {
                auto& streamData = *static_cast<StreamData*> (data);
                streamData.owner->handleStreamProcess (streamData);
            };
        }

        return events;
    }

    static const struct pw_core_events& getDeviceCoreEvents()
    {
        static struct pw_core_events events {};

        if (events.version == 0)
        {
            events.version = PW_VERSION_CORE_EVENTS;
            events.error = [] (void* data, uint32_t, int, int, const char*)
            {
                static_cast<PipeWireAudioIODevice*> (data)->handleServerLost();
            };
        }

        return events;
    }

    void handleServerLost()
    {
        if (shuttingDown.load())
            return;

        if (! serverLost.exchange (true))
            pendingLostNotify = true;

        needsRebuildAfterLoss = true;
    }

    //==============================================================================
    void handleStreamStateChanged (StreamData& data, enum pw_stream_state state,
                                   const char* error)
    {
        if (shuttingDown.load())
            return;

        JUCE_PIPEWIRE_LOG ("PipeWire stream changed state to " << juce::pw_stream_state_as_string (state)
                             << (error != nullptr ? (String (": ") + String (error)) : String()));

        if (state == PW_STREAM_STATE_ERROR)
        {
            const auto message = error != nullptr ? String::fromUTF8 (error)
                                                  : String ("The PipeWire stream failed");

            setLastError (message);

            if (! streamErrorNotified.exchange (true))
            {
                isPlaying_ = false;
                resumeAfterRecovery = true;

                // The monitor thread notifies the host and rebuilds the
                // streams; user callbacks are never invoked from here.
                pendingErrorNotify = true;
                pendingRestart = true;
            }

            streamsStartedEvent.signal();
            return;
        }

        if (state == PW_STREAM_STATE_PAUSED)
        {
            refreshStreamNodeId (data);
            data.isReady = true;

            const bool allStreamsReady = (playbackStream == nullptr || playbackStream->isReady.load())
                                      && (captureStream  == nullptr || captureStream->isReady.load());

            if (allStreamsReady)
                streamsStartedEvent.signal();
        }
    }

    void refreshStreamNodeId (StreamData& data)
    {
        const auto nodeId = juce::pw_stream_get_node_id (data.stream);

        if (nodeId == SPA_ID_INVALID)
            return;

        data.nodeId = nodeId;

        if (data.isCapture)
            captureNodeId = nodeId;
        else
            playbackNodeId = nodeId;
    }

    void handleStreamParamChanged (const struct spa_pod* param)
    {
        if (shuttingDown.load() || param == nullptr)
            return;

        uint32_t mediaType, mediaSubtype;

        if (spa_format_parse (param, &mediaType, &mediaSubtype) < 0)
            return;

        if (mediaType != SPA_MEDIA_TYPE_audio || mediaSubtype != SPA_MEDIA_SUBTYPE_raw)
            return;

        struct spa_audio_info_raw info {};

        if (spa_format_audio_raw_parse (param, &info) < 0)
            return;

        if (info.rate > 0)
            currentSampleRate = (double) info.rate;
    }

    void handleStreamProcess (StreamData& data)
    {
        if (shuttingDown.load())
            return;

        if (data.isCapture)
            processCapture (data);
        else
            processPlayback (data);
    }

    //==============================================================================
    static int getPlaneMaxFrames (const struct spa_data& data)
    {
        if (data.data == nullptr || data.chunk == nullptr || data.maxsize < sizeof (float))
            return 0;

        const auto offset = data.chunk->offset % jmax (1u, data.maxsize);
        return (int) ((data.maxsize - offset) / sizeof (float));
    }

    static float* getPlaneSamples (const struct spa_data& data)
    {
        if (data.data == nullptr || data.chunk == nullptr)
            return nullptr;

        const auto offset = data.chunk->offset % jmax (1u, data.maxsize);
        return (float*) (((char*) data.data) + offset);
    }

    // Playback buffers describe the block that the graph wants us to fill, so
    // the requested size is authoritative, but it must be clamped to what each
    // plane can actually hold.
    static int getPlaybackNumSamples (const struct pw_buffer& buffer, int capacity)
    {
        if (buffer.buffer == nullptr || buffer.buffer->n_datas == 0)
            return 0;

        int frames = buffer.requested > 0 ? (int) buffer.requested : capacity;

        for (uint32_t i = 0; i < buffer.buffer->n_datas; ++i)
            frames = jmin (frames, getPlaneMaxFrames (buffer.buffer->datas[i]));

        return jmin (frames, capacity);
    }

    // Capture buffers contain the data that the graph has written, so the
    // chunk size (clamped to the plane capacity) is authoritative.
    static int getCaptureNumSamples (const struct pw_buffer& buffer, int capacity)
    {
        if (buffer.buffer == nullptr || buffer.buffer->n_datas == 0)
            return 0;

        int frames = capacity;

        for (uint32_t i = 0; i < buffer.buffer->n_datas; ++i)
        {
            const auto& data = buffer.buffer->datas[i];

            if (data.data == nullptr || data.chunk == nullptr || data.maxsize < sizeof (float))
                return 0;

            const auto valid = data.chunk->stride > 0 ? (int) (data.chunk->size / data.chunk->stride)
                                                       : (int) (data.chunk->size / 4);
            frames = jmin (frames, jmin (valid, getPlaneMaxFrames (data)));
        }

        return jmin (frames, capacity);
    }

    //==============================================================================
    void processPlayback (StreamData& data)
    {
        auto* buffer = juce::pw_stream_dequeue_buffer (data.stream);

        if (buffer == nullptr || buffer->buffer == nullptr)
        {
            xruns++;
            return;
        }

        auto* spaBuffer = buffer->buffer;
        const auto numSamples = getPlaybackNumSamples (*buffer, maxBlockFrames);
        const auto numChannels = jmin ((int) spaBuffer->n_datas,
                                       outputPtrCapacity,
                                       enabledOutputChannels.countNumberOfSetBits());

        if (numSamples <= 0 || numChannels <= 0)
        {
            xruns++;
            juce::pw_stream_queue_buffer (data.stream, buffer);
            return;
        }

        currentBufferSize = numSamples;

        for (int i = 0; i < numChannels; ++i)
            outputPtrs[i] = getPlaneSamples (spaBuffer->datas[i]);

        // If we're recording as well as playing, point the input channels at
        // the data that was captured in the most recent capture callback.
        const auto numInputChannels = captureBuffer.getNumChannels();
        float** inputData = nullptr;

        if (numInputChannels > 0)
        {
            inputData = inputPtrs.getData();

            if (captureBuffer.getNumSamples() < numSamples)
            {
                // The capture buffer is smaller than the playback block, which
                // shouldn't normally happen. Discard the captured data.
                captureSamples = 0;
                xruns++;
            }
            else if (captureSamples > numSamples)
            {
                captureSamples = numSamples;
            }
            else if (captureSamples < numSamples)
            {
                captureBuffer.clear (captureSamples, numSamples - captureSamples);
            }

            for (int i = 0; i < numInputChannels; ++i)
                inputPtrs[i] = const_cast<float*> (captureBuffer.getReadPointer (i, 0));
        }

        invokeCallback (inputData, numInputChannels,
                        outputPtrs.getData(), numChannels,
                        numSamples);

        for (uint32_t i = 0; i < spaBuffer->n_datas; ++i)
        {
            if (auto* chunk = spaBuffer->datas[i].chunk)
            {
                chunk->size = i < (uint32_t) numChannels ? (uint32_t) (numSamples * 4) : 0;
                chunk->stride = 4;
            }
        }

        captureSamples = 0;
        juce::pw_stream_queue_buffer (data.stream, buffer);
    }

    void processCapture (StreamData& data)
    {
        auto* buffer = juce::pw_stream_dequeue_buffer (data.stream);

        if (buffer == nullptr || buffer->buffer == nullptr)
        {
            xruns++;
            return;
        }

        auto* spaBuffer = buffer->buffer;
        const auto numSamples = getCaptureNumSamples (*buffer, maxBlockFrames);
        const auto numChannels = jmin ((int) spaBuffer->n_datas,
                                       inputPtrCapacity,
                                       enabledInputChannels.countNumberOfSetBits());

        if (numSamples <= 0 || numChannels <= 0)
        {
            xruns++;
            juce::pw_stream_queue_buffer (data.stream, buffer);
            return;
        }

        // If we're only recording, drive the callback directly from here,
        // otherwise store the data for the next playback block.
        if (playbackStream == nullptr)
        {
            currentBufferSize = numSamples;

            for (int i = 0; i < numChannels; ++i)
                inputPtrs[i] = getPlaneSamples (spaBuffer->datas[i]);

            invokeCallback (inputPtrs.getData(), numChannels, nullptr, 0, numSamples);
        }
        else
        {
            if (numChannels > captureBuffer.getNumChannels() || numSamples > captureBuffer.getNumSamples())
            {
                captureSamples = 0;
                xruns++;
            }
            else
            {
                captureBuffer.clear (0, numSamples);

                for (int i = 0; i < numChannels; ++i)
                    if (auto* src = getPlaneSamples (spaBuffer->datas[i]))
                        memcpy (captureBuffer.getWritePointer (i, 0), src,
                                (size_t) numSamples * sizeof (float));

                captureSamples = numSamples;
            }
        }

        juce::pw_stream_queue_buffer (data.stream, buffer);
    }

    //==============================================================================
    static int getLatencyInSamples (struct pw_stream* stream)
    {
        struct pw_time time {};

        // The rate fraction describes the duration of one tick, so the delay
        // value is already expressed in samples.
        if (juce::pw_stream_get_time (stream, &time) == 0 && time.delay > 0)
            return (int) time.delay;

        return 0;
    }

    //==============================================================================
    class AudioThread final : public Thread
    {
    public:
        explicit AudioThread (PipeWireAudioIODevice& ownerToUse)
            : Thread ("PipeWire Audio"), owner (ownerToUse)
        {
        }

        void run() override
        {
            while (! threadShouldExit())
                juce::pw_loop_iterate (owner.pwLoop, 100);
        }

    private:
        PipeWireAudioIODevice& owner;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioThread)
    };

    //==============================================================================
    // Watches the graph so that we can follow the session default device and
    // rebuild the streams when the server goes away and comes back.
    class MonitorThread final : public Thread
    {
    public:
        explicit MonitorThread (PipeWireAudioIODevice& ownerToUse)
            : Thread ("PipeWire Monitor"), owner (ownerToUse)
        {
        }

        void run() override
        {
            while (! threadShouldExit())
            {
                for (int i = 0; i < 10 && ! threadShouldExit(); ++i)
                    Thread::sleep (100);

                if (threadShouldExit())
                    break;

                if (owner.shuttingDown.load() || ! owner.isOpen_.load())
                    continue;

                owner.checkGraph();
            }
        }

    private:
        PipeWireAudioIODevice& owner;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MonitorThread)
    };

    void startMonitorThread()
    {
        monitorThread = std::make_unique<MonitorThread> (*this);
        monitorThread->startThread (Thread::Priority::low);
    }

    void stopMonitorThread()
    {
        if (monitorThread != nullptr)
        {
            monitorThread->stopThread (2000);
            monitorThread.reset();
        }
    }

    void checkGraph()
    {
        if (pendingErrorNotify.load())
            notifyStreamError();

        if (pendingLostNotify.load())
            notifyServerLost();

        PipeWireScanResult scanResult;
        const bool serverOk = PipeWireRegistryScanner::scan (scanResult);

        if (! serverOk)
        {
            if (! serverLost.exchange (true))
                pendingLostNotify = true;

            needsRebuildAfterLoss = true;
            return;
        }

        serverLost = false;

        // Recover from a server restart, or from a stream error, by rebuilding
        // the server-side objects.
        if (needsRebuildAfterLoss || pendingRestart.exchange (false) || resumeAfterRecovery.load())
        {
            uint32_t newSink = sinkNodeId;
            uint32_t newSource = sourceNodeId;

            // The node ids from the previous server session are meaningless, so
            // the targets have to be resolved again by key. If the devices
            // aren't there yet we keep the rebuild pending and retry.
            if (sinkKey.isNotEmpty())
            {
                const auto key = (isFollowingDefaultSink() && scanResult.defaultSinkKey.isNotEmpty())
                                     ? scanResult.defaultSinkKey : sinkKey;

                if (auto id = findNodeId (scanResult.sinks, key); id != SPA_ID_INVALID)
                    newSink = id;
                else
                    return;
            }

            if (sourceKey.isNotEmpty())
            {
                const auto key = (isFollowingDefaultSource() && scanResult.defaultSourceKey.isNotEmpty())
                                     ? scanResult.defaultSourceKey : sourceKey;

                if (auto id = findNodeId (scanResult.sources, key); id != SPA_ID_INVALID)
                    newSource = id;
                else
                    return;
            }

            needsRebuildAfterLoss = false;
            setCurrentDefaultSinkKey (scanResult.defaultSinkKey);
            setCurrentDefaultSourceKey (scanResult.defaultSourceKey);
            restartServerSide (newSink, newSource, true);
            return;
        }

        // Follow a change of the session default device.
        if (isFollowingDefaultSink()
             && scanResult.defaultSinkKey.isNotEmpty()
             && scanResult.defaultSinkKey != getCurrentDefaultSinkKey())
        {
            if (auto id = findNodeId (scanResult.sinks, scanResult.defaultSinkKey); id != SPA_ID_INVALID)
            {
                setCurrentDefaultSinkKey (scanResult.defaultSinkKey);
                restartServerSide (id, sourceNodeId, false);
                return;
            }
        }

        if (isFollowingDefaultSource()
             && scanResult.defaultSourceKey.isNotEmpty()
             && scanResult.defaultSourceKey != getCurrentDefaultSourceKey())
        {
            if (auto id = findNodeId (scanResult.sources, scanResult.defaultSourceKey); id != SPA_ID_INVALID)
            {
                setCurrentDefaultSourceKey (scanResult.defaultSourceKey);
                restartServerSide (sinkNodeId, id, false);
            }
        }
    }

    // Must be called with the lifecycleLock held: that guarantees the
    // callback pointer can't be swapped while we notify the host.
    void notifyHostStopped (const String& message)
    {
        if (auto* cb = callback.load (std::memory_order_acquire))
        {
            cb->audioDeviceError (message);
            cb->audioDeviceStopped();
        }
    }

    void notifyServerLost()
    {
        if (! lifecycleLock.tryEnter())
            return;

        struct Unlocker
        {
            ~Unlocker() { lock.exit(); }
            CriticalSection& lock;
        } unlocker { lifecycleLock };

        setLastError ("Lost the connection to the PipeWire server");
        resumeAfterRecovery = isPlaying_.load();
        isPlaying_ = false;
        pendingLostNotify = false;

        notifyHostStopped (getLastErrorCopy());
    }

    void notifyStreamError()
    {
        if (! lifecycleLock.tryEnter())
            return;

        struct Unlocker
        {
            ~Unlocker() { lock.exit(); }
            CriticalSection& lock;
        } unlocker { lifecycleLock };

        if (! pendingErrorNotify.exchange (false))
            return;

        notifyHostStopped (getLastErrorCopy());
    }

    // Rebuilds the server-side objects, optionally resuming playback.
    void restartServerSide (uint32_t newSinkNodeId, uint32_t newSourceNodeId, bool resumeAfterwards)
    {
        if (! lifecycleLock.tryEnter())
            return;

        struct Unlocker
        {
            ~Unlocker() { lock.exit(); }
            CriticalSection& lock;
        } unlocker { lifecycleLock };

        if (shuttingDown.load() || ! isOpen_.load())
            return;

        const bool shouldResume = resumeAfterwards ? (resumeAfterRecovery.load() || isPlaying_.load())
                                                   : isPlaying_.load();

        setLastError ({});
        streamErrorNotified = false;
        pendingErrorNotify = false;
        serverLost = false;

        setActive (false);
        teardownServerObjects();

        sinkNodeId = newSinkNodeId;
        sourceNodeId = newSourceNodeId;

        streamErrorNotified = false;
        pendingRestart = false;

        if (! setupServerObjects() || ! createStreamObjects())
        {
            setLastError ("Could not reopen the PipeWire streams");
            teardownServerObjects();
            isPlaying_ = false;

            // Try again on the next monitor cycle.
            needsRebuildAfterLoss = true;
            return;
        }

        if (currentSampleRate.load() <= 0)
            currentSampleRate = requestedSampleRate;

        if (currentBufferSize.load() <= 0)
            currentBufferSize = requestedBufferSize;

        resumeAfterRecovery = false;

        if (shouldResume)
        {
            if (resumeAfterwards)
                if (auto* cb = callback.load())
                    cb->audioDeviceAboutToStart (this);

            setActive (true);
            isPlaying_ = true;
        }
    }

    //==============================================================================
    StringArray outputChannelNames, inputChannelNames;

    String lastError;
    mutable CriticalSection errorLock;

    // Serialises open/close and the monitor thread's restarts.
    CriticalSection lifecycleLock;

    std::atomic<AudioIODeviceCallback*> callback { nullptr };
    std::atomic<uint32_t> callbackGeneration { 0 };
    std::atomic<int> activeCallbacks { 0 };
    std::atomic<bool> isOpen_ { false }, isPlaying_ { false }, shuttingDown { false };
    std::atomic<double> currentSampleRate { 0.0 };
    std::atomic<int> currentBufferSize { 0 };
    std::atomic<int> xruns { 0 };
    std::atomic<uint32_t> playbackNodeId { SPA_ID_INVALID }, captureNodeId { SPA_ID_INVALID };
    std::atomic<bool> serverLost { false }, pendingRestart { false }, streamErrorNotified { false };
    std::atomic<bool> resumeAfterRecovery { false }, pendingLostNotify { false }, needsRebuildAfterLoss { false };
    std::atomic<bool> pendingErrorNotify { false };

    std::unique_ptr<StreamData> playbackStream, captureStream;
    std::unique_ptr<AudioThread> audioThread;
    std::unique_ptr<MonitorThread> monitorThread;

    struct pw_loop* pwLoop = nullptr;
    struct pw_context* pwContext = nullptr;
    struct pw_core* pwCore = nullptr;
    struct spa_hook coreListener {};

    HeapBlock<float*> outputPtrs, inputPtrs;
    int outputPtrCapacity = 0, inputPtrCapacity = 0;
    AudioBuffer<float> captureBuffer;
    int captureSamples = 0;
    int maxBlockFrames = 8192;

    double requestedSampleRate = 48000.0;
    int requestedBufferSize = 512;
    uint32_t sinkNodeId = SPA_ID_INVALID;
    uint32_t sourceNodeId = SPA_ID_INVALID;
    uint32_t streamIdCounter = 0;

    bool followsDefaultSink = false, followsDefaultSource = false;
    String currentDefaultSinkKey, currentDefaultSourceKey;

    BigInteger enabledInputChannels, enabledOutputChannels;
    WaitableEvent streamsStartedEvent;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PipeWireAudioIODevice)
};

//==============================================================================
class PipeWireAudioIODeviceType final : public AudioIODeviceType,
                                        private AsyncUpdater
{
public:
    PipeWireAudioIODeviceType()
        : AudioIODeviceType ("PipeWire")
    {
        watcher = std::make_unique<DeviceListWatcher> (*this);
        watcher->startThread (Thread::Priority::low);
    }

    ~PipeWireAudioIODeviceType() override
    {
        if (watcher != nullptr)
        {
            watcher->stopThread (2000);
            watcher.reset();
        }
    }

    //==============================================================================
    void scanForDevices() override
    {
        const ScopedLock sl (scanLock);
        scanForDevicesInternal();
    }

    StringArray getDeviceNames (bool wantInputNames) const override
    {
        const ScopedLock sl (scanLock);
        jassert (hasScanned); // need to call scanForDevices() before doing this
        return wantInputNames ? inputNames : outputNames;
    }

    int getDefaultDeviceIndex (bool forInput) const override
    {
        const ScopedLock sl (scanLock);
        jassert (hasScanned); // need to call scanForDevices() before doing this

        const auto& keys = forInput ? inputKeys : outputKeys;
        const auto& defaultKey = forInput ? defaultInputKey : defaultOutputKey;

        if (defaultKey.isNotEmpty())
        {
            const auto index = keys.indexOf (defaultKey);

            if (index >= 0)
                return index;
        }

        return 0;
    }

    bool hasSeparateInputsAndOutputs() const override     { return true; }

    int getIndexOfDevice (AudioIODevice* device, bool asInput) const override
    {
        const ScopedLock sl (scanLock);
        jassert (hasScanned); // need to call scanForDevices() before doing this

        if (auto* d = dynamic_cast<PipeWireAudioIODevice*> (device))
            return asInput ? inputKeys.indexOf (d->sourceKey)
                           : outputKeys.indexOf (d->sinkKey);

        return -1;
    }

    AudioIODevice* createDevice (const String& outputDeviceName,
                                 const String& inputDeviceName) override
    {
        const ScopedLock sl (scanLock);
        jassert (hasScanned); // need to call scanForDevices() before doing this

        const auto inputIndex  = inputNames.indexOf (inputDeviceName);
        const auto outputIndex = outputNames.indexOf (outputDeviceName);

        if (inputIndex < 0 && outputIndex < 0)
            return nullptr;

        const auto hasOutput = outputIndex >= 0;

        return new PipeWireAudioIODevice (getTypeName(),
                                          hasOutput ? outputKeys.getReference (outputIndex) : String(),
                                          hasOutput ? outputDeviceName : String(),
                                          hasOutput ? outputChannels.getReference (outputIndex) : StringArray(),
                                          inputIndex >= 0 ? inputKeys.getReference (inputIndex) : String(),
                                          inputIndex >= 0 ? inputDeviceName : String(),
                                          inputIndex >= 0 ? inputChannels.getReference (inputIndex) : StringArray());
    }

private:
    //==============================================================================
    // PipeWire sends registry changes over the socket, but the JUCE device list
    // only needs to be refreshed occasionally, so we poll the graph and tell
    // the listeners when something has actually changed.
    class DeviceListWatcher final : public Thread
    {
    public:
        explicit DeviceListWatcher (PipeWireAudioIODeviceType& ownerToUse)
            : Thread ("PipeWire Device List"), owner (ownerToUse)
        {
        }

        void run() override
        {
            while (! threadShouldExit())
            {
                for (int i = 0; i < 10 && ! threadShouldExit(); ++i)
                    Thread::sleep (100);

                if (threadShouldExit())
                    break;

                owner.refreshDeviceListIfNeeded();
            }
        }

    private:
        PipeWireAudioIODeviceType& owner;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DeviceListWatcher)
    };

    void scanForDevicesInternal()
    {
        outputNames.clear();
        outputKeys.clear();
        outputChannels.clear();
        inputNames.clear();
        inputKeys.clear();
        inputChannels.clear();
        defaultOutputKey.clear();
        defaultInputKey.clear();

        PipeWireScanResult result;

        if (! PipeWireRegistryScanner::scan (result))
        {
            hasScanned = true;
            return;
        }

        appendDevices (result.sinks,   result.defaultSinkKey,   outputNames, outputKeys, outputChannels, defaultOutputKey);
        appendDevices (result.sources, result.defaultSourceKey, inputNames,  inputKeys,  inputChannels,  defaultInputKey);

        outputNames.appendNumbersToDuplicates (false, true);
        inputNames.appendNumbersToDuplicates (false, true);

        hasScanned = true;
    }

    void refreshDeviceListIfNeeded()
    {
        StringArray outputs, inputs;

        {
            const ScopedLock sl (scanLock);
            scanForDevicesInternal();
            outputs = outputNames;
            inputs = inputNames;
        }

        if (! hasComparedList)
        {
            hasComparedList = true;
            lastOutputNames = outputs;
            lastInputNames = inputs;
            return;
        }

        if (outputs != lastOutputNames || inputs != lastInputNames)
        {
            lastOutputNames = outputs;
            lastInputNames = inputs;
            notifyListeners();
        }
    }

    void notifyListeners()
    {
        if (MessageManager::getInstanceWithoutCreating() != nullptr)
            triggerAsyncUpdate();
        else
            callDeviceChangeListeners();
    }

    void handleAsyncUpdate() override
    {
        callDeviceChangeListeners();
    }

    //==============================================================================
    static void appendDevices (const Array<PipeWireEndpoint>& endpoints,
                               const String& defaultKey,
                               StringArray& names,
                               StringArray& keys,
                               Array<StringArray>& channels,
                               String& defaultKeyOut)
    {
        for (auto& endpoint : endpoints)
        {
            if (endpoint.name.isEmpty())
                continue;

            names.add (endpoint.displayName);
            keys.add (endpoint.name);
            channels.add (endpoint.channels);

            // The default device is identified in the metadata by its node.name
            // or its object.path.
            if (defaultKey.isNotEmpty() && defaultKeyOut.isEmpty()
                 && (endpoint.name == defaultKey || endpoint.objectPath == defaultKey))
                defaultKeyOut = endpoint.name;
        }
    }

    mutable CriticalSection scanLock;
    StringArray outputNames, outputKeys, inputNames, inputKeys;
    Array<StringArray> outputChannels, inputChannels;
    String defaultOutputKey, defaultInputKey;
    bool hasScanned = false;

    StringArray lastOutputNames, lastInputNames;
    bool hasComparedList = false;
    std::unique_ptr<DeviceListWatcher> watcher;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PipeWireAudioIODeviceType)
};

//==============================================================================
// This is called from AudioIODeviceType::createAudioIODeviceType_PipeWire(),
// which is compiled into the same translation unit.
static inline AudioIODeviceType* createAudioIODeviceType_PipeWire_Native()
{
    if (! loadPipeWireLibrary())
        return nullptr;

    return new PipeWireAudioIODeviceType();
}

} // namespace juce
