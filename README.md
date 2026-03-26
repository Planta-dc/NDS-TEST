# DSi MP3 Player

A homebrew MP3 player for the Nintendo DSi, built by merging:
- **SSEQPlayer** (CaitSith2 / Rocket Robz) — DSi-compatible FAT init, exploit
  launch chain, and BlocksDS build system
- **LMP-ng** — libmad MP3 decode pipeline and PCM ring buffer streaming

## How it works

The **ARM9** decodes MP3 frames using libmad into a 16-bit PCM ring buffer.
The **ARM7** plays that buffer on channels 0 (left) and 1 (right) in
`SOUND_REPEAT` mode. When the ARM7 timer detects it has consumed a chunk,
it signals the ARM9 via FIFO to decode more frames.

This avoids the DSi pitch-table bug that breaks `swiGetPitchTable` on DSi
BIOS — we never call that SWI; libmad produces raw PCM directly.

## Requirements

- [BlocksDS](https://github.com/blocksds/sdk) toolchain at `/opt/blocksds`
- [Wonderful Toolchain](https://wonderful.asie.pl/) GCC at `/opt/wonderful`
- libmad for NDS (available via BlocksDS external packages at
  `/opt/blocksds/external/libs/libmad`)

## Build

```bash
make
```

This produces `DSi_MP3_Player.nds`.

## SD Card Setup

Put your `.mp3` files in `/mp3/` on your DSi SD card:

```
SD:/
  mp3/
    song1.mp3
    song2.mp3
    albums/
      track01.mp3
      ...
  DSi_MP3_Player.nds
```

The scanner also checks the root `/` if `/mp3/` is empty.

## Launch

Use any DSi exploit that can run `.nds` homebrew:
- **TWiLight Menu++** — just navigate to the `.nds` and press A
- **HiyaCFW** — place in any accessible folder
- **MoonShell 2** — supported via `extlink.dat` protocol

You can also launch a specific MP3 directly from TWiLight Menu++ if it passes
the file path as argv[1].

## Controls

| Button | Browse mode | Playback mode |
|--------|-------------|---------------|
| UP/DOWN | Scroll list | — |
| LEFT/RIGHT | Page up/down | — |
| A | Play selected | — |
| B | — | Stop, return to list |
| START | — | Pause / Resume |
| L | — | Previous track |
| R | — | Next track |
| DOWN | — | Toggle bottom backlight |

## Known Limitations

- No ID3 tag display (shows filename only)
- No VBR seek support (libmad decodes sequentially)
- Stereo 44.1 kHz is ideal; very high bitrate files may cause buffer underruns
  on original DS hardware (fine on DSi)

## License

Source derived from SSEQPlayer (CC0 / public domain) and LMP-ng (GPL v2).
This combined work is GPL v2.
