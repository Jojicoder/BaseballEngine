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
    std::vector<RunnerMovement> buildRunnerMovements(const std::vector<RunnerAnimation>& runners) const;
    std::vector<ThrowMovement> buildThrowMovements(const std::vector<ThrowAnimation>& throws) const;
    std::vector<TagPlay> buildTagPlays(const PlayResult& result,
                                       const std::vector<RunnerMovement>& runners,
                                       const std::vector<ThrowMovement>& throws) const;
    ReplayTimeline buildReplayTimeline(const AnimationPlan& plan) const;
    void rebuildDerivedTimeline(AnimationPlan& plan, const PlayResult* result = nullptr) const;
};

} // namespace joji
