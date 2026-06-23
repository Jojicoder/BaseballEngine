#include "SwingEngine.h"

#include <algorithm>
#include <cmath>

namespace joji {
namespace {

double clamp(double value, double min, double max) {
    return std::max(min, std::min(value, max));
}

bool inStrikeZone(const Pitch& pitch) {
    return std::abs(pitch.locationX) <= 0.83 && pitch.locationZ >= 1.55 && pitch.locationZ <= 3.50;
}

} // namespace

Swing SwingEngine::generate(const Player& batter,
                            const Pitch& pitch,
	                            const Count& count,
	                            const SwingDecision& decision,
	                            Random& random) const {
    const double contact = clamp((batter.contact - 50) / 50.0, -0.8, 0.8);
    const double power = clamp((batter.power - 50) / 50.0, -0.8, 0.8);
    const double locationDifficulty = std::abs(pitch.locationX) * 0.12 + std::abs(pitch.locationZ - 2.5) * 0.08;
    const double pitchDifficulty = pitch.pitchQuality * 0.18 + locationDifficulty;
    const bool zoneSwing = inStrikeZone(pitch);
    const bool chaseSwing = !zoneSwing;
    const double decisionConfidence = clamp((decision.swingProbability - 0.35) * 0.45, -0.12, 0.16);
    const double chasePenalty = chaseSwing ? 0.10 : 0.0;

    Swing swing;
    swing.powerIntent = clamp(0.50 + power * 0.22 + (count.balls >= 2 ? 0.08 : 0.0)
                                  + decisionConfidence * 0.25 - chasePenalty * 0.35
                                  + random.real(-0.12, 0.12), 0.05, 0.98);
    swing.contactIntent = clamp(0.58 + contact * 0.24 + (count.strikes >= 2 ? 0.18 : 0.0)
                                    - swing.powerIntent * 0.10 - chasePenalty * 0.45
                                    + (zoneSwing ? decisionConfidence * 0.18 : 0.0), 0.05, 0.98);
    swing.batSpeed = clamp(62.0 + batter.power * 0.28 + swing.powerIntent * 4.5 + random.real(-3.0, 3.0), 52.0, 95.0);
    swing.attackAngle = clamp(1.5 + power * 6.0 + swing.powerIntent * 5.0 + random.real(-7.0, 7.0), -12.0, 25.0);
    swing.timingError = clamp(random.real(-0.16, 0.16)
                                  + pitchDifficulty * random.real(-0.35, 0.35)
                                  + (chaseSwing ? random.real(-0.08, 0.08) : 0.0)
                                  - contact * 0.050,
                              -0.34,
                              0.34);
    swing.swingTiming = -swing.timingError;
    swing.swingQuality = clamp(0.72 + contact * 0.14
                                   - std::abs(swing.timingError) * 1.8
                                   - pitchDifficulty * 0.28
                                   - chasePenalty
                                   + (zoneSwing ? decisionConfidence * 0.20 : 0.0)
                                   + random.real(-0.10, 0.10), 0.0, 1.0);

    // 3D bat path: horizontal swing plane angle.
    // Inside pitch (high |locationX| toward batter) → batter turns early → pull angle.
    // Power intent and pull tendency add to the pull angle.
    // Contact intent (protect swing) flattens the path toward oppo.
    // locationX: positive = catcher's right = inside for RHB, outside for LHB.
    // Negate for LHB so that "inside" always yields a positive pull signal.
    const double insideSignal = (batter.battingSide == BattingSide::Left)
                                    ? -pitch.locationX
                                    : pitch.locationX;
    const double insidePull   = insideSignal * 9.0;      // inside pitch → more pull plane
    const double powerPull    = power * 6.0;              // power hitters rotate more
    const double contactFlat  = -swing.contactIntent * 4.0; // protect swing → squared up
    swing.swingPlaneAngle = clamp(insidePull + powerPull + contactFlat
                                  + random.real(-4.0, 4.0), -18.0, 22.0);
    return swing;
}

} // namespace joji
