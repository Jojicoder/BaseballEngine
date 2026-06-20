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
    result.horizontalBatError = pitch.locationX + swing.timingError * 2.2 + random.real(-0.18, 0.18);
    result.barrelAccuracy = clamp(1.0 - std::abs(result.verticalBatError) * 0.55 - std::abs(result.horizontalBatError) * 0.45
                                      + barrelHandedness, 0.0, 1.0);
    result.contactDepth = clamp(1.3 - swing.timingError * 4.0, 0.0, 2.4);
    result.contactHeight = clamp(pitch.locationZ + random.real(-0.12, 0.12), 0.8, 4.2);

    result.isJammed = result.horizontalBatError > 0.62;
    result.isTopped = result.verticalBatError < -0.44;
    result.isUnderBall = result.verticalBatError > 0.52;

    const double protectBonus = count.strikes >= 2 ? 0.20 : 0.0;
    const double contactChance = clamp(0.77 + contact * 0.22 - stuff * 0.08 - pitch.pitchQuality * 0.08
                                           + swing.contactIntent * 0.16 + result.timingQuality * 0.17
                                           - locationError + protectBonus,
                                       0.18,
                                       0.96);
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

    const double contactQuality = clamp(result.barrelAccuracy * 0.45 + result.timingQuality * 0.34
                                            + swing.swingQuality * 0.20,
                                        0.0,
                                        1.0);

    BattedBallInput input;

    // Poor contact: weak / jammed / topped → EV crashes to 50–78 mph
    const bool poorContact = contactQuality < 0.27
                          || (result.isJammed && contactQuality < 0.42)
                          || (result.isTopped  && contactQuality < 0.32);

    if (poorContact) {
        const double weakBase = 52.0 + contactQuality * 68.0;
        input.exitVelocity = clamp(weakBase + random.real(-5.0, 8.0), 45.0, 78.0);
    } else {
        input.exitVelocity = clamp(38.0 + swing.batSpeed * 0.36 + pitch.pitchVelocity * 0.15
                                       + power * 6.8 + contactQuality * 15.5
                                       - (result.isJammed ? 8.0 : 0.0)
                                       + random.real(-4.5, 4.5),
                                   45.0,
                                   116.0);
    }
    const double vbeEffect = result.isUnderBall ? result.verticalBatError * -6.0
                                                : result.verticalBatError * -14.0;
    input.launchAngle = clamp(swing.attackAngle + vbeEffect
                                  + contactQuality * 4.0
                                  + (result.isTopped ? -20.0 : 0.0)
                                  + (result.isUnderBall ? 30.0 : 0.0)
                                  + random.real(-7.0, 7.0),
                              -30.0,
                              60.0);

    const bool likelyLineDrive = input.launchAngle >= 5.0 && input.launchAngle < 20.0;
    const double groundShapeChance = clamp(0.22 + (1.0 - contactQuality) * 0.25
                                               + (count.strikes >= 2 ? 0.06 : 0.0)
                                               - power * 0.05,
                                           0.10,
                                           0.45);
    if (likelyLineDrive && random.chance(groundShapeChance)) {
        input.launchAngle = random.real(-10.0, 4.5);
        input.exitVelocity = clamp(input.exitVelocity - random.real(0.0, 4.0), 45.0, 116.0);
    } else if (likelyLineDrive && contactQuality > 0.52 && random.chance(0.32)) {
        input.launchAngle = random.real(21.0, 33.0);
        input.exitVelocity = clamp(input.exitVelocity + random.real(0.0, 2.5), 45.0, 118.0);
    } else if (input.launchAngle >= 20.0 && input.launchAngle < 38.0
               && contactQuality < 0.62 && random.chance(0.10)) {
        input.launchAngle = random.real(-4.0, 7.0);
    }

    const double popupChance = clamp((result.isUnderBall ? 0.14 : 0.02)
                                        + (1.0 - contactQuality) * 0.06
                                        - power * 0.03,
                                    0.01,
                                    0.22);
    if (input.launchAngle >= 18.0 && random.chance(popupChance)) {
        input.launchAngle = random.real(42.0, 58.0);
        input.exitVelocity = clamp(input.exitVelocity - random.real(4.0, 10.0), 45.0, 108.0);
    }

    const double barrelChance = clamp((contactQuality - 0.70) * 0.82 + power * 0.10, 0.0, 0.30);
    if (!poorContact && contactQuality > 0.72 && input.exitVelocity >= 94.0 && random.chance(barrelChance)) {
        input.exitVelocity = clamp(input.exitVelocity + random.real(4.0, 9.0), 45.0, 122.0);
        if (input.launchAngle >= 10.0 && input.launchAngle <= 38.0) {
            input.launchAngle = clamp(input.launchAngle + random.real(2.0, 6.0), 18.0, 34.0);
        }
    }
    // RHB: early timing → pull to LEFT field (negative x). LHB: pull to RIGHT (positive x).
    // timingError is negative when early, so RHB needs positive multiplier and LHB needs negative.
    const double timingSprayMult = (batter.battingSide == BattingSide::Right) ? 130.0 : -130.0;
    const double hbeSprayMult    = (batter.battingSide == BattingSide::Right) ?  28.0 :  -28.0;
    input.sprayAngle = clamp(swing.timingError * timingSprayMult + result.horizontalBatError * hbeSprayMult
                                 + random.real(-20.0, 20.0),
                             -45.0,
                             45.0);
    input.backSpin = clamp(700.0 + input.launchAngle * 52.0 + contactQuality * 900.0 + random.real(-320.0, 320.0),
                           250.0,
                           3600.0);
    input.sideSpin = clamp(input.sprayAngle * 0.06 + random.real(-2.5, 2.5), -7.5, 7.5);

    result.resultType = ContactResultType::InPlay;
    result.battedBallInput = input;
    return result;
}

} // namespace joji
