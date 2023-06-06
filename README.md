# rokuyon
An experimental N64 emulator

### Overview
My main goal with rokuyon is to learn about the Nintendo 64 hardware so I can write homebrew like [sodium64](https://github.com/Hydr8gon/sodium64). If it ends up being more than that... I wouldn't mind making a modern, accurate N64 emulator with built-in software/hardware rendering and no messy plugins.

### Downloads
rokuyon is available for Linux, macOS, Windows, and Switch. Automatic builds are provided via GitHub Actions; you can download them on the [releases page](https://github.com/Hydr8gon/rokuyon/releases).

### Usage
Although still in early stages, rokuyon should be plug-and-play with any NTSC ROMs in big-endian format. Performance will be bad and there will be plenty of issues. At this stage, rokuyon should be considered a curiosity and not a dedicated emulator for playing games.

### References
* [n64dev](https://github.com/mikeryan/n64dev) - A collection of useful materials for initial research
* [N64brew Wiki](https://n64brew.dev/wiki/Main_Page) - Extensive and organized documentation of the whole system
* [libdragon](https://github.com/DragonMinded/libdragon) - An open-source N64 SDK with good examples of using the hardware
* [RSP Vector Instructions](https://emudev.org/2020/03/28/RSP.html) - Detailed information on how vector opcodes work
* [RCP Documentation](https://dragonminded.com/n64dev/Reality%20Coprocessor.pdf) - Nice reference for a subset of RDP functionality
* [RDP Triangle Command Guide](https://docs.google.com/document/d/17ddEo61V0suXbSkKP5mY97QxgUnB-QfAjuBIsPiLWko) - Covers everything related to RDP triangles
* [N64 bilinear filter (3-point)](https://www.shadertoy.com/view/Ws2fWV) - Reference implementation of the N64's texture filter
* [dgb-n64](https://github.com/Dillonb/n64) - Provides details of the FLASH interface, since it isn't documented
* Hardware tests - I'll probably be testing things myself as I go along

### Other Links
* [Hydra's Lair](https://hydr8gon.github.io) - Blog where I may or may not write about things
* [Discord Server](https://discord.gg/JbNz7y4) - Place to chat about my projects and stuff
