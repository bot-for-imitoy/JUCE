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

// The handle is published through a function-local static so that the dlopen()
// happens exactly once: the device list scanner, the monitor thread and the
// message thread can all reach this code at the same time, and a check-then-set
// on a plain pointer would let two of them open the library and leak one handle.
static bool loadPipeWireLibrary()
{
    static const bool loaded = []
    {
        juce_libpipewireHandle = dlopen ("libpipewire-0.3.so.0", RTLD_LAZY);

        if (juce_libpipewireHandle == nullptr)
            juce_libpipewireHandle = dlopen ("libpipewire-0.3.so", RTLD_LAZY);

        return juce_libpipewireHandle != nullptr;
    }();

    return loaded;
}

static void* juce_loadPipeWireFunction (const char* const name)
{
    if (! loadPipeWireLibrary())
        return nullptr;

    return dlsym (juce_libpipewireHandle, name);
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
JUCE_DECL_VOID_PIPEWIRE_FUNCTION (pw_init, (int* argc, char*** argv), (argc, argv))
JUCE_DECL_PIPEWIRE_FUNCTION (const char*, pw_get_library_version, (), ())
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
JUCE_DECL_PIPEWIRE_FUNCTION (int, pw_core_add_listener, (struct pw_core* core, struct spa_hook* listener, const struct pw_core_events* events, void* data), (core, listener, events, data))

// Registry / proxies
JUCE_DECL_PIPEWIRE_FUNCTION (struct pw_registry*, pw_core_get_registry, (struct pw_core* core, uint32_t version, size_t user_data_size), (core, version, user_data_size))
JUCE_DECL_PIPEWIRE_FUNCTION (int, pw_registry_add_listener, (struct pw_registry* registry, struct spa_hook* listener, const struct pw_registry_events* events, void* data), (registry, listener, events, data))
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
// The dynamic loader will happily accept any libpipewire-0.3.so, but the
// structs and event tables used here are laid out by the headers this file was
// compiled against. A runtime library that is older than those headers can
// therefore describe a different ABI, which is worth a warning rather than a
// silent misbehaviour.
static void checkRuntimeVersion()
{
    const auto* version = juce::pw_get_library_version();

    if (version == nullptr)
        return;

    int major = 0, minor = 0, micro = 0;

    if (sscanf (version, "%d.%d.%d", &major, &minor, &micro) < 2)
        return;

    const auto runtime = major * 1000000 + minor * 1000 + micro;
    constexpr int oldestSupported = 0 * 1000000 + 3 * 1000 + 32;

    if (runtime < oldestSupported)
    {
        JUCE_PIPEWIRE_LOG ("libpipewire " << version << " is older than the 0.3.32 this backend needs");
    }

   #if defined (PW_MAJOR_VERSION) && defined (PW_MINOR_VERSION) && defined (PW_MICRO_VERSION)
    constexpr int headers = PW_MAJOR_VERSION * 1000000 + PW_MINOR_VERSION * 1000 + PW_MICRO_VERSION;

    if (runtime < headers)
    {
        JUCE_PIPEWIRE_LOG ("libpipewire " << version << " is older than the headers this build used ("
                                          << PW_MAJOR_VERSION << "." << PW_MINOR_VERSION << "."
                                          << PW_MICRO_VERSION << ")");
    }
   #endif
}

//==============================================================================
// pw_init() has to be called before any other PipeWire entry point, and it may
// rewrite the arguments that it is given the first time around. pw_deinit()
// used to tear down the process-wide PipeWire state without any reference
// counting (before 0.3.49), so calling the pair once per connection could pull
// the library out from under every other PipeWire user in the process (the host
// application, plugins, other libraries, ...). Both calls are therefore made
// exactly once for the lifetime of the process, and the library is deliberately
// never de-initialised afterwards.
static bool initialisePipeWireLibrary()
{
    if (! loadPipeWireLibrary())
        return false;

    static const bool initialised = []
    {
        int fakeArgc = 1;
        char executableName[] = "juce";
        char* fakeArgv[] = { executableName, nullptr };
        char** fakeArgvPtr = fakeArgv;
        juce::pw_init (&fakeArgc, &fakeArgvPtr);
        checkRuntimeVersion();
        return true;
    }();

    return initialised;
}

// True when the calling thread is a JUCE thread that has been asked to stop.
// The waits in this file can last for several seconds when the server is
// unresponsive, and this lets them be abandoned promptly instead of having the
// thread killed while it is in the middle of a PipeWire call.
static bool isCallingThreadBeingStopped()
{
    if (auto* thread = Thread::getCurrentThread())
        return thread->threadShouldExit();

    return false;
}

// How long a worker thread is given to leave its loop before we leave it to
// finish on its own. Threads are never force-killed here.
static constexpr int threadStopTimeoutMs = 2000;

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
        if (! initialisePipeWireLibrary())
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
        if (! initialisePipeWireLibrary() || nodeId == SPA_ID_INVALID)
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
        // pw_core_sync() returns the sequence number that the server echoes back
        // in its done event, so that is what we match on. Relying on the bit
        // pattern of that number instead would be depending on an undocumented
        // implementation detail - and if the assumption didn't hold, quit()
        // would never be called and every scan would stall until it timed out.
        if (id == PW_ID_CORE && seq == pendingSyncSeq)
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
            uint32_t nodeId = SPA_ID_INVALID;

            if (parseNodeId (nodeIdStr, nodeId))
            {
                PortRecord port;
                port.id = id;
                port.nodeId = nodeId;
                port.channel = String::fromUTF8 (getProp (props, PW_KEY_AUDIO_CHANNEL));
                port.isInput = String::fromUTF8 (getProp (props, PW_KEY_PORT_DIRECTION)) == "in";
                ports.add (port);
            }
        }
        else if (String (type) == PW_TYPE_INTERFACE_Metadata)
        {
            MetadataRecord record;
            record.id = id;
            record.name = String::fromUTF8 (getProp (props, PW_KEY_METADATA_NAME));

            const auto known = std::any_of (metadataIds.begin(), metadataIds.end(),
                                            [id] (const MetadataRecord& r) { return r.id == id; });

            if (! known)
                metadataIds.add (record);
        }
        else if (String (type) == PW_TYPE_INTERFACE_Link)
        {
            const auto* outputNode = getProp (props, PW_KEY_LINK_OUTPUT_NODE);
            const auto* inputNode  = getProp (props, PW_KEY_LINK_INPUT_NODE);
            uint32_t outputNodeId = SPA_ID_INVALID, inputNodeId = SPA_ID_INVALID;

            if (parseNodeId (outputNode, outputNodeId) && parseNodeId (inputNode, inputNodeId))
            {
                LinkRecord link;
                link.outputNode = outputNodeId;
                link.inputNode  = inputNodeId;
                links.add (link);
            }
        }
    }

    // Node ids are always non-zero decimal numbers - 0 means PW_ID_CORE - so
    // anything else is rejected rather than being turned into a reference to
    // the core object.
    static bool parseNodeId (const char* text, uint32_t& result)
    {
        if (text == nullptr || *text == '\0')
            return false;

        char* end = nullptr;
        const auto value = std::strtoul (text, &end, 10);

        if (end == text || *end != '\0' || value == 0 || value > (unsigned long) UINT32_MAX)
            return false;

        result = (uint32_t) value;
        return true;
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

    struct MetadataRecord
    {
        uint32_t id = SPA_ID_INVALID;
        String name;
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
        //
        // Only hooks that were actually registered may be removed:
        // spa_hook_remove() dereferences a null list head if it is called for a
        // hook that was never added, which crashes on SPA versions before
        // 0.3.58 (0.3.55 and earlier called spa_list_remove() unconditionally).
        // A failed scan - for example on a machine that has libpipewire
        // installed but no server running - must not take the process down.
        if (coreHookAdded)
            spa_hook_remove (&coreHook);

        if (registryHookAdded)
            spa_hook_remove (&registryHook);

        for (auto& binding : metadataBindings)
            if (binding->hookAdded)
                spa_hook_remove (&binding->objectHook);
    }

    //==============================================================================
    bool scanInternal (PipeWireScanResult& resultToFill)
    {
        result = resultToFill;

        if (! connection.connect())
            return false;

        juce::pw_core_add_listener (connection.core, &coreHook, &getCoreEvents(), this);
        coreHookAdded = true;

        connection.registry = juce::pw_core_get_registry (connection.core, PW_VERSION_REGISTRY, 0);

        if (connection.registry == nullptr)
            return false;

        juce::pw_registry_add_listener (connection.registry, &registryHook, &getRegistryEvents(), this);
        registryHookAdded = true;

        // Wait for the initial burst of globals to arrive.
        if (! syncWithServer())
            return false;

        if (! connection.runUntilQuit (scanTimeoutMs) || failed)
            return false;

        // The "default" metadata contains the names of the default sink and
        // source. Other metadata objects can carry the same keys, so the one
        // called "default" is preferred when the graph has one - binding all of
        // them would let whichever replied last decide what the default is.
        const auto haveDefaultMetadata = std::any_of (metadataIds.begin(), metadataIds.end(),
                                                      [] (const MetadataRecord& r) { return r.name == "default"; });

        for (const auto& metadata : metadataIds)
            if (! haveDefaultMetadata || metadata.name == "default")
                bindMetadata (metadata.id);

        if (metadataBindings.size() > 0)
        {
            // A final sync ensures that all the property events have arrived
            // before we proceed.
            if (! syncWithServer())
                return false;

            if (! connection.runUntilQuit (scanTimeoutMs) || failed)
                return false;
        }

        assembleResult();
        resultToFill = result;
        return true;
    }

    // Requests a round-trip to the server and remembers the sequence number
    // that it will report back through the done event.
    bool syncWithServer()
    {
        pendingSyncSeq = juce::pw_core_sync (connection.core, PW_ID_CORE, 0);

        return pendingSyncSeq >= 0;
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
        binding->hookAdded = true;
        metadataBindings.add (std::move (binding));
    }

    //==============================================================================
    // These tables are built through a function-local static rather than by
    // checking a "version == 0" sentinel in place: scans can be started from the
    // message thread, the device-list watcher and the monitor thread at the same
    // time, and filling the struct in place would let one of them observe a
    // half-written table and call a null callback. A function-local static is
    // initialised exactly once, even under that race.
    static const struct pw_core_events& getCoreEvents()
    {
        static const struct pw_core_events events = []
        {
            struct pw_core_events result {};
            result.version = PW_VERSION_CORE_EVENTS;
            result.done = [] (void* data, uint32_t id, int seq)
            {
                static_cast<PipeWireRegistryScanner*> (data)->handleCoreDone (id, seq);
            };
            result.error = [] (void* data, uint32_t, int, int, const char*)
            {
                static_cast<PipeWireRegistryScanner*> (data)->handleCoreError();
            };
            return result;
        }();

        return events;
    }

    static const struct pw_registry_events& getRegistryEvents()
    {
        static const struct pw_registry_events events = []
        {
            struct pw_registry_events result {};
            result.version = PW_VERSION_REGISTRY_EVENTS;
            result.global = [] (void* data, uint32_t id, uint32_t, const char* type,
                                uint32_t, const struct spa_dict* props)
            {
                static_cast<PipeWireRegistryScanner*> (data)->handleGlobal (id, type, props);
            };
            result.global_remove = [] (void*, uint32_t) {};
            return result;
        }();

        return events;
    }

    static const struct pw_metadata_events& getMetadataEvents()
    {
        static const struct pw_metadata_events events = []
        {
            struct pw_metadata_events result {};
            result.version = PW_VERSION_METADATA_EVENTS;
            result.property = [] (void* data, uint32_t, const char* key, const char*, const char* value)
            {
                static_cast<PipeWireRegistryScanner*> (data)->handleMetadataProperty (key, value);
                return 0;
            };
            return result;
        }();

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
        if (value == nullptr)
            return {};

        // The default metadata stores values as JSON, e.g. {"name": "..."}.
        // This has to go through a real parser: scanning for quotes breaks on
        // escaped characters or on any other JSON layout, and the fallback
        // below then silently stops the device from following the session
        // default.
        if (auto* object = JSON::parse (String::fromUTF8 (value)).getDynamicObject())
        {
            const auto name = object->getProperty ("name");

            if (name.isString() && name.toString().isNotEmpty())
                return name.toString();
        }

        return String::fromUTF8 (value).trim();
    }

    //==============================================================================
    struct MetadataBinding
    {
        struct pw_metadata* metadata = nullptr;
        struct spa_hook objectHook {};
        bool hookAdded = false;
    };

    struct PipeWireConnection
    {
        PipeWireConnection() = default;

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

                if (isCallingThreadBeingStopped())
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
    Array<MetadataRecord> metadataIds;
    Array<std::unique_ptr<MetadataBinding>> metadataBindings;
    struct spa_hook coreHook {}, registryHook {};
    bool coreHookAdded = false, registryHookAdded = false;

    PipeWireScanResult result;
    int pendingSyncSeq = -1;
    bool failed = false;

    static constexpr int scanTimeoutMs = 5000;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PipeWireRegistryScanner)
};

