# Joji Baseball Engine

> **Current Branch:** v1.6 Runner / Throw / Replay Foundation

A C++ baseball simulation engine focused on realistic pitch-by-pitch gameplay, batted-ball physics, fielding resolution, runner advancement, league simulation, season simulation, replay systems, and SFML visualization.

The long-term goal is to create a baseball engine capable of resolving games through physics, player attributes, and decision-making systems rather than predefined outcomes.

---

## Table of Contents

- [Architecture](#architecture)
- [Current Features](#current-features)
- [Metrics](#metrics)
- [Teams](#teams)
- [Build & Run](#build--run)
- [Roadmap](#roadmap)

---

## Architecture

The simulation core is separated from export, replay, and visualization layers.

### Export Boundary

```
GameEngine Internal Types
        ↓
ExportTypes / Snapshot
        ↓
  JsonExporter
        ↓
   Java API
        ↓
React / JojiStats
```

Snapshot types isolate external systems from mutable engine internals.

### Animation Boundary

```
  GameEngine
      ↓
PlayResult / Snapshot
      ↓
 AnimationPlan
      ↓
SFML Renderer
      ↓
 React Replay
```

Animation systems consume immutable replay-friendly data structures rather than direct engine state.

### Replay Boundary

```
  GameEngine
      ↓
ReplayTimeline
      ↓
  ReplayEvent
      ↓
 AnimationPlan
      ↓
   Renderer
```

This allows future replay support in SFML, JSON exports, Java APIs, and React clients.

---

## Current Features

### Core Simulation

| Engine | Engine | Engine |
|---|---|---|
| PitchEngine | SwingDecisionEngine | ZoneJudge |
| SwingEngine | ContactEngine | AtBatEngine |
| BallPhysicsEngine | PlayResolutionEngine | GameEngine |

### Pitching

- Pitch-by-pitch simulation
- Velocity, pitch movement, pitch quality
- Release point support, pitch logs
- Strike zone tracking
- Count-aware decision making

### Hitting

- Swing decisions, chase logic
- Contact quality, exit velocity
- Launch angle, spray angle
- Foul balls, swing and miss, in-play contact

### Batted Ball Physics

- Fly balls, line drives, ground balls, popups, home runs
- Fence interactions
- Bounce trajectories, roll trajectories

### Fielding

- Catch probability, range calculations, error generation
- Double plays, triple plays
- Force plays, fielder's choice, infield fly rule

### Runner Systems

- Runner advancement, extra-base advancement
- Tag-up support, sacrifice flies
- Stolen bases, caught stealing, pickoffs
- Wild pitches, passed balls, balks

### Simulation

- League Runner, Season Runner, Analysis Runner
- Team identities, extra innings, automatic runners
- Team statistics, player statistics

### Visualization

**Field View**
- Diamond view, pitch animation, batted ball animation
- Runner animation, throw animation
- Line score, box score

**Pitch View**
- Large strike zone, pitch trajectory, pitch type display
- Velocity display, zone display, result display
- Pitch count display
- Left/right-handed batter and pitcher support

### Replay Foundation

- AnimationPlan, AnimationPlanBuilder
- ReplayTimeline, ReplayEvent
- RunnerMovement, ThrowMovement, TagPlay

### Fatigue & Form

- Pitcher fatigue, daily form modifiers
- Hot streak / cold streak support

---

## Metrics

**League**

| Metric | Metric | Metric |
|--------|--------|--------|
| AVG | OBP | SLG |
| OPS | BABIP | K% |
| BB% | HR% | R/G |

**Runner**

| Metric | Description |
|--------|-------------|
| XBT/G | Extra bases taken per game |
| TOH/G | Thrown out at home per game |
| SF% | Sacrifice fly success rate |
| H-OFA/G | Hit outfield assists per game |

**Internal**

| Metric | Description |
|--------|-------------|
| OOB/G | Out on base per game |

---

## Teams

| Team | Identity | Style |
|------|----------|-------|
| **Bronx Wolves** | Speed-first | Aggressive running · High XBT/G · Pressure offense |
| **Harlem Eagles** | Contact and speed | High AVG · High OBP · Strong situational hitting |
| **Brooklyn Hammers** | Power offense | High SLG · Home run production |
| **Queens Titans** | Power and pitching | Run prevention · Extra-base power |
| **Newark Knights** | Balanced roster | No extreme strengths · Consistent performance |
| **Staten Island Foxes** | Rebuilding club | Lower overall ratings · Development-focused |

---

## Build & Run

```bash
# Run tests
make -C app test

# Build all
make -C app all

# Run SFML visualizer
make -C app sfml-run

# Run League Simulation
build/JojiLeagueRunner

# Run Season Simulation
build/JojiSeasonRunner

# Build & run Analysis Runner
make -C app analysis
make -C app analysis-run

# Analysis Runner modes
build/JojiAnalysisRunner 100-games
build/JojiAnalysisRunner 100-seasons
build/JojiAnalysisRunner 1000-seasons
```

---

## Roadmap

### v1.6 — Runner / Throw / Replay Foundation *(current)*

| Status | Feature |
|--------|---------|
| ✅ | ReplayTimeline, ReplayEvent |
| ✅ | RunnerMovement, ThrowMovement, TagPlay |
| ✅ | AnimationPlanBuilder integration |
| 🔲 | Runner arrival timing |
| 🔲 | Ball arrival timing |
| 🔲 | Tag timing |
| 🔲 | Replay-driven play reconstruction |

### v1.7 — Defensive Decision Engine

Fielders choose optimal throws based on game context.

- Throw home / first / second / third
- Cutoff decisions, relay decisions, hold-ball decisions

### v1.8 — Runner Physics

Runner movement becomes physics-driven.

- Acceleration, top speed, turn speed
- Route efficiency, sliding

### v1.9 — Throw Physics

Ball travel determines outcomes rather than predefined resolution.

- Throw velocity, throw accuracy, throw arc
- Bad throws, relay chains

### v2.0 — Natural Baseball Simulation

Resolve baseball plays entirely through physics and decision-making systems.

| System | System | System |
|--------|--------|--------|
| Pitch Physics | Swing Physics | Ball-Bat Collision Physics |
| Contact Physics | Batted Ball Physics | Bounce Physics |
| Wall Physics | Fielder Movement Physics | Catch Physics |
| Throw Physics | Runner Physics | Defensive Decision Engine |
| Baserunning Decision Engine | Replay Engine | |

### Post v2.0

**Simulation**
- Manager AI, Injury Engine, Advanced fatigue, Player development

**Environment**
- Weather Engine, Expanded Stadium Engine

**Analytics**
- Advanced Statcast, Heat maps, Pitch usage analysis

**Platform**
- JSON Export, Java API, React Integration
- JojiStats Integration, Live Game Viewer, Replay Viewer
