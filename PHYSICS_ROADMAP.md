# JojiBaseballEngine — Physics Model Roadmap

Last updated: 2026-06-21

---

## Implementation Coverage Summary

| Domain | Coverage |
|---|---|
| Ball flight physics | 80% |
| Batting / contact | 78% |
| Fielding | 58% |
| Base running | 70% |
| Pitching | 70% |
| Environment (wind, altitude, etc.) | 60% |

---

## Ball Flight Physics

| Model | Status | Notes | File |
|---|---|---|---|
| Gravity | ✅ | `Gravity=9.8`, RK4 integration | BallPhysicsEngine.cpp |
| Air drag | ✅ | `0.5 * ρ * Cd * A / m` | BallPhysicsEngine.cpp |
| Magnus effect | ✅ | 3D spin axis cross product: `F = Cl * ρ * A/2m * speed² * (spinAxis × v̂)` | BallPhysicsEngine.cpp |
| 3D spin axis | ✅ | `SpinState {axisX,Y,Z, rateRpm}`; batted ball axis derived from backSpin + sideSpin + spray angle | BallPhysicsEngine.cpp |
| **Seam-shift wake (SSW)** | ⚠️ | Pitch SSW implemented (gyro fraction model); batted ball SSW pending | AnimationPlanBuilder.cpp |
| Spin decay | ✅ | Batted ball: 6%/s exponential decay; pitch: 10%/s | BallPhysicsEngine.cpp, AnimationPlanBuilder.cpp |
| Ground bounce | ✅ | Restitution coefficient + simulateBounceAndRoll | BallPhysicsEngine.cpp |
| Rolling | ✅ | RollDeceleration + LandFriction | BallPhysicsEngine.cpp |
| Fence collision | ✅ | crossesFence / fenceIntersectionPoint (polar model) | BallPhysicsEngine.cpp |
| Wall carom (reflection) | ✅ | `v' = v - 2(v·n)n * WallRestitution` | AnimationPlanBuilder.cpp |
| Temperature / pressure / altitude | ✅ | `BallparkConfig.altitudeFeet` + `temperatureFahrenheit` → standard atmosphere formula | BallPhysicsEngine.cpp, PlayResolutionEngine.h |
| Wind | ✅ | `BallparkConfig.windSpeedMph/windDirectionDeg`; drag uses relative velocity (ball−wind); `windOut()/windIn()` presets | BallPhysicsEngine.cpp, PlayResolutionEngine.h |

---

## Batting / Contact

| Model | Status | Notes | File |
|---|---|---|---|
| Bat speed | ✅ | `68 + power*0.24 + powerIntent*8` | SwingEngine.cpp:40 |
| Attack angle | ✅ | −10 to +26°, scaled by batter ability | SwingEngine.cpp:41 |
| Timing error | ✅ | `timingError` → contactQuality | SwingEngine.cpp:42 |
| Exit velocity | ✅ | Function of pitch speed + bat speed + contact quality | AtBatEngine.cpp |
| **COR (coefficient of restitution)** | ✅ | Nathan/Adair model: `K=(1+COR)*q/(1+q)`, `eV=K*batSpeed+(K-1)*pitchSpeed`; q and COR scale with barrelAccuracy/contactQuality | ContactEngine.cpp |
| **3D bat path** | ✅ | `Swing.swingPlaneAngle` (degrees): inside pitch→pull, outside→opposite field; applied to spray angle via handedness-aware multiplier | SwingEngine.cpp, ContactEngine.cpp |
| Spin generation | ⚠️ | `launchAngle*55 + exitVel*8` approximation | BallPhysicsEngine.cpp:438 |

---

## Pitching

| Model | Status | Notes | File |
|---|---|---|---|
| Pitch velocity | ✅ | Per pitch type + pitcher ability | PitchEngine.cpp:60 |
| Spin rate | ✅ | Per pitch type range (1200–3050 rpm) | PitchEngine.cpp:81–106 |
| **3D spin axis** | ❌ | No axis → break direction not physically derived | — |
| **Seam-shift wake (SSW)** | ❌ | 2-seam / cutter / sinker seam effects absent | — |
| Fatigue degradation | ⚠️ | `form` reduces velocity and command | GameEngine.cpp |
| Release point | ❌ | Fixed value | — |

---

## Fielding

