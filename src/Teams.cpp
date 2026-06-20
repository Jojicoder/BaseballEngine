#include "Teams.h"

namespace joji {

namespace {

using P  = Position;
using PR = PitcherRole;
using TH = ThrowingHand;
using BS = BattingSide;

Player batter(std::string name, P position,
              int contact, int power, int eye, int speed, int fielding, int arm,
              int pitchingVelocity, int pitchingControl, int pitchingStuff,
              BS battingSide,
              TH throwingHand = TH::Right) {
    Player player{name, position, contact, power, eye, speed, fielding, arm,
                  pitchingVelocity, pitchingControl, pitchingStuff};
    player.battingSide = battingSide;
    player.throwingHand = throwingHand;
    return player;
}

Player pitcher(std::string name, int contact, int power, int eye, int speed,
               int fielding, int arm, int pitchingVelocity, int pitchingControl,
               int pitchingStuff, int pitchingStamina, PR role,
               TH throwingHand = TH::Right) {
    Player player{name, P::Pitcher, contact, power, eye, speed, fielding, arm,
                  pitchingVelocity, pitchingControl, pitchingStuff, pitchingStamina,
                  role};
    player.throwingHand = throwingHand;
    return player;
}

// ────────────────────────────────────────────
// Newark Knights — バランス型
// ────────────────────────────────────────────
Team newarkKnights() {
    return Team{
        "Newark Knights",
        {   // 打者8人 (contact power eye speed fielding arm pV pC pS pSta)
            // power>=75:-2 / 65-74:-1 / <65:0
            batter("Joji Rivera",   P::CenterField, 77, 66, 70, 74, 61, 55, 35, 34, 32, BS::Left),  // 67-1
            {"Marcus Bell",   P::LeftField,   71, 73, 60, 62, 70, 67, 30, 30, 30},  // 74-1
            {"Andre Vale",    P::FirstBase,   66, 83, 56, 48, 58, 60, 36, 35, 34},  // 85-2
            {"Nico Sterling", P::ThirdBase,   79, 60, 72, 69, 64, 58, 28, 32, 29},  // 60 nc
            batter("Dante Cruz",    P::RightField,  67, 77, 54, 55, 66, 72, 34, 31, 30, BS::Left),  // 79-2
            {"Eli Brooks",    P::Shortstop,   68, 64, 64, 71, 73, 64, 30, 31, 29},  // 64 nc
            {"Tariq Mason",   P::SecondBase,  65, 70, 57, 59, 62, 76, 32, 30, 31},  // 71-1
            {"Cal Weston",    P::Catcher,     65, 55, 67, 63, 69, 57, 35, 38, 36},  // 55 nc
        },
        pitcher("Rafael Stone", 43, 45, 42, 39, 52, 61, 78, 69, 73, 68, PR::Starter),
        {
            {"Derek Hahn",   P::Pitcher, 40, 40, 40, 38, 50, 58, 72, 64, 68, 62, PR::LongRelief},
            {"Sam Price",    P::Pitcher, 40, 40, 40, 38, 50, 58, 75, 60, 70, 58, PR::MiddleRelief},
            pitcher("Luis Cano", 40, 40, 40, 38, 50, 56, 70, 63, 65, 55, PR::MiddleRelief, TH::Left),
            {"Tony Marsh",   P::Pitcher, 40, 40, 40, 38, 50, 60, 78, 62, 74, 60, PR::Setup},
            {"Kyle Reid",    P::Pitcher, 40, 40, 40, 38, 50, 60, 82, 65, 76, 58, PR::Closer},
        }
    };
}

// ────────────────────────────────────────────
// Queens Titans — パワー型
// ────────────────────────────────────────────
Team queensTitans() {
    return Team{
        "Queens Titans",
        {
            batter("Malik Chen",   P::CenterField, 74, 61, 73, 77, 68, 59, 32, 31, 30, BS::Switch),  // 61 nc
            {"Oscar Vega",   P::LeftField,   69, 74, 59, 61, 63, 66, 34, 34, 31},  // 76-2
            {"Theo Grant",   P::FirstBase,   75, 77, 65, 52, 57, 62, 35, 32, 33},  // 79-2
            batter("Julian Frost", P::RightField,  64, 81, 53, 46, 54, 60, 36, 34, 35, BS::Left),  // 83-2
            {"Samir Holt",   P::ThirdBase,   75, 63, 69, 68, 71, 63, 29, 31, 30},  // 63 nc
            {"Isaac Monroe", P::Shortstop,   64, 71, 57, 57, 65, 74, 31, 32, 32},  // 72-1
            {"Leo Navarro",  P::SecondBase,  62, 65, 62, 72, 76, 69, 33, 34, 30},  // 66-1
            {"Miles Archer", P::Catcher,     70, 57, 66, 66, 67, 58, 32, 35, 34},  // 57 nc
        },
        pitcher("Victor Hale", 41, 48, 45, 38, 50, 64, 82, 56, 77, 55, PR::Starter, TH::Left),
        {
            {"Will Donner",  P::Pitcher, 40, 40, 40, 38, 50, 58, 70, 62, 67, 60, PR::LongRelief},
            {"Omar Diaz",    P::Pitcher, 40, 40, 40, 38, 50, 58, 73, 58, 68, 57, PR::MiddleRelief},
            {"Jake Thorn",   P::Pitcher, 40, 40, 40, 38, 50, 56, 71, 61, 66, 55, PR::MiddleRelief},
            {"Pete Crane",   P::Pitcher, 40, 40, 40, 38, 50, 60, 77, 61, 73, 60, PR::Setup},
            {"Ray Fox",      P::Pitcher, 40, 40, 40, 38, 50, 60, 83, 62, 78, 55, PR::Closer},
        }
    };
}

// ────────────────────────────────────────────
// Brooklyn Hammers — 強打型
// ────────────────────────────────────────────
Team brooklynHammers() {
    return Team{
        "Brooklyn Hammers",
        {
            batter("Devon Harris",  P::CenterField, 71, 70, 60, 74, 62, 58, 32, 30, 31, BS::Switch),  // 71-1
            {"Marcus Slade",  P::LeftField,   69, 72, 56, 60, 65, 66, 30, 30, 30},  // 73-1
            batter("Ray Coleman",   P::FirstBase,   66, 77, 52, 42, 58, 60, 34, 33, 32, BS::Left),  // 79-2
            {"Troy Banks",    P::RightField,  67, 73, 54, 54, 64, 72, 32, 31, 30},  // 75-2
            {"Kyle Jensen",   P::ThirdBase,   73, 70, 58, 64, 62, 60, 28, 30, 29},  // 71-1
            {"Felix Mora",    P::Shortstop,   70, 67, 62, 70, 72, 66, 30, 29, 28},  // 68-1
            {"Craig Dunn",    P::SecondBase,  65, 66, 58, 62, 68, 58, 31, 30, 29},  // 67-1
            {"Walt Rivera",   P::Catcher,     63, 63, 56, 54, 70, 64, 32, 34, 31},  // 63 nc
        },
        {"Sam Holt",      P::Pitcher, 42, 44, 40, 38, 50, 60, 82, 64, 76, 60, PR::Starter},
        {
            {"Dan Cross",    P::Pitcher, 40, 40, 40, 38, 50, 58, 74, 58, 70, 60, PR::LongRelief},
            {"Ty Burns",     P::Pitcher, 40, 40, 40, 38, 50, 58, 76, 56, 72, 57, PR::MiddleRelief},
            {"Vic Shaw",     P::Pitcher, 40, 40, 40, 38, 50, 56, 72, 60, 68, 55, PR::MiddleRelief},
            {"Liam Nox",     P::Pitcher, 40, 40, 40, 38, 50, 60, 80, 58, 76, 58, PR::Setup},
            {"Joe Blaze",    P::Pitcher, 40, 40, 40, 38, 50, 60, 85, 60, 80, 56, PR::Closer},
        }
    };
}

// ────────────────────────────────────────────
// Harlem Eagles — 巧打・機動型
// ────────────────────────────────────────────
Team harlemEagles() {
    return Team{
        "Harlem Eagles",
        {
            batter("Jamal West",    P::CenterField, 81, 64, 74, 84, 70, 58, 30, 30, 30, BS::Left),  // 64 nc
            {"Darius King",   P::LeftField,   77, 66, 72, 76, 68, 62, 30, 29, 29},  // 67-1
            batter("Tomas Ruiz",    P::RightField,  75, 72, 68, 68, 66, 68, 31, 30, 30, BS::Switch),  // 73-1
            {"Aaron Miles",   P::ThirdBase,   73, 70, 68, 64, 64, 62, 29, 30, 28},  // 71-1
            {"Carlos Webb",   P::FirstBase,   71, 75, 64, 52, 60, 56, 33, 32, 31},  // 77-2
            {"Kevin Nash",    P::Shortstop,   71, 64, 66, 74, 74, 68, 30, 31, 29},  // 65-1
            {"Pete Owens",    P::SecondBase,  68, 63, 64, 72, 70, 60, 31, 30, 29},  // 63 nc
            {"Sam Ford",      P::Catcher,     66, 61, 60, 58, 68, 62, 32, 34, 31},  // 61 nc
        },
        pitcher("Hector Vega", 42, 44, 42, 40, 52, 60, 76, 70, 72, 72, PR::Starter, TH::Left),
        {
            {"Al Bryce",     P::Pitcher, 40, 40, 40, 38, 50, 58, 72, 66, 68, 64, PR::LongRelief},
            {"Rico Penn",    P::Pitcher, 40, 40, 40, 38, 50, 58, 74, 64, 70, 60, PR::MiddleRelief},
            {"Sam Kerr",     P::Pitcher, 40, 40, 40, 38, 50, 56, 70, 65, 67, 60, PR::MiddleRelief},
            {"Don Reese",    P::Pitcher, 40, 40, 40, 38, 50, 60, 76, 67, 72, 62, PR::Setup},
            {"Cal Storm",    P::Pitcher, 40, 40, 40, 38, 50, 60, 80, 68, 76, 60, PR::Closer},
        }
    };
}

// ────────────────────────────────────────────
// Bronx Wolves — 投高打低型
// ────────────────────────────────────────────
Team bronxWolves() {
    return Team{
        "Bronx Wolves",
        {
            batter("Damon Price",   P::CenterField, 79, 63, 70, 82, 68, 60, 30, 30, 29, BS::Left),  // 63 nc
            {"Reggie Walsh",  P::LeftField,   77, 66, 68, 74, 66, 64, 30, 29, 30},  // 67-1
            batter("Owen Shaw",     P::FirstBase,   75, 74, 64, 58, 60, 56, 32, 31, 31, BS::Switch),  // 76-2
            {"Ben Frost",     P::RightField,  73, 73, 62, 66, 62, 68, 31, 30, 30},  // 74-1
            {"Marco Reyes",   P::ThirdBase,   75, 69, 66, 72, 66, 64, 29, 30, 28},  // 70-1
            {"Tyler Cross",   P::Shortstop,   73, 63, 64, 76, 72, 66, 30, 31, 29},  // 63 nc
            {"Nick Vega",     P::SecondBase,  71, 64, 62, 72, 68, 60, 30, 30, 30},  // 65-1
            {"Al Duncan",     P::Catcher,     69, 61, 62, 60, 70, 64, 31, 33, 30},  // 61 nc
        },
        {"Cole Maddox",   P::Pitcher, 42, 44, 42, 38, 52, 62, 84, 72, 82, 64, PR::Starter},
        {
            {"Paul West",    P::Pitcher, 40, 40, 40, 38, 50, 58, 70, 62, 66, 60, PR::LongRelief},
            {"Dave Marsh",   P::Pitcher, 40, 40, 40, 38, 50, 58, 72, 60, 68, 58, PR::MiddleRelief},
            {"Tim Cruz",     P::Pitcher, 40, 40, 40, 38, 50, 56, 68, 63, 64, 55, PR::MiddleRelief},
            {"Grant Fox",    P::Pitcher, 40, 40, 40, 38, 50, 60, 74, 62, 70, 58, PR::Setup},
            {"Nick Black",   P::Pitcher, 40, 40, 40, 38, 50, 60, 78, 64, 74, 55, PR::Closer},
        }
    };
}

// ────────────────────────────────────────────
// Staten Island Foxes — 弱小
// ────────────────────────────────────────────
Team statenIslandFoxes() {
    return Team{
        "Staten Island Foxes",
        {
            {"Joey Lane",     P::CenterField, 77, 61, 65, 68, 60, 56, 30, 29, 28},  // 61 nc
            {"Matt Chen",     P::LeftField,   75, 64, 62, 62, 62, 60, 29, 30, 29},  // 65-1
            batter("Pat Gallo",     P::FirstBase,   73, 73, 60, 50, 58, 56, 31, 30, 30, BS::Left),  // 75-2
            {"Dave Moss",     P::RightField,  71, 72, 60, 54, 60, 64, 30, 30, 29},  // 73-1
            {"Rob Stone",     P::ThirdBase,   73, 68, 62, 58, 62, 60, 28, 29, 28},  // 69-1
            {"Al Ross",       P::Shortstop,   70, 61, 60, 64, 68, 62, 30, 30, 29},  // 61 nc
            {"Tim Carr",      P::SecondBase,  68, 61, 58, 58, 64, 58, 30, 29, 28},  // 61 nc
            {"Gary Knox",     P::Catcher,     67, 57, 58, 52, 66, 60, 30, 32, 29},  // 57 nc
        },
        pitcher("Ben Mack", 40, 42, 40, 38, 50, 56, 74, 64, 67, 48, PR::Starter, TH::Left),
        {
            {"Rob Clay",     P::Pitcher, 40, 40, 40, 38, 50, 56, 68, 61, 65, 52, PR::LongRelief},
            {"Dave Kim",     P::Pitcher, 40, 40, 40, 38, 50, 56, 70, 59, 67, 50, PR::MiddleRelief},
            {"Pat Ross",     P::Pitcher, 40, 40, 40, 38, 50, 54, 66, 61, 65, 48, PR::MiddleRelief},
            {"Al Price",     P::Pitcher, 40, 40, 40, 38, 50, 56, 72, 61, 69, 52, PR::Setup},
            {"Ty Shore",     P::Pitcher, 40, 40, 40, 38, 50, 58, 74, 63, 71, 50, PR::Closer},
        }
    };
}

} // namespace

std::vector<Team> allTeams() {
    return {
        newarkKnights(),
        queensTitans(),
        brooklynHammers(),
        harlemEagles(),
        bronxWolves(),
        statenIslandFoxes(),
    };
}

} // namespace joji