//==============================================================================
// Work that has to happen on the thread that pumps the connection's loop.
//
// libpipewire marshals a client->server message straight into the connection's
// buffer from whichever thread makes the call, and there is exactly one such
// buffer per connection: two threads creating, connecting or destroying a
// stream at the same time corrupt it, which the library catches with
// assert_single_pod() in module-protocol-native (aborting the host process) or,
// depending on the timing, silently loses the message. Since 1.6 the stream
// functions additionally refuse to run - "called from wrong context" - when
// they arrive from a thread other than the one that owns the loop.
//
// The loop is pumped by AudioThread, so this queue is how the control thread
// and the monitor thread hand that work over: the loop thread runs the queued
// task between iterations, which also serialises it with the process callbacks
// the loop dispatches.
struct LoopTaskQueue final
{
    struct Task
    {
        std::function<void()> run;
        WaitableEvent finished;
        bool abandoned = false;
    };

    // Returns true if the task ran to completion. A false result means that it
    // was never started, or that it did not finish within the timeout and may
    // still be running - the caller then has to leave everything the task
    // touches allocated.
    bool post (std::function<void()> taskToRun, int timeoutMs)
    {
        auto task = std::make_shared<Task>();
        task->run = std::move (taskToRun);

        {
            const ScopedLock sl (lock);

            if (dead || threadExiting)
                return false;

            tasks.push_back (task);
        }

        if (! task->finished.wait (timeoutMs))
        {
            const ScopedLock sl (lock);
            task->abandoned = true;

            // Still queued, so it will never run and can be dropped. If it
            // isn't in the list it is running right now and cannot be stopped.
            tasks.erase (std::remove (tasks.begin(), tasks.end(), task), tasks.end());
            return false;
        }

        const ScopedLock sl (lock);
        return ! task->abandoned;
    }

    // Called on the loop thread, between iterations of the loop.
    void runPending()
    {
        for (;;)
        {
            std::shared_ptr<Task> task;

            {
                const ScopedLock sl (lock);

                if (threadExiting || tasks.empty())
                    return;

                task = tasks.front();
                tasks.erase (tasks.begin());
            }

            // A throwing task must not escape into Thread::run() - and its
            // caller would wait out the whole timeout for a signal that never
            // comes - so the queue is kept exception-free.
            try
            {
                task->run();
            }
            catch (...)
            {
            }

            const ScopedLock sl (lock);
            task->finished.signal();
        }
    }

    // Called once the loop thread has been asked to exit: nothing queued after
    // this point runs, and the callers of the queued tasks are released.
    void stop()
    {
        const ScopedLock sl (lock);
        threadExiting = true;

        for (auto& task : tasks)
        {
            task->abandoned = true;
            task->finished.signal();
        }

        tasks.clear();
    }

    // Called when the loop thread could not be stopped and its objects are
    // being leaked: the queue may outlive the device, so it must not take any
    // more work.
    void markDead()
    {
        stop();

        const ScopedLock sl (lock);
        dead = true;
    }

    // True while the calling thread is the one that pumps this queue's loop.
    // The queue is identified rather than the thread, so that the check keeps
    // working while the device that owns it is being torn down.
    static bool isCurrentThread (const LoopTaskQueue* queue)
    {
        return currentQueue == queue;
    }

    void enterLoopThread()    { currentQueue = this; }
    void leaveLoopThread()    { currentQueue = nullptr; }

    // Prepares the queue for a new connection. A queue that was marked dead
    // (its loop thread was abandoned) stays dead.
    void rearm()
    {
        const ScopedLock sl (lock);

        if (! dead)
            threadExiting = false;
    }

    CriticalSection lock;
    std::vector<std::shared_ptr<Task>> tasks;
    inline static thread_local const LoopTaskQueue* currentQueue = nullptr;
    bool threadExiting = false, dead = false;
};

//==============================================================================
/** A PipeWire sink/source pair.

    Threading contract:

    - isOpen(), isPlaying(), getCurrentSampleRate(),
      getCurrentBufferSizeSamples(), getOutputLatencyInSamples(),
      getInputLatencyInSamples() and getXRunCount() read nothing but atomics, so
      they are safe to call from a realtime thread, provided that the platform
      implements these atomics lock-free. getCurrentBitDepth() returns a
      constant, so it is safe to call from anywhere. The latency queries used to
      take the lifecycle lock and read pw_stream_get_time() directly; the values
      are now sampled by the process callbacks (see publishLatency()), which is
      what makes them usable from a host's IO callback.

    - Everything else - start(), stop(), open(), close(), the device list
      queries that hand back a container by value, getRoutedOutputDeviceName()
      and the monitor/rebuild path - may block on the lifecycle lock or on the
      server, and must not be called from a realtime thread.

    - There are exactly two threads that touch the PipeWire connection:

          loop thread ("PipeWire Audio") : pumps the loop, dispatches the
                                           process/state/param events, and runs
                                           every task posted to the LoopTaskQueue
          control thread                 : posts those tasks (stream create,
                                           connect, activate, destroy, and the
                                           connection teardown) and waits for
                                           them, with a timeout

      The process callbacks run on the loop thread, not on a data loop of their
      own: the context is created on this loop and PW_STREAM_FLAG_RT_PROCESS is
      not set, so PipeWire dispatches the node from the main loop - and therefore
      from a thread that JUCE owns, with SCHED_OTHER scheduling and no realtime
      priority. That is a limitation of this backend, not a property of
      PipeWire; making the callbacks realtime means giving the context a data
      loop and sampling the latency from the callback, and is not attempted here.

    - The lifecycle lock is held across the teardown and across the stream
      creation, and both of those wait for the loop thread, so a rebuild can hold
      it for seconds. Nothing reachable from a realtime thread may therefore take
      it - see the first point. The lock is not held while waiting for the
      streams to start, nor while the host is notified.

    - open() and close() must never be called from an audio thread at all.
      Tearing the streams down asks PipeWire to flush the callback it is running
      (pw_stream_destroy() takes the node's loop mutex through pw_loop_locked()),
      so calling them from inside the callback asks the loop thread to wait for
      itself. AudioIODevice's own contract already requires stop() to flush
      pending callbacks before returning, which no implementation can do from
      within one of them, so this is not specific to this backend.

    A loop thread that stops responding is abandoned rather than joined: the
    context, the streams and that thread are leaked on purpose, so that a stuck
    audio device cannot turn into a deadlock in the host's shutdown path. See
    abandonLoopThread().

    The host is never notified from the loop thread, and never with the
    lifecycle lock held: audioDeviceError() is sent from the monitor thread,
    audioDeviceAboutToStart() from the monitor thread during a rebuild and from
    whoever called start(), and audioDeviceStopped() from whoever detaches the
    callback - start(), stop(), open() or close().
*/
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

        // A loop thread that had to be abandoned may still be running a process
        // callback with pointers into these buffers, so they are leaked rather
        // than handed back to the allocator underneath it. This only happens for
        // a device that already failed to shut down (see abandonLoopThread()).
        if (buffersLeaked)
            buffers.release();
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
        // report the real one via getCurrentBufferSizeSamples(). The quantum is
        // requested through the stream's node.latency property, see
        // createStream().
        Array<int> sizes;

        for (int n = 16; n <= 8192; n *= 2)
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
        // Taking the host callback out of the picture waits for any IO callback
        // that is already running, and that callback may well be sitting in one
        // of the latency queries waiting for the lifecycle lock, so this has to
        // happen before the lock is taken rather than under it.
        setCallback (nullptr);

        // The monitor thread may be waiting for the lifecycle lock inside
        // checkGraph(), so it has to be stopped before that lock is taken:
        // waiting for a thread that is itself waiting for the lock we hold would
        // stall this call for the whole timeout (and leave the device half shut
        // down when the wait fails). shuttingDown makes an in-flight scan
        // abandon its work instead of touching the server objects.
        shuttingDown = true;
        stopMonitorThread();

        const auto requestedInputs  = restrictToKnownChannels (inputChannels,  inputChannelNames.size());
        const auto requestedOutputs = restrictToKnownChannels (outputChannels, outputChannelNames.size());

        String openError;

        if (requestedInputs.isZero() && requestedOutputs.isZero())
        {
            openError = "No channels were selected for the PipeWire device";
        }
        else if (! initialisePipeWireLibrary())
        {
            openError = "PipeWire is not available on this system";
        }
        else
        {
            // Resolving the target nodes means walking the registry, which is a
            // server round-trip and can take several seconds when the server is
            // unresponsive. Like the other waits in this file it happens before
            // the lifecycle lock is taken, so that a host blocking on the lock
            // (the latency queries are called from the message thread) is not
            // held up by it.
            PipeWireScanResult scanResult;

            if (! PipeWireRegistryScanner::scan (scanResult))
            {
                openError = "Could not connect to the PipeWire server";
            }
            else
            {
                const ScopedLock sl (lifecycleLock);
                openError = openInternal (requestedInputs, requestedOutputs,
                                          sampleRate, bufferSizeSamples, scanResult);
            }
        }

        if (openError.isNotEmpty())
        {
            // The monitor thread has already been stopped and shuttingDown is
            // set, so the device has to be left properly closed instead of half
            // open.
            close();
            setLastError (openError);
            return openError;
        }

        // Waiting for the server must not happen while the lifecycle lock is
        // held: the same lock is taken by close(), by the latency queries and
        // by the monitor thread, and the wait can take several seconds.
        if (! waitForStreamsToStart())
        {
            const auto error = getLastErrorCopy();
            close();
            return error;
        }

        startMonitorThread();
        return {};
    }

    void close() override
    {
        setCallback (nullptr);

        // See open(): the monitor thread is stopped before the lifecycle lock is
        // taken, because it may be blocked waiting for that same lock inside
        // checkGraph(). Waiting for it while holding the lock would stall
        // close() for the whole timeout.
        shuttingDown = true;
        stopMonitorThread();

        const ScopedLock sl (lifecycleLock);
        closeInternal();
    }

    bool isOpen() override                              { return isOpen_.load(); }
    bool isPlaying() override                           { return isPlaying_.load(); }
    String getLastError() override                      { return getLastErrorCopy(); }

    //==============================================================================
    void start (AudioIODeviceCallback* newCallback) override
    {
        if (! isOpen_.load())
            newCallback = nullptr;

        if (newCallback == callback.load (std::memory_order_acquire))
        {
            requestActive (callback.load() != nullptr);
            return;
        }

        if (newCallback != nullptr)
            newCallback->audioDeviceAboutToStart (this);

        // Swapping the callback waits for any in-flight IO callback to return,
        // so it has to happen outside of the lifecycle lock - see setCallback().
        setCallback (newCallback);

        if (callback.load() != nullptr)
            xruns = 0;

        requestActive (callback.load (std::memory_order_acquire) != nullptr);
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
        // Nothing but atomics is read here: the value is sampled by the process
        // callback, so a host may call this from its IO callback - which is what
        // AudioIODevice's "returns quickly" contract implies.
        auto latency = outputLatencySamples.load (std::memory_order_relaxed);

        // Input data is handed to the callback one block after capture, so add
        // an extra block of latency when doing full-duplex I/O.
        if (wantPlaybackStream.load() && wantCaptureStream.load())
            latency += jmax (1, getCurrentBufferSizeSamples());

        return latency;
    }

    int getInputLatencyInSamples() override
    {
        auto latency = inputLatencySamples.load (std::memory_order_relaxed);

        if (wantPlaybackStream.load() && wantCaptureStream.load())
            latency += jmax (1, getCurrentBufferSizeSamples());

        return latency;
    }

    int getXRunCount() const noexcept override          { return xruns.load(); }

    std::optional<String> getRoutedOutputDeviceName() const override
    {
        const auto nodeId = playbackNodeId.load();

        if (nodeId == SPA_ID_INVALID)
            return {};

        const auto now = Time::getMillisecondCounterHiRes();

        {
            const ScopedLock sl (routedNameLock);

            if (cachedRoutedNameNodeId == nodeId && now < cachedRoutedNameExpiry)
                return cachedRoutedName.isNotEmpty() ? std::optional<String> (cachedRoutedName)
                                                     : std::optional<String>();
        }

        // This performs a registry round-trip, which is why the method isn't
        // allowed to be called from a realtime thread. The answer is cached for
        // a moment: callers that show the routing in the UI tend to ask over and
        // over, and every uncached call is a full scan of the graph.
        const auto peerName = PipeWireRegistryScanner::findRoutedPeerName (nodeId, true);

        {
            const ScopedLock sl (routedNameLock);
            cachedRoutedName = peerName;
            cachedRoutedNameNodeId = nodeId;
            cachedRoutedNameExpiry = now + routedNameCacheMs;
        }

        if (peerName.isEmpty())
            return {};

        return peerName;
    }

    //==============================================================================
    String getSinkKey() const           { return sinkKey; }
    String getSourceKey() const         { return sourceKey; }