| Model | Status | Notes | File |
|---|---|---|---|
| Movement speed | ✅ | `speedFeetPerSecond + reactionSeconds` | PlayResolutionEngine.cpp |
| Reaction time | ✅ | `reactionSeconds` per fielder | PlayResolutionEngine.cpp |
| Catch probability | ✅ | difficulty × fielding → probability | PlayResolutionEngine.cpp:288 |
| Relay / cutoff logic | ✅ | Cutoff fielder decision | PlayResolutionEngine.cpp |
| Acceleration / deceleration | ✅ | Kinematic ramp: `d(t) = 0.5*a*t²` until top speed, then cruise; `Fielder.accelerationFeetPerSecond2 = 13` | PlayResolutionEngine.cpp |
| **Change of direction** | ❌ | Straight-line paths only | — |
| Jump / diving catch | ❌ | Included as result probability; no movement model | — |
| Throw arc | ❌ | Linear path in animation; no parabola | AnimationPlanBuilder.cpp |

---

## Base Running

| Model | Status | Notes | File |
|---|---|---|---|
| Acceleration | ✅ | `accelerationFeetPerSecond2` | AnimationTypes.h |
| Top speed | ✅ | `topSpeedFeetPerSecond` | AnimationTypes.h |
| Base turn penalty | ⚠️ | `turnPenaltySeconds` — simplified | AnimationTypes.h |
| Sliding | ⚠️ | `slideTimeSeconds` | AnimationTypes.h |
| Tag plays | ✅ | Arrival time comparison | PlayResolutionEngine.cpp |
| **Lead / return (steal)** | ❌ | No steal simulation | — |
| **Route optimization** | ❌ | Straight-line paths only | — |

---

## Priority Roadmap

### S — 3D Spin Axis Model
**Status:** ✅ Implemented (2026-06-21)  
`SpinState {axisX, axisY, axisZ, rateRpm}` replaces `backSpinRpm + sideSpin` scalars.  
Magnus force = `Cl * ρA/2m * speed² * (spinAxis × v̂)` via RK4 cross product.  
Batted ball spin axis derived from `backSpin`, `sideSpin`, and `sprayAngle` in `spinStateForBattedBall()`.  
Next: add per-pitch-type spin axes to `PitchEngine` once pitch trajectory is physically simulated.

### A — Physical Pitch Simulation + SSW
**Status:** ✅ Implemented (2026-06-21)  
- `Pitch` struct gains `spinAxisX/Y/Z` (unit vector) + `activeSpin` [0–1] per pitch type.  
- `PitchEngine` sets spin axes per pitch type/handedness (e.g. 4-seam FB: (-0.97, 0, ±0.26), curveball: (+0.97, 0, ∓0.26), slider: (0.25, 0, ±0.97)).  
- `AnimationPlanBuilder::buildPitchAnimation` replaces quadratic interpolation with RK4 at 4 ms steps.  
- Physics: gravity + drag + Magnus (`activeSpin × Cl × v² × (axis × v̂)`) + SSW (`(1−activeSpin) × Cl × v² × sswDir`).  
- `activeSpin`: 4-seam 0.93, cutter 0.50, slider 0.35, splitter 0.25 — controls how much movement comes from Magnus vs seam wake.  
- Calibrated `Cl` factor (0.571× batted-ball value) to match Statcast pitch break magnitudes.

### B — Fielder Acceleration / Deceleration
**Status:** ✅ Implemented (2026-06-21)  
`Fielder.accelerationFeetPerSecond2 = 13.0 ft/s²`.  
`travelTime` uses kinematic ramp: `tRamp = vMax/a`, `dRamp = 0.5*vMax*tRamp`; then cruise at `vMax`.  
Animation path is still linear (constant visual speed) — kinematic path animation is future work.

### B — Altitude / Temperature Correction
**Status:** ✅ Implemented (2026-06-21)  
`BallparkConfig` gains `altitudeFeet` (default 0) and `temperatureFahrenheit` (default 72).  
`computeAirDensity()` uses the standard barometric formula: `P = 101325 * (1 - 2.2558e-5 * altM)^5.2559`, `ρ = P / (287.05 * T_K)`.  
`BallparkConfig::highAltitude()` preset added (5200 ft, 68°F → ρ ≈ 1.00 kg/m³ vs 1.225 at sea level).

### S — Ground Ball Rolling Model (BABIP structural fix)
**Status:** ⛔ Investigated 2026-06-22 — blocked by RS/G calibration dependency

