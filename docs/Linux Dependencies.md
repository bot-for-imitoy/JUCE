# JUCE Dependencies on Linux

Below is a list of the current dependencies required to build JUCE projects on
Ubuntu, separated by module. Where the dependency is optional, the preprocessor
flag used to disable it is noted.

This has been tested on Ubuntu 16.04 LTS (Xenial Xerus), 18.04 LTS (Bionic
Beaver), and 20.04 LTS (Focal Fossa). Packages may differ in name or not be
available on other distributions.

## Compiler
A C++ compiler is required. JUCE has been tested thoroughly with Clang and GCC:

    sudo apt update
    sudo apt install clang

or

    sudo apt update
    sudo apt install g++

## Packages

#### juce_audio_devices
- libasound2-dev
- libjack-jackd2-dev (unless `JUCE_JACK=0`)
- libpipewire-0.3-dev (only needed when `JUCE_PIPEWIRE=1`, which is off by default)

#### juce_audio_processors
- ladspa-sdk (unless `JUCE_PLUGINHOST_LADSPA=0`)

#### juce_core
- libcurl4-openssl-dev (unless `JUCE_USE_CURL=0`)

#### juce_graphics
- libfontconfig1-dev (unless `JUCE_USE_FONTCONFIG=0`)
- libfreetype-dev (unless `JUCE_USE_FREETYPE=0`)

These packages are available on Ubuntu 22 and 24. If libfreetype-dev is not
available you could try installing the libfreetype6-dev package.

#### juce_gui_basics
- libx11-dev
- libxcomposite-dev
- libxcursor-dev (unless `JUCE_USE_XCURSOR=0`)
- libxext-dev
- libxinerama-dev (unless `JUCE_USE_XINERAMA=0`)
- libxrandr-dev (unless `JUCE_USE_XRANDR=0`)
- libxrender-dev (unless `JUCE_USE_XRENDER=0`)
- libxi-dev (unless `JUCE_USE_XINPUT=0`)

#### juce_gui_extra
- libwebkit2gtk-4.1-dev (unless `JUCE_WEB_BROWSER=0`)

On older systems, where 4.1 is not available, you can also use

- libwebkit2gtk-4.0-dev

Compiled JUCE applications will dynamically load whichever library version is
available during runtime.

#### juce_opengl
- libglu1-mesa-dev
- mesa-common-dev
- libegl-dev

The command for the required dependencies is as follows:

    sudo apt update
    sudo apt install libasound2-dev libjack-jackd2-dev \
        ladspa-sdk \
        libcurl4-openssl-dev  \
        libfreetype-dev libfontconfig1-dev \
        libx11-dev libxcomposite-dev libxcursor-dev libxext-dev libxinerama-dev libxrandr-dev libxrender-dev libxi-dev \
        libwebkit2gtk-4.1-dev \
        libglu1-mesa-dev mesa-common-dev libegl-dev

The optional backends are covered below, starting with [PipeWire](#optional-pipewire).

## Optional: PipeWire

The PipeWire backend is disabled by default. To build with it, set
`JUCE_PIPEWIRE=1` and install the development headers:

    sudo apt install libpipewire-0.3-dev

libpipewire is loaded at run time, so nothing has to be linked, but the compiler
has to be able to find the headers. The package installs them into
`/usr/include/pipewire-0.3` and `/usr/include/spa-0.2`, which are not in the
default include path. `libpipewire-0.3.pc` lists `libspa-0.2` in its `Requires:`,
so asking pkg-config for the former prints both `-I` flags - with CMake this is
enough:

    find_package(PkgConfig REQUIRED)
    pkg_check_modules(PIPEWIRE REQUIRED libpipewire-0.3)
    target_include_directories(<target> SYSTEM PRIVATE ${PIPEWIRE_INCLUDE_DIRS})

With GCC and Clang, `SYSTEM` makes CMake pass these directories with `-isystem`
instead of plain `-I`, which the compiler treats as system headers. That avoids a
number of pedantic warnings from these third-party headers. Nothing is linked
from pkg-config here: libpipewire is loaded with `dlopen()` at runtime, so adding
its `-l` flag would turn it into a hard startup dependency for no benefit.
