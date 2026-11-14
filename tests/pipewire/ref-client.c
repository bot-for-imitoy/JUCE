/*
    A deliberately minimal PipeWire client that mimics exactly what
    juce_PipeWire_linux.cpp does, without any JUCE code involved.

    It exists to answer one question when a JUCE PipeWire device misbehaves:
    is the problem in JUCE's usage of the API, or in the API/session itself?

    It creates a play stream, waits (with a timeout) for it to reach
    PW_STREAM_STATE_PAUSED, then tears everything down, in a loop. Two pumping
    modes are available:

      single   the same thread connects the stream and pumps the loop
               (what pw-cat and most simple clients do)

      threaded the loop is pumped by a second thread while the calling thread
               creates and connects the stream (what JUCE does)

    Usage:
      ref-client <single|threaded> [iterations] [timeout-ms]

    Build:
      cc -O2 -o ref-client ref-client.c $(pkg-config --cflags --libs libpipewire-0.3) -pthread
*/

#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>

#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static const char* targetName;      /* JUCE_PW_OUTPUT_NAME equivalent, may be NULL */

/*==============================================================================
  The per-iteration state, shared between the caller and the loop thread.
*/
struct iteration
{
    struct pw_stream* stream;
    struct spa_hook   listener;

    atomic_int  paused;             /* set by state_changed */
    atomic_int  error;
    char        errorText[256];
    atomic_int  formats;            /* number of param_changed events with a rate */

    atomic_int  quit;               /* tells the loop thread to stop */
};

static void onStateChanged (void* data, enum pw_stream_state oldState,
                            enum pw_stream_state state, const char* error)
{
    struct iteration* it = data;
    (void) oldState;

    if (getenv ("REF_TRACE") != NULL)
        printf ("    state %s%s%s\n", pw_stream_state_as_string (state),
                error != NULL ? " error=" : "", error != NULL ? error : "");

    if (state == PW_STREAM_STATE_PAUSED)
        atomic_store (&it->paused, 1);

    if (state == PW_STREAM_STATE_ERROR)
    {
        atomic_store (&it->error, 1);
        snprintf (it->errorText, sizeof it->errorText, "%s", error != NULL ? error : "?");
    }
}

static void onParamChanged (void* data, uint32_t id, const struct spa_pod* param)
{
    struct iteration* it = data;

    if (id == SPA_PARAM_Format && param != NULL)
        atomic_fetch_add (&it->formats, 1);
}

static void onProcess (void* data)
{
    (void) data;
}

static const struct pw_stream_events streamEvents = {
    PW_VERSION_STREAM_EVENTS,
    .state_changed = onStateChanged,
    .param_changed = onParamChanged,
    .process = onProcess,
};

/*==============================================================================
  Loop pumping.
*/
struct pumpArgs
{
    struct pw_loop* loop;
    atomic_int*     quit;
};

static void* pumpThread (void* data)
{
    struct pumpArgs* a = data;

    while (! atomic_load (a->quit))
        pw_loop_iterate (a->loop, 20);

    return NULL;
}

/* pw_core_disconnect() marshals a destroy for every proxy the client still
   owns and then frees them, so it races with whatever the loop thread is
   dispatching at that moment. It has to run on the loop thread. */
static int disconnectOnLoop (struct spa_loop* loop, bool async, uint32_t seq,
                             const void* data, size_t size, void* user)
{
    struct pw_core* core = user;
    (void) loop; (void) async; (void) seq; (void) data; (void) size;

    pw_core_disconnect (core);
    return 0;
}

static int destroyStreamOnLoop (struct spa_loop* loop, bool async, uint32_t seq,
                                const void* data, size_t size, void* user)
{
    struct iteration* it = user;
    (void) loop; (void) async; (void) seq; (void) data; (void) size;

    pw_stream_destroy (it->stream);
    it->stream = NULL;
    return 0;
}