private:
    //==============================================================================
    struct StreamData
    {
        PipeWireAudioIODevice* owner = nullptr;
        struct pw_stream* stream = nullptr;
        struct spa_hook listener {};
        bool isCapture = false;
        int numChannels = 0;

        // Set from the control thread when the device is being torn down and the
        // stream objects have been handed over to a loop thread that could not
        // be stopped. The event callbacks still receive this object in that
        // case, so they check the flag instead of the owner pointer before they
        // touch the device.
        std::atomic<bool> detached { false };
    };

    // The core listener lives on the heap and holds the owner behind an atomic:
    // a connection that had to be abandoned keeps receiving events from the
    // loop thread, and it must be able to find out that the device is gone
    // without the controller having to unlink the hook from another thread.
    struct CoreListener
    {
        struct spa_hook hook {};
        std::atomic<PipeWireAudioIODevice*> owner { nullptr };
    };

    // The stream pointers and a generation counter, shared with the tasks the
    // control thread posts to the loop thread. A task holds its own reference,
    // so it can be executed after the device that posted it is gone, and the
    // generation tells it whether its snapshot is still the current connection.
    struct ServerHandle
    {
        std::atomic<struct pw_stream*> playback { nullptr }, capture { nullptr };
        std::atomic<uint64_t> generation { 0 };
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
    // Detaches the current callback. No new IO callback can start once this has
    // returned. The caller still has to call waitForInFlightCallbacks() before
    // it notifies the detached callback, and that wait must happen without any
    // lock held - see setCallback().
    AudioIODeviceCallback* detachCallback()
    {
        callbackGeneration.fetch_add (1, std::memory_order_acq_rel);
        return callback.exchange (nullptr, std::memory_order_acq_rel);
    }

    // Waits until the IO callbacks that were already running have returned.
    //
    // This must never be called while holding a lock that an IO callback can
    // take: a host that calls in from its own IO callback would then wait for
    // the very callback this is waiting for.
    void waitForInFlightCallbacks() const
    {
        // A call that comes from inside this device's IO callback is the only
        // callback that can still be running: detachCallback() has already
        // stopped any new one from starting, and the one that is running cannot
        // return until the call that got us here returns. Waiting for it would
        // deadlock, so the wait is skipped for that one case.
        //
        // Skipping it only stops the wait from blocking on the calling thread.
        // It does not make the call safe: start() and stop() take the lifecycle
        // lock right after this, and close()/open() block on PipeWire's data
        // loop - the loop the calling callback is running on. None of them may
        // be called from the audio thread; see the threading contract above the
        // class.
        if (callbackOnCurrentThread == this)
            return;

        while (activeCallbacks.load (std::memory_order_acquire) != 0)
            Thread::sleep (1);
    }

    // Starts invoking the given callback. Must be paired with a preceding
    // detachCallback() call, and must be called under the callback lock.
    void attachCallback (AudioIODeviceCallback* newCallback)
    {
        callback.store (newCallback, std::memory_order_release);
        callbackGeneration.fetch_add (1, std::memory_order_acq_rel);

        // Attaching a callback doesn't make the device play: the streams are
        // only started - and isPlaying() only becomes true - once the server
        // has confirmed that they are streaming, see requestActive().
        if (newCallback == nullptr)
            isPlaying_ = false;
    }

    // Swaps the callback and makes sure that the previous callback isn't being
    // invoked anymore before the swap completes.
    //
    // This may block until a host IO callback returns, so it must never be
    // called while the lifecycle lock is held, and the wait is done without the
    // callback lock held: see waitForInFlightCallbacks().
    void setCallback (AudioIODeviceCallback* newCallback)
    {
        AudioIODeviceCallback* oldCallback = nullptr;

        {
            const ScopedLock sl (callbackLock);
            oldCallback = detachCallback();
        }

        waitForInFlightCallbacks();

        {
            const ScopedLock sl (callbackLock);
            attachCallback (newCallback);
        }

        if (oldCallback != nullptr)
            oldCallback->audioDeviceStopped();
    }

    // Stops the host callback and tells it about an error. The callback is
    // detached - and therefore guaranteed not to receive any further IO
    // callbacks - before it is notified, and it is notified without the lock
    // held: hosts are expected to be able to restart the device from their
    // error handler.
    //
    // Only audioDeviceError() is sent: the host may destroy its callback, or
    // restart the device with a different one, from inside the error handler,
    // so following it up with audioDeviceStopped() could reach a callback that
    // is either gone or running again. The other backends take the same
    // approach.
    void notifyHostStopped (const String& message)
    {
        AudioIODeviceCallback* stoppedCallback = nullptr;

        {
            const ScopedLock sl (callbackLock);
            stoppedCallback = detachCallback();
        }

        waitForInFlightCallbacks();

        if (stoppedCallback != nullptr)
            stoppedCallback->audioDeviceError (message);
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

        const auto* const previousCallbackDevice = callbackOnCurrentThread;
        callbackOnCurrentThread = this;

        cb->audioDeviceIOCallbackWithContext (inputData, numInputs, outputData, numOutputs, numSamples, {});

        callbackOnCurrentThread = previousCallbackDevice;
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
    String openInternal (const BigInteger& requestedInputs,
                         const BigInteger& requestedOutputs,
                         double sampleRate,
                         int bufferSizeSamples,
                         const PipeWireScanResult& scanResult)
    {
        closeInternal();

        // If the previous teardown couldn't stop the worker threads, the server
        // objects are still alive: opening again would create a second set of
        // streams on top of them.
        if (audioThread != nullptr || monitorThread != nullptr || loopAbandoned.load())
        {
            setLastError ("The PipeWire device is still shutting down");
            return getLastErrorCopy();
        }

        requestedSampleRate = sampleRate > 0 ? sampleRate : 48000.0;
        requestedBufferSize = bufferSizeSamples > 0 ? bufferSizeSamples : getDefaultBufferSize();

        enabledInputChannels  = requestedInputs;
        enabledOutputChannels = requestedOutputs;

        setLastError ({});

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

        // The monitor thread is started by open() once the streams are running.
        return {};
    }

    void closeInternal()
    {
        shuttingDown = true;

        stopMonitorThread();

        // Stopping the streams first means the process callbacks have nothing to
        // deliver while the streams are destroyed.
        setActive (false);

        // The caller has already detached the host callback, so no IO callback
        // can start while the server objects are being destroyed. The loop
        // thread is given a bounded amount of time to finish what it is doing
        // and is abandoned - not waited for forever - if it doesn't.
        //
        // An abandoned connection keeps its buffers for good: the leaked loop
        // thread may still be running a process callback that reads them, and
        // that is not worth trading for a few kilobytes.
        const auto tornDown = teardownServerObjects() && ! loopAbandoned.load();

        if (tornDown)
        {
            buffers->outputChannelPointers.free();
            buffers->inputChannelPointers.free();
            buffers->outputCapacity = 0;
            buffers->inputCapacity = 0;
            buffers->capture.setSize (0, 0);
            buffers->captureSamples = 0;
        }
        else
        {
            // The streams are still alive. Freeing the buffers their process
            // callbacks are writing to would be a use-after-free, so they are
            // left alone for the next close() attempt (or the destructor).
            JUCE_PIPEWIRE_LOG ("the PipeWire loop thread did not stop; "
                               "deferring the server object teardown");
        }

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
        clearRebuildBackoff();

        isOpen_ = false;
        isPlaying_ = false;

        // Only lift the shutdown flag once the loop and monitor threads are
        // really gone: while they are still running, the process callbacks must
        // stay inert so that they don't touch the buffers freed above, and the
        // monitor thread must not restart the server objects behind our back.
        shuttingDown = (audioThread != nullptr || monitorThread != nullptr || loopAbandoned.load());
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

        coreListener = new CoreListener();
        coreListener->owner = this;
        juce::pw_core_add_listener (pwCore, &coreListener->hook, &getDeviceCoreEvents(), coreListener);

        // The loop thread dispatches PipeWire's messages and runs the work the
        // control thread posts to it - see LoopTaskQueue. It holds nothing but
        // the loop pointer and the queue, so it stays self-contained even if the
        // device has to be abandoned (see abandonLoopThread()).
        taskQueue->rearm();
        audioThread = std::make_unique<AudioThread> (pwLoop, taskQueue);
        audioThread->startThread (Thread::Priority::high);
        return true;
    }

    // Runs a task on the thread that pumps the loop, waiting for it to finish.
    // Called from the loop thread itself the task runs inline: waiting for the
    // thread that is running this code would deadlock.
    bool runOnLoopThread (std::function<void()> task, int timeoutMs)
    {
        if (LoopTaskQueue::isCurrentThread (taskQueue.get()))
        {
            task();
            return true;
        }

        return taskQueue->post (std::move (task), timeoutMs);
    }

    // Marks the streams as detached and gives up on the loop thread without
    // stopping it.
    //
    // This is the last resort for a thread that is stuck inside libpipewire:
    // everything it may still touch is leaked instead of freed, so the host
    // gets a stuck audio device rather than a crash in its destructor path.
    void abandonLoopThread()
    {
        detachStreams();

        // The leaked loop thread can still deliver events for this connection.
        // Clearing the owner leaves the listener in place but harmless, which is
        // what makes it safe to do from here - unlinking the hook while the loop
        // thread walks its hook list is not.
        if (coreListener != nullptr)
        {
            coreListener->owner = nullptr;
            coreListener = nullptr;
        }

        pwCore = nullptr;

        // Any task that is still queued or running now sees a different
        // generation and does nothing with its snapshot of the stream pointers.
        auto handle = serverHandle;
        handle->playback = nullptr;
        handle->capture = nullptr;
        ++handle->generation;

        loopAbandoned = true;
        buffersLeaked = true;

        if (taskQueue != nullptr)
            taskQueue->markDead();

        // The thread keeps its own loop pointer and queue, so it can be dropped
        // without stopping it - joining it is exactly what would hang.
        audioThread.release();

        // The stream objects are still referenced by the loop thread's pending
        // callbacks, which now find them detached. They, and the connection
        // they belong to, are deliberately never freed.
        playbackStream.release();
        captureStream.release();

        pwContext = nullptr;
        pwLoop = nullptr;

        wantPlaybackStream = false;
        wantCaptureStream = false;
        playbackStreamReady = false;
        captureStreamReady = false;
        playbackNodeId = SPA_ID_INVALID;
        captureNodeId = SPA_ID_INVALID;
        outputLatencySamples = 0;
        inputLatencySamples = 0;
    }

    void detachStreams()
    {
        if (playbackStream != nullptr)
            playbackStream->detached = true;

        if (captureStream != nullptr)
            captureStream->detached = true;
    }

    // Returns false if the loop thread refused to stop, in which case nothing
    // that depends on the loop may be freed and the caller has to try again
    // later.
    bool teardownServerObjects()
    {
        // Everything that touches a stream proxy runs on the loop thread (see
        // LoopTaskQueue), including the destroy. The task is given the raw
        // stream pointers, so a task that is still running when this returns
        // can't reach any member of this object either.
        auto* playback = playbackStream != nullptr ? playbackStream->stream : nullptr;
        auto* capture  = captureStream  != nullptr ? captureStream->stream  : nullptr;

        if (playback != nullptr || capture != nullptr)
        {
            auto finished = std::make_shared<std::atomic<bool>> (false);

            auto handle = serverHandle;
            const auto generation = handle->generation.load();

            const auto destroyed = runOnLoopThread ([handle, playback, capture, generation, finished]
            {
                // Retire the connection first: a set_active() task queued after
                // this one finds a different generation and does nothing with
                // the pointers it snapped up, which are about to be freed.
                if (handle->generation.load() == generation)
                {
                    handle->playback = nullptr;
                    handle->capture = nullptr;
                    ++handle->generation;
                }

                // pw_stream_destroy() marshals the destroy message and then
                // flushes the data loop, so it has to run while the loop is
                // still being pumped - which is what this thread does.
                if (playback != nullptr)
                    juce::pw_stream_destroy (playback);

                if (capture != nullptr)
                    juce::pw_stream_destroy (capture);

                *finished = true;
            }, streamTaskTimeoutMs);

            if (! destroyed && ! finished->load())
            {
                JUCE_PIPEWIRE_LOG ("the PipeWire loop thread stopped responding; abandoning its objects");
                abandonLoopThread();
                return false;
            }
        }

        if (audioThread != nullptr)
        {
            // Everything that had to run on the loop has run, so the thread can
            // be stopped. waitForThreadToExit() is used rather than stopThread():
            // the latter force-kills the thread (with pthread_cancel) once the
            // timeout expires, which can leave PipeWire's internal locks in an
            // inconsistent state.
            if (taskQueue != nullptr)
                taskQueue->stop();

            audioThread->signalThreadShouldExit();

            if (! audioThread->waitForThreadToExit (loopThreadStopTimeoutMs))
            {
                JUCE_PIPEWIRE_LOG ("the PipeWire loop thread did not stop; abandoning its objects");
                abandonLoopThread();
                return false;
            }

            audioThread.reset();
        }

        if (playbackStream != nullptr || captureStream != nullptr)
        {
            detachStreams();
            playbackStream.reset();
            captureStream.reset();
        }

        wantPlaybackStream = false;
        wantCaptureStream = false;
        playbackStreamReady = false;
        captureStreamReady = false;
        playbackNodeId = SPA_ID_INVALID;
        captureNodeId = SPA_ID_INVALID;
        outputLatencySamples = 0;
        inputLatencySamples = 0;

        // Nothing is marshalling messages any more (the only thread that did is
        // gone), so the rest of the connection can be released from here.
        if (coreListener != nullptr)
        {
            // Safe here: the loop thread that dispatches these events has already
            // been joined.
            spa_hook_remove (&coreListener->hook);
            delete coreListener;
            coreListener = nullptr;
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

        // taskQueue is deliberately left in place: it is created once, the loop
        // thread holds a reference to it, and setupServerObjects() rearms it for
        // the next connection.
        return true;
    }

    //==============================================================================
    bool createStreamObjects()
    {
        streamsStartedEvent.reset();
        streamsStreamingEvent.reset();

        // These flags are published before any stream exists, so that the state
        // changed callbacks - which run on the loop thread - never have to look
        // at the stream pointers to work out whether both streams are running.
        wantPlaybackStream = ! enabledOutputChannels.isZero();
        wantCaptureStream  = ! enabledInputChannels.isZero();
        playbackStreamReady = false;
        captureStreamReady = false;
        playbackStreaming = false;
        captureStreamStreaming = false;
        outputLatencySamples = 0;
        inputLatencySamples = 0;

        if (wantPlaybackStream)
        {
            playbackStream = std::make_unique<StreamData>();
            playbackStream->owner = this;
            playbackStream->isCapture = false;
            playbackStream->numChannels = enabledOutputChannels.countNumberOfSetBits();
        }

        if (wantCaptureStream)
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

        // Published only once both streams exist, so that a set_active() that
        // arrives in between can't see a half-built pair.
        serverHandle->playback = playbackStream != nullptr ? playbackStream->stream : nullptr;
        serverHandle->capture  = captureStream  != nullptr ? captureStream->stream  : nullptr;
        ++serverHandle->generation;

        // Preallocate everything the process callbacks need. This has to happen
        // before the streams are activated, which is the caller's job once
        // waitForStreamsToStart() has returned.
        const int outChans = enabledOutputChannels.countNumberOfSetBits();
        const int inChans  = enabledInputChannels.countNumberOfSetBits();

        buffers->outputChannelPointers.calloc ((size_t) jmax (1, outChans) + 2);
        buffers->outputCapacity = jmax (1, outChans) + 2;
        buffers->inputChannelPointers.calloc ((size_t) jmax (1, inChans) + 2);
        buffers->inputCapacity = jmax (1, inChans) + 2;

        if (inChans > 0)
            buffers->capture.setSize (inChans, maxBlockFrames, false, false, true);

        return true;
    }

    // Creation and connection both marshal a message into the connection's
    // buffer, so both run on the loop thread - see LoopTaskQueue.
    //
    // Everything a task needs is handed to it by value. A task that is still
    // running when its caller gave up (see LoopTaskQueue::post()) must not read
    // any member of this object: by then the device may be gone. The StreamData
    // it writes into is leaked by abandonLoopThread() for the same reason.
    bool createStream (StreamData& data)
    {
        const auto clientName = "JUCE-PipeWire-" + (data.isCapture ? String ("Input") : String ("Output"))
                                  + "-" + String ((int) ++streamIdCounter);

        auto* const core = pwCore;
        const auto bufferSize = requestedBufferSize;
        const auto sampleRate = requestedSampleRate;
        const auto isCapture = data.isCapture;

        auto created = std::make_shared<bool> (false);
        auto errorMessage = std::make_shared<String>();

        if (! runOnLoopThread ([&data, core, clientName, isCapture, bufferSize, sampleRate,
                                created, errorMessage]
                               {
                                   *created = createStreamOnLoopThread (data, core, clientName, isCapture,
                                                                        bufferSize, sampleRate, *errorMessage);
                               }, streamTaskTimeoutMs))
        {
            setLastError ("Timed out waiting for the PipeWire loop thread");
            abandonLoopThread();
            return false;
        }

        if (! *created)
            setLastError (errorMessage->isNotEmpty() ? *errorMessage
                                                     : String ("Could not create a PipeWire stream"));

        return *created;
    }

    static bool createStreamOnLoopThread (StreamData& data,
                                          struct pw_core* core,
                                          const String& clientName,
                                          bool isCapture,
                                          int bufferSize,
                                          double sampleRate,
                                          String& errorMessage)
    {
        auto* props = createEmptyPipeWireProperties();

        if (props == nullptr)
        {
            errorMessage = "Could not create a PipeWire stream";
            return false;
        }

        juce::pw_properties_set (props, PW_KEY_MEDIA_TYPE, "Audio");
        juce::pw_properties_set (props, PW_KEY_MEDIA_CATEGORY, isCapture ? "Capture" : "Playback");
        juce::pw_properties_set (props, PW_KEY_MEDIA_ROLE, "Music");
        juce::pw_properties_set (props, PW_KEY_NODE_NAME, clientName.toRawUTF8());
        juce::pw_properties_set (props, PW_KEY_NODE_DESCRIPTION, clientName.toRawUTF8());

        // Ask the server to schedule us with the requested block size. Without
        // this the graph's default quantum is used and the buffer size that the
        // host asked for is silently ignored, so getCurrentBufferSizeSamples()
        // would never match getDefaultBufferSize() / the value passed to open().
        // The latency is a fraction of a second, expressed as frames/rate.
        const auto latency = String (jmax (1, bufferSize)) + "/" + String (jmax (1, (int) sampleRate));
        juce::pw_properties_set (props, PW_KEY_NODE_LATENCY, latency.toRawUTF8());

        // ... and let the server know which rate we would like the graph to run
        // at when it can be changed.
        const auto requestedRate = "1/" + String (jmax (1, (int) sampleRate));
        juce::pw_properties_set (props, PW_KEY_NODE_RATE, requestedRate.toRawUTF8());

        data.stream = juce::pw_stream_new (core, clientName.toRawUTF8(), props);

        if (data.stream == nullptr)
        {
            juce::pw_properties_free (props);
            errorMessage = "Could not create a PipeWire stream";
            return false;
        }

        juce::pw_stream_add_listener (data.stream, &data.listener, &getStreamEvents(), &data);
        return true;
    }

    bool connectStream (StreamData& data, uint32_t targetId)
    {
        // The channel layout is resolved here, not in the task: the task must
        // not touch the channel name arrays, which belong to this object.
        struct spa_audio_info_raw info {};
        info.format = SPA_AUDIO_FORMAT_F32P;
        info.channels = (uint32_t) data.numChannels;

        {
            const auto& channelNames = data.isCapture ? inputChannelNames : outputChannelNames;
            const auto& enabledChannels = data.isCapture ? enabledInputChannels : enabledOutputChannels;
            int position = 0;

            for (int channel = 0; channel < channelNames.size() && position < (int) SPA_AUDIO_MAX_CHANNELS; ++channel)
                if (enabledChannels[channel])
                    info.position[position++] = getChannelPosition (channelNames[channel]);
        }

        auto* const stream = data.stream;
        const auto isCapture = data.isCapture;
        const auto sampleRate = requestedSampleRate;
        auto connected = std::make_shared<bool> (false);
        auto errorMessage = std::make_shared<String>();

        if (! runOnLoopThread ([stream, info, isCapture, sampleRate, targetId, connected, errorMessage]
                               {
                                   *connected = connectStreamOnLoopThread (stream, info, isCapture, sampleRate,
                                                                           targetId, *errorMessage);
                               }, streamTaskTimeoutMs))
        {
            setLastError ("Timed out waiting for the PipeWire loop thread");
            abandonLoopThread();
            return false;
        }

        if (! *connected && errorMessage->isNotEmpty())
            setLastError (*errorMessage);

        return *connected;
    }

    static bool connectStreamOnLoopThread (struct pw_stream* stream,
                                           const struct spa_audio_info_raw& requestedInfo,
                                           bool isCapture,
                                           double sampleRate,
                                           uint32_t targetId,
                                           String& errorMessage)
    {
        if (stream == nullptr)
        {
            errorMessage = "The PipeWire stream was not created";
            return false;
        }

        // Build the format pods requesting floating point, non-interleaved
        // audio. The buffer is sized for the maximum number of formats and
        // channels that can be offered below (a format pod is roughly 60 bytes
        // plus 4 bytes per channel position); a pod that doesn't fit makes the
        // builder return nullptr, which is handled by stopping early.
        uint8_t podBuffer[16384];
        // SPA_POD_BUILDER_INIT expands to a compound literal, which is a C
        // extension that -Wpedantic reports when it is used from C++.
        JUCE_BEGIN_IGNORE_WARNINGS_GCC_LIKE ("-Wpedantic")
        auto builder = SPA_POD_BUILDER_INIT (podBuffer, sizeof podBuffer);
        JUCE_END_IGNORE_WARNINGS_GCC_LIKE

        auto info = requestedInfo;

        // Offer every rate that getAvailableSampleRates() advertises - the
        // requested one first - so that a device that can't run at the requested
        // rate is negotiated down instead of failing to open. Both lists come
        // from the same helper: a rate that is advertised but not offered here
        // would be silently replaced by one of these. The negotiated rate is
        // reported back through param_changed.
        const auto& offeredRates = SampleRateHelpers::getCommonSampleRates();
        constexpr uint32_t maxNumFormats = 32;

        const struct spa_pod* params[maxNumFormats];
        uint32_t numParams = 0;

        for (size_t i = 0; i <= offeredRates.size() && numParams < maxNumFormats; ++i)
        {
            const auto rateToUse = (uint32_t) jmax (1.0, i == 0 ? sampleRate : offeredRates[i - 1]);

            // Skip duplicates: the requested rate is usually one of the common
            // ones, so it appears twice.
            bool duplicate = false;

            for (uint32_t j = 0; j < numParams; ++j)
                if (offeredRateMatches (params[j], rateToUse))
                    duplicate = true;

            if (duplicate)
                continue;

            info.rate = rateToUse;
            params[numParams] = spa_format_audio_raw_build (&builder, SPA_PARAM_EnumFormat, &info);

            if (params[numParams] == nullptr)
                break;

            ++numParams;
        }

        if (numParams == 0)
        {
            errorMessage = "Could not build the PipeWire audio format";
            return false;
        }

        const auto direction = isCapture ? PW_DIRECTION_INPUT : PW_DIRECTION_OUTPUT;
        const auto flags = (enum pw_stream_flags) (PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS);
        const auto result = juce::pw_stream_connect (stream, direction, targetId, flags, params, numParams);

        if (result < 0)
        {
            errorMessage = "Could not connect to the PipeWire server (error " + String (-result) + ")";
            return false;
        }

        // Resolve pw_stream_get_time() here rather than in the first process
        // callback: it is a dynamically loaded symbol, and the first call pays
        // for the lookup.
        struct pw_time time {};

        if (juce::pw_stream_get_time (stream, &time) == 0)
            juce::ignoreUnused (time);

        return true;
    }

    static bool offeredRateMatches (const struct spa_pod* param, uint32_t rate)
    {
        if (param == nullptr)
            return false;

        struct spa_audio_info_raw parsed {};

        return spa_format_audio_raw_parse (param, &parsed) >= 0 && parsed.rate == rate;
    }

    // PipeWire names the channels of its ports with the short names ("FL",
    // "FR", ...), but a device created through the JUCE API may also use the
    // longer JUCE names. Both are accepted, and anything unknown falls back to
    // SPA_AUDIO_CHANNEL_UNKNOWN, which lets the server pick a sensible layout.
    static uint32_t getChannelPosition (const String& channelName)
    {
        const auto shortName = spa_type_audio_channel_from_short_name (channelName.toRawUTF8());

        if (shortName != SPA_AUDIO_CHANNEL_UNKNOWN)
            return shortName;

        if (channelName == "Left")              return SPA_AUDIO_CHANNEL_FL;
        if (channelName == "Right")             return SPA_AUDIO_CHANNEL_FR;
        if (channelName == "Centre"
             || channelName == "Center")        return SPA_AUDIO_CHANNEL_FC;
        if (channelName == "LFE")               return SPA_AUDIO_CHANNEL_LFE;
        if (channelName == "Side Left")         return SPA_AUDIO_CHANNEL_SL;
        if (channelName == "Side Right")        return SPA_AUDIO_CHANNEL_SR;
        if (channelName == "Back Left")         return SPA_AUDIO_CHANNEL_RL;
        if (channelName == "Back Right")        return SPA_AUDIO_CHANNEL_RR;

        return SPA_AUDIO_CHANNEL_UNKNOWN;
    }

    // Waits until every stream this open() created has reached PW_STREAM_STATE_PAUSED,
    // which is when the server has finished the format negotiation.
    bool waitForStreamsToStart()
    {
        const auto deadline = Time::getMillisecondCounterHiRes() + (double) streamStartTimeoutMs;

        // Waiting in short slices keeps this responsive to a calling thread that
        // is being asked to stop - the monitor thread can end up here during a
        // rebuild, and it must not have to be killed to get out.
        while (Time::getMillisecondCounterHiRes() < deadline)
        {
            if (streamsStartedEvent.wait (50))
                return getLastErrorCopy().isEmpty();

            if (isCallingThreadBeingStopped())
            {
                setLastError ("The PipeWire device is shutting down");
                return false;
            }
        }

        setLastError ("Timed out waiting for the PipeWire server to start the streams");
        return false;
    }

    // Waits until a STREAMING transition has arrived after the given generation
    // was taken, i.e. until the server really linked the graph.
    //
    // Waiting for the flags alone would be wrong: they are still set from the
    // previous take when a host stops and starts the device again, and a server
    // that drops the activation request would then look like it worked.
    bool waitForStreamsToBeStreaming (uint64_t generationBefore, int timeoutMs)
    {
        const auto deadline = Time::getMillisecondCounterHiRes() + (double) timeoutMs;

        while (Time::getMillisecondCounterHiRes() < deadline)
        {
            if (streamingGeneration.load() != generationBefore && areAllWantedStreamsStreaming())
                return true;

            if (isCallingThreadBeingStopped())
                return false;

            streamsStreamingEvent.wait (10);
        }

        return streamingGeneration.load() != generationBefore && areAllWantedStreamsStreaming();
    }

    bool areAllWantedStreamsStreaming() const
    {
        return (! wantPlaybackStream.load() || playbackStreaming.load())
            && (! wantCaptureStream.load()  || captureStreamStreaming.load());
    }

    // Starts or stops the streams and, when starting, waits until they are
    // really streaming.
    //
    // This is the difference between "the server was asked to play" and "the
    // host is being fed": pw_stream_set_active() only queues the request, and a
    // request that arrives before the graph has linked the node is dropped by
    // the server. The device would then report isPlaying() while never calling
    // back, which is the failure that made hosts appear to be playing silence.
    void requestActive (bool shouldBeActive)
    {
        if (! shouldBeActive)
        {
            setPlaying (false);
            setActive (false);
            playbackStreaming = false;
            captureStreamStreaming = false;
            return;
        }

        for (int attempt = 0; attempt < numStartAttempts; ++attempt)
        {
            if (attempt > 0)
            {
                // pw_impl_node_set_active() ignores a request that doesn't change
                // the node's state, so activating an already active node never
                // re-issues the Start command. Taking it down first is what makes
                // the retry an actual retry.
                setActive (false);
            }

            const auto generationBefore = streamingGeneration.load();

            if (! setActive (true))
                continue;

            if (waitForStreamsToBeStreaming (generationBefore, startHandshakeTimeoutMs))
            {
                // A stream error or a lost server can land while the handshake is
                // waiting; those latches are set before the flags below, so
                // checking them keeps the device from claiming to play when the
                // stream has already failed.
                if (! streamErrorNotified.load() && ! serverLost.load())
                {
                    setPlaying (true);

                    // A previous failure may have left the notify/rebuild flags
                    // set; the streams are running now, so the monitor thread must
                    // not tear them down and tell the host that the device
                    // stopped.
                    pendingErrorNotify = false;
                    pendingRestart = false;
                    clearRebuildBackoff();
                    return;
                }

                break;
            }

            if (getLastErrorCopy().isEmpty())
                setLastError ("The PipeWire server did not start the streams");

            if (isCallingThreadBeingStopped())
                break;
        }

        // The streams are not running, so claiming to play would make the host
        // show a running device that delivers nothing. The monitor thread
        // notifies the host about the failure and rebuilds the streams.
        setPlaying (false);
        pendingErrorNotify = true;
        pendingRestart = true;
    }

    void setPlaying (bool shouldBePlaying)
    {
        isPlaying_ = shouldBePlaying;
    }

    // Posts the activation request to the loop thread. The stream pointers and
    // the generation are taken from the shared handle, so this never reads a
    // member of an object that may be gone, and a request that lost the race
    // against a teardown turns into a no-op instead of touching freed streams.
    bool setActive (bool shouldBeActive)
    {
        auto handle = serverHandle;

        // Both writers - createStreamObjects() and the destroy task - publish
        // the pointers first and bump the generation afterwards, so a generation
        // that doesn't move while the pointers are read means they belong to it.
        uint64_t generation = 0;
        struct pw_stream* playback = nullptr;
        struct pw_stream* capture = nullptr;

        for (int attempt = 0; attempt < 8; ++attempt)
        {
            generation = handle->generation.load();
            playback = handle->playback.load();
            capture = handle->capture.load();

            if (handle->generation.load() == generation)
                break;
        }

        if (playback == nullptr && capture == nullptr)
            return true;

        auto result = std::make_shared<int> (0);

        if (! runOnLoopThread ([handle, playback, capture, generation, shouldBeActive, result]
                               {
                                   if (handle->generation.load() != generation)
                                       return;

                                   *result = setStreamsActiveOnLoopThread (playback, capture, shouldBeActive);
                               }, streamTaskTimeoutMs))
        {
            setLastError ("Timed out waiting for the PipeWire loop thread");
            return false;
        }

        return *result >= 0;
    }

    // Runs on the loop thread. pw_stream_set_active() only touches the local
    // node and takes the stream's data-loop lock, so it is cheap - but it must
    // still happen on the loop thread, which is where the node state lives.
    static int setStreamsActiveOnLoopThread (struct pw_stream* playback,
                                             struct pw_stream* capture,
                                             bool shouldBeActive)
    {
        int result = 0;

        if (playback != nullptr)
            result = juce::pw_stream_set_active (playback, shouldBeActive);

        if (capture != nullptr)
        {
            const auto captureResult = juce::pw_stream_set_active (capture, shouldBeActive);

            if (result >= 0)
                result = captureResult;
        }

        return result;
    }

    //==============================================================================
    // Built through a function-local static for the same reason as the scanner's
    // tables - see getCoreEvents() there. These are used from the loop thread and
    // from the control thread.
    static const struct pw_stream_events& getStreamEvents()
    {
        static const struct pw_stream_events events = []
        {
            struct pw_stream_events result {};
            result.version = PW_VERSION_STREAM_EVENTS;
            result.state_changed = [] (void* data, enum pw_stream_state /* oldState */,
                                       enum pw_stream_state state, const char* error)
            {
                auto& streamData = *static_cast<StreamData*> (data);

                if (! streamData.detached.load (std::memory_order_acquire))
                    streamData.owner->handleStreamStateChanged (streamData, state, error);
            };
            result.param_changed = [] (void* data, uint32_t, const struct spa_pod* param)
            {
                auto& streamData = *static_cast<StreamData*> (data);

                if (! streamData.detached.load (std::memory_order_acquire))
                    streamData.owner->handleStreamParamChanged (param);
            };
            result.process = [] (void* data)
            {
                auto& streamData = *static_cast<StreamData*> (data);

                if (! streamData.detached.load (std::memory_order_acquire))
                    streamData.owner->handleStreamProcess (streamData);
            };
            return result;
        }();

        return events;
    }

    static const struct pw_core_events& getDeviceCoreEvents()
    {
        static const struct pw_core_events events = []
        {
            struct pw_core_events result {};
            result.version = PW_VERSION_CORE_EVENTS;
            result.error = [] (void* data, uint32_t, int, int, const char*)
            {
                // The owner is cleared when the connection is abandoned, which
                // can happen while this thread is running.
                if (auto* listener = static_cast<CoreListener*> (data))
                    if (auto* owner = listener->owner.load())
                        owner->handleServerLost();
            };
            return result;
        }();

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
                // Resume after recovery only if the host still had us playing,
                // mirroring notifyServerLost(): a stream that fails while the
                // device is idle must not make it start playing on its own.
                resumeAfterRecovery = isPlaying_.load();
                isPlaying_ = false;

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

            if (data.isCapture)
            {
                captureStreamReady = true;
                captureStreamStreaming = false;
            }
            else
            {
                playbackStreamReady = true;
                playbackStreaming = false;
            }

            // This runs on the loop thread and only reads atomics: the control
            // thread may still be publishing the other stream, so the stream
            // pointers must not be dereferenced from here.
            const bool allStreamsReady = (! wantPlaybackStream.load() || playbackStreamReady.load())
                                      && (! wantCaptureStream.load()  || captureStreamReady.load());

            if (allStreamsReady)
                streamsStartedEvent.signal();
        }
        else if (state == PW_STREAM_STATE_STREAMING)
        {
            refreshStreamNodeId (data);

            if (data.isCapture)
                captureStreamStreaming = true;
            else
                playbackStreaming = true;

            // The handshake in requestActive() waits for this: a stream that
            // stays paused after being activated is one the server never linked,
            // and its process callback would never run. The generation makes
            // sure it waits for a transition that happened after the request,
            // not for a flag left over from the previous take.
            streamingGeneration.fetch_add (1);
            streamsStreamingEvent.signal();
        }
        else if (state == PW_STREAM_STATE_UNCONNECTED || state == PW_STREAM_STATE_CONNECTING)
        {
            if (data.isCapture)
            {
                captureStreamReady = false;
                captureStreamStreaming = false;
            }
            else
            {
                playbackStreamReady = false;
                playbackStreaming = false;
            }
        }
    }

    void refreshStreamNodeId (StreamData& data)
    {
        const auto nodeId = juce::pw_stream_get_node_id (data.stream);

        if (nodeId == SPA_ID_INVALID)
            return;

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

            // A positive stride is the size of one frame in bytes; anything
            // else is treated as 4 (one float per channel). The division is
            // done in unsigned arithmetic because chunk->size is a uint32_t.
            const auto stride = data.chunk->stride > 0 ? (uint32_t) data.chunk->stride : 4u;
            const auto valid = (int) (data.chunk->size / stride);
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
                                       buffers->outputCapacity,
                                       enabledOutputChannels.countNumberOfSetBits());

        if (numSamples <= 0 || numChannels <= 0)
        {
            xruns++;
            juce::pw_stream_queue_buffer (data.stream, buffer);
            return;
        }

        currentBufferSize = numSamples;

        for (int i = 0; i < numChannels; ++i)
            buffers->outputChannelPointers[i] = getPlaneSamples (spaBuffer->datas[i]);

        // If we're recording as well as playing, point the input channels at
        // the data that was captured in the most recent capture callback.
        const auto numInputChannels = buffers->capture.getNumChannels();
        float** inputData = nullptr;

        if (numInputChannels > 0)
        {
            inputData = buffers->inputChannelPointers.getData();

            if (buffers->capture.getNumSamples() < numSamples)
            {
                // The capture buffer is smaller than the playback block, which
                // shouldn't normally happen. Discard the captured data and hand
                // the host silence - the buffer may still hold the previous
                // block, and passing that on would repeat a chunk of audio.
                // AudioBuffer::clear() doesn't clamp its range, so it is
                // clamped here.
                buffers->capture.clear (0, jmin (numSamples, buffers->capture.getNumSamples()));
                buffers->captureSamples = 0;
                xruns++;
            }
            else if (buffers->captureSamples > numSamples)
            {
                buffers->captureSamples = numSamples;
            }
            else if (buffers->captureSamples < numSamples)
            {
                buffers->capture.clear (buffers->captureSamples, numSamples - buffers->captureSamples);
            }

            for (int i = 0; i < numInputChannels; ++i)
                buffers->inputChannelPointers[i] = const_cast<float*> (buffers->capture.getReadPointer (i, 0));
        }

        invokeCallback (inputData, numInputChannels,
                        buffers->outputChannelPointers.getData(), numChannels,
                        numSamples);

        for (uint32_t i = 0; i < spaBuffer->n_datas; ++i)
        {
            if (auto* chunk = spaBuffer->datas[i].chunk)
            {
                chunk->size = i < (uint32_t) numChannels ? (uint32_t) (numSamples * 4) : 0;
                chunk->stride = 4;
            }
        }

        buffers->captureSamples = 0;
        juce::pw_stream_queue_buffer (data.stream, buffer);
        publishLatency (data);
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
                                       buffers->inputCapacity,
                                       enabledInputChannels.countNumberOfSetBits());

        if (numSamples <= 0 || numChannels <= 0)
        {
            xruns++;
            juce::pw_stream_queue_buffer (data.stream, buffer);
            return;
        }

        // If we're only recording, drive the callback directly from here,
        // otherwise store the data for the next playback block.
        //
        // This runs on the loop thread, while the playback stream is created and
        // destroyed by the control thread, so the atomic flag is used as the
        // criterion rather than the pointer itself: it is set before either
        // stream is connected (which is the earliest a capture callback can
        // arrive) and cleared only once the loop thread has stopped.
        if (! wantPlaybackStream.load())
        {
            currentBufferSize = numSamples;

            for (int i = 0; i < numChannels; ++i)
                buffers->inputChannelPointers[i] = getPlaneSamples (spaBuffer->datas[i]);

            invokeCallback (buffers->inputChannelPointers.getData(), numChannels, nullptr, 0, numSamples);
        }
        else
        {
            if (numChannels > buffers->capture.getNumChannels() || numSamples > buffers->capture.getNumSamples())
            {
                buffers->captureSamples = 0;
                xruns++;
            }
            else
            {
                buffers->capture.clear (0, numSamples);

                for (int i = 0; i < numChannels; ++i)
                    if (auto* src = getPlaneSamples (spaBuffer->datas[i]))
                        memcpy (buffers->capture.getWritePointer (i, 0), src,
                                (size_t) numSamples * sizeof (float));

                buffers->captureSamples = numSamples;
            }
        }

        juce::pw_stream_queue_buffer (data.stream, buffer);
        publishLatency (data);
    }

    //==============================================================================
    // Converts a pw_time snapshot into samples at the stream's own rate.
    //
    // pw_time.delay is expressed in ticks, and the duration of a tick is given
    // by the rate fraction (documented as "usually 1/<samplerate>"). Assuming
    // that a tick is always one sample would be out by a large factor whenever
    // the stream is running at a different rate from the graph, so convert
    // explicitly.
    static int getLatencyInSamples (const struct pw_time& time, double sampleRateForStream)
    {
        if (time.delay <= 0 || time.rate.num == 0 || time.rate.denom == 0 || sampleRateForStream <= 0.0)
            return 0;

        const auto delaySeconds = (double) time.delay * (double) time.rate.num / (double) time.rate.denom;

        return (int) jlimit (0.0, (double) std::numeric_limits<int>::max(), delaySeconds * sampleRateForStream);
    }

    // Samples the reported latency from a process callback and publishes it.
    // pw_stream_get_time() is documented as realtime safe and doesn't take the
    // lifecycle lock, which is what lets the latency getters be lock-free - see
    // getOutputLatencyInSamples().
    void publishLatency (StreamData& data)
    {
        struct pw_time time {};

        if (juce::pw_stream_get_time (data.stream, &time) != 0)
            return;

        const auto latency = getLatencyInSamples (time, currentSampleRate.load());

        if (data.isCapture)
            inputLatencySamples.store (latency, std::memory_order_relaxed);
        else
            outputLatencySamples.store (latency, std::memory_order_relaxed);
    }

    //==============================================================================
    // Pumps the loop and runs the work the control thread has posted. It holds
    // nothing but the loop and the queue, so a stuck instance can be abandoned
    // without leaving this object's members dangling (see abandonLoopThread()).
    class AudioThread final : public Thread
    {
    public:
        AudioThread (struct pw_loop* loopToPump, std::shared_ptr<LoopTaskQueue> queueToUse)
            : Thread ("PipeWire Audio"), loop (loopToPump), queue (std::move (queueToUse))
        {
        }

        void run() override
        {
            queue->enterLoopThread();

            while (! threadShouldExit())
            {
                // Serving the queue before pumping keeps this the only thread
                // that ever touches the connection, apart from the process
                // callbacks below.
                queue->runPending();
                juce::pw_loop_iterate (loop, 20);
            }

            queue->leaveLoopThread();
        }

    private:
        struct pw_loop* loop = nullptr;
        std::shared_ptr<LoopTaskQueue> queue;

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
            // A host that closes the device from within its audioDeviceError()
            // handler would get here from the monitor thread itself, and waiting
            // for a thread to stop from inside that thread would deadlock.
            if (monitorThread->getThreadId() == Thread::getCurrentThreadId())
            {
                monitorThread->signalThreadShouldExit();
                return;
            }

            // waitForThreadToExit() rather than stopThread(): the latter
            // force-kills the thread once the timeout expires, and a monitor
            // thread that is killed in the middle of a PipeWire call could leave
            // the library's internal state inconsistent. The scans and the
            // stream start waits abandon themselves once the thread has been
            // asked to stop.
            monitorThread->signalThreadShouldExit();

            if (! monitorThread->waitForThreadToExit (threadStopTimeoutMs))
                return; // it will finish on its own and be cleaned up later

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
            scheduleRebuildRetry();
            return;
        }

        serverLost = false;

        // Recover from a server restart, or from a stream error, by rebuilding
        // the server-side objects. Retries are spaced out: while the server is
        // coming back up, reconnecting once per monitor cycle would hammer it
        // and keep the lifecycle lock permanently busy.
        const bool rebuildPending = needsRebuildAfterLoss.load()
                                      || pendingRestart.load()
                                      || resumeAfterRecovery.load();

        if (rebuildPending)
        {
            if (! canAttemptRebuild())
                return;

            pendingRestart = false;

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
                {
                    scheduleRebuildRetry();
                    return;
                }
            }

            if (sourceKey.isNotEmpty())
            {
                const auto key = (isFollowingDefaultSource() && scanResult.defaultSourceKey.isNotEmpty())
                                     ? scanResult.defaultSourceKey : sourceKey;

                if (auto id = findNodeId (scanResult.sources, key); id != SPA_ID_INVALID)
                    newSource = id;
                else
                {
                    scheduleRebuildRetry();
                    return;
                }
            }

            needsRebuildAfterLoss = false;
            setCurrentDefaultSinkKey (scanResult.defaultSinkKey);
            setCurrentDefaultSourceKey (scanResult.defaultSourceKey);

            if (! restartServerSide (newSink, newSource, true))
                scheduleRebuildRetry();

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

                if (! restartServerSide (id, sourceNodeId, false))
                    scheduleRebuildRetry();

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

                if (! restartServerSide (sinkNodeId, id, false))
                    scheduleRebuildRetry();
            }
        }
    }

    //==============================================================================
    // A failed rebuild is retried with an exponentially growing delay (capped at
    // maxRebuildBackoffMs) so that a server that is down doesn't get hammered
    // once per monitor cycle.
    bool canAttemptRebuild() const
    {
        // A device whose loop thread had to be abandoned is left alone: every
        // rebuild attempt would leak another thread, connection and stream pair.
        if (loopAbandoned.load())
            return false;

        return Time::getMillisecondCounterHiRes() >= nextRebuildAttemptTime;
    }

    void scheduleRebuildRetry()
    {
        nextRebuildAttemptTime = Time::getMillisecondCounterHiRes() + (double) rebuildBackoffMs;
        rebuildBackoffMs = jmin (rebuildBackoffMs * 2, maxRebuildBackoffMs);
    }

    void clearRebuildBackoff()
    {
        nextRebuildAttemptTime = 0.0;
        rebuildBackoffMs = 1000;
    }

    // Tells the host that the connection to the server has been lost. The host
    // is notified without the lifecycle lock held: it is allowed to call back
    // into the device (for example to switch devices) from its error handler.
    void notifyServerLost()
    {
        {
            const ScopedLock sl (lifecycleLock);

            if (! pendingLostNotify.exchange (false))
                return;

            setLastError ("Lost the connection to the PipeWire server");
            resumeAfterRecovery = isPlaying_.load();
            isPlaying_ = false;
        }

        notifyHostStopped (getLastErrorCopy());
    }

    // As above, for a stream that reported an error.
    void notifyStreamError()
    {
        {
            const ScopedLock sl (lifecycleLock);

            if (! pendingErrorNotify.exchange (false))
                return;
        }

        const auto errorText = getLastErrorCopy();
        notifyHostStopped (errorText.isEmpty() ? String ("The PipeWire stream failed") : errorText);
    }

    // Rebuilds the server-side objects, optionally resuming playback. Returns
    // false when the rebuild didn't complete, in which case the caller has to
    // retry later.
    //
    // The waiting for the server deliberately happens without the lifecycle
    // lock held: holding it across the wait would freeze the message thread for
    // as long as it takes the server to start the streams.
    bool restartServerSide (uint32_t newSinkNodeId, uint32_t newSourceNodeId, bool resumeAfterwards)
    {
        bool shouldResume = false;

        {
            const ScopedLock sl (lifecycleLock);

            if (shuttingDown.load() || ! isOpen_.load())
                return true;

            // The streams of an abandoned connection are still owned by a thread
            // that could not be stopped: building a second set on top of them
            // would leak another connection, and read the buffers that were kept
            // for the first one.
            if (loopAbandoned.load())
                return false;

            shouldResume = resumeAfterwards ? (resumeAfterRecovery.load() || isPlaying_.load())
                                            : isPlaying_.load();

            setLastError ({});
            streamErrorNotified = false;
            pendingErrorNotify = false;
            serverLost = false;

            setActive (false);

            if (! teardownServerObjects())
            {
                // The loop thread hasn't stopped yet, so nothing can be
                // destroyed. Leave everything alone and try again later.
                needsRebuildAfterLoss = true;
                return false;
            }

            sinkNodeId = newSinkNodeId;
            sourceNodeId = newSourceNodeId;

            pendingRestart = false;

            if (! setupServerObjects() || ! createStreamObjects())
            {
                setLastError ("Could not reopen the PipeWire streams");
                teardownServerObjects();
                isPlaying_ = false;

                // Try again on the next monitor cycle.
                needsRebuildAfterLoss = true;
                return false;
            }

            // The rebuild may be running against a device with a different rate
            // or quantum (following the session default is the common case), so
            // the published values are restored to what we ask for rather than
            // left at the previous device's numbers. The negotiated values
            // replace them as soon as the new streams report them.
            currentSampleRate = requestedSampleRate;
            currentBufferSize = requestedBufferSize;
        }

        if (! waitForStreamsToStart())
        {
            const ScopedLock sl (lifecycleLock);

            teardownServerObjects();
            isPlaying_ = false;
            needsRebuildAfterLoss = true;
            return false;
        }

        // Resume only when there is still a host callback to resume: a rebuild
        // that follows a stream error or a lost server detaches the callback
        // (notifyHostStopped()) and tells the host that the device has stopped,
        // so claiming to be playing would report a running device that silently
        // delivers nothing - and the host would miss the
        // audioDeviceAboutToStart() that publishes the new sample rate and
        // block size.
        const bool haveCallback = callback.load (std::memory_order_acquire) != nullptr;

        // The sample rate or block size may have changed since the host was last
        // told about this device - both after a recovery and after following a
        // new session default - so notify it before the streams are activated.
        // This happens outside of the lifecycle lock, like in start(), because
        // the host is allowed to call back into the device from here.
        if (shouldResume && haveCallback)
            if (auto* cb = callback.load (std::memory_order_acquire))
                cb->audioDeviceAboutToStart (this);

        // The rebuild worked, so start the retry backoff from scratch.
        {
            const ScopedLock sl (lifecycleLock);
            clearRebuildBackoff();
            resumeAfterRecovery = false;
        }

        if (shuttingDown.load() || ! isOpen_.load())
            return true;

        // Resuming goes through the same handshake as start(): the host may have
        // started us again while the streams were being rebuilt, but claiming to
        // be playing without the server actually dispatching would leave it with
        // a silently dead device.
        if (shouldResume && callback.load (std::memory_order_acquire) != nullptr)
        {
            requestActive (true);

            if (! isPlaying_.load())
            {
                // The caller retries with a growing delay, so a server that is
                // not ready yet doesn't get hammered once per monitor cycle.
                needsRebuildAfterLoss = true;
                return false;
            }
        }
        else
        {
            isPlaying_ = false;
        }

        return true;
    }

    //==============================================================================
    // The keys this device was created with, see getSinkKey()/getSourceKey().
    String sinkKey, sourceKey;
    StringArray outputChannelNames, inputChannelNames;

    String lastError;
    mutable CriticalSection errorLock;

    // Serialises open/close and the monitor thread's restarts, and the state
    // changes start()/stop() make.
    //
    // The waits for the server at the end of open() and in restartServerSide()
    // deliberately happen without it. The teardown and the stream creation do
    // hold it, and both of those round-trip through the loop thread (up to
    // streamTaskTimeoutMs), so a rebuild can hold this lock for seconds. That is
    // why nothing reachable from a realtime thread may take it - the latency
    // queries used to, and are now lock-free (see getOutputLatencyInSamples()).
    CriticalSection lifecycleLock;

    // Serialises callback swaps. Never taken while lifecycleLock is held.
    CriticalSection callbackLock;

    std::atomic<AudioIODeviceCallback*> callback { nullptr };
    std::atomic<uint32_t> callbackGeneration { 0 };
    std::atomic<int> activeCallbacks { 0 };

    // Set on the thread that is currently running the host's IO callback for
    // this device, so that a host which calls back into us from there does not
    // wait for itself - see waitForInFlightCallbacks().
    inline static thread_local const PipeWireAudioIODevice* callbackOnCurrentThread = nullptr;
    std::atomic<bool> isOpen_ { false }, isPlaying_ { false }, shuttingDown { false };
    std::atomic<double> currentSampleRate { 0.0 };
    std::atomic<int> currentBufferSize { 0 };
    std::atomic<int> xruns { 0 };
    std::atomic<uint32_t> playbackNodeId { SPA_ID_INVALID }, captureNodeId { SPA_ID_INVALID };

    // The last answer from getRoutedOutputDeviceName(), which stays valid for a
    // short while: the query itself is a registry round-trip, and the routing of
    // a stream doesn't change on its own more often than this.
    mutable CriticalSection routedNameLock;
    mutable String cachedRoutedName;
    mutable uint32_t cachedRoutedNameNodeId = SPA_ID_INVALID;
    mutable double cachedRoutedNameExpiry = 0.0;
    static constexpr double routedNameCacheMs = 1000.0;
    std::atomic<bool> serverLost { false }, pendingRestart { false }, streamErrorNotified { false };
    std::atomic<bool> resumeAfterRecovery { false }, pendingLostNotify { false }, needsRebuildAfterLoss { false };
    std::atomic<bool> pendingErrorNotify { false };

    // Which streams this open() created, which of them have reached the paused
    // state and which are really streaming. These let the loop thread work out
    // whether both streams are running without touching the stream pointers,
    // which the control thread may be publishing at the same time.
    std::atomic<bool> wantPlaybackStream { false }, wantCaptureStream { false };
    std::atomic<bool> playbackStreamReady { false }, captureStreamReady { false };
    std::atomic<bool> playbackStreaming { false }, captureStreamStreaming { false };

    // Bumped by the loop thread on every STREAMING transition; the start
    // handshake waits for it to move (see waitForStreamsToBeStreaming()).
    std::atomic<uint64_t> streamingGeneration { 0 };

    // Set when the loop thread had to be abandoned - see abandonLoopThread().
    std::atomic<bool> loopAbandoned { false };

    // Published by the process callbacks so that the latency queries are
    // lock-free - see publishLatency().
    std::atomic<int> outputLatencySamples { 0 }, inputLatencySamples { 0 };

    // Exponential backoff for rebuild attempts after the server went away.
    // Written by the monitor thread and by requestActive(), so these are
    // atomics rather than plain members.
    std::atomic<double> nextRebuildAttemptTime { 0.0 };
    std::atomic<int> rebuildBackoffMs { 1000 };

    std::unique_ptr<StreamData> playbackStream, captureStream;
    std::unique_ptr<AudioThread> audioThread;
    std::unique_ptr<MonitorThread> monitorThread;

    // Created once and never replaced: the loop thread holds its own reference,
    // so it has to survive the connection it was created for.
    std::shared_ptr<LoopTaskQueue> taskQueue = std::make_shared<LoopTaskQueue>();
    std::shared_ptr<ServerHandle> serverHandle = std::make_shared<ServerHandle>();

    struct pw_loop* pwLoop = nullptr;
    struct pw_context* pwContext = nullptr;
    struct pw_core* pwCore = nullptr;
    CoreListener* coreListener = nullptr;   // never freed once abandoned - see abandonLoopThread()

    // Everything a process callback needs to build its channel pointers. It is
    // held through a pointer so that an abandoned connection can leak it whole -
    // see the destructor.
    struct ProcessBuffers
    {
        HeapBlock<float*> outputChannelPointers, inputChannelPointers;
        int outputCapacity = 0, inputCapacity = 0;

        // Written by the capture stream's process callback and read by the
        // playback one. Those run on the same thread for as long as the context
        // has a single data loop, which is the default
        // (context.num-data-loops == 1). With more than one data loop the two
        // streams could be dispatched on different threads, and this would have
        // to become a lock-free hand-over instead.
        AudioBuffer<float> capture;
        int captureSamples = 0;
    };

    std::unique_ptr<ProcessBuffers> buffers = std::make_unique<ProcessBuffers>();
    bool buffersLeaked = false;

    int maxBlockFrames = 8192;

    double requestedSampleRate = 48000.0;
    int requestedBufferSize = 512;
    // Which nodes the streams are connected to. Written by the control thread
    // and read by the monitor thread (which decides where to rebuild them) as
    // well as by the latency/routing queries, so these are atomics.
    std::atomic<uint32_t> sinkNodeId { SPA_ID_INVALID }, sourceNodeId { SPA_ID_INVALID };
    uint32_t streamIdCounter = 0;

    bool followsDefaultSink = false, followsDefaultSource = false;
    String currentDefaultSinkKey, currentDefaultSourceKey;

    BigInteger enabledInputChannels, enabledOutputChannels;
    WaitableEvent streamsStartedEvent, streamsStreamingEvent;

    static constexpr int streamStartTimeoutMs = 10000;
    static constexpr int loopThreadStopTimeoutMs = 2000;

    // How long the control thread waits for the loop thread to run a task it
    // posted. Every one of those is a handful of messages to the server, so
    // anything close to this means the loop is stuck.
    static constexpr int streamTaskTimeoutMs = 2000;
    static constexpr int startHandshakeTimeoutMs = 1200;
    static constexpr int numStartAttempts = 2;
    static constexpr int maxRebuildBackoffMs = 30000;

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
        // The watcher is started by the first scanForDevices(): every
        // AudioDeviceManager creates this type, including the many hosts and
        // plug-ins that never touch PipeWire, and a thread that connects to the
        // server every second for the lifetime of the process is a real cost on
        // a laptop. Nothing is watched before somebody has asked for the list.
    }

    ~PipeWireAudioIODeviceType() override
    {
        std::unique_ptr<DeviceListWatcher> watcherToStop;

        {
            const ScopedLock sl (watcherLock);
            watcherToStop = std::move (watcher);
            watcherStopped = true;
        }

        if (watcherToStop != nullptr)
        {
            // Never force-kill the watcher: it may be in the middle of a
            // PipeWire call, and the scan abandons itself once the thread has
            // been asked to stop.
            watcherToStop->signalThreadShouldExit();

            if (watcherToStop->waitForThreadToExit (threadStopTimeoutMs))
            {
                watcherToStop.reset();
            }
            else
            {
                // The thread is still inside a PipeWire call. Its destructor
                // will block until it has left it (juce::Thread::stopThread (-1)
                // in ~Thread()), so the wait above is only a best effort - it
                // cannot make the destructor return any earlier.
                JUCE_PIPEWIRE_LOG ("the PipeWire device list thread did not stop in time");
            }
        }
    }

    //==============================================================================
    // Scanning the graph is a round-trip to the server and can take several
    // seconds, so its results are published into this snapshot and the
    // enumeration methods only ever read the snapshot. Holding a lock across
    // the scan would block the message thread - which is where the host calls
    // getDeviceNames()/createDevice() - until the server answers.
    void scanForDevices() override
    {
        startWatcherIfNeeded();

        DeviceListSnapshot newSnapshot;
        PipeWireScanResult result;

        if (! PipeWireRegistryScanner::scan (result))
        {
            // A failed scan - a server that is restarting, or one that didn't
            // answer within the timeout - must not be reported as "there are no
            // devices any more": replacing the list would make the host tear
            // down the device it is currently using, so the previous snapshot is
            // kept instead.
            const ScopedLock sl (snapshotLock);
            snapshot.valid = true;
            return;
        }

        appendDevices (result.sinks,   result.defaultSinkKey,   newSnapshot.outputNames,
                       newSnapshot.outputKeys, newSnapshot.outputChannels, newSnapshot.defaultOutputKey);
        appendDevices (result.sources, result.defaultSourceKey, newSnapshot.inputNames,
                       newSnapshot.inputKeys,  newSnapshot.inputChannels,  newSnapshot.defaultInputKey);

        newSnapshot.outputNames.appendNumbersToDuplicates (false, true);
        newSnapshot.inputNames.appendNumbersToDuplicates (false, true);

        newSnapshot.valid = true;

        const ScopedLock sl (snapshotLock);
        snapshot = std::move (newSnapshot);
    }

    StringArray getDeviceNames (bool wantInputNames) const override
    {
        const ScopedLock sl (snapshotLock);
        jassert (snapshot.valid); // need to call scanForDevices() before doing this
        return wantInputNames ? snapshot.inputNames : snapshot.outputNames;
    }

    int getDefaultDeviceIndex (bool forInput) const override
    {
        const ScopedLock sl (snapshotLock);
        jassert (snapshot.valid); // need to call scanForDevices() before doing this

        const auto& keys = forInput ? snapshot.inputKeys : snapshot.outputKeys;
        const auto& defaultKey = forInput ? snapshot.defaultInputKey : snapshot.defaultOutputKey;

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
        const ScopedLock sl (snapshotLock);
        jassert (snapshot.valid); // need to call scanForDevices() before doing this

        if (auto* d = dynamic_cast<PipeWireAudioIODevice*> (device))
            return asInput ? snapshot.inputKeys.indexOf (d->getSourceKey())
                           : snapshot.outputKeys.indexOf (d->getSinkKey());

        return -1;
    }

    AudioIODevice* createDevice (const String& outputDeviceName,
                                 const String& inputDeviceName) override
    {
        const ScopedLock sl (snapshotLock);
        jassert (snapshot.valid); // need to call scanForDevices() before doing this

        const auto inputIndex  = snapshot.inputNames.indexOf (inputDeviceName);
        const auto outputIndex = snapshot.outputNames.indexOf (outputDeviceName);

        if (inputIndex < 0 && outputIndex < 0)
            return nullptr;

        const auto hasOutput = outputIndex >= 0;

        return new PipeWireAudioIODevice (getTypeName(),
                                          hasOutput ? snapshot.outputKeys.getReference (outputIndex) : String(),
                                          hasOutput ? outputDeviceName : String(),
                                          hasOutput ? snapshot.outputChannels.getReference (outputIndex) : StringArray(),
                                          inputIndex >= 0 ? snapshot.inputKeys.getReference (inputIndex) : String(),
                                          inputIndex >= 0 ? inputDeviceName : String(),
                                          inputIndex >= 0 ? snapshot.inputChannels.getReference (inputIndex) : StringArray());
    }

