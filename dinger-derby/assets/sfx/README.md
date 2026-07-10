# Bat / park sound effects

## Optional WAV drops

Place files here to override the built-in synthesized bat crack:

| File | Use |
|------|-----|
| `bat_crack.wav` | Solid barrel contact (preferred) |
| `bat_crack_soft.wav` | Mishit / handle (optional) |
| `crowd_pop.wav` | Crowd surge after HRs (optional) |

Formats: mono or stereo WAV that SFML can load (PCM works best).

The game looks for these relative to the working directory (`assets/sfx/…`
when run from the repo root, or `../assets/sfx/…` from `build/`).

## Important: MLB audio

**Do not rip audio from MLB.TV, broadcasts, or official game footage.**
That material is copyrighted.

Use:

- your own recordings (batting cage / BP with permission), or
- royalty-free / licensed SFX packs that allow redistribution in games.

If no file is present, Dinger Derby uses a multi-layer procedural wood-bat
crack designed to *evoke* a broadcast hit — it is not a copy of any MLB sound.
