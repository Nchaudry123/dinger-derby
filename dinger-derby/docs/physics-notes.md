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

Side-view swing lab (separate from the pitching simulator):

1. **Kinematic bat** — angle profile about the hands (pivot); point velocity **v_bat = f(ω, s)** along the barrel.
2. **Segment vs circle** contact; radius tapers handle → barrel.
3. **Sweet spot** scales COR and effective bat mass (mishits lose energy).
4. **Impulse** with finite effective bat mass:  
   `j = −(1+e) v_rel·n / (1/m_ball + 1/m_eff)`  
   Exit speed / launch angle read from post-impulse ball velocity.
5. Light **tangential friction** for spin feel.

Demo: `./build/bat_physics_demo` — Space swing, R reset, `[ ]` pitch speed, `- =` power, `1/2/3` height, `A/D` timing.