private:
    //==============================================================================
    // Created on the first scan and never before - see the constructor.
    void startWatcherIfNeeded()
    {
        const ScopedLock sl (watcherLock);

        if (watcher != nullptr || watcherStopped)
            return;

        watcher = std::make_unique<DeviceListWatcher> (*this);
        watcher->startThread (Thread::Priority::low);
    }

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
            int idleCycles = 0;

            while (! threadShouldExit())
            {
                // Poll frequently while the graph is changing, and back off once
                // it has been stable for a while: every poll opens a fresh
                // connection to the server and walks the whole graph.
                const auto cycleMs = idleCycles < 5 ? 1000 : 5000;

                for (int i = 0; i < cycleMs / 100 && ! threadShouldExit(); ++i)
                    Thread::sleep (100);

                if (threadShouldExit())
                    break;

                idleCycles = owner.refreshDeviceListIfNeeded() ? 0 : idleCycles + 1;
            }
        }

    private:
        PipeWireAudioIODeviceType& owner;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DeviceListWatcher)
    };

    // Returns true when the device list changed.
    bool refreshDeviceListIfNeeded()
    {
        // The scan runs outside of the snapshot lock, so a slow or misbehaving
        // server can't block the enumeration methods.
        scanForDevices();
        StringArray outputs, inputs;

        {
            const ScopedLock sl (snapshotLock);
            outputs = snapshot.outputNames;
            inputs = snapshot.inputNames;
        }

        if (! hasComparedList)
        {
            hasComparedList = true;
            lastOutputNames = outputs;
            lastInputNames = inputs;
            return false;
        }

        if (outputs != lastOutputNames || inputs != lastInputNames)
        {
            lastOutputNames = outputs;
            lastInputNames = inputs;
            notifyListeners();
            return true;
        }

        return false;
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

    struct DeviceListSnapshot
    {
        StringArray outputNames, outputKeys, inputNames, inputKeys;
        Array<StringArray> outputChannels, inputChannels;
        String defaultOutputKey, defaultInputKey;
        bool valid = false;
    };

    // Only ever read under snapshotLock; written by scanForDevices().
    DeviceListSnapshot snapshot;
    mutable CriticalSection snapshotLock;

    StringArray lastOutputNames, lastInputNames;
    bool hasComparedList = false;

    // Created by the first scanForDevices() - see the constructor.
    std::unique_ptr<DeviceListWatcher> watcher;
    CriticalSection watcherLock;
    bool watcherStopped = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PipeWireAudioIODeviceType)
};

//==============================================================================
// This is called from AudioIODeviceType::createAudioIODeviceType_PipeWire(),
// which is compiled into the same translation unit.
static inline AudioIODeviceType* createAudioIODeviceType_PipeWire_Native()
{
    if (! initialisePipeWireLibrary())
        return nullptr;

    return new PipeWireAudioIODeviceType();
}

} // namespace juce
