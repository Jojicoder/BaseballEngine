#include "PitchEngine.h"

#include <algorithm>

namespace joji {
namespace {

double clamp(double value, double min, double max) {
    return std::max(min, std::min(value, max));
}

// same=true: breaking ball moves away from batter → more slider/cutter, less changeup
// same=false: platoon advantage for batter → more changeup, less slider
PitchType choosePitchType(bool sameHanded, Random& random) {
    const double roll = random.real(0.0, 1.0);
    if (sameHanded) {
        if (roll < 0.45) return PitchType::Fastball;
        if (roll < 0.65) return PitchType::Slider;    // +3%: breaking ball escapes
        if (roll < 0.75) return PitchType::Changeup;  // -5%: less effective vs same hand
        if (roll < 0.87) return PitchType::Curveball; // +1%
        if (roll < 0.97) return PitchType::Cutter;    // +4%
        return PitchType::Splitter;
    } else {
        if (roll < 0.45) return PitchType::Fastball;
        if (roll < 0.58) return PitchType::Slider;    // -8%: breaks back in, less safe
        if (roll < 0.77) return PitchType::Changeup;  // +11%: platoon weapon
        if (roll < 0.88) return PitchType::Curveball;
        if (roll < 0.93) return PitchType::Cutter;    // -4%
        return PitchType::Splitter;
    }
}

double velocityFor(PitchType type, const Player& pitcher, Random& random) {
    const double baseFastball = 88.0 + (pitcher.pitchingVelocity - 50) * 0.16;
    switch (type) {
        case PitchType::Fastball:
            return baseFastball + random.real(-1.8, 2.2);
        case PitchType::Cutter:
            return baseFastball - 3.0 + random.real(-1.8, 1.8);
        case PitchType::Slider:
            return baseFastball - 6.5 + random.real(-2.0, 2.0);
        case PitchType::Changeup:
            return baseFastball - 9.5 + random.real(-2.5, 2.0);
        case PitchType::Splitter:
            return baseFastball - 7.5 + random.real(-2.3, 2.0);
        case PitchType::Curveball:
            return baseFastball - 12.0 + random.real(-2.5, 2.5);
    }
    return baseFastball;
}

} // namespace

Pitch PitchEngine::generate(const Player& pitcher, const Count& count, BattingSide batterSide, Random& random) const {
    const bool sameHanded = (pitcher.throwingHand == ThrowingHand::Right && batterSide == BattingSide::Right)
                         || (pitcher.throwingHand == ThrowingHand::Left  && batterSide == BattingSide::Left);
    Pitch pitch;
    pitch.pitchType = choosePitchType(sameHanded, random);
    pitch.pitchVelocity = clamp(velocityFor(pitch.pitchType, pitcher, random), 68.0, 102.0);

    const double control = clamp((pitcher.pitchingControl - 50) / 50.0, -0.8, 0.8);
    const double behindInCount = count.balls >= 3 ? 0.16 : 0.0;
    const double aheadInCount = count.strikes >= 2 ? 0.12 : 0.0;
    const double commandSpread = clamp(0.88 - control * 0.18 - behindInCount + aheadInCount, 0.44, 1.20);

    pitch.locationX = random.real(-commandSpread, commandSpread);
    pitch.locationZ = random.real(1.30, 3.70);
    if (random.chance(0.36 - control * 0.08)) {
        pitch.locationX += random.chance(0.5) ? random.real(0.35, 1.10) : random.real(-1.10, -0.35);
    }
    if (random.chance(0.34 - control * 0.06)) {
        pitch.locationZ += random.chance(0.5) ? random.real(0.25, 0.90) : random.real(-0.90, -0.25);
    }

    const double stuff = clamp((pitcher.pitchingStuff - 50) / 50.0, -0.8, 0.8);
    switch (pitch.pitchType) {
        case PitchType::Fastball:
            pitch.movementX = random.real(-3.0, 3.0);
            pitch.movementZ = random.real(7.0, 15.0) + stuff * 3.0;
            pitch.spinRate = random.real(2050.0, 2550.0);
            break;
        case PitchType::Slider:
            pitch.movementX = random.real(5.0, 14.0);
            pitch.movementZ = random.real(-2.0, 5.0);
            pitch.spinRate = random.real(2250.0, 2900.0);
            break;
        case PitchType::Curveball:
            pitch.movementX = random.real(-5.0, 5.0);
            pitch.movementZ = random.real(-16.0, -7.0);
            pitch.spinRate = random.real(2350.0, 3050.0);
            break;
        case PitchType::Changeup:
            pitch.movementX = random.real(-7.0, 5.0);
            pitch.movementZ = random.real(0.0, 8.0);
            pitch.spinRate = random.real(1500.0, 2100.0);
            break;
        case PitchType::Cutter:
            pitch.movementX = random.real(2.0, 8.0);
            pitch.movementZ = random.real(4.0, 11.0);
            pitch.spinRate = random.real(2200.0, 2750.0);
            break;
        case PitchType::Splitter:
            pitch.movementX = random.real(-4.0, 4.0);
            pitch.movementZ = random.real(-9.0, -2.0);
            pitch.spinRate = random.real(1200.0, 1800.0);
            break;
    }

    const double edgePenalty = std::abs(pitch.locationX) * 0.15 + std::abs(pitch.locationZ - 2.5) * 0.08;
    pitch.pitchQuality = clamp(0.52 + control * 0.18 + stuff * 0.18 - edgePenalty + random.real(-0.12, 0.12), 0.0, 1.0);
    return pitch;
}

} // namespace joji
