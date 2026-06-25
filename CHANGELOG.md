# Changelog

## v2.4 - BB% Fix, Breaking Ball Fatigue, Batter-Specific Approach (2026-06-24)

**Confirmed metrics (100-season average):**

| Metric | v2.3 → v2.4 | MLB ref |
|--------|-------------|---------|
| K% | 22.1% → **21.6%** | ~22% ✓ |
| BB% | 7.3% → **8.2%** | ~8.5% ✓ |
| BABIP | 0.297 → **0.296** | ~0.300 ✓ |
| RS/G top | 4.60–4.75 → **4.74–4.88** | 4.5–5.5 ✓ |
| RS/G bottom | 3.36–3.51 → **3.37–3.55** | ~3.5 ✓ |
| ERA range | 3.44–4.24 → **3.48–4.31** | 3.5–4.5 ✓ |

### BB% Fix — PitchEngine commandSpread

- `behindInCount` reduced `0.16 → 0.10` — pitchers in 3-ball counts don't become more accurate under pressure; previously the large spread reduction caused too many grooved strikes in ball-heavy counts
- Effect: BB% 7.3% → 8.2% (target ~8.5%)

### Breaking Ball Fatigue — GameEngine effectivePitcher

- Slider and curveball pitch count tracked via `currentPitcherArsenal_` per pitcher per game
- Every 5 breaking balls thrown: `−1 pitchingControl` (max penalty: −8 at 40 breaking balls)
- Effect: late-inning breaking ball artists lose command, `armDrag` increases naturally via `applySpinParams`, SSW effect strengthens for high-gyro pitchers
- No new state required — reads existing arsenal counts

### Batter-Specific Approach in Pitcher's Count — PitchEngine

- `cvbAdj` (contact vs. breaking) weight raised `0.35 → 0.50` in 0-2/1-2 counts
- Pitchers exploit breaking ball weakness more aggressively when ahead in count; avoid breaking balls more if batter handles them well
- Contributes to lower K% from improved contact (elite cvb batters see fewer breaking balls in key situations)

---

## v2.3 - Batter Psychology, Pitch AI, Predictive Fielding Routes (2026-06-24)

**Confirmed metrics (100-season average):**

| Metric | v2.2 → v2.3 | MLB ref |
|--------|-------------|---------|
| K% | 22.8% → **22.1%** | 22-23% ✓ |
| BB% | 7.5% → **7.3%** | ~8.5% |
| BABIP | 0.300 → **0.297–0.300** | ~0.300 ✓ |
| RS/G top | 4.74–4.85 → **4.60–4.75** | 4.5–5.5 ✓ |
| RS/G bottom | 3.47–3.57 → **3.36–3.51** | ~3.5 |
| ERA range | 3.60–4.32 → **3.44–4.24** | 3.5–4.5 ✓ |

### Batter Psychology — SwingDecisionEngine

- **2-strike plate protection scales with contact stat**: high-contact batters expand their zone more when protecting the plate (up to `+0.11` out-of-zone swing probability), low-contact batters get minimal benefit (`+0.02`)
- Previously: flat `+0.02` for all batters on out-of-zone 2-strike pitches
- Effect: elite contact hitters (contact ≥70) distinguish themselves in two-strike counts

### Pitch AI — PitchEngine

- **3-0 count separate branch**: strong zone/fastball bias (`inZone +0.9`, `fastball+inZone +0.5`, `chase -2.0`) vs. generic `balls≥3` which applies to 3-1/3-2 as well
- **0-0 first pitch bonus**: fastball+zone gets `+0.25` on first pitch — establishes early strike count
- **armDrag 3D spin axis**: `armDrag = clamp((55 − control) / 60, 0, 0.42)` — low-control pitchers have more gyro spin (spinAxisY) from arm-slot deviation, activating SSW batted ball effect per pitch type:
  - Fastball: `spinAxisY = armDrag × 0.18`
  - Slider: `spinAxisY = 0.15 + armDrag × 0.22`
  - Changeup: `spinAxisY = 0.22 + armDrag × 0.28`
  - Splitter: `spinAxisY = 0.28 + armDrag × 0.18`
- Spin axis renormalized to unit vector after adding gyro component

### Predictive Fielding Routes — PlayResolutionEngine + GameEngine

