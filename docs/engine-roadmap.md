# Engine Roadmap — Offseason & Player Progression

Notes from a planning discussion, kept here so the next round of engine work
has a concrete starting point instead of re-deriving it.

## Current state (as of 2026-07)

- `docs/jbl-team-ai-design.md` documents a full draft/free-agency/trade AI
  per team philosophy (Win Now, Rebuild, Player Development, etc.) — but this
  is **design spec only**. None of it is implemented in the engine.
- Rosters are fixed: each team's lineup/rotation/bullpen is hand-authored in
  `src/Teams.cpp` (`batter()`/`pitcher()` factory calls). There is no
  mechanism to add, remove, or replace a player.
- `Player.age` is stored and exported, but nothing reads it to modify
  `contact`/`power`/`speed`/`pitchingVelocity`/etc. No aging curve exists —
  a 22-year-old and a 38-year-old with the same ratings play identically
  forever.
- `LeagueConfig.h` has a draft **date** on the season calendar, but no draft
  **mechanic** behind it.
- Net effect: a season replayed next year would be the same 18 rosters with
  no evolution. The team-personality work (Team Identity page, the AI design
  doc) currently only affects flavor text, not any actual simulation outcome.

## Why this is the highest-value next engine feature

It's the one gap that would make the already-written front-office AI design
actually *do* something, and it's the prerequisite for any kind of
multi-season narrative (a team's Rebuild philosophy only means something if
rosters can actually get younger over time).

## Proposed phasing

Don't build this as one big feature — each phase should be independently
testable and shippable:

1. **Player pool generation** — a generator for draft-eligible prospects
   (name, age, position, ratings distribution), separate from the AI that
   picks them.
2. **Draft** — 18 teams pick in turn from the pool, each team's pick logic
   driven by its documented philosophy (e.g. Win Now favors MLB-ready
   polish; Rebuild favors ceiling). Testable in isolation once the pool
   generator exists.
3. **Free agency** — a subset of rostered players become free agents each
   offseason; teams bid/sign per their FA tendency from the design doc.
4. **Trades** — in-season and offseason trades between teams, gated by each
   team's "current state" (contending vs. rebuilding) per the design doc's
   trade-AI section.
5. **Season-to-season carryover + aging** — roll a season's roster into next
   season's starting state; apply an age-based adjustment to ratings (young
   players trend up toward a peak, veterans trend down past it) instead of
   ratings being static for a player's whole career. This phase is what
   makes phases 1–4 matter over multiple seasons rather than just one
   offseason event.

Phase 5 (aging/growth) was raised specifically because it's the piece that
turns "a team drafted well" into "and now, three years later, that pick is
a star" — without it, the draft/FA/trade system has no visible long-term
payoff.

## Suggested entry point

Start with phase 1 (player pool generation) — it has no dependencies on the
others and is easy to verify in isolation (generate N prospects, eyeball the
ratings distribution, confirm it's sane before anything consumes it).
