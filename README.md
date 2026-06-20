# Joji Baseball Engine

## Version

v1.0 Fixed Balance

Joji Baseball Engine is a C++ baseball simulation engine with pitch-by-pitch
logic, batted-ball physics, fielding resolution, league simulation, season
simulation, and SFML visualization.

v1.0 fixes the core balance target for team identity, run environment, runner
metrics, and season-runner output.

## Architecture

The engine is organized around a stable simulation core and a future export
boundary:

```text
GameEngine internal types
↓
ExportTypes / Snapshot
↓
JsonExporter
↓
Java API
↓
React / JojiStats
```

The v1.1 foundation starts this boundary with `include/ExportTypes.h`.
Snapshot types are intended to keep future JSON, API, and frontend code from
depending directly on mutable `GameEngine` internals.

The v1.2 animation foundation adds a display/replay boundary:

```text
GameEngine
↓
PlayResult / Snapshot
↓
AnimationPlan
↓
SFML Renderer / React Replay
```

`include/AnimationTypes.h` defines primitive, STL-only animation data structures
that can be shared by SFML, JSON export, and future React replay work.

## Current Features

Core engine:

- `PitchEngine`
- `SwingDecisionEngine`
- `ZoneJudge`
- `SwingEngine`
- `ContactEngine`
- `AtBatEngine`
- `BallPhysicsEngine`
- `PlayResolutionEngine`
- `GameEngine`

Visualization:

- SFML renderer
- Pitch-by-pitch mode
- Strike zone display
- Pitch location display
- Ball trajectory display
- Line score
- Box score
- Player stats

Simulation:

- League runner
- Season runner
- Team identities
- Extra innings
- Automatic runner

Metrics:

- `AVG`
- `OBP`
- `SLG`
- `OPS`
- `K%`
- `BB%`
- `HR%`
- `BABIP`
- `R/G`
- `XBT/G`
- `TOH/G`
- `SF%`
- `H-OFA/G`

## Teams

Official team names:

- Bronx Wolves
- Harlem Eagles
- Brooklyn Hammers
- Queens Titans
- Newark Knights
- Staten Island Foxes

Team identity targets:

- Bronx Wolves: speed-based runner value
- Harlem Eagles: contact and speed
- Brooklyn Hammers: power
- Queens Titans: power and pitching
- Newark Knights: balanced
- Staten Island Foxes: rebuilding team

## Runner Metrics

- `XBT/G`: Extra base taken per game. Measures aggressive advancement on hits.
- `TOH/G`: Times out at home per game. Measures runners thrown out trying to score.
- `SF%`: Sacrifice fly success rate.
- `H-OFA/G`: Home outfield assists per game. Measures outfield throws that result in outs at home.
- `OOB/G`: Outs on bases per game. Tracked internally but hidden in v1.0 because current runner outs are mostly home-plate outs.

## Build And Run

Build and run tests:

```bash
make -C app test
```

Build the main app target:

```bash
make -C app all
```

Run the league simulation:

```bash
build/JojiLeagueRunner
```

Run the season simulation:

```bash
build/JojiSeasonRunner
```

Build the analysis runner:

```bash
make -C app analysis
```

Run the default analysis mode:

```bash
make -C app analysis-run
```

Run a specific analysis mode:

```bash
build/JojiAnalysisRunner 100-games
build/JojiAnalysisRunner 100-seasons
build/JojiAnalysisRunner 1000-seasons
```

## Roadmap

### v1.1 Foundation & Analysis

- `README.md`
- `CHANGELOG.md`
- `ExportTypes.h`
- `app/analysis_runner.cpp`
- Makefile analysis target

Analysis runner outputs:

- League `AVG`, `OBP`, `SLG`, `OPS`
- League `K%`, `BB%`, `HR%`, `R/G`
- Team `W`, `L`, `RF/G`, `RA/G`
- Team `AVG`, `OBP`, `SLG`, `OPS`
- Team `XBT/G`, `TOH/G`, `SF%`, `H-OFA/G`

### v1.2 SFML Animation Foundation

- `AnimationTypes.h`
- `PitchAnimation`
- `BattedBallAnimation`
- `RunnerAnimation`
- `AnimationPlan`
- Pitch animation
- Batted-ball trajectory replay
- Runner animation
- Animation snapshot types

### v1.3 Defensive Animation

- Fielder routes
- Catch and miss animation
- Throw animation
- Relay throw foundation

### v1.4 Defensive Baserunning Expansion

- Runner outs at second and third
- Relay plays
- `OOB/G` re-enabled
- `TOH/G` separated from broader outs-on-bases output

### v1.5 Team Identity Polish

- Team strengths visible in standings
- Team strengths visible in statistics
- Team strengths visible in simulation output

### v1.6 JSON Export

- `GameSnapshot`
- `GameResultSnapshot`
- `TeamSnapshot`
- `PlayerSnapshot`
- Single-game, season, and league export formats

### v1.7 Java API

- Spring Boot API
- Game, team, player, standings, and season endpoints

### v1.8 React / JojiStats Integration

- Live game viewer
- Pitch-by-pitch replay
- Ball trajectory replay
- Box score
- Line score
- Team pages
- Player pages
- Season simulator
