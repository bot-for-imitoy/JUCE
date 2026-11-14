cmake_minimum_required(VERSION 3.22)

project(PipeWireStressTest VERSION 1.0.0 LANGUAGES C CXX)

# Standalone harness: builds the JUCE modules straight out of the repository so
# that the PipeWire backend can be exercised without installing JUCE first.
set(JUCE_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(JUCE_BUILD_EXTRAS OFF CACHE BOOL "" FORCE)
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../.." JUCE)

find_package(PkgConfig REQUIRED)
pkg_check_modules(PIPEWIRE REQUIRED libpipewire-0.3)

juce_add_console_app(PipeWireStressTest PRODUCT_NAME "PipeWireStressTest")

target_sources(PipeWireStressTest PRIVATE main.cpp)

target_compile_definitions(PipeWireStressTest PRIVATE
    JUCE_PIPEWIRE=1
    # Compiles the backend's internal log lines in. They stay silent unless the
    # run sets PW_LOG=1, which routes them to stdout (see pwlog in main.cpp).
    JUCE_PIPEWIRE_LOGGING=1
    JUCE_ALSA=1
    # The device-list notifications are delivered through an AsyncUpdater, and
    # this console app has to pump the message loop by hand to receive them.
    JUCE_MODAL_LOOPS_PERMITTED=1
    JUCE_JACK=0
    JUCE_USE_CURL=0
    JUCE_WEB_BROWSER=0
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:PipeWireStressTest,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:PipeWireStressTest,JUCE_VERSION>")

# libpipewire is dlopen()ed, so only the headers are needed here.
target_include_directories(PipeWireStressTest SYSTEM PRIVATE ${PIPEWIRE_INCLUDE_DIRS})

target_link_libraries(PipeWireStressTest PRIVATE
    juce::juce_audio_devices
    juce::juce_audio_basics
    juce::juce_events
    juce::juce_core
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)

# A reference client that does the same thing as the backend without any JUCE
# code, so that a misbehaving device can be pinned on JUCE or on the session.
# It links libpipewire directly - being a diagnostic tool, it is allowed to.
add_executable(PipeWireRefClient ref-client.c)
target_include_directories(PipeWireRefClient SYSTEM PRIVATE ${PIPEWIRE_INCLUDE_DIRS})
target_link_libraries(PipeWireRefClient PRIVATE ${PIPEWIRE_LIBRARIES} pthread)