**Root cause identified:** The auto-hit behavior (BABIP ~0.473) is NOT primarily a "no-man's land" problem but a **candidate selection bug**: `groundCandidateIndices` uses `landingPoint` distance. Very-negative-LA balls (LA −4° to 0°) land at 10–30 ft → fall into `d < 34` branch → candidates = {Catcher, Pitcher} → both excluded by depth filter in `evaluateFielding` → `fielderId = -1` → auto-hit. This accounts for ~20–25% of all BIP.

**Fix attempted (candidateFix):** Changed `d = max(landDist, min(restDist, 174.0))` so `groundCandidateIndices` uses `finalRestPoint` distance. Correct physics — these balls DO roll into SS/2B territory.

**Result:** BABIP collapsed from 0.473 → 0.295, RS/G from 3.91–5.22 → 2.31–2.36. The sc3 RS/G calibration depended on the auto-hit bug for ~25% of BIP. Fixing it converts those balls from 100% hits to 30–90% outs (zone model), halving RS.

**Attempted compensations (all insufficient):**
- LA range -10° → -4° (ContactEngine): no RS/G effect
- `contactChance` 0.73 → 0.80 (K% 26% → 24%): RS/G 2.36 only
- `availableTime = 4.0s` (runner-based) + `arrivesInTime` check: 2.33 only
- `softPenalty` in `groundBallOutProb` for EV < 65mph: 2.34 only
- All combined: 2.34

**Conclusion:** candidateFix cannot be retained without a full engine recalibration. To restore RS/G to 3.91–5.22 with correct physics, HR% must be raised from 1.5% → 3%+ AND K% reduced to 20–22%.

**Current state (post-session revert):** candidateFix reverted. LA range -4° minimum retained (physical realism, no RS/G effect). `groundFieldingTarget` finalRestPoint fallback retained (minor improvement).

**Correct implementation path (future):**
1. Fix candidateFix + re-run with HR% 3% (raise barrel thresholds or park factors) + K% 22% (horizontalBatError redesign, see below)
2. After those two changes, recalibrate groundBallOutProb base rates to achieve BABIP ~0.300
3. Expected effort: High. Requires full system recalibration (2–3 sessions).

**Files:** `PlayResolutionEngine.cpp::groundCandidateIndices`, `groundFieldingTarget`, `groundBallOutProb`

---

### K% / contactQuality Coupling — Structural Limit
**Problem:** K% floor ~25–26% cannot be reduced without raising RS/G. Root cause:
```
timingError → horizontalBatError (×2.2) → barrelAccuracy → contactQuality
```
Tighter `timingError` improves contactChance (K% ↓) AND barrelAccuracy (contactQuality ↑ → RS/G ↑). They are structurally coupled through `horizontalBatError`.  
**Attempted fix (2026-06-22):** Removed `timingQuality` from `contactQuality` formula, raised its weight in `contactChance`. Failed — BABIP worsened (0.473 → 0.487) because barrelAccuracy still inherits timingError signal.  
**True fix:** Redesign `horizontalBatError` to separate bat-tracking skill from pitch timing:
```cpp
// Replace: swing.timingError * 2.2
// With:    swing.timingError * 0.6 + batTrackingNoise(batter.contact)
```
This weakens the timing→barrelAccuracy coupling while preserving directional realism (late → off handle).  
**Estimated effort:** Medium. Requires recalibrating bimodal BIP thresholds after distribution shift.

---

### C — Stolen Base / Lead Model
**What to do:** Wire up `GameEngine::checkStolenBase` (stub already exists) with pitcher attention, catcher arm, and runner speed.  
**Files:** `GameEngine.cpp`  
**Status:** ❌ Not started (stub only)

### C — Throw Arc Model
**Status:** ✅ Implemented (2026-06-21)  
`ThrowAnimation.arcHeightFeet` = `clamp(dist * 0.07, 4, 25)` ft.  
Guide arc: 16-segment polyline. Ball dot and shadow follow parabola `z = arcH * 4t(1-t)`.  
Screen lift = `arcZ * FieldScale * 0.7` pixels upward.

---

## Completed Milestones

| Version | Features |
|---|---|
| v1.0 | Base-running AI, team defensive stats |
| v1.1 | SFML line score, batting-order strip, pitcher arsenal |
| v1.2 | Team select screen, series summary overlay, play result banner |
| v1.2+ | Wall carom reflection, HR fade clip, fence model unified to polar |
