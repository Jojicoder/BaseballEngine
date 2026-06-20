#include "SwingEngine.h"

#include <algorithm>
#include <cmath>

namespace joji {
namespace {

double clamp(double value, double min, double max) {
    return std::max(min, std::min(value, max));
}

} // namespace

Swing SwingEngine::generate(const Player& batter,
                            const Pitch& pitch,
                            const Count& count,
                            const SwingDecision& decision,
                            Random& random) const {
    (void)decision;

    const double contact = clamp((batter.contact - 50) / 50.0, -0.8, 0.8);
    const double power = clamp((batter.power - 50) / 50.0, -0.8, 0.8);
    const double locationDifficulty = std::abs(pitch.locationX) * 0.12 + std::abs(pitch.locationZ - 2.5) * 0.08;
    const double pitchDifficulty = pitch.pitchQuality * 0.18 + locationDifficulty;

    Swing swing;
    swing.powerIntent = clamp(0.50 + power * 0.22 + (count.balls >= 2 ? 0.08 : 0.0) + random.real(-0.12, 0.12), 0.05, 0.98);
    swing.contactIntent = clamp(0.58 + contact * 0.24 + (count.strikes >= 2 ? 0.18 : 0.0) - swing.powerIntent * 0.10, 0.05, 0.98);
    swing.batSpeed = clamp(68.0 + batter.power * 0.24 + swing.powerIntent * 8.0 + random.real(-3.5, 3.5), 55.0, 95.0);
    swing.attackAngle = clamp(4.0 + power * 8.0 + swing.powerIntent * 6.0 + random.real(-6.0, 6.0), -10.0, 26.0);
    swing.timingError = clamp(random.real(-0.16, 0.16) + pitchDifficulty * random.real(-0.35, 0.35) - contact * 0.035, -0.32, 0.32);
    swing.swingTiming = -swing.timingError;
    swing.swingQuality = clamp(0.72 + contact * 0.14 - std::abs(swing.timingError) * 1.8 - pitchDifficulty * 0.28 + random.real(-0.10, 0.10), 0.0, 1.0);
    return swing;
}

} // namespace joji
