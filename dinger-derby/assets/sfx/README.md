# Bat crack sound effects

The game only plays **bat crack** audio (no crowd, wall, or music beds).

| File | Use |
|------|-----|
| `bat_crack.wav` | Barrel / best contact |
| `bat_crack_solid.wav` | Solid square-up |
| `bat_crack_soft.wav` | Light / mishit |

Shipped samples are a wooden-bat crack (~0.24s mono 44.1 kHz). Contact quality
picks soft / solid / barrel via pitch and volume in `ProceduralSfx`.

The game searches `assets/sfx/` relative to cwd (`build/../assets/sfx` works).

## Important: MLB audio

**Do not rip audio from MLB.TV, broadcasts, or official game footage.**
