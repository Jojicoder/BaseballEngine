#include "AtBatTypes.h"

namespace joji {

bool isWalk(const Count& count) {
    return count.balls >= 4;
}

bool isStrikeOut(const Count& count) {
    return count.strikes >= 3;
}

Count afterBall(Count count) {
    count.balls += 1;
    return count;
}

Count afterStrike(Count count) {
    count.strikes += 1;
    return count;
}

Count afterFoul(Count count) {
    if (count.strikes < 2) {
        count.strikes += 1;
    }
    return count;
}

std::string toString(PitchType type) {
    switch (type) {
        case PitchType::Fastball:
            return "fastball";
        case PitchType::Slider:
            return "slider";
        case PitchType::Curveball:
            return "curveball";
        case PitchType::Changeup:
            return "changeup";
        case PitchType::Cutter:
            return "cutter";
        case PitchType::Splitter:
            return "splitter";
    }
    return "unknown pitch";
}

std::string toString(SwingDecisionType type) {
    switch (type) {
        case SwingDecisionType::Swing:
            return "swing";
        case SwingDecisionType::Take:
            return "take";
    }
    return "unknown decision";
}

std::string toString(ZoneResultType type) {
    switch (type) {
        case ZoneResultType::Ball:
            return "ball";
        case ZoneResultType::CalledStrike:
            return "called strike";
    }
    return "unknown zone result";
}

std::string toString(ContactResultType type) {
    switch (type) {
        case ContactResultType::SwingMiss:
            return "swing miss";
        case ContactResultType::Foul:
            return "foul";
        case ContactResultType::InPlay:
            return "in play";
    }
    return "unknown contact";
}

std::string toString(PitchOutcome outcome) {
    switch (outcome) {
        case PitchOutcome::Ball:
            return "ball";
        case PitchOutcome::CalledStrike:
            return "called strike";
        case PitchOutcome::SwingingStrike:
            return "swinging strike";
        case PitchOutcome::Foul:
            return "foul";
        case PitchOutcome::InPlay:
            return "in play";
    }
    return "unknown pitch outcome";
}

std::string toString(AtBatOutcome outcome) {
    switch (outcome) {
        case AtBatOutcome::Walk:
            return "walk";
        case AtBatOutcome::StrikeOut:
            return "strikeout";
        case AtBatOutcome::InPlay:
            return "in play";
        case AtBatOutcome::HitByPitch:
            return "hit by pitch";
    }
    return "unknown at-bat outcome";
}

} // namespace joji