- **routeEfficiency coefficient widened**: `0.035 → 0.06` — spread between best/worst fielders increases from ~5% to ~9%
  - fielding=80: routeEfficiency ~0.936 vs previous ~0.921
  - fielding=20: routeEfficiency ~0.864 vs previous ~0.879
- **flyReadDelay for outfielders**: poor outfielders pay extra time from misreading initial ball direction
  - `flyReadDelay = clamp(0.18 − fielding_stat × 0.20, 0.0, 0.10)`
  - fielding ≥ 0.90: no delay (reads ball immediately)
  - fielding = 0.75: ~0.03s delay
  - fielding = 0.62: ~0.056s delay
  - Applied only to outfielders in `evaluateFielding`

---

## v2.2 - Multithreading, CoD Penalty, SSW, RISP AI, Team Rebalance (2026-06-24)

**Confirmed metrics (100-season average):**

| Metric | v2.1 → v2.2 | MLB ref |
|--------|-------------|---------|
| K% | 23.7% → **22.8%** | 22-23% ✓ |
| BB% | 7.3% → **7.5%** | ~8.5% (close) |
| BABIP | 0.300 → **0.300** | ~0.300 ✅ |
| RS/G top (A) | 4.72 → **4.74** | 4.5–5.5 ✓ |
| RS/G top (B) | 4.85 → **4.85** | — |
| RS/G bottom | ~3.17 → **3.47–3.57** | ~3.5 ✓ |
| ERA range | 3.41–4.24 → **3.60–4.32** | 3.5–4.5 ✓ |
| SB% | 67% → **79%** | ~79% ✅ |

### Multithreaded Parallel Season Simulation — season_runner

- `std::thread::hardware_concurrency()` threads, each running independent season batches
- Thread-local result containers merged after `join()` — no mutex on hot path
- Seed isolation: thread `t` uses base `42 + t × 100000`, incrementing per game
- `Makefile`: `-O3 -march=native -pthread` for season target

### Fast Mode — BallPhysicsEngine / GameEngine

- `BallPhysicsEngine::simulate(input, ballpark, fastMode)` — skips trajectory recording in fast mode
- `GameEngine::setFastMode(bool)` — propagates to physics engine for season simulation

### Change-of-Direction Penalty Improvements — PlayResolutionEngine

- **Outfielder diagonal routes**: `lateralBonus = |dxNorm| × 0.15` added to over-the-head penalty (0.44 max)
- **Infielder arm-side range**: SS/3B ranging to catcher's right (or 2B/1B to left) gets `+0.04–0.10s` penalty when `armDot > 0.50`

### SSW Batted Ball — ContactEngine

- `gyroFraction = (1 − activeSpin) × |spinAxisY|` computed at contact
- `sideSpin += gyroFraction × 4.5 × rand(−1, 1)` — seam-shift wake perturbs lateral batted-ball spin
- Cutter/slider affected most; 4-seam almost none

### RISP Situational AI — GameEngine formedBatter

- When runner on 2B or 3B: `eye +1`, `contact +1` before each at-bat
- Minor plate discipline boost with runners in scoring position

### SB% Calibration — GameEngine checkStolenBase

- 1B→2B attempt rate `0.044 → 0.066`, success `0.73 → 0.79`
- 2B→3B attempt rate `0.022 → 0.033`, success `0.67 → 0.73`
- Double steal lead runner success `0.67 → 0.73`, trail `0.75 → 0.81`

### groundBallOutProb Calibration — PlayResolutionEngine

- Zone base rates raised: `0.82/0.76/0.68/0.58/0.44 → 0.85/0.79/0.71/0.60/0.45`

### Per-Game Weather Randomization — season_runner

- Each game: temperature ±10°F around park baseline (clamped 45–95°F)
- Wind speed: 0–8 mph; direction: 0–360° random; season-seeded for reproducibility

### Team Rebalance — Teams.cpp

- **South Philly Stallions**: all 8 batters `contact/power/speed +4–6`
- **Fairmount Rams**: all 8 batters `contact/power/speed +2–4`
- **Germantown Colonials**: all 8 batters `contact/power/speed +2–3`

---

## v2.1 - Full Calibration: K%, BABIP, RS/G, SBA/G (2026-06-23)

**Confirmed metrics (100-season average):**

