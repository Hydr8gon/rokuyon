# rokuyon
An experimental N64 emulator

### Overview
My main goal with rokuyon is to learn about the N64's hardware so I can write homebrew like
[sodium64](https://github.com/Hydr8gon/sodium64). If it ends up being more than that, I wouldn't mind making a modern,
accurate N64 emulator with built-in software/hardware rendering and no messy plugins.

### Downloads
rokuyon is available for Windows, macOS, Linux, and Switch. The latest builds are automatically provided via GitHub
Actions, and can be downloaded from the [releases page](https://github.com/Hydr8gon/rokuyon/releases).

### Usage
No setup is required to run things in rokuyon, but right now it only supports NTSC ROMs in big-endian format. Save types
are not automatically detected, and must be manually selected in the file menu to work. Performance will be bad without
a powerful CPU, and you'll probably encounter plenty of emulation issues. At this stage, rokuyon should be considered a
curiosity and not a dedicated emulator for playing games.

### Contributing
This is a personal project, and I've decided to not review or accept pull requests for it. If you want to help, you can
test things and report issues or provide feedback. If you can afford it, you can also donate to motivate me and allow me
to spend more time on things like this. Nothing is mandatory, and I appreciate any interest in my projects, even if
you're just a user!

### Building
**Windows:** Install [MSYS2](https://www.msys2.org) and run the command
`pacman -Syu mingw-w64-x86_64-{gcc,pkg-config,wxWidgets,portaudio,jbigkit} make` to get dependencies. Navigate to the
project root directory and run `make -j$(nproc)` to start building.

**macOS/Linux:** On the target system, install [wxWidgets](https://www.wxwidgets.org) and
[PortAudio](https://www.portaudio.com). This can be done with the [Homebrew](https://brew.sh) package manager on macOS,
or a built-in package manager on Linux. Run `make -j$(nproc)` in the project root directory to start building.

**Switch:** Install [devkitPro](https://devkitpro.org/wiki/Getting_Started) and its `switch-dev` package. Run
`make switch -j$(nproc)` in the project root directory to start building.

### Hardware References
* [N64brew Wiki](https://n64brew.dev/wiki/Main_Page) - Extensive documentation of both hardware and software
* [RSP Vector Instructions](https://emudev.org/2020/03/28/RSP.html) - Detailed information on how vector opcodes work
* [RCP Documentation](https://dragonminded.com/n64dev/Reality%20Coprocessor.pdf) - Nice reference for a subset of RDP
functionality
* [RDP Triangle Command Guide](https://docs.google.com/document/d/17ddEo61V0suXbSkKP5mY97QxgUnB-QfAjuBIsPiLWko) - Covers
everything related to RDP triangles
* [n64-systemtest](https://github.com/lemmy-64/n64-systemtest) - Comprehensive tests that target all parts of the system
* [n64dev](https://github.com/mikeryan/n64dev) - A collection of useful documents and source code

### Other Links
* [Hydra's Lair](https://hydr8gon.github.io) - Blog where I may or may not write about things
* [Discord Server](https://discord.gg/JbNz7y4) - A place to chat about my projects and stuff
