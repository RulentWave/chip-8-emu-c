
# chip-8-emu

A Chip-8 emulator written in C using Raylib.
![Space Invaders](screenshots/Screenshot_20250219_230909-1.avif)
You will need to install the Raylib-dev files to build.
```
sudo apt install raylib-dev
```
```
sudo dnf install raylib-devel
```

## Building

To compile the emulator, run:

```bash
cc -o nob nob.c
./nob
```

This starts the build process using `nob` and will compile `chip-8-emu`.

## Usage

```bash
chip-8-emu [OPTION...] FILEPATH
```

### Options

*   `-f, --fps=NUMBER`: FPS limit. Defaults to 60.
*   `-h, --cpuherz=NUMBER`: Set clock speed in hz. By default, uses per instruction cycle speed that aproximates the original COSMIC VIP CHIP-8 timings
*   `-s, --scalefactor=NUMBER`: Scaling factor. Defaults to 32.
*   `-?, --help`: Give this help list.
*   `--usage`: Give a short usage message.

### Example

To run the emulator with a ROM file named `pong.ch8` with a scaling factor of 16 and an FPS limit of 120:

```bash
chip-8-emu -s 16 -f 120 pong.ch8
```

## Dependencies

*   Raylib

## License

This project is licensed under the MIT License - see the `LICENSE` file for details.