| Metric | v2.0 → v2.1 | MLB ref |
|--------|-------------|---------|
| K% | 24.2% → 22.9% | 22-23% ✓ |
| BB% | ~9% | ~9% ✓ |
| BABIP | 0.316 → 0.300 | ~0.300 ✅ |
| SBA/G | 0.66 → ~1.0 | ~1.0 ✅ |
| Staten Island RS/G | 2.98 → 3.49 | — |
| Fairmount RS/G | 2.85 → 3.22 | — |
| South Philly RS/G | 2.83 → 3.12 | — |

### K% Calibration (multi-round) — ContactEngine

- contactChance base raised from `0.84 → 0.87` over 3 rounds
- `stuff` coefficient `0.11 → 0.07`, `pitchQuality` `0.11 → 0.07` (triple-lever approach)
- `timingQuality` weight `0.17 → 0.14`
- Result: K% converged at 22.9% (MLB 2024 actual: 22.7%)

### BABIP Calibration — PlayResolutionEngine groundBallOutProb

- Zone base rates tuned over 2 rounds: `0.80/0.74/0.66/0.56/0.42 → 0.83/0.77/0.69/0.58/0.44`
- Inner 3 zones raised to route more grounders to fielders and reduce bloop rate

### RS/G Balancing — Teams

- **Staten Island Foxes**: all 8 lineup batters `contact/power/eye +4–6`; ace Ben Mack `vel/ctrl/stuff +2`
- **Fairmount Rams**: all 8 lineup batters `+6` across the board
- **South Philly Stallions**: all 8 lineup batters `+4–6` across the board

### SBA/G Calibration — GameEngine checkStolenBase

- Attempt multipliers ×1.5: double steal `0.026→0.039`, 1B→2B `0.044→0.066`, 2B→3B `0.022→0.033`

---

## v2.0 - Ground Ball Rolling Fix + horizontalBatError Redesign (2026-06-22)

**Entering state (from v1.9):** K%=26%, BB%=8.7%, BABIP=0.473, RS/G=3.91–5.22  
**Exiting state:** K%=24.2%, BABIP=0.316, RS/G recalibrated  

### candidateFix — Ground Ball Rolling (PlayResolutionEngine)

- **Root cause:** `groundCandidateIndices` used `landingPoint` distance. Very-negative-LA balls (LA −4° to 0°) land at 10–30 ft → assigned to Catcher/Pitcher only → both excluded by depth filter → auto-hit. ~20–25% of BIP.
- **Fix:** `d = max(landDist, min(restDist, 174.0))` — uses `finalRestPoint` so slow rollers are correctly assigned to SS/2B/3B/1B.
- **Recalibration required:** candidateFix converted ~25% of auto-hits to proper fielding; `groundBallOutProb` zones recalibrated to restore target BABIP.

### horizontalBatError Redesign — ContactEngine

- **Problem:** `timingError × 2.2 → horizontalBatError → barrelAccuracy` coupling forced K% floor at ~25–26%. Reducing timingError to lower K% also improved barrelAccuracy → RS/G rose.
- **Fix:** `horizontalBatError = timingError × 0.6 + batTracking(contact)` where `batTracking = rand(−0.24, 0.24) × (1 − contact × 0.42)`.
- **Effect:** K%/contactQuality decoupled. K% dropped from 26% to 24.2% without RS/G spike.

### 3D Pitch Spin Axis — PitchEngine

- `Pitch` struct gains `spinAxisX/Y/Z` (unit vector), `spinRate`, `activeSpin` per pitch
- Per pitch type: 4-seam FB axis `(−0.97, 0, ±0.26)`, curveball `(+0.97, 0, ∓0.26)`, slider `(0.25, 0, ±0.97)`, cutter `(0.60, 0, ±0.80)`, etc.
- `activeSpin`: 4-seam 0.93, cutter 0.50, slider 0.35, splitter 0.25
- `armDrag = (55 − pitchingControl) / 60` — low-control pitchers have more natural tail/sink
- spinAxisY (gyro component) now varies by pitch type and control, enabling SSW batted ball

---

## v1.9 - ERA Spread + BABIP Reduction (2026-06-22)

**Confirmed metrics (100-season average):**

| Metric | Value | MLB ref |
|--------|-------|---------|
| K% | 26.0% | 22-23% |
| BB% | 8.7% | 9% |
| BABIP | 0.473 | 0.300* |
| RS/G | 3.91–5.22 | 4.0–5.0 ✓ |
| ERA (individual) | 2.81–6.51 | — |
| Team PCT range | .305–.637 | — |

