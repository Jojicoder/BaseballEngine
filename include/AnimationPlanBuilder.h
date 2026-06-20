#pragma once

#include "AnimationTypes.h"
#include "AtBatTypes.h"
#include "PlayResolutionEngine.h"
#include "PlayResult.h"

#include <string>
#include <vector>

namespace joji {

class AnimationPlanBuilder {
public:
    AnimationPlan build(const AtBatResult& atBat,
                        const PlayResolution& resolution,
                        const BattedBall& battedBall,
                        const DefenseAlignment& defense = DefenseAlignment{},
                        const std::string& gameId = "") const;

    PitchAnimation buildPitchAnimation(const AtBatResult& atBat) const;
    BattedBallAnimation buildBattedBallAnimation(const AtBatResult& atBat,
                                                 const PlayResolution& resolution,
                                                 const BattedBall& battedBall) const;
    std::vector<RunnerAnimation> buildRunnerAnimations(const AtBatResult& atBat,
                                                       const PlayResolution& resolution) const;
    std::vector<DefenseAnimation> buildDefenseAnimations(const PlayResolution& resolution,
                                                         const BattedBall& battedBall,
                                                         const DefenseAlignment& defense) const;
    // Called by applyPendingAtBatResult after applyPlay — uses actual ThrowDecision
    std::vector<ThrowAnimation>   buildThrowAnimations(const PlayResult& result) const;
};

} // namespace joji
