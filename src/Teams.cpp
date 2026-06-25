#include "Teams.h"
#include "PlayResolutionEngine.h"

namespace joji {

namespace {

using P  = Position;
using PR = PitcherRole;
using PL = PlayerRole;
using TH = ThrowingHand;
using BS = BattingSide;
using PT = PitchType;

// ── Fluent helpers ────────────────────────────────────────────────────────────

Player withArsenal(Player p, std::vector<PitchGrade> a) {
    p.arsenal = std::move(a);
    return p;
}

Player withTend(Player p, int pull, int hb, int chase, int cvb) {
    p.pullTendency = pull; p.highBallHitter = hb;
    p.chaseRate = chase;   p.contactVsBreaking = cvb;
    return p;
}

Player withRole(Player p, PL role) {
    p.role = role;
    return p;
}

// batter: fielding stats (pitching fields left at defaults)
Player batter(std::string name, P pos,
              int contact, int power, int eye, int speed, int fielding, int arm,
              BS side = BS::Right, TH hand = TH::Right) {
    Player p;
    p.name = std::move(name); p.position = pos;
    p.contact = contact; p.power = power; p.eye = eye; p.speed = speed;
    p.fielding = fielding; p.arm = arm;
    p.battingSide = side; p.throwingHand = hand;
    return p;
}

// pitcher: pitching stats (batting fields left at 50 or set by hand)
Player pitcher(std::string name, int vel, int ctrl, int stuff, int stamina,
               PR role, TH hand = TH::Right) {
    Player p;
    p.name = std::move(name); p.position = P::Pitcher;
    p.pitchingVelocity = vel; p.pitchingControl = ctrl;
    p.pitchingStuff = stuff; p.pitchingStamina = stamina;
    p.pitcherRole = role; p.throwingHand = hand;
    // Pitchers have minimal batting stats
    p.contact = 42; p.power = 40; p.eye = 40; p.speed = 40;
    p.fielding = 52; p.arm = 58;
    return p;
}

// ── Newark Knights ────────────────────────────────────────────────────────────
// チームカラー: スピード&コンタクト型。足で点を取る。先発ローテが安定。
Team newarkKnights() {
    return Team{
        "Newark Knights",
        // ── lineup (8人) ─────────────────────────────────────────────────────
        {
            // 1. Joji Rivera  CF  リードオフ: contact/eye/speed の塊
            withRole(withTend(batter("Joji Rivera",  P::CenterField, 78,62,74,82,72,58, BS::Left),  44,56,36,64), PL::Leadoff),
            // 2. Marcus Bell  LF  コンタクト型: gap power、打率.290 クラス
            withRole(withTend(batter("Marcus Bell",  P::LeftField,   74,68,64,66,70,65),             58,48,52,48), PL::ContactHitter),
            // 3. Andre Vale   1B  クリーンアップ候補: power 特化、三振多め
            withRole(withTend(batter("Andre Vale",   P::FirstBase,   66,84,58,46,62,60, BS::Left),  70,44,66,36), PL::CornerIF),
            // 4. Nico Sterling 3B  バランス: 中距離パワー + fielding
            withRole(withTend(batter("Nico Sterling",P::ThirdBase,   80,62,72,68,68,62),             40,54,32,64), PL::CornerIF),
            // 5. Dante Cruz   RF  ミドルパワー、ゴーアヘッドヒット型
            withRole(withTend(batter("Dante Cruz",   P::RightField,  68,76,56,58,66,70, BS::Left),  66,46,60,44), PL::CornerOF),
            // 6. Eli Brooks   SS  守備型ショート: fielding/arm 高め
            withRole(withTend(batter("Eli Brooks",   P::Shortstop,   68,54,66,74,80,76),             48,52,44,56), PL::MiddleIF),
            // 7. Tariq Mason  2B  つなぎ: eye 高め、足でかき回す
            withRole(withTend(batter("Tariq Mason",  P::SecondBase,  66,58,70,76,74,66),             54,50,46,56), PL::MiddleIF),
            // 8. Cal Weston   C   強肩捕手: arm 特化、打撃は控えめ
            withRole(withTend(batter("Cal Weston",   P::Catcher,     62,54,64,52,72,80),             42,58,38,60), PL::Catcher),
        },
        // ── rotation (5人) ───────────────────────────────────────────────────
        {
            // Ace: コントロール型、奪三振より打ち取る
            withRole(withArsenal(pitcher("Rafael Stone",  80,74,78,76, PR::Starter),
                {{PT::Fastball,74},{PT::Slider,72},{PT::Changeup,66},{PT::Curveball,58}}), PL::Ace),
            // #2: 左腕フライボール系
            withRole(withArsenal(pitcher("Luis Cano",     76,72,76,72, PR::Starter, TH::Left),
                {{PT::Fastball,68},{PT::Curveball,74},{PT::Changeup,68},{PT::Slider,58}}), PL::Starter),
            // #3: ゴロ系右腕、制球重視
            withRole(withArsenal(pitcher("Derek Hahn",    72,76,68,70, PR::Starter),
                {{PT::Fastball,64},{PT::Cutter,72},{PT::Changeup,64},{PT::Curveball,54}}), PL::Starter),
            // #4: パワー系リリーフから転向、スタミナ低め
            withRole(withArsenal(pitcher("Sam Price",     74,64,72,62, PR::Starter),
                {{PT::Fastball,68},{PT::Slider,66},{PT::Changeup,54}}), PL::BackOfRotation),
            // #5: 若手右腕、制球に課題
            withRole(withArsenal(pitcher("Cole Brady",    70,60,66,64, PR::Starter),
                {{PT::Fastball,62},{PT::Curveball,60},{PT::Changeup,56}}), PL::BackOfRotation),
        },
        // ── bullpen (8人) ────────────────────────────────────────────────────
        {
            withRole(withArsenal(pitcher("Kyle Reid",   84,66,82,46, PR::Closer),
                {{PT::Fastball,76},{PT::Slider,74}}), PL::Closer),
            withRole(withArsenal(pitcher("Tony Marsh",  80,68,78,52, PR::Setup),
                {{PT::Fastball,70},{PT::Slider,68},{PT::Cutter,62}}), PL::Setup),
            withRole(withArsenal(pitcher("Jared Voss",  78,66,74,54, PR::Setup),
                {{PT::Fastball,68},{PT::Curveball,66},{PT::Changeup,58}}), PL::Setup),
            withRole(withArsenal(pitcher("Matt Oiler",  74,64,70,60, PR::MiddleRelief),
                {{PT::Fastball,64},{PT::Slider,62}}), PL::MiddleRelief),
            withRole(withArsenal(pitcher("Remy Cruz",   72,66,68,62, PR::MiddleRelief),
                {{PT::Fastball,62},{PT::Changeup,64},{PT::Cutter,58}}), PL::MiddleRelief),
            withRole(withArsenal(pitcher("Owen Park",   68,70,64,74, PR::LongRelief),
                {{PT::Fastball,60},{PT::Curveball,62},{PT::Changeup,60}}), PL::LongRelief),
            withRole(withArsenal(pitcher("Felix Lara",  70,68,66,50, PR::MiddleRelief, TH::Left),
                {{PT::Fastball,60},{PT::Changeup,68},{PT::Curveball,62}}), PL::Specialist),
            withRole(withArsenal(pitcher("Brock Shaw",  72,64,68,66, PR::LongRelief),
                {{PT::Fastball,62},{PT::Slider,60},{PT::Changeup,56}}), PL::LongRelief),
        },
        // ── bench (5人) ──────────────────────────────────────────────────────
        {
            // 控えC: arm 強め
            withRole(withTend(batter("Ray Santos",  P::Catcher,    56,50,60,46,68,78, BS::Left),  44,52,40,56), PL::BackupCatcher),
            // ユーティリティINF: SS/2B/3B どこでも守れる
            withRole(withTend(batter("Lou Ferris",  P::SecondBase, 62,54,62,68,76,72),             50,50,46,54), PL::UtilityIF),
            // 4th OF: 守備交代・代走
            withRole(withTend(batter("Kent Blair",  P::CenterField,60,50,58,78,72,60, BS::Switch), 48,52,44,52), PL::ExtraOF),
            // 代打専門: contact/eye 高め
            withRole(withTend(batter("Pete Cruz",   P::LeftField,  72,60,70,46,50,52, BS::Left),   58,50,42,62), PL::PinchHitter),
            // スペアベンチ: パワー系代打
            withRole(withTend(batter("Walt Rooney", P::FirstBase,  58,72,54,44,56,58),             68,44,64,40), PL::PinchHitter),
        },
        BallparkConfig::knightsField()
    };
}

// ── Queens Titans ─────────────────────────────────────────────────────────────
// チームカラー: パワー打線。本塁打で点を取る。投手陣は平均的。
Team queensTitans() {
    return Team{
        "Queens Titans",
        {
            withRole(withTend(batter("Malik Chen",   P::CenterField, 76,62,74,78,70,58, BS::Switch), 46,52,34,60), PL::Leadoff),
            withRole(withTend(batter("Oscar Vega",   P::LeftField,   70,72,62,64,64,66),             62,46,58,44), PL::ContactHitter),
            withRole(withTend(batter("Theo Grant",   P::FirstBase,   72,80,66,48,60,58),             68,46,60,42), PL::CornerIF),
            withRole(withTend(batter("Julian Frost", P::RightField,  64,88,54,44,56,60, BS::Left),   74,42,68,36), PL::PowerHitter),
            withRole(withTend(batter("Samir Holt",   P::ThirdBase,   74,66,70,66,72,64),             44,56,36,62), PL::CornerIF),
            withRole(withTend(batter("Isaac Monroe",  P::Shortstop,  64,66,60,60,70,76),             58,48,54,48), PL::MiddleIF),
            withRole(withTend(batter("Leo Navarro",  P::SecondBase,  62,60,64,72,76,68),             52,50,50,52), PL::MiddleIF),
            withRole(withTend(batter("Miles Archer", P::Catcher,     68,58,66,54,70,78),             44,56,38,60), PL::Catcher),
        },
        {
            withRole(withArsenal(pitcher("Victor Hale", 84,62,82,72, PR::Starter, TH::Left),
                {{PT::Fastball,78},{PT::Curveball,74},{PT::Changeup,68},{PT::Slider,62}}), PL::Ace),
            withRole(withArsenal(pitcher("Deon Clarke", 78,68,74,68, PR::Starter),
                {{PT::Fastball,70},{PT::Slider,68},{PT::Changeup,62}}), PL::Starter),
            withRole(withArsenal(pitcher("Hal Morris",  74,70,68,66, PR::Starter),
                {{PT::Fastball,64},{PT::Curveball,66},{PT::Changeup,60}}), PL::Starter),
            withRole(withArsenal(pitcher("Al Ruiz",     70,64,64,62, PR::Starter),
                {{PT::Fastball,60},{PT::Slider,62},{PT::Changeup,56}}), PL::BackOfRotation),
            withRole(withArsenal(pitcher("Bo Tanner",   68,62,62,60, PR::Starter),
                {{PT::Fastball,58},{PT::Changeup,60},{PT::Curveball,54}}), PL::BackOfRotation),
        },
        {
            withRole(withArsenal(pitcher("Ray Fox",    86,62,84,44, PR::Closer),
                {{PT::Fastball,78},{PT::Slider,76}}), PL::Closer),
            withRole(withArsenal(pitcher("Pete Crane", 80,66,76,52, PR::Setup),
                {{PT::Fastball,68},{PT::Slider,66},{PT::Cutter,60}}), PL::Setup),
            withRole(withArsenal(pitcher("Gio Ware",   78,64,72,54, PR::Setup),
                {{PT::Fastball,66},{PT::Changeup,64},{PT::Slider,60}}), PL::Setup),
            withRole(withArsenal(pitcher("Jake Thorn", 74,62,68,60, PR::MiddleRelief),
                {{PT::Fastball,62},{PT::Curveball,62},{PT::Changeup,56}}), PL::MiddleRelief),
            withRole(withArsenal(pitcher("Omar Diaz",  72,64,66,62, PR::MiddleRelief),
                {{PT::Fastball,62},{PT::Slider,60}}), PL::MiddleRelief),
            withRole(withArsenal(pitcher("Will Donner",68,68,62,72, PR::LongRelief),
                {{PT::Fastball,58},{PT::Changeup,62},{PT::Slider,56}}), PL::LongRelief),
            withRole(withArsenal(pitcher("Ken Shea",   72,66,64,52, PR::MiddleRelief, TH::Left),
                {{PT::Fastball,60},{PT::Curveball,64},{PT::Changeup,60}}), PL::Specialist),
            withRole(withArsenal(pitcher("Ned Lowe",   70,64,64,66, PR::LongRelief),
                {{PT::Fastball,60},{PT::Slider,58},{PT::Changeup,54}}), PL::LongRelief),
        },
        {
            withRole(withTend(batter("Curt Ellis",  P::Catcher,    54,52,58,48,66,76, BS::Left), 42,54,36,58), PL::BackupCatcher),
            withRole(withTend(batter("Ben Lowe",    P::ThirdBase,  60,68,58,56,68,66),            62,46,56,46), PL::UtilityIF),
            withRole(withTend(batter("Wade Pryor",  P::LeftField,  58,60,60,68,66,58),            52,50,50,50), PL::ExtraOF),
            withRole(withTend(batter("Tate Hall",   P::FirstBase,  68,66,64,44,52,54),            66,46,60,42), PL::PinchHitter),
            withRole(withTend(batter("Duke Nolan",  P::RightField, 62,72,54,50,56,60, BS::Left), 70,42,66,38), PL::PinchHitter),
        },
        BallparkConfig::queensColiseum()
    };
}

// ── Brooklyn Hammers ──────────────────────────────────────────────────────────
// チームカラー: 強打線。リーグ最高クラスの得点力。
Team brooklynHammers() {
    return Team{
        "Brooklyn Hammers",
        {
            withRole(withTend(batter("Devon Harris", P::CenterField, 74,66,66,76,68,62, BS::Switch), 56,50,50,50), PL::Leadoff),
            withRole(withTend(batter("Owen Shaw",    P::LeftField,   74,76,70,58,62,64, BS::Left),   60,52,44,60), PL::PowerHitter),
            withRole(withTend(batter("Ray Coleman",  P::FirstBase,   68,78,60,42,60,60, BS::Left),   70,44,66,38), PL::CornerIF),
            withRole(withTend(batter("Troy Banks",   P::RightField,  72,76,60,54,66,72),             66,46,60,44), PL::CornerOF),
            withRole(withTend(batter("Kyle Jensen",  P::ThirdBase,   74,72,62,62,64,62),             58,52,52,50), PL::CornerIF),
            withRole(withTend(batter("Felix Mora",   P::Shortstop,   72,60,64,70,74,68),             52,50,48,54), PL::MiddleIF),
            withRole(withTend(batter("Craig Dunn",   P::SecondBase,  66,62,62,64,70,60),             54,50,50,50), PL::MiddleIF),
            withRole(withTend(batter("Reggie Walsh", P::Catcher,     66,64,66,50,74,80, BS::Left),   44,56,36,62), PL::Catcher),
        },
        {
            withRole(withArsenal(pitcher("Cole Maddox", 82,70,82,76, PR::Starter),
                {{PT::Fastball,76},{PT::Slider,74},{PT::Curveball,66},{PT::Changeup,62}}), PL::Ace),
            withRole(withArsenal(pitcher("Kai Tanaka",  76,70,76,72, PR::Starter),
                {{PT::Fastball,70},{PT::Cutter,68},{PT::Changeup,66},{PT::Slider,60}}), PL::Starter),
            withRole(withArsenal(pitcher("Ed Foley",    76,74,72,70, PR::Starter),
                {{PT::Fastball,68},{PT::Curveball,70},{PT::Changeup,64}}), PL::Starter),
            withRole(withArsenal(pitcher("Trey Holt",   72,66,68,66, PR::Starter),
                {{PT::Fastball,64},{PT::Slider,64},{PT::Changeup,58}}), PL::BackOfRotation),
            withRole(withArsenal(pitcher("Vin Cross",   70,64,64,62, PR::Starter),
                {{PT::Fastball,60},{PT::Curveball,62},{PT::Changeup,56}}), PL::BackOfRotation),
        },
        {
            withRole(withArsenal(pitcher("Sal Reyes",  86,64,84,44, PR::Closer),
                {{PT::Fastball,78},{PT::Slider,76},{PT::Cutter,66}}), PL::Closer),
            withRole(withArsenal(pitcher("Don Wells",  82,66,80,50, PR::Setup),
                {{PT::Fastball,72},{PT::Slider,70},{PT::Changeup,60}}), PL::Setup),
            withRole(withArsenal(pitcher("Bret Nye",   80,68,76,52, PR::Setup),
                {{PT::Fastball,70},{PT::Cutter,68},{PT::Curveball,62}}), PL::Setup),
            withRole(withArsenal(pitcher("Kirk Soto",  76,64,70,60, PR::MiddleRelief),
                {{PT::Fastball,64},{PT::Slider,64},{PT::Changeup,56}}), PL::MiddleRelief),
            withRole(withArsenal(pitcher("Dale Penn",  74,66,68,62, PR::MiddleRelief),
                {{PT::Fastball,62},{PT::Curveball,64},{PT::Changeup,58}}), PL::MiddleRelief),
            withRole(withArsenal(pitcher("Art Beal",   70,70,64,74, PR::LongRelief),
                {{PT::Fastball,60},{PT::Changeup,64},{PT::Curveball,60}}), PL::LongRelief),
            withRole(withArsenal(pitcher("Hank Grim",  72,68,64,52, PR::MiddleRelief, TH::Left),
                {{PT::Fastball,60},{PT::Curveball,66},{PT::Changeup,62}}), PL::Specialist),
            withRole(withArsenal(pitcher("Rob Dunn",   72,64,66,66, PR::LongRelief),
                {{PT::Fastball,62},{PT::Slider,60},{PT::Changeup,56}}), PL::LongRelief),
        },
        {
            withRole(withTend(batter("Marc Cole",   P::Catcher,    58,52,60,48,68,76),             42,56,38,58), PL::BackupCatcher),
            withRole(withTend(batter("Zach Olin",   P::Shortstop,  62,54,62,70,76,72),             50,50,46,54), PL::UtilityIF),
            withRole(withTend(batter("Deon Gray",   P::RightField, 64,58,60,74,68,60, BS::Left),   48,52,44,52), PL::ExtraOF),
            withRole(withTend(batter("Chris Webb",  P::FirstBase,  70,68,66,46,54,56, BS::Left),   64,48,58,44), PL::PinchHitter),
            withRole(withTend(batter("Hugo Nava",   P::LeftField,  64,74,58,52,54,58),             70,44,64,40), PL::PinchHitter),
        },
        BallparkConfig::ironYard()
    };
}

// ── Bronx Wolves ──────────────────────────────────────────────────────────────
// チームカラー: リーグ最強の総合力。エース+強打線+守備が高水準。
Team bronxWolves() {
    return Team{
        "Bronx Wolves",
        {
            withRole(withTend(batter("Tomas Ruiz",  P::LeftField,   74,78,70,68,66,64, BS::Left),   62,52,46,58), PL::PowerHitter),
            withRole(withTend(batter("Jamal West",  P::CenterField, 74,66,72,80,74,62, BS::Switch),  50,52,40,62), PL::Leadoff),
            withRole(withTend(batter("Nick Stone",  P::FirstBase,   70,74,66,50,64,62, BS::Left),   66,46,60,44), PL::CornerIF),
            withRole(withTend(batter("Darius Knox", P::RightField,  68,72,64,58,68,72),             62,50,56,48), PL::CornerOF),
            withRole(withTend(batter("Brett Walsh", P::ThirdBase,   72,68,66,60,68,64),             56,52,48,56), PL::CornerIF),
            withRole(withTend(batter("Marco Silva", P::Shortstop,   70,58,68,76,82,80),             46,52,40,58), PL::MiddleIF),
            withRole(withTend(batter("Cody Park",   P::SecondBase,  68,62,68,70,76,66),             52,50,48,56), PL::MiddleIF),
            withRole(withTend(batter("Roy Evans",   P::Catcher,     66,60,66,52,74,82),             44,58,36,62), PL::Catcher),
        },
        {
            withRole(withArsenal(pitcher("Max Rivera",   84,70,84,78, PR::Starter),
                {{PT::Fastball,80},{PT::Slider,78},{PT::Curveball,70},{PT::Changeup,66}}), PL::Ace),
            withRole(withArsenal(pitcher("Ivan Reyes",   78,72,78,74, PR::Starter),
                {{PT::Fastball,72},{PT::Cutter,70},{PT::Slider,66},{PT::Changeup,62}}), PL::Starter),
            withRole(withArsenal(pitcher("Dom Vance",    78,76,76,72, PR::Starter),
                {{PT::Fastball,70},{PT::Curveball,72},{PT::Changeup,68}}), PL::Starter),
            withRole(withArsenal(pitcher("Levi Bonn",    74,70,70,68, PR::Starter),
                {{PT::Fastball,66},{PT::Slider,66},{PT::Changeup,62}}), PL::BackOfRotation),
            withRole(withArsenal(pitcher("Ash Ford",     72,66,66,64, PR::Starter),
                {{PT::Fastball,62},{PT::Curveball,64},{PT::Changeup,58}}), PL::BackOfRotation),
        },
        {
            withRole(withArsenal(pitcher("Clay Burns",  88,66,88,44, PR::Closer),
                {{PT::Fastball,82},{PT::Slider,80},{PT::Cutter,70}}), PL::Closer),
            withRole(withArsenal(pitcher("Rex Moore",   84,68,82,50, PR::Setup),
                {{PT::Fastball,76},{PT::Slider,74},{PT::Changeup,62}}), PL::Setup),
            withRole(withArsenal(pitcher("Seth Long",   82,70,78,52, PR::Setup),
                {{PT::Fastball,74},{PT::Cutter,72},{PT::Curveball,64}}), PL::Setup),
            withRole(withArsenal(pitcher("Evan Cole",   78,66,74,60, PR::MiddleRelief),
                {{PT::Fastball,68},{PT::Slider,66},{PT::Changeup,58}}), PL::MiddleRelief),
            withRole(withArsenal(pitcher("Jace Hill",   76,68,72,62, PR::MiddleRelief),
                {{PT::Fastball,66},{PT::Curveball,66},{PT::Changeup,60}}), PL::MiddleRelief),
            withRole(withArsenal(pitcher("Hugh Carr",   72,72,66,76, PR::LongRelief),
                {{PT::Fastball,62},{PT::Changeup,66},{PT::Curveball,62}}), PL::LongRelief),
            withRole(withArsenal(pitcher("Noel Rios",   74,70,68,52, PR::MiddleRelief, TH::Left),
                {{PT::Fastball,62},{PT::Curveball,68},{PT::Changeup,64}}), PL::Specialist),
            withRole(withArsenal(pitcher("Troy Kane",   74,66,68,68, PR::LongRelief),
                {{PT::Fastball,64},{PT::Slider,62},{PT::Changeup,58}}), PL::LongRelief),
        },
        {
            withRole(withTend(batter("Lars Beck",   P::Catcher,    60,54,62,46,70,78),             40,58,36,60), PL::BackupCatcher),
            withRole(withTend(batter("Abe Cole",    P::ThirdBase,  64,58,66,66,76,72),             50,50,46,56), PL::UtilityIF),
            withRole(withTend(batter("Flash Wells", P::LeftField,  62,54,60,82,68,60, BS::Left),   46,52,42,54), PL::ExtraOF),
            withRole(withTend(batter("Gus Mora",    P::FirstBase,  72,72,66,46,54,56),             68,46,62,42), PL::PinchHitter),
            withRole(withTend(batter("Stan Cruz",   P::RightField, 68,64,64,54,56,60, BS::Left),   60,50,52,50), PL::PinchHitter),
        },
        BallparkConfig::wolfDen()
    };
}

// ── Harlem Eagles ─────────────────────────────────────────────────────────────
// チームカラー: 中堅。パワーとスピードのバランス型。特出した選手が少ない。
Team harlemEagles() {
    return Team{
        "Harlem Eagles",
        {
            withRole(withTend(batter("Remy Jones",  P::CenterField, 72,60,70,80,72,60, BS::Left),   46,54,38,62), PL::Leadoff),
            withRole(withTend(batter("Carl Diaz",   P::LeftField,   70,68,62,64,66,64),             60,48,56,46), PL::ContactHitter),
            withRole(withTend(batter("Hank Green",  P::FirstBase,   70,78,62,50,62,60),             66,46,60,42), PL::CornerIF),
            withRole(withTend(batter("Brice Allen", P::RightField,  68,80,58,54,64,70, BS::Left),   70,44,64,38), PL::CornerOF),
            withRole(withTend(batter("Fred Mann",   P::ThirdBase,   72,68,64,60,68,64),             54,52,50,52), PL::CornerIF),
            withRole(withTend(batter("Sid Lane",    P::Shortstop,   66,54,64,72,78,76),             48,52,44,56), PL::MiddleIF),
            withRole(withTend(batter("Tom Cruz",    P::SecondBase,  64,58,66,68,74,66),             52,50,50,52), PL::MiddleIF),
            withRole(withTend(batter("Ed Nash",     P::Catcher,     62,56,62,50,70,78),             42,56,38,60), PL::Catcher),
        },
        {
            withRole(withArsenal(pitcher("Roy Holt",     84,68,82,74, PR::Starter),
                {{PT::Fastball,78},{PT::Slider,74},{PT::Curveball,66},{PT::Changeup,62}}), PL::Ace),
            withRole(withArsenal(pitcher("Carl Webb",    76,70,72,70, PR::Starter),
                {{PT::Fastball,68},{PT::Changeup,68},{PT::Slider,64}}), PL::Starter),
            withRole(withArsenal(pitcher("Zane Park",    72,72,68,68, PR::Starter),
                {{PT::Fastball,64},{PT::Curveball,66},{PT::Changeup,62}}), PL::Starter),
            withRole(withArsenal(pitcher("Drew Finn",    70,64,64,64, PR::Starter),
                {{PT::Fastball,60},{PT::Slider,62},{PT::Changeup,58}}), PL::BackOfRotation),
            withRole(withArsenal(pitcher("Joe Vance",    66,62,60,62, PR::Starter),
                {{PT::Fastball,58},{PT::Changeup,58},{PT::Curveball,54}}), PL::BackOfRotation),
        },
        {
            withRole(withArsenal(pitcher("Bill Knox",   84,64,82,44, PR::Closer),
                {{PT::Fastball,76},{PT::Slider,74}}), PL::Closer),
            withRole(withArsenal(pitcher("Al Grant",    80,66,76,52, PR::Setup),
                {{PT::Fastball,70},{PT::Slider,68},{PT::Changeup,60}}), PL::Setup),
            withRole(withArsenal(pitcher("Ron Slade",   78,64,74,54, PR::Setup),
                {{PT::Fastball,68},{PT::Cutter,66},{PT::Curveball,60}}), PL::Setup),
            withRole(withArsenal(pitcher("Hal Dunn",    74,62,68,60, PR::MiddleRelief),
                {{PT::Fastball,62},{PT::Slider,62}}), PL::MiddleRelief),
            withRole(withArsenal(pitcher("Ned Fox",     72,64,66,62, PR::MiddleRelief),
                {{PT::Fastball,62},{PT::Changeup,62},{PT::Curveball,56}}), PL::MiddleRelief),
            withRole(withArsenal(pitcher("Lee Dean",    68,68,62,72, PR::LongRelief),
                {{PT::Fastball,58},{PT::Changeup,62},{PT::Curveball,58}}), PL::LongRelief),
            withRole(withArsenal(pitcher("Jay Lund",    70,66,64,52, PR::MiddleRelief, TH::Left),
                {{PT::Fastball,58},{PT::Curveball,64},{PT::Changeup,60}}), PL::Specialist),
            withRole(withArsenal(pitcher("Vick Ross",   70,62,64,66, PR::LongRelief),
                {{PT::Fastball,60},{PT::Slider,58},{PT::Changeup,54}}), PL::LongRelief),
        },
        {
            withRole(withTend(batter("Lew Bond",    P::Catcher,    54,50,58,46,66,74),             40,54,36,58), PL::BackupCatcher),
            withRole(withTend(batter("Ray Blaine",  P::Shortstop,  60,52,60,66,74,70),             48,50,44,54), PL::UtilityIF),
            withRole(withTend(batter("Hank Pena",   P::LeftField,  60,56,58,72,66,58, BS::Left),   46,52,42,52), PL::ExtraOF),
            withRole(withTend(batter("Gus Webb",    P::FirstBase,  66,64,62,48,54,56),             64,46,58,44), PL::PinchHitter),
            withRole(withTend(batter("Ed Cross",    P::RightField, 60,68,56,54,54,58, BS::Left),   68,42,62,40), PL::PinchHitter),
        },
        BallparkConfig::eaglePark()
    };
}

// ── Staten Island Foxes ───────────────────────────────────────────────────────
// チームカラー: リビルド中の弱小チーム。若手中心、投打ともに低水準。
Team statenIslandFoxes() {
    return Team{
        "Staten Island Foxes",
        {
            withRole(withTend(batter("Nick Dole",   P::CenterField, 76,62,70,76,66,56, BS::Left),   44,52,46,56), PL::Leadoff),
            withRole(withTend(batter("Wes Lyons",   P::LeftField,   74,66,66,64,62,62),             56,48,54,48), PL::ContactHitter),
            withRole(withTend(batter("Bart Shaw",   P::FirstBase,   72,74,64,50,60,58),             64,46,60,42), PL::CornerIF),
            withRole(withTend(batter("Kirk James",  P::RightField,  72,78,62,52,60,66, BS::Left),   68,42,64,40), PL::CornerOF),
            withRole(withTend(batter("Walt Holt",   P::ThirdBase,   72,70,66,58,64,62),             54,50,50,50), PL::CornerIF),
            withRole(withTend(batter("Vince Pena",  P::Shortstop,   70,58,68,70,72,70),             48,50,44,56), PL::MiddleIF),
            withRole(withTend(batter("Dale Ross",   P::SecondBase,  70,60,70,64,68,62),             52,50,50,52), PL::MiddleIF),
            withRole(withTend(batter("Hank Olson",  P::Catcher,     68,58,66,50,64,72),             42,54,40,58), PL::Catcher),
        },
        {
            withRole(withArsenal(pitcher("Ben Mack",     80,66,76,72, PR::Starter),
                {{PT::Fastball,70},{PT::Slider,66},{PT::Changeup,62},{PT::Curveball,58}}), PL::Ace),
            withRole(withArsenal(pitcher("Carl Kent",    74,62,68,66, PR::Starter),
                {{PT::Fastball,64},{PT::Changeup,62},{PT::Slider,58}}), PL::Starter),
            withRole(withArsenal(pitcher("Al Boone",     66,62,62,62, PR::Starter),
                {{PT::Fastball,58},{PT::Curveball,60},{PT::Changeup,56}}), PL::Starter),
            withRole(withArsenal(pitcher("Ted Short",    64,58,58,60, PR::Starter),
                {{PT::Fastball,54},{PT::Slider,56},{PT::Changeup,52}}), PL::BackOfRotation),
            withRole(withArsenal(pitcher("Rex Pryor",    62,56,56,58, PR::Starter),
                {{PT::Fastball,52},{PT::Changeup,54},{PT::Curveball,50}}), PL::BackOfRotation),
        },
        {
            withRole(withArsenal(pitcher("Joe Carr",    78,60,76,44, PR::Closer),
                {{PT::Fastball,70},{PT::Slider,68}}), PL::Closer),
            withRole(withArsenal(pitcher("Art Dunn",    74,62,70,50, PR::Setup),
                {{PT::Fastball,64},{PT::Slider,62},{PT::Changeup,56}}), PL::Setup),
            withRole(withArsenal(pitcher("Lee Fox",     72,60,66,52, PR::Setup),
                {{PT::Fastball,62},{PT::Cutter,60},{PT::Curveball,56}}), PL::Setup),
            withRole(withArsenal(pitcher("Ray Holt",    68,58,62,58, PR::MiddleRelief),
                {{PT::Fastball,58},{PT::Slider,58}}), PL::MiddleRelief),
            withRole(withArsenal(pitcher("Ed Grant",    66,60,60,60, PR::MiddleRelief),
                {{PT::Fastball,56},{PT::Changeup,58},{PT::Curveball,54}}), PL::MiddleRelief),
            withRole(withArsenal(pitcher("Walt Dean",   64,62,58,68, PR::LongRelief),
                {{PT::Fastball,54},{PT::Changeup,58},{PT::Curveball,54}}), PL::LongRelief),
            withRole(withArsenal(pitcher("Jim Ross",    66,62,60,50, PR::MiddleRelief, TH::Left),
                {{PT::Fastball,54},{PT::Curveball,60},{PT::Changeup,56}}), PL::Specialist),
            withRole(withArsenal(pitcher("Ned Kane",    66,58,60,64, PR::LongRelief),
                {{PT::Fastball,56},{PT::Slider,54},{PT::Changeup,52}}), PL::LongRelief),
        },
        {
            withRole(withTend(batter("Dex Cole",    P::Catcher,    56,50,58,44,62,68),             40,52,42,54), PL::BackupCatcher),
            withRole(withTend(batter("Ed Lyons",    P::SecondBase, 60,50,58,62,68,64),             48,50,46,54), PL::UtilityIF),
            withRole(withTend(batter("Jake Voss",   P::LeftField,  60,52,58,70,62,56, BS::Left),   44,52,44,52), PL::ExtraOF),
            withRole(withTend(batter("Ike Dunn",    P::FirstBase,  62,60,58,46,50,54),             62,44,58,44), PL::PinchHitter),
            withRole(withTend(batter("Clay Park",   P::RightField, 58,62,54,52,52,56, BS::Left),   66,42,62,42), PL::PinchHitter),
        },
        BallparkConfig::foxField()
    };
}

// ── Fishtown Ferals ───────────────────────────────────────────────────────────
// チームカラー: 深緑×金。パワー打線＋エース級先発。League B の最強チーム。
Team fishtownFerals() {
    return Team{
        "Fishtown Ferals",
        {
            withRole(withTend(batter("Leo Kane",   P::CenterField, 78,66,72,80,70,62, BS::Left),   44,52,38,64), PL::Leadoff),
            withRole(withTend(batter("Dre Moss",   P::LeftField,   74,80,68,60,64,64, BS::Left),   62,46,44,46), PL::PowerHitter),
            withRole(withTend(batter("Cal Ross",   P::FirstBase,   70,78,64,44,62,60, BS::Left),   66,44,62,40), PL::CornerIF),
            withRole(withTend(batter("Trey Hunt",  P::RightField,  72,76,60,56,66,70),             68,44,58,42), PL::CornerOF),
            withRole(withTend(batter("Jack Ames",  P::ThirdBase,   74,72,66,60,66,64),             58,52,52,52), PL::CornerIF),
            withRole(withTend(batter("Luis Diaz",  P::Shortstop,   72,60,68,76,80,78),             50,50,42,60), PL::MiddleIF),
            withRole(withTend(batter("Brad Cole",  P::SecondBase,  68,62,66,68,74,64),             52,50,48,54), PL::MiddleIF),
            withRole(withTend(batter("Pete Vega",  P::Catcher,     66,64,68,50,74,82, BS::Left),   42,58,36,64), PL::Catcher),
        },
        {
            withRole(withArsenal(pitcher("Jake Ford",  84,70,84,76, PR::Starter),
                {{PT::Fastball,78},{PT::Slider,76},{PT::Curveball,68},{PT::Changeup,64}}), PL::Ace),
            withRole(withArsenal(pitcher("Sam Cruz",   78,70,78,74, PR::Starter),
                {{PT::Fastball,72},{PT::Cutter,70},{PT::Changeup,66},{PT::Slider,62}}), PL::Starter),
            withRole(withArsenal(pitcher("Rich Daly",  76,74,74,72, PR::Starter),
                {{PT::Fastball,70},{PT::Curveball,70},{PT::Changeup,66}}), PL::Starter),
            withRole(withArsenal(pitcher("Norm Banks", 72,68,68,68, PR::Starter),
                {{PT::Fastball,64},{PT::Slider,64},{PT::Changeup,60}}), PL::BackOfRotation),
            withRole(withArsenal(pitcher("Ed Vann",    70,64,66,62, PR::Starter),
                {{PT::Fastball,60},{PT::Curveball,62},{PT::Changeup,56}}), PL::BackOfRotation),
        },
        {
            withRole(withArsenal(pitcher("Rex Holt",  86,64,84,44, PR::Closer),
                {{PT::Fastball,80},{PT::Slider,76},{PT::Cutter,66}}), PL::Closer),
            withRole(withArsenal(pitcher("Carl Fox",  82,66,80,50, PR::Setup),
                {{PT::Fastball,74},{PT::Slider,70},{PT::Changeup,62}}), PL::Setup),
            withRole(withArsenal(pitcher("Tony Ruiz", 80,68,76,52, PR::Setup),
                {{PT::Fastball,70},{PT::Cutter,68},{PT::Curveball,62}}), PL::Setup),
            withRole(withArsenal(pitcher("Hal Penn",  76,64,70,60, PR::MiddleRelief),
                {{PT::Fastball,64},{PT::Slider,64},{PT::Changeup,58}}), PL::MiddleRelief),
            withRole(withArsenal(pitcher("Roy Vick",  74,66,68,62, PR::MiddleRelief),
                {{PT::Fastball,62},{PT::Curveball,64},{PT::Changeup,58}}), PL::MiddleRelief),
            withRole(withArsenal(pitcher("Art Sage",  70,70,64,74, PR::LongRelief),
                {{PT::Fastball,60},{PT::Changeup,64},{PT::Curveball,60}}), PL::LongRelief),
            withRole(withArsenal(pitcher("Fran Keys", 72,68,64,52, PR::MiddleRelief, TH::Left),
                {{PT::Fastball,60},{PT::Curveball,66},{PT::Changeup,62}}), PL::Specialist),
            withRole(withArsenal(pitcher("Bill Nash",  72,64,66,66, PR::LongRelief),
                {{PT::Fastball,62},{PT::Slider,60},{PT::Changeup,56}}), PL::LongRelief),
        },
        {
            withRole(withTend(batter("Cole Lake",  P::Catcher,    60,52,60,48,68,76),             42,56,40,58), PL::BackupCatcher),
            withRole(withTend(batter("Greg Voss",  P::Shortstop,  62,54,62,70,76,72),             50,50,46,54), PL::UtilityIF),
            withRole(withTend(batter("Dan Foster", P::RightField, 64,58,60,74,68,62, BS::Left),   48,52,44,52), PL::ExtraOF),
            withRole(withTend(batter("Wade Cruz",  P::FirstBase,  72,68,66,46,54,56, BS::Left),   64,48,58,44), PL::PinchHitter),
            withRole(withTend(batter("Levi Park",  P::LeftField,  64,74,58,52,54,58),             70,44,64,40), PL::PinchHitter),
        },
        BallparkConfig::feralsField(),
        League::B
    };
}

// ── Kensington Iron ───────────────────────────────────────────────────────────
// チームカラー: チャコール×鉄青。工業系の守備堅い総合力チーム。
Team kensingtonIron() {
    return Team{
        "Kensington Iron",
        {
            withRole(withTend(batter("Vic Haro",   P::CenterField, 74,64,70,82,74,62, BS::Switch),  48,52,40,60), PL::Leadoff),
            withRole(withTend(batter("Omar Bell",  P::LeftField,   72,76,68,64,64,62, BS::Left),    62,46,46,48), PL::PowerHitter),
            withRole(withTend(batter("Glen Reed",  P::FirstBase,   68,74,64,46,62,60, BS::Left),    64,46,58,44), PL::CornerIF),
            withRole(withTend(batter("Walt Dunn",  P::RightField,  68,72,62,58,66,72),              62,46,56,46), PL::CornerOF),
            withRole(withTend(batter("Kurt Mays",  P::ThirdBase,   72,68,64,60,66,64),              56,52,50,54), PL::CornerIF),
            withRole(withTend(batter("Deon Ruiz",  P::Shortstop,   70,58,68,74,80,78),              48,52,42,58), PL::MiddleIF),
            withRole(withTend(batter("Cody Lane",  P::SecondBase,  68,60,66,70,76,66),              52,50,46,56), PL::MiddleIF),
            withRole(withTend(batter("Frank Obi",  P::Catcher,     64,60,64,50,72,80),              44,58,38,62), PL::Catcher),
        },
        {
            withRole(withArsenal(pitcher("Nate Cross", 82,70,82,76, PR::Starter),
                {{PT::Fastball,78},{PT::Slider,74},{PT::Curveball,68},{PT::Changeup,62}}), PL::Ace),
            withRole(withArsenal(pitcher("Gil Reyes",  78,70,76,72, PR::Starter),
                {{PT::Fastball,72},{PT::Cutter,68},{PT::Changeup,64},{PT::Slider,60}}), PL::Starter),
            withRole(withArsenal(pitcher("Mark Dunn",  76,74,72,70, PR::Starter),
                {{PT::Fastball,68},{PT::Curveball,70},{PT::Changeup,64}}), PL::Starter),
            withRole(withArsenal(pitcher("Stan Holt",  70,68,66,66, PR::Starter),
                {{PT::Fastball,62},{PT::Slider,62},{PT::Changeup,58}}), PL::BackOfRotation),
            withRole(withArsenal(pitcher("Dave Keys",  70,64,64,60, PR::Starter),
                {{PT::Fastball,60},{PT::Curveball,60},{PT::Changeup,54}}), PL::BackOfRotation),
        },
        {
            withRole(withArsenal(pitcher("Mick Wade",  84,62,82,44, PR::Closer),
                {{PT::Fastball,78},{PT::Slider,74},{PT::Cutter,64}}), PL::Closer),
            withRole(withArsenal(pitcher("Ray Lowe",   80,66,78,50, PR::Setup),
                {{PT::Fastball,70},{PT::Slider,68},{PT::Changeup,60}}), PL::Setup),
            withRole(withArsenal(pitcher("Pat Cross",  78,68,74,52, PR::Setup),
                {{PT::Fastball,68},{PT::Cutter,66},{PT::Curveball,60}}), PL::Setup),
            withRole(withArsenal(pitcher("Joe Finn",   74,64,68,58, PR::MiddleRelief),
                {{PT::Fastball,62},{PT::Slider,62},{PT::Changeup,56}}), PL::MiddleRelief),
            withRole(withArsenal(pitcher("Al Greene",  72,66,66,60, PR::MiddleRelief),
                {{PT::Fastball,60},{PT::Curveball,62},{PT::Changeup,56}}), PL::MiddleRelief),
            withRole(withArsenal(pitcher("Ken Morse",  70,70,62,74, PR::LongRelief),
                {{PT::Fastball,58},{PT::Changeup,62},{PT::Curveball,58}}), PL::LongRelief),
            withRole(withArsenal(pitcher("Lou Harte",  70,68,62,52, PR::MiddleRelief, TH::Left),
                {{PT::Fastball,58},{PT::Curveball,64},{PT::Changeup,60}}), PL::Specialist),
            withRole(withArsenal(pitcher("Ben Cole",   70,64,64,66, PR::LongRelief),
                {{PT::Fastball,60},{PT::Slider,58},{PT::Changeup,54}}), PL::LongRelief),
        },
        {
            withRole(withTend(batter("Rod Nash",   P::Catcher,    58,50,58,46,66,76),             40,54,40,58), PL::BackupCatcher),
            withRole(withTend(batter("Sam Blair",  P::Shortstop,  62,52,60,68,74,70),             48,50,44,54), PL::UtilityIF),
            withRole(withTend(batter("Hal Gray",   P::LeftField,  62,54,58,72,66,58, BS::Left),   46,52,44,52), PL::ExtraOF),
            withRole(withTend(batter("Leo Webb",   P::FirstBase,  68,68,64,44,52,54, BS::Left),   64,46,56,44), PL::PinchHitter),
            withRole(withTend(batter("Max Nava",   P::RightField, 62,72,56,50,52,56),             68,42,62,40), PL::PinchHitter),
        },
        BallparkConfig::ironWorks(),
        League::B
    };
}

// ── Germantown Colonials ──────────────────────────────────────────────────────
// チームカラー: 紺×クリーム。クラシックなコンタクト野球、堅実なローテ。
Team germantownColonials() {
    return Team{
        "Germantown Colonials",
        {
            withRole(withTend(batter("Eli Stone",  P::CenterField, 74,62,70,78,68,58, BS::Left),   44,54,38,62), PL::Leadoff),
            withRole(withTend(batter("Clem Hunt",  P::LeftField,   74,70,68,64,62,60, BS::Left),   56,50,44,54), PL::ContactHitter),
            withRole(withTend(batter("Phil Ross",  P::FirstBase,   70,74,62,50,60,58),             64,46,56,44), PL::CornerIF),
            withRole(withTend(batter("Russ Page",  P::RightField,  66,74,58,56,62,68),             66,44,60,42), PL::PowerHitter),
            withRole(withTend(batter("Todd Ames",  P::ThirdBase,   72,70,62,60,64,62),             58,52,50,50), PL::CornerIF),
            withRole(withTend(batter("Nels Diaz",  P::Shortstop,   70,58,66,72,78,76),             48,52,42,58), PL::MiddleIF),
            withRole(withTend(batter("Bert Lane",  P::SecondBase,  68,60,64,68,74,64),             50,50,46,54), PL::MiddleIF),
            withRole(withTend(batter("Wes Obi",    P::Catcher,     64,62,62,52,72,78),             44,56,38,60), PL::Catcher),
        },
        {
            withRole(withArsenal(pitcher("Phil Nash",  78,70,78,74, PR::Starter),
                {{PT::Fastball,72},{PT::Slider,70},{PT::Changeup,66},{PT::Curveball,62}}), PL::Ace),
            withRole(withArsenal(pitcher("Bert Cole",  76,70,74,70, PR::Starter),
                {{PT::Fastball,68},{PT::Cutter,66},{PT::Changeup,64},{PT::Slider,58}}), PL::Starter),
            withRole(withArsenal(pitcher("Clem Ford",  72,70,70,68, PR::Starter),
                {{PT::Fastball,64},{PT::Curveball,66},{PT::Changeup,62}}), PL::Starter),
            withRole(withArsenal(pitcher("Walt Keys",  70,66,64,64, PR::Starter),
                {{PT::Fastball,60},{PT::Slider,60},{PT::Changeup,56}}), PL::BackOfRotation),
            withRole(withArsenal(pitcher("Fred Vann",  68,62,62,60, PR::Starter),
                {{PT::Fastball,58},{PT::Curveball,58},{PT::Changeup,54}}), PL::BackOfRotation),
        },
        {
            withRole(withArsenal(pitcher("Sid Park",  80,62,78,44, PR::Closer),
                {{PT::Fastball,74},{PT::Slider,70},{PT::Cutter,62}}), PL::Closer),
            withRole(withArsenal(pitcher("Len Wade",  78,64,74,50, PR::Setup),
                {{PT::Fastball,68},{PT::Slider,66},{PT::Changeup,58}}), PL::Setup),
            withRole(withArsenal(pitcher("Tim Holt",  74,66,72,52, PR::Setup),
                {{PT::Fastball,66},{PT::Cutter,64},{PT::Curveball,58}}), PL::Setup),
            withRole(withArsenal(pitcher("Jim Finn",  72,62,66,58, PR::MiddleRelief),
                {{PT::Fastball,60},{PT::Slider,60},{PT::Changeup,54}}), PL::MiddleRelief),
            withRole(withArsenal(pitcher("Don Ross",  70,64,64,60, PR::MiddleRelief),
                {{PT::Fastball,58},{PT::Curveball,60},{PT::Changeup,54}}), PL::MiddleRelief),
            withRole(withArsenal(pitcher("Ted Vick",  68,68,60,72, PR::LongRelief),
                {{PT::Fastball,56},{PT::Changeup,60},{PT::Curveball,56}}), PL::LongRelief),
            withRole(withArsenal(pitcher("Roy Sage",  68,66,60,52, PR::MiddleRelief, TH::Left),
                {{PT::Fastball,56},{PT::Curveball,62},{PT::Changeup,58}}), PL::Specialist),
            withRole(withArsenal(pitcher("Sam Park",  68,62,62,64, PR::LongRelief),
                {{PT::Fastball,58},{PT::Slider,56},{PT::Changeup,52}}), PL::LongRelief),
        },
        {
            withRole(withTend(batter("Ned Cole",   P::Catcher,    56,48,56,44,64,74),             40,54,42,56), PL::BackupCatcher),
            withRole(withTend(batter("Al Blair",   P::Shortstop,  60,50,60,66,72,68),             48,50,44,52), PL::UtilityIF),
            withRole(withTend(batter("Rex Gray",   P::LeftField,  60,52,58,70,64,56, BS::Left),   46,52,46,50), PL::ExtraOF),
            withRole(withTend(batter("Dan Webb",   P::FirstBase,  66,66,62,42,50,52, BS::Left),   62,46,56,42), PL::PinchHitter),
            withRole(withTend(batter("Cal Nava",   P::RightField, 60,70,54,48,50,54),             66,42,60,38), PL::PinchHitter),
        },
        BallparkConfig::colonialField(),
        League::B
    };
}

// ── Manayunk Runners ──────────────────────────────────────────────────────────
// チームカラー: 森緑×オレンジ。足と出塁率で点を稼ぐスピード型。
Team manayunkRunners() {
    return Team{
        "Manayunk Runners",
        {
            withRole(withTend(batter("Jace Ray",   P::CenterField, 76,62,74,86,74,60, BS::Left),   42,56,36,66), PL::Leadoff),
            withRole(withTend(batter("Arlo Keen",  P::LeftField,   74,64,70,80,68,60, BS::Left),   46,54,38,62), PL::ContactHitter),
            withRole(withTend(batter("Milo Cruz",  P::Shortstop,   72,58,68,80,82,80),             48,52,42,60), PL::MiddleIF),
            withRole(withTend(batter("Oren Page",  P::RightField,  66,72,60,62,64,70),             64,44,58,44), PL::CornerOF),
            withRole(withTend(batter("Hugo Reed",  P::FirstBase,   68,68,62,52,60,58, BS::Left),   62,46,54,46), PL::CornerIF),
            withRole(withTend(batter("Drew Lane",  P::ThirdBase,   68,64,62,68,68,64),             56,50,50,52), PL::CornerIF),
            withRole(withTend(batter("Cole Mays",  P::SecondBase,  68,56,64,74,76,68),             50,52,44,58), PL::MiddleIF),
            withRole(withTend(batter("Kurt Obi",   P::Catcher,     62,58,62,52,70,76),             44,56,40,58), PL::Catcher),
        },
        {
            withRole(withArsenal(pitcher("Mel Ross",  78,70,76,74, PR::Starter),
                {{PT::Fastball,70},{PT::Cutter,68},{PT::Changeup,66},{PT::Slider,62}}), PL::Ace),
            withRole(withArsenal(pitcher("Ike Holt",  74,70,74,72, PR::Starter),
                {{PT::Fastball,68},{PT::Slider,66},{PT::Changeup,62},{PT::Curveball,60}}), PL::Starter),
            withRole(withArsenal(pitcher("Don Keys",  72,70,70,68, PR::Starter),
                {{PT::Fastball,64},{PT::Curveball,66},{PT::Changeup,62}}), PL::Starter),
            withRole(withArsenal(pitcher("Lee Wade",  70,66,66,64, PR::Starter),
                {{PT::Fastball,62},{PT::Slider,62},{PT::Changeup,56}}), PL::BackOfRotation),
            withRole(withArsenal(pitcher("Ray Ford",  68,62,62,60, PR::Starter),
                {{PT::Fastball,58},{PT::Curveball,58},{PT::Changeup,52}}), PL::BackOfRotation),
        },
        {
            withRole(withArsenal(pitcher("Bud Finn",  80,62,78,44, PR::Closer),
                {{PT::Fastball,74},{PT::Slider,70},{PT::Cutter,62}}), PL::Closer),
            withRole(withArsenal(pitcher("Curt Nash", 78,64,74,50, PR::Setup),
                {{PT::Fastball,68},{PT::Slider,66},{PT::Changeup,58}}), PL::Setup),
            withRole(withArsenal(pitcher("Ed Lowe",   74,66,70,52, PR::Setup),
                {{PT::Fastball,64},{PT::Cutter,62},{PT::Curveball,58}}), PL::Setup),
            withRole(withArsenal(pitcher("Al Cross",  72,62,66,58, PR::MiddleRelief),
                {{PT::Fastball,60},{PT::Slider,60},{PT::Changeup,54}}), PL::MiddleRelief),
            withRole(withArsenal(pitcher("Joe Vick",  70,64,64,60, PR::MiddleRelief),
                {{PT::Fastball,58},{PT::Curveball,60},{PT::Changeup,54}}), PL::MiddleRelief),
            withRole(withArsenal(pitcher("Hal Sage",  68,68,60,72, PR::LongRelief),
                {{PT::Fastball,56},{PT::Changeup,60},{PT::Curveball,56}}), PL::LongRelief),
            withRole(withArsenal(pitcher("Dan Cole",  68,66,60,52, PR::MiddleRelief, TH::Left),
                {{PT::Fastball,56},{PT::Curveball,62},{PT::Changeup,58}}), PL::Specialist),
            withRole(withArsenal(pitcher("Ned Penn",  68,62,62,64, PR::LongRelief),
                {{PT::Fastball,58},{PT::Slider,56},{PT::Changeup,52}}), PL::LongRelief),
        },
        {
            withRole(withTend(batter("Art Cole",   P::Catcher,    54,46,54,50,62,72),             40,54,42,54), PL::BackupCatcher),
            withRole(withTend(batter("Rex Blair",  P::Shortstop,  60,50,60,70,74,68),             48,50,44,52), PL::UtilityIF),
            withRole(withTend(batter("Jay Gray",   P::LeftField,  62,52,58,78,68,58, BS::Left),   44,52,42,54), PL::ExtraOF),
            withRole(withTend(batter("Lon Webb",   P::FirstBase,  66,66,62,46,50,52, BS::Left),   62,46,56,42), PL::PinchHitter),
            withRole(withTend(batter("Van Nava",   P::RightField, 60,68,54,50,50,54),             66,42,60,38), PL::PinchHitter),
        },
        BallparkConfig::canalPark(),
        League::B
    };
}

// ── Fairmount Rams ────────────────────────────────────────────────────────────
// チームカラー: 深紅×銀。若手主体の発展途上チーム。投手陣は堅め。
Team fairmountRams() {
    return Team{
        "Fairmount Rams",
        {
            withRole(withTend(batter("Finn Ray",   P::CenterField, 76,62,70,74,66,56, BS::Left),   44,54,40,60), PL::Leadoff),
            withRole(withTend(batter("Amos Hunt",  P::LeftField,   76,68,68,68,60,58, BS::Left),   54,50,46,52), PL::ContactHitter),
            withRole(withTend(batter("Lyle Ross",  P::FirstBase,   72,74,62,54,58,56),             62,46,56,44), PL::CornerIF),
            withRole(withTend(batter("Kirk Page",  P::RightField,  70,74,60,58,60,66),             66,44,60,42), PL::PowerHitter),
            withRole(withTend(batter("Neil Ames",  P::ThirdBase,   74,70,64,60,62,62),             58,50,50,50), PL::CornerIF),
            withRole(withTend(batter("Zeke Diaz",  P::Shortstop,   72,60,66,68,76,74),             48,52,44,56), PL::MiddleIF),
            withRole(withTend(batter("Boyd Lane",  P::SecondBase,  70,60,64,64,72,62),             50,50,46,52), PL::MiddleIF),
            withRole(withTend(batter("Ivan Obi",   P::Catcher,     68,62,64,52,70,76),             44,54,40,58), PL::Catcher),
        },
        {
            withRole(withArsenal(pitcher("Hank Vann", 76,68,76,72, PR::Starter),
                {{PT::Fastball,70},{PT::Slider,68},{PT::Curveball,64},{PT::Changeup,60}}), PL::Ace),
            withRole(withArsenal(pitcher("Carl Wade", 72,68,70,68, PR::Starter),
                {{PT::Fastball,64},{PT::Cutter,64},{PT::Changeup,60},{PT::Slider,56}}), PL::Starter),
            withRole(withArsenal(pitcher("Paul Ford", 70,66,68,64, PR::Starter),
                {{PT::Fastball,62},{PT::Curveball,62},{PT::Changeup,58}}), PL::Starter),
            withRole(withArsenal(pitcher("Kirk Nash", 66,62,62,60, PR::Starter),
                {{PT::Fastball,58},{PT::Slider,58},{PT::Changeup,52}}), PL::BackOfRotation),
            withRole(withArsenal(pitcher("Glen Keys", 64,60,60,56, PR::Starter),
                {{PT::Fastball,54},{PT::Curveball,54},{PT::Changeup,50}}), PL::BackOfRotation),
        },
        {
            withRole(withArsenal(pitcher("Rex Penn",  76,60,74,44, PR::Closer),
                {{PT::Fastball,70},{PT::Slider,66},{PT::Cutter,58}}), PL::Closer),
            withRole(withArsenal(pitcher("Gus Wade",  72,62,70,50, PR::Setup),
                {{PT::Fastball,64},{PT::Slider,62},{PT::Changeup,56}}), PL::Setup),
            withRole(withArsenal(pitcher("Les Holt",  70,64,66,52, PR::Setup),
                {{PT::Fastball,62},{PT::Cutter,60},{PT::Curveball,54}}), PL::Setup),
            withRole(withArsenal(pitcher("Abe Finn",  68,62,62,58, PR::MiddleRelief),
                {{PT::Fastball,58},{PT::Slider,58},{PT::Changeup,52}}), PL::MiddleRelief),
            withRole(withArsenal(pitcher("Zeb Ross",  66,62,62,58, PR::MiddleRelief),
                {{PT::Fastball,56},{PT::Curveball,58},{PT::Changeup,52}}), PL::MiddleRelief),
            withRole(withArsenal(pitcher("Jed Vick",  64,66,58,72, PR::LongRelief),
                {{PT::Fastball,54},{PT::Changeup,58},{PT::Curveball,54}}), PL::LongRelief),
            withRole(withArsenal(pitcher("Roy Cole",  66,64,58,52, PR::MiddleRelief, TH::Left),
                {{PT::Fastball,54},{PT::Curveball,60},{PT::Changeup,56}}), PL::Specialist),
            withRole(withArsenal(pitcher("Eli Park",  64,60,60,62, PR::LongRelief),
                {{PT::Fastball,54},{PT::Slider,54},{PT::Changeup,50}}), PL::LongRelief),
        },
        {
            withRole(withTend(batter("Cy Nash",    P::Catcher,    52,46,52,44,60,70),             40,54,42,54), PL::BackupCatcher),
            withRole(withTend(batter("Al Voss",    P::Shortstop,  58,48,56,62,68,66),             46,50,46,50), PL::UtilityIF),
            withRole(withTend(batter("Ray Gray",   P::LeftField,  58,50,54,68,62,54, BS::Left),   44,52,46,48), PL::ExtraOF),
            withRole(withTend(batter("Lon Webb",   P::FirstBase,  62,62,58,42,48,50),             62,44,58,42), PL::PinchHitter),
            withRole(withTend(batter("Pete Nava",  P::RightField, 56,64,50,46,48,52),             66,42,62,38), PL::PinchHitter),
        },
        BallparkConfig::fairmountField(),
        League::B
    };
}

// ── South Philly Stallions ────────────────────────────────────────────────────
// チームカラー: ミッドナイト紺×赤。再建中。Rocky 精神で戦う若手集団。
Team southPhillyStallions() {
    return Team{
        "South Philly Stallions",
        {
            withRole(withTend(batter("Bo Kane",    P::CenterField, 78,62,74,74,62,54, BS::Left),   44,52,42,62), PL::Leadoff),
            withRole(withTend(batter("Cass Bell",  P::LeftField,   76,66,72,68,58,56, BS::Left),   52,50,46,54), PL::ContactHitter),
            withRole(withTend(batter("Wyn Reed",   P::FirstBase,   74,74,68,54,56,54),             60,46,54,48), PL::CornerIF),
            withRole(withTend(batter("Duke Page",  P::RightField,  74,76,66,58,58,64),             66,44,60,44), PL::PowerHitter),
            withRole(withTend(batter("Beau Ames",  P::ThirdBase,   76,68,68,60,60,60),             58,48,50,52), PL::CornerIF),
            withRole(withTend(batter("Remy Diaz",  P::Shortstop,   74,58,70,66,72,70),             48,50,44,56), PL::MiddleIF),
            withRole(withTend(batter("Otis Lane",  P::SecondBase,  74,60,68,64,68,60),             50,50,46,54), PL::MiddleIF),
            withRole(withTend(batter("Vern Obi",   P::Catcher,     72,62,68,50,66,74),             44,52,40,60), PL::Catcher),
        },
        {
            withRole(withArsenal(pitcher("Buck Nash",  74,66,72,68, PR::Starter),
                {{PT::Fastball,68},{PT::Slider,66},{PT::Curveball,60},{PT::Changeup,58}}), PL::Ace),
            withRole(withArsenal(pitcher("Finn Ross",  70,66,68,64, PR::Starter),
                {{PT::Fastball,62},{PT::Cutter,62},{PT::Changeup,58},{PT::Slider,54}}), PL::Starter),
            withRole(withArsenal(pitcher("Ward Keys",  68,62,64,62, PR::Starter),
                {{PT::Fastball,60},{PT::Curveball,58},{PT::Changeup,54}}), PL::Starter),
            withRole(withArsenal(pitcher("Hal Holt",   64,58,58,58, PR::Starter),
                {{PT::Fastball,54},{PT::Slider,54},{PT::Changeup,50}}), PL::BackOfRotation),
            withRole(withArsenal(pitcher("Stu Ford",   62,56,56,54, PR::Starter),
                {{PT::Fastball,50},{PT::Curveball,52},{PT::Changeup,48}}), PL::BackOfRotation),
        },
        {
            withRole(withArsenal(pitcher("Zeb Penn",  72,60,70,42, PR::Closer),
                {{PT::Fastball,66},{PT::Slider,62},{PT::Cutter,56}}), PL::Closer),
            withRole(withArsenal(pitcher("Len Cole",  68,60,66,48, PR::Setup),
                {{PT::Fastball,60},{PT::Slider,58},{PT::Changeup,52}}), PL::Setup),
            withRole(withArsenal(pitcher("Phil Lowe", 66,62,62,50, PR::Setup),
                {{PT::Fastball,58},{PT::Cutter,58},{PT::Curveball,52}}), PL::Setup),
            withRole(withArsenal(pitcher("Cal Finn",  64,60,58,56, PR::MiddleRelief),
                {{PT::Fastball,54},{PT::Slider,54},{PT::Changeup,50}}), PL::MiddleRelief),
            withRole(withArsenal(pitcher("Ned Ross",  62,60,58,56, PR::MiddleRelief),
                {{PT::Fastball,52},{PT::Curveball,54},{PT::Changeup,50}}), PL::MiddleRelief),
            withRole(withArsenal(pitcher("Joe Vann",  60,64,54,70, PR::LongRelief),
                {{PT::Fastball,50},{PT::Changeup,56},{PT::Curveball,52}}), PL::LongRelief),
            withRole(withArsenal(pitcher("Sam Hunt",  64,62,56,50, PR::MiddleRelief, TH::Left),
                {{PT::Fastball,52},{PT::Curveball,58},{PT::Changeup,54}}), PL::Specialist),
            withRole(withArsenal(pitcher("Eli Wade",  60,58,56,60, PR::LongRelief),
                {{PT::Fastball,50},{PT::Slider,50},{PT::Changeup,48}}), PL::LongRelief),
        },
        {
            withRole(withTend(batter("Ike Nash",   P::Catcher,    58,48,60,42,58,66),             40,52,42,54), PL::BackupCatcher),
            withRole(withTend(batter("Gus Blair",  P::Shortstop,  62,50,60,58,64,62),             46,50,44,52), PL::UtilityIF),
            withRole(withTend(batter("Bo Gray",    P::LeftField,  62,50,60,66,60,52, BS::Left),   42,52,42,50), PL::ExtraOF),
            withRole(withTend(batter("Roy Webb",   P::FirstBase,  64,62,60,40,46,48),             60,44,54,44), PL::PinchHitter),
            withRole(withTend(batter("Van Cruz",   P::RightField, 60,64,54,44,46,50),             66,40,60,42), PL::PinchHitter),
        },
        BallparkConfig::stallionsField(),
        League::B
    };
}

} // namespace

// ── Public ────────────────────────────────────────────────────────────────────

std::vector<Team> allTeams() {
    return {
        // League A — New York Metro
        newarkKnights(),
        queensTitans(),
        brooklynHammers(),
        bronxWolves(),
        harlemEagles(),
        statenIslandFoxes(),
        // League B — Philadelphia Districts
        fishtownFerals(),
        kensingtonIron(),
        germantownColonials(),
        manayunkRunners(),
        fairmountRams(),
        southPhillyStallions(),
    };
}

} // namespace joji
