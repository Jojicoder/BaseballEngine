#pragma once

#include "AtBatTypes.h"
#include "BallPhysicsEngine.h"
#include "ContactEngine.h"
#include "Player.h"
#include "PitchEngine.h"
#include "PlayResult.h"
#include "Random.h"
#include "SwingDecisionEngine.h"
#include "SwingEngine.h"
#include "ZoneJudge.h"

namespace joji {

class AtBatEngine {
public:
    explicit AtBatEngine(BallPhysicsEngine physicsEngine = BallPhysicsEngine{},
                         PitchEngine pitchEngine = PitchEngine{},
                         SwingDecisionEngine swingDecisionEngine = SwingDecisionEngine{},
                         ZoneJudge zoneJudge = ZoneJudge{},
                         SwingEngine swingEngine = SwingEngine{},
                         ContactEngine contactEngine = ContactEngine{});

    PlayResult simulate(const Player& batter, const Player& pitcher, Random& random) const;
    AtBatResult simulatePlateAppearance(const Player& batter, const Player& pitcher, Random& random) const;

    // 1球ずつモード
    AtBatState startAtBat(const Player& batter, const Player& pitcher) const;
    bool simulateNextPitch(AtBatState& state, Random& random) const; // true = 打席終了

private:
    PitchLog simulatePitch(const Player& batter,
                           const Player& pitcher,
                           const Count& count,
                           int pitchNumber,
                           Random& random) const;

    BallPhysicsEngine physicsEngine_;
    PitchEngine pitchEngine_;
    SwingDecisionEngine swingDecisionEngine_;
    ZoneJudge zoneJudge_;
    SwingEngine swingEngine_;
    ContactEngine contactEngine_;
};

} // namespace joji