*BABIP 0.300 is real MLB; our 0.473 floor is structural (see PHYSICS_ROADMAP.md).

### ERA Spread — PitchEngine + Teams

- `PitchEngine`: pitchQuality & commandSpread coefficients raised `0.18 → 0.26`
- `ContactEngine`: stuff/pitchQuality influence on contactChance raised `0.09 → 0.14`
- `Teams.cpp`: pitcher stat range widened
  - Cole Maddox (ace): velocity/control/stuff `84/72/82 → 88/76/86`
  - Ben Mack (weak starter): control/stuff `64/67 → 52/60`
  - Victor Hale: control `56 → 46` (wild power pitcher)
  - All closers and bullpens rescaled to match team tier

### BABIP Reduction — Bimodal BIP Model

- `ContactEngine`: good contact flyRate `0–38% → 0–65%`, LA `30–44° → 33–50°`
- Bloop model in `PlayResolutionEngine`: extended coverage `193ft → 218ft`
- `ZoneJudge`: umpire xLimit `0.79 → 0.73` (narrower zone → BB%↑)
- `protectBonus`: `0.20 → 0.13` (2-strike protection reduced)

### Structural Limits Identified

**K% floor ~25–26%** and **BABIP floor ~0.470** share the same root:
`timingError → horizontalBatError → barrelAccuracy → contactQuality`
Any change that reduces K% (tighter timingError, lower locationError) simultaneously improves contactQuality → RS/G rises. True decoupling requires redesigning `horizontalBatError` to not scale with `timingError`. Next fix path: ground ball rolling model (see PHYSICS_ROADMAP.md § Ground Ball Rolling).

---

## v1.8 - Bimodal BIP + Player Spread (2026-06-21)

- Bimodal BIP shaping (ContactEngine): 3-tier system by contactQuality
  - poor (cq < 0.45): grounder-dominant
  - medium (0.45–0.62): mixed with moderate GB rate
  - good (cq > 0.62): elevated fly or hard line drive
- Player spread: `contact*0.30`, `power*0.13`, `timingError contact*0.050`
- RS/G recalibrated to MLB range via contactChance base adjustment

---

## v1.2 - SFML Animation Foundation

- Added `include/AnimationTypes.h`.
- Added primitive, STL-only animation data structures:
  - `AnimationPoint`
  - `PitchAnimation`
  - `BattedBallAnimation`
  - `RunnerAnimation`
  - `AnimationPlan`
- Established the animation boundary:
  - `GameEngine`
  - `PlayResult / Snapshot`
  - `AnimationPlan`
  - `SFML Renderer / React Replay`
- No `GameEngine`, SFML drawing, JSON export, or React replay integration yet.

## v1.1 - Foundation & Analysis

- Added `README.md` as the v1.0 fixed balance project guide.
- Added `include/ExportTypes.h` with snapshot types for future JSON, API, and React integration.
- Added `app/analysis_runner.cpp` for focused balance verification.
- Added analysis Makefile targets:
  - `make -C app analysis`
  - `make -C app analysis-run`
- Added analysis modes:
  - `100-games`
  - `100-seasons`
  - `1000-seasons`
- Analysis output includes league averages, team records, run environment, hitting rates, and runner metrics.

## v1.0 - Fixed Balance

- Finalized core simulation balance for league and season runners.
- Confirmed team identity differences, including Bronx speed-based runner value.
- Finalized runner metrics:
  - `XBT/G`: Extra base taken per game.
  - `TOH/G`: Times out at home per game.
  - `SF%`: Sacrifice fly success rate.
  - `H-OFA/G`: Home outfield assists per game.
- Hid `OOB/G` from public runner output while keeping internal tracking for future baserunning expansion.
- Confirmed build targets:
  - `JojiLeagueRunner`
  - `JojiSeasonRunner`

## v1.0 Runner Metric Definitions

- `XBT/G`: Extra base taken per game. Measures aggressive advancement on hits.
- `TOH/G`: Times out at home per game. Measures runners thrown out trying to score.
- `SF%`: Sacrifice fly success rate.
- `H-OFA/G`: Home outfield assists per game. Measures outfield throws that result in outs at home.
- `OOB/G`: Outs on bases per game. Tracked internally but hidden in v1.0 because current runner outs are mostly home-plate outs.
