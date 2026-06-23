#include "ContactEngine.h"

#include <algorithm>
#include <cmath>

namespace joji {
namespace {

double clamp(double value, double min, double max) {
    return std::max(min, std::min(value, max));
}

} // namespace

ContactResult ContactEngine::resolve(const Player& batter,
                                     const Player& pitcher,
                                     const Pitch& pitch,
                                     const Swing& swing,
                                     const Count& count,
                                     Random& random) const {
    ContactResult result;
    const double contact = clamp((batter.contact - 50) / 50.0, -0.8, 0.8);
    const double power = clamp((batter.power - 50) / 50.0, -0.8, 0.8);
    const double stuff = clamp((pitcher.pitchingStuff - 50) / 50.0, -0.8, 0.8);
    const double locationError = std::abs(pitch.locationX) * 0.14 + std::abs(pitch.locationZ - 2.5) * 0.09;

    // Same-handed: breaking ball escapes → more jammed/barrel penalty
    // Opposite:    platoon advantage → slight barrel bonus
    const bool sameHanded = (pitcher.throwingHand == ThrowingHand::Right && batter.battingSide == BattingSide::Right)
                         || (pitcher.throwingHand == ThrowingHand::Left  && batter.battingSide == BattingSide::Left);
    const double barrelHandedness = sameHanded ? -0.06 : 0.05;

    result.timingQuality = clamp(1.0 - std::abs(swing.timingError) * 3.6, 0.0, 1.0);
    result.verticalBatError = pitch.locationZ - (2.45 + swing.attackAngle * 0.018) + random.real(-0.22, 0.22);
    // Decouple bat-tracking from pitch timing: timingError drives plate-coverage (×0.6),
    // while contact stat drives bat-path precision independently. This breaks the
    // timingError→barrelAccuracy coupling that prevented K% from dropping below 25%.
    const double batTracking = random.real(-0.24, 0.24) * (1.0 - contact * 0.42);
    result.horizontalBatError = pitch.locationX + swing.timingError * 0.6 + batTracking;
    result.barrelAccuracy = clamp(1.0 - std::abs(result.verticalBatError) * 0.55 - std::abs(result.horizontalBatError) * 0.45
                                      + barrelHandedness, 0.0, 1.0);
    result.contactDepth = clamp(1.3 - swing.timingError * 4.0, 0.0, 2.4);
    result.contactHeight = clamp(pitch.locationZ + random.real(-0.12, 0.12), 0.8, 4.2);

    result.isJammed = result.horizontalBatError > 0.62;
    result.isTopped = result.verticalBatError < -0.44;
    result.isUnderBall = result.verticalBatError > 0.52;

    const double protectBonus = count.strikes >= 2 ? 0.13 : 0.0;
    const double contactChance = clamp(0.80 + contact * 0.30 - stuff * 0.11 - pitch.pitchQuality * 0.11
                                           + swing.contactIntent * 0.16 + result.timingQuality * 0.17
                                           - locationError + protectBonus,
                                       0.11,
                                       0.97);
    if (!random.chance(contactChance)) {
        result.resultType = ContactResultType::SwingMiss;
        return result;
    }

    const double inPlayChance = clamp(0.43 + result.barrelAccuracy * 0.25 + result.timingQuality * 0.13
                                          + swing.swingQuality * 0.08 - pitch.pitchQuality * 0.10,
                                      0.16,
                                      0.82);
    if (!random.chance(inPlayChance)) {
        result.resultType = ContactResultType::Foul;
        const double foulFlyChance = clamp(0.035
                                               + (result.isUnderBall ? 0.075 : 0.0)
                                               + (1.0 - result.barrelAccuracy) * 0.045,
                                           0.025,
                                           0.16);
        if (random.chance(foulFlyChance)) {
            BattedBallInput foulFly;
            foulFly.exitVelocity = random.real(48.0, 72.0);
            foulFly.launchAngle = random.real(46.0, 64.0);
            foulFly.sprayAngle = random.chance(0.5)
                ? random.real(-64.0, -49.0)
                : random.real(49.0, 64.0);
            foulFly.backSpin = random.real(1800.0, 3100.0);
            foulFly.sideSpin = foulFly.sprayAngle * 0.04 + random.real(-1.5, 1.5);
            result.battedBallInput = foulFly;
        }
        return result;
    }

    const bool isBreakingPitch = (pitch.pitchType == PitchType::Slider
                               || pitch.pitchType == PitchType::Curveball
                               || pitch.pitchType == PitchType::Splitter);
    const double cvbBonus = isBreakingPitch
        ? clamp((batter.contactVsBreaking - 50) / 50.0, -0.6, 0.6) * 0.08
        : 0.0;
    // highBallHitter: high pitch (locationZ > 2.8) is easier; low pitch is harder.
    const double hbhAdj = clamp((batter.highBallHitter - 50) / 50.0, -0.6, 0.6);
    const double hbhBonus = (pitch.locationZ > 2.8) ?  hbhAdj * 0.06
                          : (pitch.locationZ < 2.0) ? -hbhAdj * 0.05
                          : 0.0;
    const double contactQuality = clamp(result.barrelAccuracy * 0.45 + result.timingQuality * 0.34
                                            + swing.swingQuality * 0.20
                                            + cvbBonus + hbhBonus,
                                        0.0,
                                        1.0);

    BattedBallInput input;

    // Nathan/Adair bat-ball collision model:
    //   K = (1+COR)*q/(1+q)  where q = effective bat mass / ball mass
    //   eV = K*batSpeed + max(0, K-1)*pitchSpeed
    // Both q and COR scale with barrel accuracy and contact quality.
    // Poor contact (low barrelAccuracy) → small q, low COR → K < 1 → weak EV naturally.
    const double q   = clamp(1.6 + result.barrelAccuracy * 2.2, 1.6, 3.8);
    const double cor = clamp(0.16 + contactQuality * 0.36, 0.16, 0.52);
    const double K   = (1.0 + cor) * q / (1.0 + q);
    const double eVphys = K * swing.batSpeed
                        + std::max(0.0, K - 1.0) * pitch.pitchVelocity
                        + power * 5.0
                        - (result.isJammed ? 6.0 : 0.0);
    input.exitVelocity = clamp(eVphys + random.real(-4.5, 4.5), 35.0, 122.0);
    const double vbeEffect = result.isUnderBall ? result.verticalBatError * -6.0
                                                : result.verticalBatError * -14.0;
    input.launchAngle = clamp(swing.attackAngle + vbeEffect
                                  + contactQuality * 3.0
                                  + (result.isTopped ? -20.0 : 0.0)
                                  + (result.isUnderBall ? 30.0 : 0.0)
                                  + random.real(-7.0, 7.0),
                              -30.0,
                              60.0);

    // Barrel check BEFORE BIP shaping — prevents barrel hits from being
    // mis-converted to ground balls and losing their HR trajectory.
    const double barrelChance = clamp((contactQuality - 0.60) * 1.50 + power * 0.22, 0.0, 0.42);
    const bool isBarrel = result.barrelAccuracy > 0.44 && contactQuality > 0.61
                          && input.exitVelocity >= 76.0 && random.chance(barrelChance);
    if (isBarrel) {
        input.exitVelocity = clamp(input.exitVelocity + random.real(6.0, 14.0), 45.0, 120.0);
        input.launchAngle  = clamp(24.0 + power * 5.0 + random.real(0.0, 8.0), 22.0, 37.0);
    }

    // ── Bimodal BIP shaping ──────────────────────────────────────────────────
    // MLB target: ~45% GB, ~20% LD, ~35% FB.
    // Key insight: avoid producing balls in LA 8-28° except at very high EV (true hard LDs).
    // Medium-quality contact that misses the grounder band should go to fly zone (28-48°),
    // not stay in the 8-28° no-man's land where neither IF nor OF can reach in time.
    if (!isBarrel && input.launchAngle >= 5.0 && input.launchAngle < 35.0) {
        if (contactQuality < 0.45) {
            // Poor contact: topped/jammed → grounder dominant
            const double gbRate = clamp(0.52 + (0.45 - contactQuality) * 1.50, 0.52, 0.86);
            if (random.chance(gbRate)) {
                input.launchAngle  = random.real(-4.0, 5.0);
                input.exitVelocity = clamp(input.exitVelocity - random.real(3.0, 9.0), 35.0, 110.0);
            } else if (random.chance(0.55)) {
                // Weak popup / soft fly — goes high, OF has time
                input.launchAngle  = random.real(38.0, 58.0);
                input.exitVelocity = clamp(input.exitVelocity - random.real(5.0, 14.0), 35.0, 78.0);
            }
            // Remaining ~10%: true bloop — cap EV so it stays in isBloop catch range
            else { input.exitVelocity = clamp(input.exitVelocity - random.real(8.0, 18.0), 35.0, 72.0); }
        } else if (contactQuality < 0.62) {
            // Medium contact: grounder OR fly — avoid 8-27° soft liner zone entirely.
            const double gbRate = clamp(0.46 + (1.0 - contactQuality) * 0.30
                                            + (count.strikes >= 2 ? 0.05 : 0.0)
                                            - power * 0.06 - contact * 0.06,
                                        0.22, 0.62);
            if (random.chance(gbRate)) {
                input.launchAngle  = random.real(-4.0, 4.5);
                input.exitVelocity = clamp(input.exitVelocity - random.real(0.0, 4.0), 45.0, 116.0);
            } else {
                // Non-grounder medium contact → fly zone (28-42°).
                // Trim EV to avoid XBH; some variance so occasional doubles still occur.
                input.launchAngle  = random.real(28.0, 42.0);
                input.exitVelocity = clamp(input.exitVelocity - random.real(1.0, 5.0), 45.0, 96.0);
            }
        } else {
            // Good contact: deep fly or hard grounder. Residual "hard LD" pushed to LA 26-38°
            // (above the no-man's land minimum, caught by OF fly model with better coverage).
            const double flyRate = clamp((contactQuality - 0.62) * 1.71, 0.0, 0.65);
            if (random.chance(flyRate)) {
                input.launchAngle  = random.real(33.0, 50.0);
                input.exitVelocity = clamp(input.exitVelocity - random.real(0.0, 5.0), 45.0, 98.0);
            } else {
                const double gbRate2 = clamp(0.30 + (1.0 - contactQuality) * 0.20
                                                 - contact * 0.05, 0.10, 0.40);
                if (random.chance(gbRate2)) {
                    input.launchAngle = random.real(-8.0, 5.0);
                } else {
                    // Hard contact neither fly nor grounder → shallow fly (26-36°, EV trimmed)
                    input.launchAngle  = random.real(26.0, 36.0);
                    input.exitVelocity = clamp(input.exitVelocity - random.real(2.0, 7.0), 45.0, 92.0);
                }
            }
        }
    }

    const double popupChance = clamp((result.isUnderBall ? 0.14 : 0.02)
                                        + (1.0 - contactQuality) * 0.06
                                        - power * 0.03,
                                    0.01, 0.22);
    if (!isBarrel && input.launchAngle >= 18.0 && random.chance(popupChance)) {
        input.launchAngle  = random.real(42.0, 58.0);
        input.exitVelocity = clamp(input.exitVelocity - random.real(4.0, 10.0), 45.0, 108.0);
    }
    // RHB: early timing → pull to LEFT field (negative x). LHB: pull to RIGHT (positive x).
    // timingError is negative when early, so RHB needs positive multiplier and LHB needs negative.
    // swingPlaneAngle: positive = pull-side bat path; converted to spray direction by handedness.
    const double timingSprayMult = (batter.battingSide == BattingSide::Right) ? 130.0 : -130.0;
    const double hbeSprayMult    = (batter.battingSide == BattingSide::Right) ?  28.0 :  -28.0;
    const double planeMult       = (batter.battingSide == BattingSide::Right) ?  -1.0 :   1.0;
    // pullTendency: high = more pull-side spray angle bias
    const double pullBias = (batter.pullTendency - 50) / 50.0 * 7.0 * planeMult;
    input.sprayAngle = clamp(swing.timingError * timingSprayMult
                                 + result.horizontalBatError * hbeSprayMult
                                 + swing.swingPlaneAngle * planeMult * 0.7
                                 + pullBias
                                 + random.real(-20.0, 20.0),
                             -45.0,
                             45.0);
    // Physics-based spin: positive = backspin (fly ball lift), negative = topspin (grounder drop)
    // tanh transition centered at ~4° launch angle; magnitude scales with EV and contact quality
    const double spinMag = clamp(input.exitVelocity * 25.0 + contactQuality * 550.0 + random.real(-320.0, 320.0),
                                 700.0, 3400.0);
    const double spinDir = std::tanh((input.launchAngle - 4.0) / 10.0);  // -1=topspin, +1=backspin
    input.backSpin = spinMag * spinDir;
    input.sideSpin = clamp(input.sprayAngle * 0.06 + random.real(-2.5, 2.5), -7.5, 7.5);

    result.resultType = ContactResultType::InPlay;
    result.battedBallInput = input;
    return result;
}

} // namespace joji
