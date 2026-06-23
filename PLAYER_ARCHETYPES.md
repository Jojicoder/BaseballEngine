# Player Archetypes — Role-Based Parameter Reference

This file records parameter ranges for each `PlayerRole`. Use when creating new players or
evaluating trade targets. Ranges are guidelines — variation within ±5 is fine.

All stats are on the **20–80 scouting scale**. 50 = MLB average.

---

## Position Players

### Leadoff (`PlayerRole::Leadoff`)
High contact + eye + speed. The table-setter.

| Stat     | Target | Range  |
|----------|--------|--------|
| contact  | 76     | 70–82  |
| power    | 60     | 50–68  |
| eye      | 72     | 66–78  |
| speed    | 80     | 74–86  |
| fielding | 70     | 64–78  |
| arm      | 60     | 54–68  |

**Batting tendencies:** pull 44–52, highBall 50–58, chase 34–40, cvb 60–68  
**Typical BA:** .285–.310 | **OBP:** .355–.390 | **SB/162:** 20–40

---

### ContactHitter (`PlayerRole::ContactHitter`)
High contact, moderate power. Gap hitter, batting 2nd or 5th.

| Stat     | Target | Range  |
|----------|--------|--------|
| contact  | 74     | 68–82  |
| power    | 68     | 60–76  |
| eye      | 64     | 58–72  |
| speed    | 64     | 56–72  |
| fielding | 66     | 60–74  |
| arm      | 64     | 58–72  |

**Batting tendencies:** pull 56–64, highBall 46–54, chase 50–58, cvb 44–54  
**Typical BA:** .280–.300 | **OBP:** .340–.370

---

### PowerHitter (`PlayerRole::PowerHitter`)
Power > contact. Cleanup bat, accepts K's.

| Stat     | Target | Range  |
|----------|--------|--------|
| contact  | 66     | 58–74  |
| power    | 84     | 78–90  |
| eye      | 58     | 50–66  |
| speed    | 48     | 40–58  |
| fielding | 60     | 54–68  |
| arm      | 62     | 56–70  |

**Batting tendencies:** pull 68–76, highBall 42–50, chase 62–72, cvb 34–44  
**Typical SLG:** .490–.560 | **HR/162:** 28–42

---

### CornerIF (`PlayerRole::CornerIF`)
1B or 3B. Power + fielding. Power skews to 1B; arm skews to 3B.

| Stat     | 1B target | 3B target | Range  |
|----------|-----------|-----------|--------|
| contact  | 70        | 74        | 64–80  |
| power    | 76        | 70        | 64–84  |
| eye      | 62        | 66        | 56–74  |
| speed    | 48        | 62        | 42–70  |
| fielding | 62        | 70        | 56–76  |
| arm      | 60        | 66        | 56–74  |

**Batting tendencies:** pull 60–70, chase 50–64, cvb 40–54

---

### MiddleIF (`PlayerRole::MiddleIF`)
2B or SS. Defense first; speed + arm critical. SS arm > 2B.

| Stat     | 2B target | SS target | Range  |
|----------|-----------|-----------|--------|
| contact  | 66        | 68        | 60–76  |
| power    | 58        | 56        | 50–66  |
| eye      | 64        | 64        | 58–72  |
| speed    | 72        | 74        | 66–82  |
| fielding | 76        | 80        | 70–86  |
| arm      | 66        | 76        | 62–82  |

**Batting tendencies:** pull 48–56, chase 44–52, cvb 52–62

---

### CenterFielder (`PlayerRole::CenterFielder`)
Speed + fielding. Often doubles as Leadoff.

| Stat     | Target | Range  |
|----------|--------|--------|
| contact  | 74     | 68–80  |
| power    | 60     | 52–68  |
| eye      | 70     | 62–76  |
| speed    | 84     | 78–90  |
| fielding | 78     | 72–86  |
| arm      | 62     | 56–70  |

---

### CornerOF (`PlayerRole::CornerOF`)
LF or RF. Power bat; arm critical for RF.

| Stat     | LF target | RF target | Range  |
|----------|-----------|-----------|--------|
| contact  | 68        | 68        | 60–76  |
| power    | 76        | 74        | 68–84  |
| eye      | 58        | 56        | 50–66  |
| speed    | 58        | 58        | 50–68  |
| fielding | 64        | 66        | 58–74  |
| arm      | 64        | 72        | 58–78  |

---

### Catcher (`PlayerRole::Catcher`)
Defense-first. arm 76–84. Contact modest.

| Stat     | Target | Range  |
|----------|--------|--------|
| contact  | 64     | 56–72  |
| power    | 56     | 48–66  |
| eye      | 64     | 56–72  |
| speed    | 52     | 44–60  |
| fielding | 72     | 66–80  |
| arm      | 80     | 74–88  |

**Batting tendencies:** pull 42–50, highBall 54–62, chase 36–44, cvb 58–68

---

### UtilityIF (`PlayerRole::UtilityIF`)
Can play 2B/SS/3B. Moderate across the board.

| Stat     | Target | Range  |
|----------|--------|--------|
| contact  | 62     | 56–70  |
| power    | 54     | 46–64  |
| eye      | 62     | 56–68  |
| speed    | 68     | 60–76  |
| fielding | 76     | 70–82  |
| arm      | 72     | 66–78  |

---

### ExtraOF / 4th OF (`PlayerRole::ExtraOF`)
Speed & defense. Can spell all 3 OF spots.

| Stat     | Target | Range  |
|----------|--------|--------|
| contact  | 60     | 54–68  |
| power    | 52     | 44–62  |
| eye      | 60     | 54–68  |
| speed    | 76     | 70–84  |
| fielding | 70     | 64–78  |
| arm      | 60     | 54–68  |

