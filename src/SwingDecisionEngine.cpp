#include "SwingDecisionEngine.h"

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

bool nearStrikeZone(const Pitch& pitch) {
    return std::abs(pitch.locationX) <= 1.05 && pitch.locationZ >= 1.30 && pitch.locationZ <= 3.75;
}

} // namespace

SwingDecision SwingDecisionEngine::decide(const Player& batter, const Pitch& pitch, const Count& count, ThrowingHand pitcherHand, Random& random) const {
    const double eye      = clamp((batter.eye       - 50) / 50.0, -0.8, 0.8);
    const double contact  = clamp((batter.contact   - 50) / 50.0, -0.8, 0.8);
    const double chaseAdj = clamp((batter.chaseRate - 50) / 50.0, -0.6, 0.6);
    const bool zone = inStrikeZone(pitch);
    const bool chase = !zone && nearStrikeZone(pitch);

    // Same-handed: breaking ball escapes away → harder to read, more chasing and takes
    // Opposite:    ball breaks back in → batter reads it better, chases less
    const bool sameHanded = (pitcherHand == ThrowingHand::Right && batter.battingSide == BattingSide::Right)
                         || (pitcherHand == ThrowingHand::Left  && batter.battingSide == BattingSide::Left);
    const double handednessChase = sameHanded ? 0.07 : -0.06;

    double probability = zone ? 0.43 : (chase ? 0.12 : 0.04);
    probability += contact * 0.09;
    probability -= eye * (zone ? 0.03 : 0.20);
    // 2-strike plate protection: high-contact batters expand their zone more when
    // protecting the plate — they can handle borderline pitches that poor-contact
    // batters foul off or miss. Scales from 0.02 (poor contact) to 0.11 (elite).
    const double strikeProtect = count.strikes >= 2
        ? std::clamp(contact * 0.08 + 0.04, 0.02, 0.11) : 0.0;
    probability += count.strikes >= 2 ? (zone ? 0.18 : strikeProtect) : 0.0;
    probability -= count.balls >= 3 ? (zone ? 0.16 : 0.42) : 0.0;
    probability += pitch.pitchQuality > 0.72 ? -0.06 : 0.0;
    if (chase) probability += handednessChase;
    // chaseRate: high = swings more at off-zone, low = lays off
    if (!zone) probability += chaseAdj * 0.12;
    probability = clamp(probability, zone ? 0.05 : 0.03, 0.88);

    SwingDecision decision;
    decision.swingProbability = probability;
    decision.decision = random.chance(probability) ? SwingDecisionType::Swing : SwingDecisionType::Take;
    if (decision.decision == SwingDecisionType::Swing) {
        decision.reason = zone ? "attacks a strike" : "chases outside the zone";
    } else {
        decision.reason = zone ? "takes a strike" : "lays off";
    }
    return decision;
}

} // namespace joji
