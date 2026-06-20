# Changelog

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