/*==============================================================================
  One open/close cycle. Returns 0 on success, -1 on timeout, 1 on stream error.
*/
static int runOnce (int threaded, int timeoutMs, int index)
{
    struct iteration* state = calloc (1, sizeof *state);
    struct iteration* it = state;

    char streamName[128];
    snprintf (streamName, sizeof streamName, "ref-client-%d", index);

    struct pw_main_loop* loop = pw_main_loop_new (NULL);

    if (loop == NULL)
    {
        printf ("    pw_main_loop_new failed\n");
        return -1;
    }

    struct pw_context* context = pw_context_new (pw_main_loop_get_loop (loop), NULL, 0);

    if (context == NULL)
    {
        printf ("    pw_context_new failed\n");
        return -1;
    }

    struct pw_core* core = pw_context_connect (context, NULL, 0);

    if (core == NULL)
    {
        printf ("    connect failed\n");
        return -1;
    }

    struct pw_properties* props = pw_properties_new (PW_KEY_MEDIA_TYPE, "Audio",
                                                     PW_KEY_MEDIA_CATEGORY, "Playback",
                                                     PW_KEY_MEDIA_ROLE, "Music",
                                                     PW_KEY_NODE_NAME, streamName,
                                                     PW_KEY_NODE_DESCRIPTION, streamName,
                                                     NULL);

    if (targetName != NULL)
        pw_properties_set (props, PW_KEY_TARGET_OBJECT, targetName);

    it->stream = pw_stream_new (core, streamName, props);
    pw_stream_add_listener (it->stream, &it->listener, &streamEvents, it);

    pthread_t thread;
    struct pumpArgs args = { pw_main_loop_get_loop (loop), &it->quit };

    if (threaded)
        pthread_create (&thread, NULL, pumpThread, &args);

    /* The format pods, exactly like the JUCE implementation. */
    uint8_t podBuffer[1024];
    struct spa_pod_builder builder = SPA_POD_BUILDER_INIT (podBuffer, sizeof podBuffer);

    struct spa_audio_info_raw info;
    memset (&info, 0, sizeof info);
    info.format = SPA_AUDIO_FORMAT_F32P;
    info.rate = 48000;
    info.channels = 2;
    info.position[0] = SPA_AUDIO_CHANNEL_FL;
    info.position[1] = SPA_AUDIO_CHANNEL_FR;

    const struct spa_pod* params[1];
    params[0] = spa_format_audio_raw_build (&builder, SPA_PARAM_EnumFormat, &info);

    const int connectResult = pw_stream_connect (it->stream, PW_DIRECTION_OUTPUT, PW_ID_ANY,
                                                 PW_STREAM_FLAG_AUTOCONNECT
                                                   | PW_STREAM_FLAG_MAP_BUFFERS
                                                   | PW_STREAM_FLAG_RT_PROCESS,
                                                 params, 1);

    /* "single" mode: the loop only starts being pumped once connect() has
       returned, so no server message can be delivered while the proxy that
       pw_stream_connect() just created is still being set up. */
    if (! threaded)
        pthread_create (&thread, NULL, pumpThread, &args);

    int status = 0;

    if (connectResult < 0)
    {
        printf ("    pw_stream_connect failed: %d\n", connectResult);
        status = -1;
    }
    else
    {
        const struct timespec start = { 0, 0 };
        (void) start;

        struct timespec deadline;
        clock_gettime (CLOCK_MONOTONIC, &deadline);
        deadline.tv_sec += timeoutMs / 1000;
        deadline.tv_nsec += (long) (timeoutMs % 1000) * 1000000L;

        if (deadline.tv_nsec >= 1000000000L)
        {
            deadline.tv_sec += 1;
            deadline.tv_nsec -= 1000000000L;
        }

        for (;;)
        {
            if (atomic_load (&it->paused))
                break;

            if (atomic_load (&it->error))
            {
                status = 1;
                break;
            }

            struct timespec now;
            clock_gettime (CLOCK_MONOTONIC, &now);

            if (now.tv_sec > deadline.tv_sec
                 || (now.tv_sec == deadline.tv_sec && now.tv_nsec >= deadline.tv_nsec))
            {
                status = -1;
                break;
            }

            struct timespec nap = { 0, 2 * 1000000L };
            nanosleep (&nap, NULL);
        }

        if (status == 0)
            atomic_store (&it->quit, 1);
    }

    if (status != 0)
        printf ("    paused=%d formats=%d error=%s\n",
                atomic_load (&it->paused), atomic_load (&it->formats),
                atomic_load (&it->error) ? it->errorText : "-");

    /* Teardown, mirroring closeInternal(): destroy the stream and drop the
       connection while the loop is still being pumped (both marshal a message
       and wait for the loop to write it out), then stop pumping.
    */
    if (getenv ("REF_TRACE") != NULL) printf ("    destroy:\n");
    pw_loop_invoke (pw_main_loop_get_loop (loop), destroyStreamOnLoop,
                    0, NULL, 0, true, it);
    if (getenv ("REF_TRACE") != NULL) printf ("    disconnect:\n");
    pw_loop_invoke (pw_main_loop_get_loop (loop), disconnectOnLoop,
                    0, NULL, 0, true, core);
    if (getenv ("REF_TRACE") != NULL) printf ("    join:\n");

    atomic_store (&it->quit, 1);
    pthread_join (thread, NULL);
    if (getenv ("REF_TRACE") != NULL) printf ("    context destroy:\n");

    pw_context_destroy (context);
    pw_main_loop_destroy (loop);

    return status;
}

int main (int argc, char** argv)
{
    const char* mode = argc > 1 ? argv[1] : "threaded";
    const int iterations = argc > 2 ? atoi (argv[2]) : 30;
    const int timeoutMs = argc > 3 ? atoi (argv[3]) : 3000;

    setbuf (stdout, NULL);
    setbuf (stderr, NULL);

    targetName = getenv ("REF_TARGET");

    fprintf (stderr, "before pw_init\n");
    pw_init (&argc, &argv);
    fprintf (stderr, "after pw_init\n");

    printf ("ref-client: mode=%s iterations=%d timeout=%dms pipewire=%s\n",
            mode, iterations, timeoutMs, pw_get_library_version());

    int failures = 0;

    for (int i = 0; i < iterations; ++i)
    {
        const int status = runOnce (strcmp (mode, "threaded") == 0, timeoutMs, i);

        if (status != 0)
        {
            printf ("  run %d: %s\n", i, status == 1 ? "STREAM ERROR" : "TIMEOUT");
            ++failures;
        }
    }

    printf ("ref-client: %d/%d failed\n", failures, iterations);

    pw_deinit();

    return failures == 0 ? 0 : 1;
}
