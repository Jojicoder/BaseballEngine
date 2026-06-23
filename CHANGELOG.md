# Changelog

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