---

### PinchHitter (`PlayerRole::PinchHitter`)
Contact/eye max, speed low, fielding irrelevant.

| Stat     | Target | Range  |
|----------|--------|--------|
| contact  | 70     | 64–78  |
| power    | 64     | 56–74  |
| eye      | 66     | 60–74  |
| speed    | 46     | 38–56  |
| fielding | 52     | 44–60  |
| arm      | 55     | 48–64  |

---

### BackupCatcher (`PlayerRole::BackupCatcher`)
Similar to Catcher but lower contact.

| Stat     | Target | Range  |
|----------|--------|--------|
| contact  | 56     | 48–64  |
| power    | 50     | 42–60  |
| eye      | 58     | 50–66  |
| speed    | 46     | 38–54  |
| fielding | 68     | 62–76  |
| arm      | 76     | 70–84  |

---

## Pitchers

All pitcher stats on 40–80 scale. See also `PitchGrade` per pitch type.

### Ace (`PlayerRole::Ace`)
Rotation anchor. At least 2 plus pitches (grade ≥ 70).

| Stat              | Target | Range  |
|-------------------|--------|--------|
| pitchingVelocity  | 86     | 82–92  |
| pitchingControl   | 70     | 66–78  |
| pitchingStuff     | 84     | 80–90  |
| pitchingStamina   | 78     | 72–84  |

**Pitch grades:** Primary 78–84, Secondary 70–78, 3rd 64–70  
**Typical ERA:** 2.80–3.40 | **K/9:** 8.5–10.5

---

### Starter #2–#3 (`PlayerRole::Starter`)
Solid innings-eater. One plus pitch.

| Stat              | Target | Range  |
|-------------------|--------|--------|
| pitchingVelocity  | 78     | 74–84  |
| pitchingControl   | 70     | 66–76  |
| pitchingStuff     | 76     | 72–82  |
| pitchingStamina   | 72     | 66–78  |

**Pitch grades:** Primary 68–76, Secondary 66–74  
**Typical ERA:** 3.40–4.10

---

### BackOfRotation (`PlayerRole::BackOfRotation`)
#4–#5 starter. Below-average stuff, survival on control.

| Stat              | Target | Range  |
|-------------------|--------|--------|
| pitchingVelocity  | 70     | 66–76  |
| pitchingControl   | 64     | 58–70  |
| pitchingStuff     | 66     | 60–72  |
| pitchingStamina   | 62     | 56–70  |

**Pitch grades:** Primary 60–66, Secondary 58–64  
**Typical ERA:** 4.20–5.00

---

### Closer (`PlayerRole::Closer`)
Max velo + one elite wipeout pitch. Low stamina OK.

| Stat              | Target | Range  |
|-------------------|--------|--------|
| pitchingVelocity  | 86     | 82–92  |
| pitchingControl   | 64     | 60–70  |
| pitchingStuff     | 84     | 80–90  |
| pitchingStamina   | 44     | 38–52  |

**Pitch grades:** Primary 76–84, Secondary 72–80  
**Typical ERA:** 2.50–3.20 | **K/9:** 10–13

---

### Setup (`PlayerRole::Setup`)
High leverage. Slightly below Closer quality.

| Stat              | Target | Range  |
|-------------------|--------|--------|
| pitchingVelocity  | 80     | 76–86  |
| pitchingControl   | 66     | 62–72  |
| pitchingStuff     | 78     | 72–84  |
| pitchingStamina   | 52     | 44–60  |

**Typical ERA:** 3.00–3.80

---

### MiddleRelief (`PlayerRole::MiddleRelief`)
Middle innings bridge. Variety of profiles.

| Stat              | Target | Range  |
|-------------------|--------|--------|
| pitchingVelocity  | 74     | 68–80  |
| pitchingControl   | 64     | 58–72  |
| pitchingStuff     | 70     | 64–76  |
| pitchingStamina   | 60     | 52–68  |

**Typical ERA:** 3.60–4.40

---

### LongRelief (`PlayerRole::LongRelief`)
Starter-quality stamina, below starter stuff. Spot starts.

| Stat              | Target | Range  |
|-------------------|--------|--------|
| pitchingVelocity  | 70     | 64–76  |
| pitchingControl   | 68     | 62–74  |
| pitchingStuff     | 64     | 58–70  |
| pitchingStamina   | 74     | 66–82  |

**Typical ERA:** 4.00–4.80

---

### Specialist (`PlayerRole::Specialist`)
LOOGY/ROOGY. Used for 1–2 batters. Extreme platoon split.

| Stat              | Target | Range  |
|-------------------|--------|--------|
| pitchingVelocity  | 72     | 66–78  |
| pitchingControl   | 68     | 62–74  |
| pitchingStuff     | 66     | 60–72  |
| pitchingStamina   | 52     | 44–60  |

**Note:** Typically `TH::Left`. Primary pitch should be breaking ball vs same-handed batters.

---

## Trade Valuation Guide

When evaluating a player trade, score each attribute relative to the role's target:

- `stat ≥ target + 8` → **plus tool** (high value)
- `target - 4 ≤ stat < target + 8` → **average** (standard value)
- `stat < target - 4` → **below target** (discount)

A player with 2+ plus tools in their primary role is a potential franchise piece.  
A player with 0 plus tools but no below-target stats is a rotation/roster filler.

**Position scarcity (highest to lowest):** Catcher > SS > CF > SP > Closer > 2B/3B > Corner

---

*Last updated: 2026-06-22. Re-calibrate if RS/G or ERA targets shift significantly.*
