# b2-libretro

Adaptation of [Tom Seddon's b2 emulator](https://github.com/tom-seddon/b2) for BBC Micro, to run as a libretro core, possibly via RetroArch or some other frontend.

# Changes compared to standalone b2 emulator:
- Display is limited to the emulation itself. That also means that there is no windowing UI or any function that would be available via menus. Certain options are exposed either via their obvious libretro counterpart (such as reset), or as core options.
- Input is handled via libretro, that means joypad (RetroPad) and keyboard is supported. Analogue joystick input is mapped to left analog stick and button A, but all digital inputs (D-pad and other retropad buttons) can be individually assigned.
- Drive 0 image can be selected via "Load content" option of libretro, and if needed, it can be changed using disc control functions.
- Not all machine models are covered, notably, BBC Master is not supported (yet).

# Documentation

To be added to libretro doc site.

# Bugs/feedback/etc.

Please submit feedback to this repo: https://github.com/zoltanvb/b2-libretro/issues

# Licence

For b2 licence, see https://github.com/tom-seddon/b2#licence

Licence of libretro adaptation: GPL v3.

