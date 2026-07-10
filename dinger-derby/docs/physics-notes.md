# Baseball pitch aerodynamics

Flight uses `PhysicsWorld3D` with:

1. **Gravity** −9.8 m/s² (world Y)
2. **Quadratic drag** (`AirResistance3D::calculateDragForce`) — Cd slightly lower for high useful spin
3. **Magnus force** (`AirResistance3D::calculateMagnusForce`) from the ball’s `angularVelocity`

## Spin → movement

World axes: **+Z plate**, **+Y up**, **+X first base**. RHP glove side is **−X**.

Force direction is **ω_active × v** (right-hand rule), where gyrospin (ω parallel to v) is removed:

| Spin (ω) | Movement |
|----------|----------|
| Backspin ≈ (−X) | Ride / lift (+Y) — four-seam |
| Topspin ≈ (+X) | Drop (−Y) — curve |
| Glove sidespin ≈ (−Y) | Break toward −X — slider / cutter |

`spinEfficiency` (0–1) scales active spin (splitters sit low; pure backspin near 1).

Spin parameter **S = r |ω| / |v|**, lift coefficient  
**Cl = 1.4 · S / (0.4 + S)** (Nathan-style saturation).

Pitch identity in the simulator is **speed + RPM + spin axis**, not baked break accelerations. Visual seam spin tracks the same `angularVelocity`.

# Bat–ball contact (`bat_physics_demo`)

**3D batting cage** (separate from the pitching simulator):

1. **Mouse PCI** on the plate plane (X / height); bat orients hands → aim.
2. **Swing types** Z Power · X Contact · C Regular (power, duration, sweet size, COR).
3. **Kinematic bat** about the hands; **v_bat = ω × r** along the barrel.
4. **Capsule vs sphere** contact; sweet spot scales COR and effective mass.
5. **Impulse** `j = −(1+e) v_rel·n / (1/m + 1/m_eff)` → exit mph + launch angle.
6. Overlay: zone, PCI ring, projected bat silhouette.

`./build/bat_physics_demo` — mouse aim, Z/X/C, Space swing, R reset, `[ ]` pitch speed.
