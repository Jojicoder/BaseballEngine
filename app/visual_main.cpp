#include "GameEngine.h"
#include "Teams.h"

#include <SFML/Graphics.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace {

constexpr float WindowWidth = 1280.0f;
constexpr float WindowHeight = 820.0f;
constexpr float FieldScale = 1.28f;
const sf::Vector2f HomePlate{450.0f, 720.0f};
const sf::Color Grass{42, 130, 76};
const sf::Color Dirt{174, 116, 64};
const sf::Color Chalk{238, 232, 216};
const sf::Color Ink{28, 32, 35};

enum class VisualMode {
    Field,
    Pitch
};

enum class BatterPose {
    Stance,       // take / called strike / ball
    Impact,       // contact / foul
    FollowThrough // swinging strike (miss)
};

sf::Vector2f fieldPoint(double xFeet, double yFeet) {
    return {
        HomePlate.x + static_cast<float>(xFeet) * FieldScale,
        HomePlate.y - static_cast<float>(yFeet) * FieldScale
    };
}

sf::Vector2f pitchPoint(const joji::AnimationPoint& point) {
    const float locationScale = 12.0f;
    const float heightLift = static_cast<float>(point.z) * 1.8f;
    return {
        HomePlate.x + static_cast<float>(point.x) * FieldScale * locationScale,
        HomePlate.y - static_cast<float>(point.y) * FieldScale - heightLift
    };
}

// ── OPS / BABIP 計算ヘルパー ───────────────────────────────────────────────

double calcOBP(const joji::PlayerBoxScore& p) {
    const int denom = p.atBats + p.walks + p.hitByPitch + p.sacFlies;
    return denom > 0 ? static_cast<double>(p.hits + p.walks + p.hitByPitch) / denom : 0.0;
}
double calcSLG(const joji::PlayerBoxScore& p) {
    return p.atBats > 0 ? static_cast<double>(p.totalBases) / p.atBats : 0.0;
}
double calcOPS(const joji::PlayerBoxScore& p) { return calcOBP(p) + calcSLG(p); }
double calcBABIP(const joji::PlayerBoxScore& p) {
    const int denom = p.atBats - p.strikeouts - p.homeRuns + p.sacFlies;
    return denom > 0 ? static_cast<double>(p.hits - p.homeRuns) / denom : 0.0;
}

// ── 守備位置略称 ───────────────────────────────────────────────────────────
std::string positionAbbr(joji::Position pos) {
    switch (pos) {
        case joji::Position::Pitcher:     return "P";
        case joji::Position::Catcher:     return "C";
        case joji::Position::FirstBase:   return "1B";
        case joji::Position::SecondBase:  return "2B";
        case joji::Position::ThirdBase:   return "3B";
        case joji::Position::Shortstop:   return "SS";
        case joji::Position::LeftField:   return "LF";
        case joji::Position::CenterField: return "CF";
        case joji::Position::RightField:  return "RF";
    }
    return "??";
}

sf::Color outcomeColor(joji::AtBatResultType type) {
    switch (type) {
        case joji::AtBatResultType::HomeRun:
            return {243, 185, 54};
        case joji::AtBatResultType::Triple:
            return {54, 151, 230};
        case joji::AtBatResultType::Double:
            return {235, 116, 48};
        case joji::AtBatResultType::Single:
            return {80, 190, 100};
        case joji::AtBatResultType::Walk:
        case joji::AtBatResultType::HitByPitch:
        case joji::AtBatResultType::Error:
            return {153, 116, 228};
        case joji::AtBatResultType::StrikeOut:
        case joji::AtBatResultType::GroundOut:
        case joji::AtBatResultType::FlyOut:
        case joji::AtBatResultType::InfieldFly:
            return {220, 72, 66};
        case joji::AtBatResultType::FielderChoice:
            return {153, 116, 228};
        case joji::AtBatResultType::SacrificeBunt:
        case joji::AtBatResultType::SqueezeBunt:
            return {150, 210, 240}; // light blue
        case joji::AtBatResultType::BuntSingle:
        case joji::AtBatResultType::BuntFielderChoice:
            return {80, 190, 100};  // green
    }
    return sf::Color::White;
}

bool isGroundTrajectory(const joji::PlayResult& play,
                        const joji::AnimationPlan& plan) {
    const std::string& classification = plan.battedBall.classification;
    return play.type == joji::AtBatResultType::GroundOut
        || play.battedBall.launchAngle < 5.0
        || classification.find("ground") != std::string::npos;
}

bool isAirTrajectory(const joji::PlayResult& play,
                     const joji::AnimationPlan& plan) {
    if (isGroundTrajectory(play, plan)) return false;
    const std::string& cls = plan.battedBall.classification;
    // "line drive", "low liner", "fly ball", "pop fly", "deep drive", "warning track" など
    for (const char* kw : {"line", "liner", "fly", "drive", "pop"}) {
        if (cls.find(kw) != std::string::npos) return true;
    }
    return plan.battedBall.maxHeight >= 18.0 || play.battedBall.launchAngle >= 12.0;
}

// 分類ごとに視覚リフト倍率を返す: ラインドライブは控えめ、高弾道フライは大きく
float airLiftFactor(const joji::AnimationPlan& plan) {
    const std::string& cls = plan.battedBall.classification;
    if (cls.find("low liner") != std::string::npos) return 0.04f;
    if (cls.find("line drive") != std::string::npos) return 0.08f;
    if (cls.find("pop") != std::string::npos)        return 0.20f;
    return 0.16f; // fly ball / deep drive / warning track
}

sf::Vector2f battedBallDrawPoint(const joji::AnimationPoint& point,
                                  bool airTrajectory,
                                  float liftFactor = 0.16f) {
    const double visualLift = airTrajectory
        ? std::clamp(point.z, 0.0, 180.0) * static_cast<double>(liftFactor)
        : 0.0;
    return fieldPoint(point.x, point.y + visualLift);
}


std::string halfLabel(const joji::GameState& state) {
    return std::string(state.isTop ? "Top " : "Bottom ") + std::to_string(state.inning);
}

std::string countText(const joji::Count& count) {
    return std::to_string(count.balls) + "-" + std::to_string(count.strikes);
}

std::string fitLine(std::string value, std::size_t maxLength) {
    if (value.size() <= maxLength) {
        return value;
    }
    return value.substr(0, maxLength - 3) + "...";
}

void drawText(sf::RenderWindow& window,
              const sf::Font& font,
              const std::string& value,
              sf::Vector2f position,
              unsigned int size,
              sf::Color color,
              std::uint32_t style = sf::Text::Regular) {
    sf::Text text(font, value, size);
    text.setFillColor(color);
    text.setStyle(style);
    text.setPosition(position);
    window.draw(text);
}

void drawPanel(sf::RenderWindow& window) {
    sf::RectangleShape panel({350.0f, WindowHeight});
    panel.setPosition({930.0f, 0.0f});
    panel.setFillColor({246, 243, 232});
    window.draw(panel);

    sf::RectangleShape rule({2.0f, WindowHeight});
    rule.setPosition({928.0f, 0.0f});
    rule.setFillColor({32, 84, 60});
    window.draw(rule);
}

// ── ラインスコア (イニング別得点) ─────────────────────────────────────────

void drawLineScore(sf::RenderWindow& window,
                   const sf::Font& font,
                   const joji::GameEngine& engine,
                   bool complete) {
    const auto& state      = engine.state();
    const auto& awayLS     = engine.awayLineScore();
    const auto& homeLS     = engine.homeLineScore();
    const int   currInning = state.inning;

    // 表示イニング: 最大9（延長は最後の方を表示）
    constexpr int kDisplayInnings = 9;
    const int lastInn = std::max(kDisplayInnings, currInning);
    const int firstInn = std::max(1, lastInn - kDisplayInnings + 1);

    constexpr float panelX  = 935.0f;
    constexpr float startY  = 118.0f;
    constexpr float colW    = 22.0f;
    constexpr float labelW  = 80.0f;


    // チーム名列
    const sf::Color headerCol{90, 100, 96};
    const sf::Color awayCol = (complete && state.awayScore > state.homeScore)
        ? sf::Color{243, 185, 54} : sf::Color{42, 48, 45};
    const sf::Color homeCol = (complete && state.homeScore > state.awayScore)
        ? sf::Color{243, 185, 54} : sf::Color{42, 48, 45};

    // header row: inning numbers
    drawText(window, font, "", {panelX, startY}, 11, headerCol);
    for (int inn = firstInn; inn <= firstInn + kDisplayInnings - 1; ++inn) {
        const float x = panelX + labelW + (inn - firstInn) * colW;
        drawText(window, font, std::to_string(inn), {x, startY}, 10, headerCol);
    }
    // R H E headers (right-aligned)
    const float rheX = panelX + labelW + kDisplayInnings * colW + 4.0f;
    drawText(window, font, "R",  {rheX,            startY}, 10, headerCol);
    drawText(window, font, "H",  {rheX + colW,     startY}, 10, headerCol);
    drawText(window, font, "E",  {rheX + colW * 2, startY}, 10, headerCol);

    const float awayY = startY + 14.0f;
    const float homeY = startY + 26.0f;

    auto abbr3 = [](const std::string& name) {
        return name.size() >= 3 ? name.substr(0, 3) : name;
    };

    drawText(window, font, abbr3(engine.awayTeamName()), {panelX, awayY}, 11, awayCol, sf::Text::Bold);
    drawText(window, font, abbr3(engine.homeTeamName()), {panelX, homeY}, 11, homeCol, sf::Text::Bold);

    for (int i = 0; i < kDisplayInnings; ++i) {
        const int inn = firstInn + i;
        const float x = panelX + labelW + i * colW;
        const std::size_t idx = static_cast<std::size_t>(inn - 1);

        // Away
        {
            std::string val = "-";
            sf::Color col{130, 140, 136};
            if (idx < awayLS.size() && awayLS[idx] >= 0) {
                val = std::to_string(awayLS[idx]);
                col = awayLS[idx] > 0 ? sf::Color{80, 190, 100} : sf::Color{80, 88, 84};
            } else if (!complete && inn == currInning && state.isTop) {
                val = "·"; col = {200, 200, 200};
            }
            drawText(window, font, val, {x, awayY}, 11, col);
        }
        // Home
        {
            std::string val = "-";
            sf::Color col{130, 140, 136};
            if (idx < homeLS.size() && homeLS[idx] >= 0) {
                val = std::to_string(homeLS[idx]);
                col = homeLS[idx] > 0 ? sf::Color{80, 190, 100} : sf::Color{80, 88, 84};
            } else if (!complete && inn == currInning && !state.isTop) {
                val = "·"; col = {200, 200, 200};
            }
            drawText(window, font, val, {x, homeY}, 11, col);
        }
    }

    // R H E totals
    const auto& abs = engine.awayBoxScore();
    const auto& hbs = engine.homeBoxScore();
    drawText(window, font, std::to_string(state.awayScore), {rheX,            awayY}, 12, awayCol, sf::Text::Bold);
    drawText(window, font, std::to_string(abs.hits),        {rheX + colW,     awayY}, 11, awayCol);
    drawText(window, font, std::to_string(abs.errors),      {rheX + colW * 2, awayY}, 11,
             abs.errors > 0 ? sf::Color{220, 72, 66} : awayCol);
    drawText(window, font, std::to_string(state.homeScore), {rheX,            homeY}, 12, homeCol, sf::Text::Bold);
    drawText(window, font, std::to_string(hbs.hits),        {rheX + colW,     homeY}, 11, homeCol);
    drawText(window, font, std::to_string(hbs.errors),      {rheX + colW * 2, homeY}, 11,
             hbs.errors > 0 ? sf::Color{220, 72, 66} : homeCol);
}

// ── 打順ストリップ (フィールド下部) ──────────────────────────────────────────

void drawBattingOrderStrip(sf::RenderWindow& window,
                           const sf::Font& font,
                           const joji::GameEngine& engine) {
    const bool isTop    = engine.state().isTop;
    const auto& lineup  = isTop ? engine.awayLineup() : engine.homeLineup();
    const int   curIdx  = isTop ? engine.awayBattingIndex() : engine.homeBattingIndex();
    const int   n       = static_cast<int>(lineup.size());
    if (n == 0) return;

    constexpr float baseX = 34.0f;
    constexpr float baseY = 744.0f;
    constexpr float slotW = 120.0f;

    // 現在 + 次の 6 人まで表示
    const int show = std::min(7, n);
    for (int i = 0; i < show; ++i) {
        const int idx = (curIdx + i) % n;
        const auto& p = lineup[static_cast<std::size_t>(idx)];
        const float x = baseX + i * slotW;
        const bool isCurrent = (i == 0);
        const sf::Color nameCol = isCurrent ? sf::Color{243, 185, 54} : sf::Color{200, 205, 202};
        const sf::Color numCol  = isCurrent ? sf::Color{243, 185, 54} : sf::Color{100, 108, 105};

        if (isCurrent) {
            sf::RectangleShape bg({slotW - 4.0f, 28.0f});
            bg.setPosition({x, baseY - 4.0f});
            bg.setFillColor({40, 52, 44, 200});
            bg.setOutlineColor({243, 185, 54, 160});
            bg.setOutlineThickness(1.0f);
            window.draw(bg);
        }

        drawText(window, font, std::to_string(idx + 1), {x + 2.0f, baseY - 2.0f}, 9, numCol);
        drawText(window, font,
                 p.name.size() > 10 ? p.name.substr(0, 10) : p.name,
                 {x + 2.0f, baseY + 8.0f}, 12, nameCol,
                 isCurrent ? sf::Text::Bold : sf::Text::Regular);
    }
    // チーム名ラベル
    const std::string tmLabel = (isTop ? engine.awayTeamName() : engine.homeTeamName())
                               + " batting";
    drawText(window, font, tmLabel, {baseX, baseY - 16.0f}, 11, {90, 100, 96});
}

// ── アーセナル表示 (右パネル内、投手情報の右側) ──────────────────────────────

void drawPitcherArsenal(sf::RenderWindow& window,
                        const sf::Font& font,
                        const std::map<std::string, int>& arsenal,
                        float x, float y) {
    if (arsenal.empty()) return;

    // 球数の多い順にソート
    std::vector<std::pair<int, std::string>> sorted;
    sorted.reserve(arsenal.size());
    for (const auto& [type, cnt] : arsenal) sorted.push_back({cnt, type});
    std::sort(sorted.rbegin(), sorted.rend());

    const int total = [&] { int s = 0; for (auto& kv : arsenal) s += kv.second; return s; }();

    for (std::size_t i = 0; i < sorted.size(); ++i) {
        const std::string abbr = sorted[i].second.size() >= 2
            ? sorted[i].second.substr(0, 2) : sorted[i].second;
        const float pct = total > 0 ? 100.0f * sorted[i].first / total : 0.0f;
        std::ostringstream line;
        line << abbr << " " << sorted[i].first
             << " " << static_cast<int>(std::round(pct)) << "%";
        drawText(window, font, line.str(),
                 {x + i * 72.0f, y}, 11, sf::Color{130, 160, 145});
    }
}

// 物理フェンスと同じ極座標モデルで (x,y) を計算
// angle_deg: CF=0, コーナー=±45 (右=正)
sf::Vector2f fencePoint(double angle_deg, const joji::BallparkConfig& bp) {
    const double a   = std::abs(angle_deg);
    const double r   = a <= 25.0
        ? bp.centerFieldFenceFeet + (bp.gapFenceFeet - bp.centerFieldFenceFeet) * (a / 25.0)
        : bp.gapFenceFeet         + (bp.cornerFenceFeet - bp.gapFenceFeet)      * ((a - 25.0) / 20.0);
    const double rad = angle_deg * 3.14159265358979 / 180.0;
    return fieldPoint(r * std::sin(rad), r * std::cos(rad));
}

void drawField(sf::RenderWindow& window, const joji::BallparkConfig& bp) {
    window.clear(Grass);

    sf::ConvexShape infield;
    infield.setPointCount(4);
    infield.setPoint(0, fieldPoint(0.0, 0.0));
    infield.setPoint(1, fieldPoint(90.0, 90.0));
    infield.setPoint(2, fieldPoint(0.0, 180.0));
    infield.setPoint(3, fieldPoint(-90.0, 90.0));
    infield.setFillColor(Dirt);
    window.draw(infield);

    // ファウルライン: コーナーフェンス端まで
    const sf::Vector2f lfCorner = fencePoint(-45.0, bp);
    const sf::Vector2f rfCorner = fencePoint( 45.0, bp);
    sf::VertexArray foulLines(sf::PrimitiveType::Lines, 4);
    foulLines[0].position = fieldPoint(0.0, 0.0);
    foulLines[1].position = lfCorner;
    foulLines[2].position = fieldPoint(0.0, 0.0);
    foulLines[3].position = rfCorner;
    for (std::size_t i = 0; i < foulLines.getVertexCount(); ++i) {
        foulLines[i].color = Chalk;
    }
    window.draw(foulLines);

    // 外野壁: 極座標モデル (物理フェンスと完全一致)
    constexpr float WallThickPx = 9.0f;
    const sf::Color WallBody{18, 72, 38};
    const sf::Color WallTop{240, 200, 50};

    sf::VertexArray wallBody(sf::PrimitiveType::TriangleStrip);
    sf::VertexArray wallCap (sf::PrimitiveType::TriangleStrip);
    constexpr float TopH = 3.5f;
    for (int deg = -45; deg <= 45; ++deg) {
        const sf::Vector2f top = fencePoint(static_cast<double>(deg), bp);
        wallBody.append(sf::Vertex{top,                                  WallBody});
        wallBody.append(sf::Vertex{top + sf::Vector2f{0.f, WallThickPx}, WallBody});
        wallCap .append(sf::Vertex{top,                                  WallTop});
        wallCap .append(sf::Vertex{top + sf::Vector2f{0.f, TopH},        WallTop});
    }
    window.draw(wallBody);
    window.draw(wallCap);

    for (const auto& base : {fieldPoint(90.0, 90.0), fieldPoint(0.0, 180.0), fieldPoint(-90.0, 90.0), fieldPoint(0.0, 0.0)}) {
        sf::CircleShape marker(8.0f);
        marker.setOrigin({8.0f, 8.0f});
        marker.setPosition(base);
        marker.setFillColor(Chalk);
        window.draw(marker);
    }

    sf::CircleShape mound(18.0f);
    mound.setOrigin({18.0f, 18.0f});
    mound.setPosition(fieldPoint(0.0, 60.0));
    mound.setFillColor({146, 91, 50});
    window.draw(mound);
}

bool containsName(const std::vector<std::string>& names, const std::string& target) {
    return std::find(names.begin(), names.end(), target) != names.end();
}

void drawBases(sf::RenderWindow& window,
               const joji::GameState& state,
               const std::vector<std::string>& hiddenRunnerNames) {
    const std::array<sf::Vector2f, 3> positions{
        fieldPoint(90.0, 90.0),
        fieldPoint(0.0, 180.0),
        fieldPoint(-90.0, 90.0)
    };

    for (std::size_t i = 0; i < positions.size(); ++i) {
        if (!state.bases[i].has_value()) {
            continue;
        }
        if (containsName(hiddenRunnerNames, *state.bases[i])) {
            continue;
        }

        sf::CircleShape runner(13.0f);
        runner.setOrigin({13.0f, 13.0f});
        runner.setPosition(positions[i]);
        runner.setFillColor({252, 203, 88});
        runner.setOutlineColor(Ink);
        runner.setOutlineThickness(2.0f);
        window.draw(runner);
    }
}

sf::Vector2f runnerPointAt(const joji::RunnerAnimation& animation, float elapsed) {
    const auto& points = animation.points;
    if (points.empty()) {
        return HomePlate;
    }

    std::size_t idx = 0;
    while (idx + 1 < points.size()
           && points[idx + 1].timeSeconds <= static_cast<double>(elapsed)) {
        ++idx;
    }
    if (idx + 1 >= points.size()) {
        return fieldPoint(points.back().x, points.back().y);
    }

    const auto& start = points[idx];
    const auto& end = points[idx + 1];
    const double span = std::max(0.001, end.timeSeconds - start.timeSeconds);
    const double rawAmount = std::clamp((static_cast<double>(elapsed) - start.timeSeconds) / span, 0.0, 1.0);
    const double amount = rawAmount * rawAmount * (3.0 - 2.0 * rawAmount);
    return fieldPoint(start.x + (end.x - start.x) * amount,
                      start.y + (end.y - start.y) * amount);
}

const joji::TagPlay* tagForRunner(const joji::AnimationPlan& plan,
                                  const joji::RunnerAnimation& animation) {
    for (const auto& tag : plan.tagPlays) {
        if (tag.runnerName == animation.runnerName && tag.base == animation.toBase) {
            return &tag;
        }
    }
    return nullptr;
}

bool isSlidingAtTag(const joji::AnimationPlan& plan,
                    const joji::RunnerAnimation& animation,
                    float elapsed) {
    const joji::TagPlay* tag = tagForRunner(plan, animation);
    if (!tag) {
        return false;
    }
    const float arrival = static_cast<float>(tag->runnerArrivalTime);
    return elapsed >= arrival - 0.28f && elapsed <= arrival + 0.16f;
}

std::vector<std::string> animatedRunnerNames(const joji::AnimationPlan& plan) {
    std::vector<std::string> names;
    names.reserve(plan.runners.size());
    for (const auto& runner : plan.runners) {
        names.push_back(runner.runnerName);
    }
    return names;
}

void drawRunnerAnimations(sf::RenderWindow& window,
                          const sf::Font& font,
                          const joji::AnimationPlan& plan,
                          float elapsed) {
    for (const auto& animation : plan.runners) {
        if (animation.points.empty()) {
            continue;
        }

        sf::VertexArray trail(sf::PrimitiveType::LineStrip);
        for (const auto& point : animation.points) {
            if (point.timeSeconds > static_cast<double>(elapsed)) {
                break;
            }
            trail.append(sf::Vertex{fieldPoint(point.x, point.y), sf::Color{252, 203, 88, 150}});
        }
        if (trail.getVertexCount() >= 2) {
            window.draw(trail);
        }

        const sf::Vector2f current = runnerPointAt(animation, elapsed);
        if (isSlidingAtTag(plan, animation, elapsed)) {
            sf::CircleShape slide(12.0f);
            slide.setOrigin({12.0f, 12.0f});
            slide.setScale({1.65f, 0.58f});
            slide.setPosition(current);
            slide.setFillColor(animation.scored ? sf::Color{80, 190, 100} : sf::Color{252, 203, 88});
            slide.setOutlineColor(Ink);
            slide.setOutlineThickness(2.0f);
            window.draw(slide);

            sf::VertexArray scrape(sf::PrimitiveType::Lines, 2);
            scrape[0] = sf::Vertex{{current.x - 28.0f, current.y + 7.0f}, sf::Color{238, 232, 216, 95}};
            scrape[1] = sf::Vertex{{current.x - 7.0f, current.y + 3.0f}, sf::Color{238, 232, 216, 35}};
            window.draw(scrape);
        } else {
            sf::CircleShape runner(13.0f);
            runner.setOrigin({13.0f, 13.0f});
            runner.setPosition(current);
            runner.setFillColor(animation.scored ? sf::Color{80, 190, 100} : sf::Color{252, 203, 88});
            runner.setOutlineColor(Ink);
            runner.setOutlineThickness(2.0f);
            window.draw(runner);
        }

        drawText(window,
                 font,
                 fitLine(animation.runnerName, 12),
                 {current.x + 15.0f, current.y - 18.0f},
                 11,
                 sf::Color::White,
                 sf::Text::Bold);
    }
}

// Interpolate a defender's field position at a given elapsed time.
sf::Vector2f defAnimPoint(const joji::DefenseAnimation& anim, float elapsed) {
    const auto& pts = anim.points;
    if (pts.size() < 2) return fieldPoint(pts.empty() ? 0.0 : pts[0].x, pts.empty() ? 0.0 : pts[0].y);
    if (elapsed <= 0.0f)  return fieldPoint(pts.front().x, pts.front().y);
    if (elapsed >= static_cast<float>(anim.durationSeconds)) return fieldPoint(pts.back().x, pts.back().y);

    std::size_t idx = 0;
    while (idx + 1 < pts.size() && pts[idx + 1].timeSeconds <= static_cast<double>(elapsed)) ++idx;
    if (idx + 1 >= pts.size()) return fieldPoint(pts.back().x, pts.back().y);

    const auto& a = pts[idx];
    const auto& b = pts[idx + 1];
    const double span = std::max(0.001, b.timeSeconds - a.timeSeconds);
    const double t = std::clamp((static_cast<double>(elapsed) - a.timeSeconds) / span, 0.0, 1.0);
    return fieldPoint(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t);
}

void drawDefense(sf::RenderWindow& window,
                 const sf::Font& font,
                 const joji::DefenseAlignment& defense,
                 const std::optional<joji::PlayResult>& currentPlay,
                 const std::optional<joji::AnimationPlan>& plan = {},
                 float elapsed = 0.0f) {
    const int activeFielderId = currentPlay.has_value() ? currentPlay->fielderId : -1;

    // Find defense animation for active fielder (if any)
    const joji::DefenseAnimation* defAnim = nullptr;
    if (plan.has_value()) {
        for (const auto& d : plan->defenders) {
            if (d.fielderId == activeFielderId) { defAnim = &d; break; }
        }
    }

    for (const auto& fielder : defense.fielders) {
        const bool active = fielder.id == activeFielderId;

        // Animated position: interpolate along path during trajectory phase
        const sf::Vector2f pos = (active && defAnim)
            ? defAnimPoint(*defAnim, elapsed)
            : fieldPoint(fielder.startPosition.x, fielder.startPosition.y);

        sf::CircleShape marker(active ? 12.0f : 9.0f);
        marker.setOrigin({marker.getRadius(), marker.getRadius()});
        marker.setPosition(pos);
        marker.setFillColor(active ? sf::Color{243, 185, 54} : sf::Color{22, 62, 45});
        marker.setOutlineColor(sf::Color::White);
        marker.setOutlineThickness(active ? 2.5f : 1.5f);
        window.draw(marker);

        drawText(window, font, fielder.name,
                 {pos.x + 6.0f, pos.y + 5.0f}, 11, sf::Color::White, sf::Text::Bold);
    }
}

void drawPitchAnimation(sf::RenderWindow& window,
                        const joji::AnimationPlan& plan,
                        float elapsed) {
    const auto& pitch = plan.pitch;
    const auto& points = pitch.points;
    if (points.empty()) {
        return;
    }

    std::size_t idx = 0;
    while (idx + 1 < points.size()
           && points[idx + 1].timeSeconds <= static_cast<double>(elapsed)) {
        ++idx;
    }

    const sf::Color pitchCol = pitch.isBall    ? sf::Color{54, 151, 230}
                             : pitch.isInPlay  ? sf::Color{80, 190, 100}
                                               : sf::Color{220, 72, 66};

    // テーパー付きトレイル (TriangleStrip: テール透明0px → ヘッド3px)
    sf::VertexArray trail(sf::PrimitiveType::TriangleStrip);
    sf::Vector2f prevPerp{1.f, 0.f};
    for (std::size_t i = 0; i <= idx; ++i) {
        const float progress = static_cast<float>(i)
            / static_cast<float>(std::max(idx, std::size_t{1}));
        const float halfW = progress * 3.0f;
        const std::uint8_t alpha = static_cast<std::uint8_t>(185.0f * progress);
        const sf::Color col{pitchCol.r, pitchCol.g, pitchCol.b, alpha};
        const sf::Vector2f pos = pitchPoint(points[i]);

        sf::Vector2f perp = prevPerp;
        if (i > 0) {
            const sf::Vector2f prev = pitchPoint(points[i - 1]);
            sf::Vector2f dir{pos.x - prev.x, pos.y - prev.y};
            const float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
            if (len > 0.1f) {
                dir.x /= len; dir.y /= len;
                perp = {-dir.y, dir.x};
                prevPerp = perp;
            }
        }
        trail.append(sf::Vertex{pos + perp * halfW, col});
        trail.append(sf::Vertex{pos - perp * halfW, col});
    }
    window.draw(trail);

    const auto& current = points[idx];
    const float height = static_cast<float>(std::clamp(current.z, 0.0, 7.0) / 7.0);

    // シャドウ: 高さに応じてわずかに拡大
    const float shadowR = 5.5f + height * 1.5f;
    sf::CircleShape shadow(shadowR);
    shadow.setOrigin({shadow.getRadius(), shadow.getRadius()});
    shadow.setPosition(fieldPoint(current.x, current.y));
    shadow.setFillColor({10, 30, 20, static_cast<std::uint8_t>(80 - height * 30)});
    window.draw(shadow);

    // ボール本体
    sf::CircleShape ball(6.5f + height * 2.0f);
    ball.setOrigin({ball.getRadius(), ball.getRadius()});
    ball.setPosition(pitchPoint(current));
    ball.setFillColor(sf::Color::White);
    ball.setOutlineColor(pitchCol);
    ball.setOutlineThickness(2.0f);
    window.draw(ball);
}

void drawTrajectory(sf::RenderWindow& window,
                    const joji::PlayResult& play,
                    const joji::AnimationPlan& plan,
                    float elapsed,
                    float hideAfter = 1e9f) {
    const auto& points = plan.battedBall.points;
    if (points.empty()) {
        sf::CircleShape plateEvent(18.0f);
        plateEvent.setOrigin({18.0f, 18.0f});
        plateEvent.setPosition(HomePlate);
        plateEvent.setFillColor(outcomeColor(play.type));
        plateEvent.setOutlineColor(sf::Color::White);
        plateEvent.setOutlineThickness(3.0f);
        window.draw(plateEvent);
        return;
    }

    const bool throwStarted = elapsed >= hideAfter;
    const float drawElapsed = throwStarted ? hideAfter : elapsed;

    std::size_t idx = 0;
    while (idx + 1 < points.size()
           && points[idx + 1].timeSeconds <= static_cast<double>(drawElapsed)) {
        ++idx;
    }

    const bool groundTrajectory = isGroundTrajectory(play, plan);
    const bool airTrajectory    = isAirTrajectory(play, plan);
    const float liftFactor      = airTrajectory ? airLiftFactor(plan) : 0.0f;

    // 壁カロム判定: フェンス未越え かつ 壁接触高度 > 0
    const bool isWallCarom       = !plan.battedBall.crossesFence
                                   && plan.battedBall.landingPoint.z > 0.5;
    const double wallContactTime = isWallCarom
                                   ? plan.battedBall.landingPoint.timeSeconds : 1e9;

    // ホームラン: 物理計算済みのフェンス通過時刻を使ってクリップ
    const double fenceCrossSeconds = plan.battedBall.fenceCrossSeconds; // -1 = 非HR
    const bool   isHR              = plan.battedBall.crossesFence && fenceCrossSeconds >= 0.0;
    const bool   ballPastFence     = isHR && elapsed >= static_cast<float>(fenceCrossSeconds);

    // トレイルをフェンス通過時刻の直前インデックスまでに制限
    std::size_t trailEnd = idx;
    if (isHR) {
        while (trailEnd > 0
               && points[trailEnd].timeSeconds > fenceCrossSeconds + 0.05) {
            --trailEnd;
        }
    }

    const sf::Color flightColor = groundTrajectory ? sf::Color{214, 142, 72}
                                : airTrajectory    ? sf::Color{238, 232, 216}
                                                   : sf::Color{238, 232, 216}; // 未分類も白
    const sf::Color caromColor{214, 142, 72};

    // ── テーパー付きトレイル (TriangleStrip) ─────────────────────────────
    const float maxHalfWidth = groundTrajectory ? 2.0f : 3.0f;

    sf::VertexArray trail(sf::PrimitiveType::TriangleStrip);
    sf::Vector2f prevPerp{0.f, 1.f};

    for (std::size_t i = 0; i <= trailEnd; ++i) {
        const float progress = static_cast<float>(i)
            / static_cast<float>(std::max(trailEnd, std::size_t{1}));
        const float halfW = progress * maxHalfWidth;

        const bool inCarom   = isWallCarom && points[i].timeSeconds > wallContactTime + 0.001;
        const sf::Color& baseCol = inCarom ? caromColor : flightColor;

        // HR: trailEnd 付近でトレイルを徐々に透明にして消える
        float alphaScale = 1.0f;
        if (isHR && i + 5 > trailEnd) {
            alphaScale = static_cast<float>(trailEnd - i + 5) / 5.0f;
            alphaScale = std::clamp(alphaScale, 0.0f, 1.0f);
        }
        const std::uint8_t alpha = throwStarted
            ? static_cast<std::uint8_t>(35.0f * progress * alphaScale)
            : static_cast<std::uint8_t>(200.0f * progress * alphaScale);
        sf::Color col{baseCol.r, baseCol.g, baseCol.b, alpha};

        const bool useAirLift = airTrajectory && !inCarom;
        const sf::Vector2f pos = battedBallDrawPoint(points[i], useAirLift, liftFactor);

        // 進行方向の垂直ベクトル (閾値 0.1px に緩和)
        sf::Vector2f perp = prevPerp;
        if (i > 0) {
            const bool prevCarom = isWallCarom
                && points[i-1].timeSeconds > wallContactTime + 0.001;
            const sf::Vector2f prev = battedBallDrawPoint(
                points[i-1], airTrajectory && !prevCarom, liftFactor);
            sf::Vector2f dir{pos.x - prev.x, pos.y - prev.y};
            const float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
            if (len > 0.1f) {
                dir.x /= len; dir.y /= len;
                perp = {-dir.y, dir.x};
                prevPerp = perp;
            }
        }

        trail.append(sf::Vertex{pos + perp * halfW, col});
        trail.append(sf::Vertex{pos - perp * halfW, col});
    }
    window.draw(trail);

    // ── グラウンドボール: ダスト粒子 ─────────────────────────────────────
    if (groundTrajectory) {
        for (std::size_t i = 2; i <= trailEnd; i += 6) {
            sf::CircleShape dust(3.0f);
            dust.setOrigin({3.0f, 3.0f});
            dust.setPosition(fieldPoint(points[i].x, points[i].y));
            dust.setFillColor({196, 126, 62, 90});
            window.draw(dust);
        }
    }

    // ── 壁衝突フラッシュリング ────────────────────────────────────────────
    if (isWallCarom && !throwStarted) {
        const float contactAge = elapsed - static_cast<float>(wallContactTime);
        if (contactAge >= 0.0f && contactAge < 0.35f) {
            const float fade = 1.0f - contactAge / 0.35f;
            const sf::Vector2f wallPos = fieldPoint(
                plan.battedBall.landingPoint.x,
                plan.battedBall.landingPoint.y);
            sf::CircleShape ring(12.0f + contactAge * 40.0f);
            ring.setOrigin({ring.getRadius(), ring.getRadius()});
            ring.setPosition(wallPos);
            ring.setFillColor(sf::Color::Transparent);
            ring.setOutlineColor({240, 200, 80,
                static_cast<std::uint8_t>(200.0f * fade)});
            ring.setOutlineThickness(2.5f);
            window.draw(ring);
        }
    }

    // ── ホームラン: フェンス通過フラッシュ ───────────────────────────────
    if (isHR) {
        const float crossAge = elapsed - static_cast<float>(fenceCrossSeconds);
        if (crossAge >= 0.0f && crossAge < 0.55f) {
            const float fade = 1.0f - crossAge / 0.55f;
            // フェンス通過点: trailEnd 時点のボール位置
            const sf::Vector2f fencePos = fieldPoint(
                points[trailEnd].x, points[trailEnd].y);
            for (int ri = 0; ri < 2; ++ri) {
                const float r = 14.0f + crossAge * (55.0f + ri * 25.0f);
                sf::CircleShape ring(r);
                ring.setOrigin({ring.getRadius(), ring.getRadius()});
                ring.setPosition(fencePos);
                ring.setFillColor(sf::Color::Transparent);
                ring.setOutlineColor({243, 185, 54,
                    static_cast<std::uint8_t>(210.0f * fade / (ri + 1))});
                ring.setOutlineThickness(2.5f);
                window.draw(ring);
            }
        }
    }

    // ── 移動中のボールドット ──────────────────────────────────────────────
    if (!throwStarted && !ballPastFence) {
        const auto& current  = points[idx];
        const bool inCarom   = isWallCarom && current.timeSeconds > wallContactTime + 0.001;
        const bool useAirLift = airTrajectory && !inCarom;
        const float height   = static_cast<float>(
            std::clamp(current.z, 0.0, 180.0) / 180.0);

        const sf::Vector2f shadowPos = fieldPoint(current.x, current.y);
        const sf::Vector2f ballPos   = battedBallDrawPoint(current, useAirLift, liftFactor);

        // シャドウ (高度が上がるほど大きく・薄く)
        const bool isGround = groundTrajectory || inCarom;
        const float shadowR = isGround ? 5.0f : 7.0f + height * 5.0f;
        sf::CircleShape shadow(shadowR);
        shadow.setOrigin({shadow.getRadius(), shadow.getRadius()});
        shadow.setPosition(shadowPos);
        shadow.setFillColor(isGround
            ? sf::Color{110, 70, 38, 90}
            : sf::Color{10, 30, 20, static_cast<std::uint8_t>(80 - height * 50)});
        window.draw(shadow);

        // 高度ライン (liftFactor > 0.06 かつ十分な高さがある場合のみ)
        if (useAirLift && liftFactor > 0.06f && height > 0.06f) {
            sf::VertexArray heightLine(sf::PrimitiveType::Lines, 2);
            heightLine[0] = sf::Vertex{shadowPos, sf::Color{255, 255, 255, 30}};
            heightLine[1] = sf::Vertex{ballPos,   sf::Color{255, 255, 255, 90}};
            window.draw(heightLine);
        }

        // ボール本体 (ラインドライブは小さめ固定、フライは高さで拡大)
        const float ballR = isGround ? 5.5f
                          : liftFactor < 0.10f ? 6.0f               // low liner
                          : 7.0f + height * 5.0f;                   // fly ball
        sf::CircleShape ball(ballR);
        ball.setOrigin({ball.getRadius(), ball.getRadius()});
        ball.setPosition(ballPos);
        ball.setFillColor(sf::Color::White);
        ball.setOutlineColor(isGround
            ? sf::Color{174, 116, 64}
            : outcomeColor(play.type));
        ball.setOutlineThickness(isGround ? 1.5f : 2.0f);
        window.draw(ball);
    }
}

// Screen position for a throw at interpolation parameter t ∈ [0,1],
// incorporating the parabolic arc height (lifts ball upward on screen).
sf::Vector2f throwArcPoint(const joji::ThrowAnimation& thr, float t) {
    const float px = thr.points[0].x + (thr.points[1].x - thr.points[0].x) * t;
    const float py = thr.points[0].y + (thr.points[1].y - thr.points[0].y) * t;
    const float arcZ = static_cast<float>(thr.arcHeightFeet) * 4.0f * t * (1.0f - t);
    const sf::Vector2f base = fieldPoint(px, py);
    // Lift: arc height in field-feet × FieldScale × 0.7 (visual perspective compression)
    return {base.x, base.y - arcZ * FieldScale * 0.7f};
}

void drawThrowAnimations(sf::RenderWindow& window,
                         const joji::AnimationPlan& plan,
                         float elapsed) {
    for (const auto& thr : plan.throws) {
        if (thr.points.size() < 2) continue;
        const float startT = static_cast<float>(thr.startTimeOffset);
        const float endT   = startT + static_cast<float>(thr.durationSeconds);
        if (elapsed < startT || elapsed > endT + 0.3f) continue;

        const sf::Color guideColor = thr.badThrow
            ? sf::Color{235, 116, 48, 90}
            : sf::Color{255, 255, 255, 45};

        // Parabolic guide arc (16-segment polyline)
        constexpr int ArcSegs = 16;
        sf::VertexArray arc(sf::PrimitiveType::LineStrip, ArcSegs + 1);
        for (int i = 0; i <= ArcSegs; ++i) {
            const float u = static_cast<float>(i) / ArcSegs;
            arc[i] = sf::Vertex{throwArcPoint(thr, u), guideColor};
        }
        window.draw(arc);

        // Ball dot moving along the arc
        if (elapsed >= startT && elapsed <= endT) {
            const float t = std::clamp((elapsed - startT) /
                                       static_cast<float>(thr.durationSeconds), 0.0f, 1.0f);
            const sf::Vector2f pos = throwArcPoint(thr, t);

            // Shadow on the field directly below the arc ball
            const float shadowR = 4.0f + static_cast<float>(thr.arcHeightFeet) * 4.0f * t * (1.0f-t) * 0.06f;
            sf::CircleShape shadow(shadowR);
            shadow.setOrigin({shadow.getRadius(), shadow.getRadius()});
            shadow.setPosition(fieldPoint(
                thr.points[0].x + (thr.points[1].x - thr.points[0].x) * t,
                thr.points[0].y + (thr.points[1].y - thr.points[0].y) * t));
            shadow.setFillColor({10, 30, 20, 55});
            window.draw(shadow);

            sf::CircleShape ball(5.0f);
            ball.setOrigin({5.0f, 5.0f});
            ball.setPosition(pos);
            ball.setFillColor(thr.badThrow ? sf::Color{255, 205, 92} : sf::Color::White);
            ball.setOutlineColor(thr.badThrow
                ? sf::Color{220, 72, 66, 230}
                : sf::Color{200, 220, 255, 200});
            ball.setOutlineThickness(1.5f);
            window.draw(ball);
        }
    }
}

sf::Vector2f tagBasePoint(int base) {
    switch (base) {
        case 1: return fieldPoint(90.0, 90.0);
        case 2: return fieldPoint(0.0, 180.0);
        case 3: return fieldPoint(-90.0, 90.0);
        case 4: return fieldPoint(0.0, 0.0);
        default: return fieldPoint(0.0, 0.0);
    }
}

void drawTagFeedback(sf::RenderWindow& window,
                     const sf::Font& font,
                     const joji::AnimationPlan& plan,
                     float elapsed) {
    constexpr float VisibleSeconds = 0.75f;
    for (const auto& tag : plan.tagPlays) {
        const sf::Vector2f pos = tagBasePoint(tag.base);
        const float catchTime = static_cast<float>(tag.ballArrivalTime);
        const float catchAge = elapsed - catchTime;
        if (catchAge >= 0.0f && catchAge <= 0.28f) {
            const float fade = 1.0f - catchAge / 0.28f;
            sf::CircleShape catchRing(18.0f + catchAge * 16.0f);
            catchRing.setOrigin({catchRing.getRadius(), catchRing.getRadius()});
            catchRing.setPosition(pos);
            catchRing.setFillColor(sf::Color::Transparent);
            catchRing.setOutlineColor(sf::Color{255, 255, 255,
                static_cast<std::uint8_t>(210 * fade)});
            catchRing.setOutlineThickness(3.0f);
            window.draw(catchRing);
        }

        const float eventTime = static_cast<float>(
            std::max(tag.ballArrivalTime, tag.runnerArrivalTime) + tag.tagTime);
        const float age = elapsed - eventTime;
        if (age < 0.0f || age > VisibleSeconds) {
            continue;
        }

        const float fade = 1.0f - age / VisibleSeconds;
        const float pulse = 1.0f + age * 0.9f;
        const sf::Color color = tag.runnerSafe
            ? sf::Color{80, 190, 100, static_cast<std::uint8_t>(220 * fade)}
            : sf::Color{220, 72, 66, static_cast<std::uint8_t>(230 * fade)};

        sf::CircleShape ring(25.0f * pulse);
        ring.setOrigin({ring.getRadius(), ring.getRadius()});
        ring.setPosition(pos);
        ring.setFillColor(sf::Color::Transparent);
        ring.setOutlineColor(color);
        ring.setOutlineThickness(4.0f);
        window.draw(ring);

        sf::Text label(font, tag.runnerSafe ? "SAFE" : "OUT", 17);
        label.setStyle(sf::Text::Bold);
        label.setFillColor(color);
        const sf::FloatRect bounds = label.getLocalBounds();
        label.setOrigin({bounds.position.x + bounds.size.x / 2.0f,
                         bounds.position.y + bounds.size.y / 2.0f});
        label.setPosition({pos.x, pos.y - 38.0f - age * 8.0f});
        window.draw(label);

        std::ostringstream timing;
        timing << "R " << std::fixed << std::setprecision(2) << tag.runnerArrivalTime
               << " / B " << tag.ballArrivalTime;
        sf::Text timingLabel(font, timing.str(), 10);
        timingLabel.setFillColor(sf::Color{255, 255, 255,
            static_cast<std::uint8_t>(190 * fade)});
        const sf::FloatRect timingBounds = timingLabel.getLocalBounds();
        timingLabel.setOrigin({timingBounds.position.x + timingBounds.size.x / 2.0f,
                               timingBounds.position.y + timingBounds.size.y / 2.0f});
        timingLabel.setPosition({pos.x, pos.y - 17.0f - age * 8.0f});
        window.draw(timingLabel);
    }
}

// ── HOT/COLD ヘルパー ─────────────────────────────────────────────────────

sf::Color formColor(double form) {
    if (form >= 1.05) return {235, 116, 48};   // HOT  orange
    if (form >= 1.02) return {210, 175, 55};   // WARM yellow-gold
    if (form >= 0.98) return {82,  93,  88};   // NORMAL grey
    if (form >= 0.95) return {100, 160, 200};  // COOL blue-grey
    return                    {54,  151, 230};  // COLD blue
}

std::string formLabel(double form) {
    if (form >= 1.05) return "HOT";
    if (form >= 1.02) return "warm";
    if (form >= 0.98) return "";
    if (form >= 0.95) return "cool";
    return                    "COLD";
}

// フォームバッジを小さい角丸ラベルで描画 (x,y はラベル左端)
void drawFormBadge(sf::RenderWindow& window, const sf::Font& font,
                   double form, sf::Vector2f pos) {
    const std::string lbl = formLabel(form);
    if (lbl.empty()) return;
    const sf::Color col = formColor(form);
    sf::RectangleShape bg({44.0f, 17.0f});
    bg.setPosition(pos);
    bg.setFillColor({col.r, col.g, col.b, 55});
    bg.setOutlineColor({col.r, col.g, col.b, 180});
    bg.setOutlineThickness(1.0f);
    window.draw(bg);
    drawText(window, font, lbl, {pos.x + 4.0f, pos.y + 1.0f}, 11, col, sf::Text::Bold);
}

// ── 投手交代バナー (フィールド左側オーバーレイ) ──────────────────────────

sf::Color reasonColor(const std::string& reason) {
    if (reason == "SAVE SITUATION") return {80,  190, 100};
    if (reason == "FATIGUE")        return {220, 72,  66};
    if (reason == "BLOWOUT")        return {235, 116, 48};
    if (reason == "EARLY HOOK")     return {243, 185, 54};
    return                                 {153, 153, 153};
}

void drawPitcherChangeBanner(sf::RenderWindow& window, const sf::Font& font,
                             const joji::GameEngine::PitcherChangeEvent& ev,
                             float alpha01) {
    const auto a = static_cast<std::uint8_t>(std::clamp(alpha01, 0.0f, 1.0f) * 255.0f);

    // 背景パネル
    sf::RectangleShape bg({700.0f, 130.0f});
    bg.setPosition({115.0f, 295.0f});
    bg.setFillColor({20, 28, 24, static_cast<std::uint8_t>(a * 0.88f)});
    bg.setOutlineColor({reasonColor(ev.reason).r,
                        reasonColor(ev.reason).g,
                        reasonColor(ev.reason).b, a});
    bg.setOutlineThickness(2.0f);
    window.draw(bg);

    // "PITCHING CHANGE" ヘッダー
    sf::Text hdr(font, "PITCHING CHANGE", 16);
    hdr.setFillColor({200, 200, 190, a});
    hdr.setStyle(sf::Text::Bold);
    hdr.setPosition({135.0f, 302.0f});
    window.draw(hdr);

    // 理由バッジ
    const sf::Color rc = reasonColor(ev.reason);
    sf::RectangleShape badge({static_cast<float>(ev.reason.size()) * 8.6f + 14.0f, 20.0f});
    badge.setPosition({135.0f, 323.0f});
    badge.setFillColor({rc.r, rc.g, rc.b, static_cast<std::uint8_t>(a * 0.30f)});
    badge.setOutlineColor({rc.r, rc.g, rc.b, a});
    badge.setOutlineThickness(1.2f);
    window.draw(badge);
    sf::Text reasonTxt(font, ev.reason, 12);
    reasonTxt.setFillColor({rc.r, rc.g, rc.b, a});
    reasonTxt.setStyle(sf::Text::Bold);
    reasonTxt.setPosition({142.0f, 326.0f});
    window.draw(reasonTxt);

    // 名前 "From → To"
    const std::string nameStr = ev.fromName + "  ->  " + ev.toName;
    sf::Text nameTxt(font, nameStr, 26);
    nameTxt.setFillColor({238, 232, 216, a});
    nameTxt.setStyle(sf::Text::Bold);
    nameTxt.setPosition({135.0f, 350.0f});
    window.draw(nameTxt);
}

// ── 試合終了オーバーレイ (R/H/E) ──────────────────────────────────────────

void drawGameEndSummary(sf::RenderWindow& window, const sf::Font& font,
                        const joji::GameEngine& engine) {
    const auto& state = engine.state();
    const bool awayWon = state.awayScore > state.homeScore;
    const bool homeWon = state.homeScore > state.awayScore;

    // 半透明背景
    sf::RectangleShape bg({720.0f, 180.0f});
    bg.setPosition({105.0f, 270.0f});
    bg.setFillColor({18, 26, 22, 230});
    bg.setOutlineColor({200, 180, 80});
    bg.setOutlineThickness(2.0f);
    window.draw(bg);

    drawText(window, font, "FINAL", {420.0f, 278.0f}, 22, {220, 190, 60}, sf::Text::Bold);

    // ヘッダー行
    const float col0 = 125.0f, col1 = 570.0f, col2 = 640.0f, col3 = 710.0f;
    drawText(window, font, "R",   {col1, 308.0f}, 15, {130, 140, 135});
    drawText(window, font, "H",   {col2, 308.0f}, 15, {130, 140, 135});
    drawText(window, font, "E",   {col3, 308.0f}, 15, {130, 140, 135});

    sf::VertexArray divider(sf::PrimitiveType::Lines, 2);
    divider[0].position = {col0, 326.0f}; divider[0].color = {80, 90, 85};
    divider[1].position = {780.0f, 326.0f}; divider[1].color = {80, 90, 85};
    window.draw(divider);

    // Away 行
    const sf::Color awayCol = awayWon ? sf::Color{243, 185, 54} : sf::Color{200, 200, 195};
    drawText(window, font, fitLine(engine.awayTeamName(), 22), {col0, 332.0f}, 20, awayCol,
             awayWon ? sf::Text::Bold : sf::Text::Regular);
    drawText(window, font, std::to_string(state.awayScore), {col1, 332.0f}, 22, awayCol, sf::Text::Bold);
    drawText(window, font, std::to_string(engine.awayBoxScore().hits),   {col2, 332.0f}, 20, awayCol);
    drawText(window, font, std::to_string(engine.awayBoxScore().errors), {col3, 332.0f}, 20, awayCol);

    // Home 行
    const sf::Color homeCol = homeWon ? sf::Color{243, 185, 54} : sf::Color{200, 200, 195};
    drawText(window, font, fitLine(engine.homeTeamName(), 22), {col0, 362.0f}, 20, homeCol,
             homeWon ? sf::Text::Bold : sf::Text::Regular);
    drawText(window, font, std::to_string(state.homeScore), {col1, 362.0f}, 22, homeCol, sf::Text::Bold);
    drawText(window, font, std::to_string(engine.homeBoxScore().hits),   {col2, 362.0f}, 20, homeCol);
    drawText(window, font, std::to_string(engine.homeBoxScore().errors), {col3, 362.0f}, 20, homeCol);

    // 勝利チーム
    const std::string winner = awayWon ? engine.awayTeamName()
                             : homeWon ? engine.homeTeamName()
                             :           "TIE";
    drawText(window, font, winner + " wins!", {col0, 400.0f}, 18, {220, 190, 60}, sf::Text::Bold);
}

// ── スコアボード (右パネル) ────────────────────────────────────────────────

void drawScoreboard(sf::RenderWindow& window,
                    const sf::Font& font,
                    const joji::GameState& state,
                    const std::optional<joji::PlayResult>& currentPlay,
                    const joji::GameEngine& engine,
                    bool complete) {
    drawText(window, font, "Joji Baseball", {960.0f, 12.0f}, 24, Ink, sf::Text::Bold);
    drawText(window, font, halfLabel(state), {960.0f, 44.0f}, 18, {56, 76, 70}, sf::Text::Bold);
    if (complete)
        drawText(window, font, "FINAL", {1168.0f, 44.0f}, 17, {220, 72, 66}, sf::Text::Bold);

    // ラインスコアは drawLineScore で描画 (y=118 周辺)
    // ─── drawLineScore はここより前に呼ばれているため省略 ───

    drawText(window, font, "Outs", {960.0f, 168.0f}, 15, {82, 93, 88}, sf::Text::Bold);
    for (int i = 0; i < 3; ++i) {
        sf::CircleShape outLight(8.0f);
        outLight.setOrigin({8.0f, 8.0f});
        outLight.setPosition({1010.0f + i * 26.0f, 178.0f});
        outLight.setFillColor(i < state.outs ? sf::Color{220, 72, 66} : sf::Color{206, 201, 186});
        window.draw(outLight);
    }

    // ── 現在の投手 (HOT/COLD + ERA + 球数 + アーセナル) ──────────────────
    const joji::Player& pitcher = engine.currentPitcher();
    const int pitchCount = engine.currentPitcherPitchCount();
    const double pitcherForm = engine.pitcherFormValue();
    const double era = engine.currentPitcherERA();

    const sf::Color pitcherNameColor = formColor(pitcherForm);
    drawText(window, font, fitLine(pitcher.name, 18), {960.0f, 196.0f}, 15, pitcherNameColor, sf::Text::Bold);
    drawFormBadge(window, font, pitcherForm, {1168.0f, 197.0f});

    const sf::Color pitchCountColor = pitchCount > 100 ? sf::Color{220, 72, 66}
                                    : pitchCount >  75 ? sf::Color{235, 116, 48}
                                                       : sf::Color{82,  93,  88};
    std::ostringstream eraStr;
    eraStr << std::fixed << std::setprecision(2) << era;
    const std::string pitcherMeta = joji::toString(pitcher.pitcherRole)
                                  + "  " + std::to_string(pitchCount) + "P"
                                  + "  ERA " + eraStr.str();
    drawText(window, font, fitLine(pitcherMeta, 30), {960.0f, 213.0f}, 12, pitchCountColor);

    // 球速疲労トレンドバー
    {
        const double velDrop = engine.currentPitcherVelocityDrop();
        std::ostringstream velStr;
        if (velDrop >= -0.3) {
            velStr << "Vel  --";
        } else {
            velStr << "Vel " << std::fixed << std::setprecision(1) << velDrop << " mph";
        }
        // Color: green→yellow→red as fatigue grows
        const sf::Color velColor = (velDrop > -1.5) ? sf::Color{82, 150, 90}
                                 : (velDrop > -3.5) ? sf::Color{215, 170, 40}
                                                    : sf::Color{210, 75, 60};
        drawText(window, font, velStr.str(), {960.0f, 226.0f}, 11, velColor);

        // Small fatigue bar (max drop ≈ 8 mph shown as full bar)
        const float barW = 120.0f;
        const float fill = std::clamp(static_cast<float>(-velDrop / 8.0), 0.0f, 1.0f);
        sf::RectangleShape barBg({barW, 4.0f});
        barBg.setPosition({1060.0f, 229.0f});
        barBg.setFillColor({50, 55, 52});
        window.draw(barBg);
        if (fill > 0.0f) {
            sf::RectangleShape barFill({barW * fill, 4.0f});
            barFill.setPosition({1060.0f, 229.0f});
            barFill.setFillColor(velColor);
            window.draw(barFill);
        }
    }

    // 球種アーセナル
    drawPitcherArsenal(window, font, engine.currentPitcherArsenal(), 960.0f, 240.0f);

    // ── 打者セクション ───────────────────────────────────────────────────
    const bool atBatActive = engine.isAtBatInProgress() || engine.hasPendingAtBatResult();
    if (atBatActive) {
        const joji::AtBatState& ab = engine.currentAtBat();
        const double batterForm = engine.batterFormValue(ab.batter.name);

        drawText(window, font, "At bat", {960.0f, 244.0f}, 13, {82, 93, 88}, sf::Text::Bold);
        const sf::Color batterColor = formColor(batterForm);
        drawText(window, font, fitLine(ab.batter.name, 22), {960.0f, 260.0f}, 20, batterColor, sf::Text::Bold);
        drawFormBadge(window, font, batterForm, {1168.0f, 261.0f});

        const std::string countStr = "Count  " + std::to_string(ab.count.balls)
                                     + " - " + std::to_string(ab.count.strikes)
                                     + "  (#" + std::to_string(ab.pitchNumber - 1) + ")";
        drawText(window, font, countStr, {960.0f, 284.0f}, 14, {56, 76, 70}, sf::Text::Bold);

        if (engine.hasPendingAtBatResult() && ab.finalOutcome.has_value()) {
            std::string outcomeStr;
            switch (*ab.finalOutcome) {
                case joji::AtBatOutcome::StrikeOut:  outcomeStr = "Strikeout K"; break;
                case joji::AtBatOutcome::Walk:        outcomeStr = "Walk BB";     break;
                case joji::AtBatOutcome::HitByPitch:  outcomeStr = "Hit By Pitch"; break;
                case joji::AtBatOutcome::InPlay:      outcomeStr = "In Play";     break;
            }
            drawText(window, font, outcomeStr, {960.0f, 308.0f}, 19,
                     *ab.finalOutcome == joji::AtBatOutcome::InPlay
                         ? sf::Color{80, 190, 100}
                         : sf::Color{220, 72, 66},
                     sf::Text::Bold);
            drawText(window, font, "Space to apply", {960.0f, 334.0f}, 13, {82, 93, 88});
        }
    } else if (currentPlay.has_value()) {
        const auto& play = *currentPlay;
        drawText(window, font, "Result", {960.0f, 244.0f}, 13, {82, 93, 88}, sf::Text::Bold);
        drawText(window, font, fitLine(play.batterName, 22), {960.0f, 260.0f}, 20, Ink, sf::Text::Bold);
        drawText(window, font, joji::toString(play.type), {960.0f, 283.0f}, 18, outcomeColor(play.type), sf::Text::Bold);

        if (play.battedBall.estimatedDistance > 0.0) {
            std::ostringstream details;
            details << static_cast<int>(std::round(play.battedBall.estimatedDistance)) << " ft  "
                    << play.battedBall.classification;
            drawText(window, font, fitLine(details.str(), 28), {960.0f, 308.0f}, 14, {47, 58, 54});

            std::ostringstream metrics;
            metrics << "EV " << static_cast<int>(std::round(play.battedBall.exitVelocity))
                    << "  LA " << static_cast<int>(std::round(play.battedBall.launchAngle))
                    << "  Spray " << static_cast<int>(std::round(play.battedBall.sprayAngle));
            drawText(window, font, fitLine(metrics.str(), 31), {960.0f, 326.0f}, 13, {47, 58, 54});

            if (play.fielderId >= 0) {
                std::ostringstream fielder;
                fielder << "Fielder " << play.fielderName
                        << "  " << std::fixed << std::setprecision(1)
                        << play.fielderTravelTime << "/" << play.fieldingAvailableTime << "s";
                drawText(window, font, fitLine(fielder.str(), 31), {960.0f, 344.0f}, 13, {47, 58, 54});
            }
            if (!play.defensiveDecision.reason.empty()) {
                std::ostringstream decision;
                decision << "Decision ";
                if (play.defensiveDecision.holdBall) {
                    decision << "hold";
                } else {
                    decision << "throw " << play.defensiveDecision.chosenTargetBase << "B";
                    if (play.throwDecision.useCutoff) {
                        decision << " via " << play.throwDecision.cutoffFielderName;
                    }
                    if (play.throwDecision.badThrow) {
                        decision << " bad throw";
                        if (play.throwingError) {
                            decision << " E";
                        }
                    }
                }
                decision << "  " << play.defensiveDecision.reason;
                drawText(window,
                         font,
                         fitLine(decision.str(), 35),
                         {960.0f, 362.0f},
                         12,
                         play.defensiveDecision.holdBall
                             ? sf::Color{100, 110, 106}
                             : sf::Color{80, 190, 100});
            }
        }
    } else if (!complete) {
        drawText(window, font, "Ready", {960.0f, 244.0f}, 13, {82, 93, 88}, sf::Text::Bold);
        drawText(window, font, "Space to pitch", {960.0f, 262.0f}, 20, Ink, sf::Text::Bold);
    }

    // ── 試合終了: 打撃成績 + 守備成績 (右パネル中段) ────────────────────
    if (complete) {
        const auto& awayStats = engine.awayPlayerStats();
        const auto& homeStats = engine.homePlayerStats();

        float y = 244.0f;
        const float xLabel = 960.0f;
        const float xAB    = 1082.0f;
        const float xH     = 1108.0f;
        const float xHR    = 1132.0f;
        const float xRBI   = 1156.0f;
        const float xOPS   = 1192.0f;

        auto drawBattingSection = [&](const std::vector<joji::PlayerBoxScore>& stats,
                                      const std::string& teamName) {
            drawText(window, font, fitLine(teamName, 16), {xLabel, y}, 13,
                     {100, 110, 106}, sf::Text::Bold);
            drawText(window, font, "AB", {xAB,  y}, 11, {100, 110, 106});
            drawText(window, font, "H",  {xH,   y}, 11, {100, 110, 106});
            drawText(window, font, "HR", {xHR,  y}, 11, {100, 110, 106});
            drawText(window, font, "RBI",{xRBI, y}, 11, {100, 110, 106});
            drawText(window, font, "OPS",   {xOPS, y}, 11, {100, 110, 106});
            y += 14.0f;
            for (const auto& p : stats) {
                const sf::Color rowCol = (p.hits > 0 || p.walks > 0 || p.hitByPitch > 0)
                    ? Ink : sf::Color{120, 128, 124};
                drawText(window, font, fitLine(p.name, 12), {xLabel, y}, 12, rowCol);
                drawText(window, font, std::to_string(p.atBats), {xAB,  y}, 12, rowCol);
                drawText(window, font, std::to_string(p.hits),   {xH,   y}, 12, rowCol);
                drawText(window, font, std::to_string(p.homeRuns),{xHR, y}, 12, rowCol);
                drawText(window, font, std::to_string(p.rbi),    {xRBI, y}, 12, rowCol);
                std::ostringstream ops;
                ops << std::fixed << std::setprecision(3) << calcOPS(p);
                // BABIP in tooltip: append to OPS if room
                const double babip = calcBABIP(p);
                if (babip > 0.0) {
                    std::ostringstream bab;
                    bab << std::fixed << std::setprecision(3) << babip;
                    ops << "/" << bab.str();
                }
                drawText(window, font, fitLine(ops.str(), 10), {xOPS, y}, 12, rowCol);
                y += 13.0f;
            }
            y += 4.0f;
        };

        drawBattingSection(awayStats, engine.awayTeamName() + " BATTING");
        drawBattingSection(homeStats, engine.homeTeamName() + " BATTING");

        // ── 守備成績 ────────────────────────────────────────────────────
        const float xPos  = 960.0f;
        const float xName = 984.0f;
        const float xPO   = 1118.0f;
        const float xA    = 1153.0f;
        const float xE    = 1186.0f;

        auto drawDefenseSection = [&](const std::vector<joji::PlayerBoxScore>& stats,
                                      const std::string& teamName) {
            if (y > 720.0f) return; // skip if no room
            drawText(window, font, fitLine(teamName, 16), {xPos, y}, 13,
                     {100, 110, 106}, sf::Text::Bold);
            drawText(window, font, "PO", {xPO, y}, 11, {100, 110, 106});
            drawText(window, font, "A",  {xA,  y}, 11, {100, 110, 106});
            drawText(window, font, "E",  {xE,  y}, 11, {100, 110, 106});
            y += 14.0f;
            for (const auto& p : stats) {
                const sf::Color rowCol = (p.errors > 0)
                    ? sf::Color{220, 72, 66} : sf::Color{82, 93, 88};
                std::string posLabel = positionAbbr(p.position);
                drawText(window, font, posLabel,               {xPos,  y}, 11, {100, 110, 106});
                drawText(window, font, fitLine(p.name, 10),   {xName, y}, 11, rowCol);
                drawText(window, font, std::to_string(p.putouts), {xPO, y}, 11, rowCol);
                drawText(window, font, std::to_string(p.assists), {xA,  y}, 11, rowCol);
                drawText(window, font, std::to_string(p.errors),  {xE,  y}, 11, rowCol);
                y += 12.0f;
            }
            y += 4.0f;
        };

        drawDefenseSection(homeStats, engine.homeTeamName() + " DEFENSE");
        drawDefenseSection(awayStats, engine.awayTeamName() + " DEFENSE");
    }
}

void drawAtBatFlow(sf::RenderWindow& window, const sf::Font& font,
	                   const std::vector<joji::PitchLog>& logs) {
    drawText(window, font, "At-bat flow", {960.0f, 696.0f}, 14, {82, 93, 88}, sf::Text::Bold);

    const std::size_t start = logs.size() > 1 ? logs.size() - 1 : 0;
    float y = 714.0f;

    for (std::size_t i = start; i < logs.size(); ++i) {
        const auto& log = logs[i];
        const bool isLatest = (i == logs.size() - 1);
        const sf::Color rowColor = isLatest ? Ink : sf::Color{100, 110, 106};

        std::ostringstream pitchLine;
        pitchLine << "P" << log.pitchNumber << " "
                  << countText(log.countBefore) << "->" << countText(log.countAfter)
                  << " " << joji::toString(log.pitch.pitchType)
                  << " " << static_cast<int>(std::round(log.pitch.pitchVelocity));
        drawText(window, font, fitLine(pitchLine.str(), 34), {960.0f, y}, 14, rowColor, sf::Text::Bold);
        y += 17.0f;

        std::ostringstream flowLine;
        flowLine << joji::toString(log.swingDecision.decision);
        if (log.zoneResult.has_value()) {
            flowLine << " -> " << joji::toString(log.zoneResult->result);
        }
        if (log.contactResult.has_value()) {
            flowLine << " -> " << joji::toString(log.contactResult->resultType);
        }
        flowLine << " -> " << joji::toString(log.pitchOutcome);
        drawText(window, font, fitLine(flowLine.str(), 39), {970.0f, y}, 13, {47, 58, 54});
        y += 24.0f;
    }
}

std::string replayEventTypeLabel(joji::ReplayEventType type) {
    switch (type) {
        case joji::ReplayEventType::Pitch:      return "Pitch";
        case joji::ReplayEventType::Contact:    return "Contact";
        case joji::ReplayEventType::BallFlight: return "Ball";
        case joji::ReplayEventType::Field:      return "Field";
        case joji::ReplayEventType::Throw:      return "Throw";
        case joji::ReplayEventType::Runner:     return "Runner";
        case joji::ReplayEventType::Tag:        return "Tag";
        case joji::ReplayEventType::Result:     return "Result";
    }
    return "Replay";
}

std::string replayPhaseLabel(joji::ReplayPhase phase) {
    switch (phase) {
        case joji::ReplayPhase::Pitch:      return "Pitch";
        case joji::ReplayPhase::Contact:    return "Contact";
        case joji::ReplayPhase::BallFlight: return "Ball flight";
        case joji::ReplayPhase::Field:      return "Field";
        case joji::ReplayPhase::Throw:      return "Throw";
        case joji::ReplayPhase::Runner:     return "Runner";
        case joji::ReplayPhase::Tag:        return "Tag";
        case joji::ReplayPhase::Result:     return "Result";
    }
    return "Result";
}

sf::Color replayPhaseColor(joji::ReplayPhase phase) {
    switch (phase) {
        case joji::ReplayPhase::Pitch:      return {54, 151, 230};
        case joji::ReplayPhase::Contact:    return {153, 116, 228};
        case joji::ReplayPhase::BallFlight: return {80, 190, 100};
        case joji::ReplayPhase::Field:      return {235, 178, 86};
        case joji::ReplayPhase::Throw:      return {235, 116, 48};
        case joji::ReplayPhase::Runner:     return {80, 190, 100};
        case joji::ReplayPhase::Tag:        return {220, 72, 66};
        case joji::ReplayPhase::Result:     return {82, 93, 88};
    }
    return Ink;
}

void drawReplayTimelineStatus(sf::RenderWindow& window,
                              const sf::Font& font,
                              const std::optional<joji::AnimationPlan>& plan,
                              float elapsed,
                              bool paused) {
    if (!plan.has_value() || plan->replayTimeline.events.empty()) {
        return;
    }

    const float duration = static_cast<float>(
        std::max(plan->replayTimeline.durationSeconds, plan->totalDurationSeconds));
    const float progress = duration > 0.0f
        ? std::clamp(elapsed / duration, 0.0f, 1.0f)
        : 0.0f;
    const sf::Color barBack{205, 199, 184};
    const sf::Color barFill = paused ? sf::Color{235, 116, 48} : sf::Color{54, 151, 230};

    sf::RectangleShape scrubBack({260.0f, 4.0f});
    scrubBack.setPosition({960.0f, 644.0f});
    scrubBack.setFillColor(barBack);
    window.draw(scrubBack);

    sf::RectangleShape scrubFill({260.0f * progress, 4.0f});
    scrubFill.setPosition({960.0f, 644.0f});
    scrubFill.setFillColor(barFill);
    window.draw(scrubFill);

    sf::CircleShape knob(5.0f);
    knob.setOrigin({5.0f, 5.0f});
    knob.setPosition({960.0f + 260.0f * progress, 646.0f});
    knob.setFillColor(barFill);
    window.draw(knob);

    std::ostringstream timeText;
    timeText << (paused ? "PAUSED " : "PLAY ")
             << std::fixed << std::setprecision(1)
             << std::clamp(elapsed, 0.0f, std::max(duration, elapsed))
             << "/" << std::max(0.0f, duration) << "s";
    drawText(window,
             font,
             timeText.str(),
             {960.0f, 628.0f},
             11,
             paused ? sf::Color{235, 116, 48} : sf::Color{82, 93, 88},
             sf::Text::Bold);

    const joji::ReplayCursor cursor = joji::cursorForReplayTimeline(plan->replayTimeline,
                                                                     elapsed);
    if (!cursor.activeEventIndex.has_value()
        && !cursor.nextEventIndex.has_value()) {
        return;
    }
    const joji::ReplayEvent* active = cursor.activeEventIndex.has_value()
        ? &plan->replayTimeline.events[*cursor.activeEventIndex]
        : &plan->replayTimeline.events[*cursor.nextEventIndex];
    const joji::ReplayEvent* next = cursor.nextEventIndex.has_value()
        ? &plan->replayTimeline.events[*cursor.nextEventIndex]
        : nullptr;
    const sf::Color phaseColor = replayPhaseColor(cursor.phase);

    drawText(window, font, "Replay", {960.0f, 656.0f}, 13, {82, 93, 88}, sf::Text::Bold);
    drawText(window, font, replayPhaseLabel(cursor.phase), {1028.0f, 656.0f}, 13, phaseColor, sf::Text::Bold);

    std::ostringstream line;
    line << std::fixed << std::setprecision(2) << active->timeSeconds
         << "  " << replayEventTypeLabel(active->type)
         << "  " << active->label;
    if (!active->actor.empty()) {
        line << "  " << active->actor;
    }
    drawText(window, font, fitLine(line.str(), 39), {960.0f, 671.0f}, 11, Ink);

    if (next != nullptr) {
        std::ostringstream nextLine;
        nextLine << "next " << std::fixed << std::setprecision(2) << next->timeSeconds
                 << "  " << replayEventTypeLabel(next->type)
                 << "  " << next->label;
        drawText(window, font, fitLine(nextLine.str(), 39), {960.0f, 684.0f}, 10, {100, 110, 106});
    }
}

sf::Color pitchDotColor(const joji::PitchLog& log, bool isLatest) {
    if (!isLatest) return {160, 155, 140, 180};
    switch (log.pitchOutcome) {
        case joji::PitchOutcome::Ball:           return {54, 151, 230};
        case joji::PitchOutcome::CalledStrike:   return {220, 72, 66};
        case joji::PitchOutcome::SwingingStrike: return {220, 72, 66};
        case joji::PitchOutcome::Foul:           return {153, 116, 228};
        case joji::PitchOutcome::InPlay:         return {80, 190, 100};
    }
    return sf::Color::White;
}

sf::Color pitchAnimationColor(const joji::PitchAnimation& pitch) {
    if (pitch.isBall) return {54, 151, 230};
    if (pitch.isInPlay) return {80, 190, 100};
    return {220, 72, 66};
}

sf::Vector2f pitchViewZonePoint(const joji::AnimationPoint& point,
                                sf::Vector2f zoneOrigin,
                                sf::Vector2f zoneSize) {
    const float nX = static_cast<float>((point.x + 1.25) / 2.5);
    const float nZ = static_cast<float>((3.9 - point.z) / 2.8);
    return {
        zoneOrigin.x + std::clamp(nX, 0.0f, 1.0f) * zoneSize.x,
        zoneOrigin.y + std::clamp(nZ, 0.0f, 1.0f) * zoneSize.y
    };
}

sf::Vector2f pitchPathPoint(const joji::PitchAnimation& pitch,
                            const joji::AnimationPoint& point,
                            sf::Vector2f pitcherPoint,
                            sf::Vector2f zoneOrigin,
                            sf::Vector2f zoneSize) {
    const sf::Vector2f zonePoint = pitchViewZonePoint(pitch.end, zoneOrigin, zoneSize);
    const float progress = std::clamp(static_cast<float>((60.5 - point.y) / 60.5), 0.0f, 1.0f);
    const float breakShape = progress * progress;
    const float movementPixelsX = static_cast<float>((point.x - pitch.endLocationX * progress) * 34.0);
    const float movementPixelsZ = static_cast<float>((point.z - (6.0 + (pitch.endLocationZ - 6.0) * progress)) * -18.0);
    const float curveLift = std::sin(progress * 3.14159265f) * 20.0f;
    return {
        pitcherPoint.x + (zonePoint.x - pitcherPoint.x) * progress + movementPixelsX * breakShape,
        pitcherPoint.y + (zonePoint.y - pitcherPoint.y) * progress - curveLift + movementPixelsZ * breakShape
    };
}

void drawZoneGrid(sf::RenderWindow& window,
                  const sf::Font& font,
                  sf::Vector2f origin,
                  sf::Vector2f size,
                  bool showNumbers = true) {
    sf::RectangleShape zone(size);
    zone.setPosition(origin);
    zone.setFillColor({250, 247, 238});
    zone.setOutlineColor({220, 72, 66});
    zone.setOutlineThickness(2.0f);
    window.draw(zone);

    sf::VertexArray thirds(sf::PrimitiveType::Lines, 8);
    for (int i = 1; i <= 2; ++i) {
        const float x = origin.x + size.x * i / 3.0f;
        thirds[(i - 1) * 4 + 0].position = {x, origin.y};
        thirds[(i - 1) * 4 + 1].position = {x, origin.y + size.y};
        const float y = origin.y + size.y * i / 3.0f;
        thirds[(i - 1) * 4 + 2].position = {origin.x, y};
        thirds[(i - 1) * 4 + 3].position = {origin.x + size.x, y};
    }
    for (std::size_t i = 0; i < thirds.getVertexCount(); ++i) {
        thirds[i].color = {220, 72, 66, 160};
    }
    window.draw(thirds);

    if (showNumbers) {
        for (int row = 0; row < 3; ++row) {
            for (int col = 0; col < 3; ++col) {
                const int zoneNumber = row * 3 + col + 1;
                const sf::Vector2f center{
                    origin.x + size.x * (static_cast<float>(col) + 0.5f) / 3.0f,
                    origin.y + size.y * (static_cast<float>(row) + 0.5f) / 3.0f
                };
                drawText(window,
                         font,
                         std::to_string(zoneNumber),
                         {center.x - 5.0f, center.y - 10.0f},
                         15,
                         {100, 105, 102});
            }
        }
    }
}

void drawPitchLocationHistory(sf::RenderWindow& window,
                              const std::vector<joji::PitchLog>& logs,
                              sf::Vector2f origin,
                              sf::Vector2f size) {
    for (std::size_t i = 0; i < logs.size(); ++i) {
        const auto& log = logs[i];
        const bool isLatest = (i == logs.size() - 1);
        const float nX = static_cast<float>((log.pitch.locationX + 1.25) / 2.5);
        const float nZ = static_cast<float>((3.9 - log.pitch.locationZ) / 2.8);
        const float dotX = origin.x + std::clamp(nX, 0.0f, 1.0f) * size.x;
        const float dotY = origin.y + std::clamp(nZ, 0.0f, 1.0f) * size.y;
        const float r = isLatest ? 6.0f : 3.5f;

        sf::CircleShape dot(r);
        dot.setOrigin({r, r});
        dot.setPosition({dotX, dotY});
        dot.setFillColor(pitchDotColor(log, isLatest));
        dot.setOutlineColor(isLatest ? Ink : sf::Color{80, 80, 80, 120});
        dot.setOutlineThickness(isLatest ? 1.5f : 0.8f);
        window.draw(dot);
    }
}

void drawPitchPath(sf::RenderWindow& window,
                   const joji::PitchAnimation& pitch,
                   sf::Vector2f pitcherPoint,
                   sf::Vector2f zoneOrigin,
                   sf::Vector2f zoneSize,
                   float elapsed) {
    const auto& points = pitch.points;
    if (points.empty()) {
        return;
    }

    std::size_t idx = 0;
    while (idx + 1 < points.size()
           && points[idx + 1].timeSeconds <= static_cast<double>(elapsed)) {
        ++idx;
    }

    sf::VertexArray path(sf::PrimitiveType::LineStrip);
    const sf::Color color = pitchAnimationColor(pitch);
    for (std::size_t i = 0; i <= idx; ++i) {
        sf::Color c = color;
        c.a = static_cast<std::uint8_t>(110 + 120 * i / std::max<std::size_t>(idx + 1, 1));
        path.append(sf::Vertex{pitchPathPoint(pitch, points[i], pitcherPoint, zoneOrigin, zoneSize), c});
    }
    if (path.getVertexCount() >= 2) {
        window.draw(path);
    }

    for (std::size_t i = 1; i <= idx; i += 2) {
        const sf::Vector2f dotPoint = pitchPathPoint(pitch, points[i], pitcherPoint, zoneOrigin, zoneSize);
        sf::CircleShape motionDot(2.5f);
        motionDot.setOrigin({2.5f, 2.5f});
        motionDot.setPosition(dotPoint);
        motionDot.setFillColor({255, 255, 255, 175});
        window.draw(motionDot);
    }

    const sf::Vector2f current = pitchPathPoint(pitch, points[idx], pitcherPoint, zoneOrigin, zoneSize);
    sf::CircleShape ball(6.0f);
    ball.setOrigin({6.0f, 6.0f});
    ball.setPosition(current);
    ball.setFillColor(sf::Color::White);
    ball.setOutlineColor(color);
    ball.setOutlineThickness(2.0f);
    window.draw(ball);

    const sf::Vector2f endpoint = pitchViewZonePoint(pitch.end, zoneOrigin, zoneSize);
    sf::CircleShape endDot(7.0f);
    endDot.setOrigin({7.0f, 7.0f});
    endDot.setPosition(endpoint);
    endDot.setFillColor(color);
    endDot.setOutlineColor(Ink);
    endDot.setOutlineThickness(1.5f);
    window.draw(endDot);
}

void drawPitchView(sf::RenderWindow& window,
                   const sf::Font& font,
                   const std::optional<joji::AnimationPlan>& plan,
                   const std::vector<joji::PitchLog>& logs,
                   float elapsed) {
    const sf::Vector2f zoneOrigin{1030.0f, 510.0f};
    const sf::Vector2f zoneSize{148.0f, 128.0f};
    const sf::Vector2f pitcherPoint{1105.0f, 414.0f};

    drawText(window, font, "Pitch View", {960.0f, 386.0f}, 15, {82, 93, 88}, sf::Text::Bold);

    sf::CircleShape pitcher(8.0f);
    pitcher.setOrigin({8.0f, 8.0f});
    pitcher.setPosition(pitcherPoint);
    pitcher.setFillColor({22, 62, 45});
    pitcher.setOutlineColor(sf::Color::White);
    pitcher.setOutlineThickness(1.5f);
    window.draw(pitcher);
    drawText(window, font, "Pitcher", {1078.0f, 394.0f}, 11, {82, 93, 88});

    sf::RectangleShape moundLine({54.0f, 3.0f});
    moundLine.setOrigin({27.0f, 1.5f});
    moundLine.setPosition({pitcherPoint.x, pitcherPoint.y + 18.0f});
    moundLine.setFillColor({174, 116, 64});
    window.draw(moundLine);

    drawZoneGrid(window, font, zoneOrigin, zoneSize);
    drawText(window, font, "Batter", {1084.0f, 641.0f}, 11, {82, 93, 88});

    if (!logs.empty()) {
        drawPitchLocationHistory(window, logs, zoneOrigin, zoneSize);
    }

    if (plan.has_value() && plan->hasPitch) {
        const auto& pitch = plan->pitch;
        drawPitchPath(window, pitch, pitcherPoint, zoneOrigin, zoneSize, elapsed);

        std::ostringstream label;
        label << pitch.pitchType << " "
              << static_cast<int>(std::round(pitch.velocity))
              << "  Z" << pitch.zoneNumber;
        drawText(window, font, fitLine(label.str(), 25), {960.0f, 404.0f}, 13, Ink, sf::Text::Bold);

        const std::string result = pitch.isBall ? "Ball"
                                : pitch.isInPlay ? "In Play"
                                                 : "Strike";
        drawText(window, font, result, {960.0f, 420.0f}, 13, pitchAnimationColor(pitch), sf::Text::Bold);
    }
}

std::string throwingHandLabel(joji::ThrowingHand hand) {
    return hand == joji::ThrowingHand::Left ? "LHP" : "RHP";
}


sf::Vector2f largePitchLocationPoint(double locationX,
                                     double locationZ,
                                     sf::Vector2f zoneOrigin,
                                     sf::Vector2f zoneSize) {
    const float nX = static_cast<float>((locationX + 1.25) / 2.5);
    const float nZ = static_cast<float>((3.9 - locationZ) / 2.8);
    return {
        zoneOrigin.x + std::clamp(nX, 0.0f, 1.0f) * zoneSize.x,
        zoneOrigin.y + std::clamp(nZ, 0.0f, 1.0f) * zoneSize.y
    };
}

sf::Vector2f largePitchPathPoint(const joji::PitchAnimation& pitch,
                                 const joji::AnimationPoint& point,
                                 sf::Vector2f releasePoint,
                                 sf::Vector2f zoneOrigin,
                                 sf::Vector2f zoneSize) {
    const sf::Vector2f endpoint = largePitchLocationPoint(pitch.endLocationX,
                                                          pitch.endLocationZ,
                                                          zoneOrigin,
                                                          zoneSize);
    const float progress = std::clamp(static_cast<float>((60.5 - point.y) / 60.5), 0.0f, 1.0f);

    // Pitch-type-specific break amplification
    float xBreakMult = 1.0f;
    float zBreakMult = 1.0f;
    float liftMult   = 1.0f;
    float breakPow   = 2.0f;
    const std::string& pt = pitch.pitchType;
    if (pt == "slider") {
        xBreakMult = 2.4f;   // strong lateral late break
        zBreakMult = 1.1f;
        liftMult   = 0.65f;  // slight drop
        breakPow   = 2.8f;   // very late break
    } else if (pt == "curveball") {
        xBreakMult = 0.9f;
        zBreakMult = 3.0f;   // pronounced 12-6 drop
        liftMult   = 0.12f;  // minimal arc, ball tumbles down
        breakPow   = 2.0f;
    } else if (pt == "changeup") {
        xBreakMult = 1.2f;
        zBreakMult = 2.2f;   // sinking action
        liftMult   = 0.45f;  // reduced arc vs fastball
        breakPow   = 2.0f;
    } else if (pt == "cutter") {
        xBreakMult = 1.6f;   // modest late cut
        zBreakMult = 0.8f;
        liftMult   = 0.90f;
        breakPow   = 2.2f;
    }
    // fastball: defaults (mostly straight, natural arc)

    const float breakShape = std::pow(progress, breakPow);
    const float movementPixelsX = static_cast<float>((point.x - pitch.endLocationX * progress) * 58.0 * xBreakMult);
    const float movementPixelsZ = static_cast<float>((point.z - (6.0 + (pitch.endLocationZ - 6.0) * progress)) * -34.0 * zBreakMult);
    const float speedLift = std::clamp(static_cast<float>((pitch.velocity - 78.0) * 0.7), -10.0f, 18.0f);
    const float lift = std::sin(progress * 3.14159265f) * (30.0f + speedLift) * liftMult;
    return {
        releasePoint.x + (endpoint.x - releasePoint.x) * progress + movementPixelsX * breakShape,
        releasePoint.y + (endpoint.y - releasePoint.y) * progress - lift + movementPixelsZ * breakShape
    };
}

void drawLargeHomePlate(sf::RenderWindow& window, sf::Vector2f center) {
    sf::ConvexShape plate;
    plate.setPointCount(5);
    plate.setPoint(0, {center.x - 48.0f, center.y - 24.0f});
    plate.setPoint(1, {center.x + 48.0f, center.y - 24.0f});
    plate.setPoint(2, {center.x + 38.0f, center.y + 22.0f});
    plate.setPoint(3, {center.x, center.y + 48.0f});
    plate.setPoint(4, {center.x - 38.0f, center.y + 22.0f});
    plate.setFillColor({238, 232, 216});
    plate.setOutlineColor(Ink);
    plate.setOutlineThickness(2.0f);
    window.draw(plate);
}

void drawBatterLimb(sf::RenderWindow& window,
                    sf::Vector2f start,
                    sf::Vector2f end,
                    float thickness,
                    sf::Color color,
                    sf::Color outline = sf::Color::Transparent) {
    const sf::Vector2f delta{end.x - start.x, end.y - start.y};
    const float length = std::sqrt(delta.x * delta.x + delta.y * delta.y);
    if (length <= 0.01f) {
        return;
    }

    sf::RectangleShape limb({length, thickness});
    limb.setOrigin({0.0f, thickness * 0.5f});
    limb.setPosition(start);
    limb.setRotation(sf::radians(std::atan2(delta.y, delta.x)));
    limb.setFillColor(color);
    if (outline != sf::Color::Transparent) {
        limb.setOutlineColor(outline);
        limb.setOutlineThickness(1.2f);
    }
    window.draw(limb);
}

void drawBatterFigure(sf::RenderWindow& window,
                      const sf::Font& font,
                      sf::Vector2f plateCenter,
                      bool rightHanded,
                      BatterPose pose = BatterPose::Stance) {
    const float side = rightHanded ? 1.0f : -1.0f; // +1 = stands left, faces right
    const float anchorX = plateCenter.x - side * 245.0f;
    const sf::Color uniform{245, 245, 238};
    const sf::Color trim{220, 72, 66};
    const float baseY = plateCenter.y;

    // ── pose-dependent parameters ─────────────────────────────────
    // Stance:       ready, bat behind back shoulder
    // Impact:       bat through the ball, arms extended toward zone
    // FollowThrough: bat has wrapped past contact (miss)
    float torsoAngle;
    sf::Vector2f hands, batTip;

    switch (pose) {
        case BatterPose::Impact:
            torsoAngle   = side * -18.0f;
            hands        = {anchorX + side * 68.0f, baseY - 212.0f};
            batTip       = {anchorX + side * 176.0f, baseY - 218.0f};
            break;
        case BatterPose::FollowThrough:
            torsoAngle   = side * -24.0f;
            hands        = {anchorX + side * 30.0f, baseY - 250.0f};
            batTip       = {anchorX - side * 12.0f, baseY - 354.0f};
            break;
        default: // Stance
            torsoAngle   = side * -5.0f;
            hands        = {anchorX + side * 40.0f, baseY - 226.0f};
            batTip       = {anchorX - side * 34.0f, baseY - 362.0f};
            break;
    }

    const sf::Vector2f rearShoulder{anchorX - side * 20.0f, baseY - 214.0f};
    const sf::Vector2f frontShoulder{anchorX + side * 20.0f, baseY - 208.0f};

    // Bat first (drawn behind body)
    drawBatterLimb(window, hands, batTip, 8.0f, {126, 82, 42});

    sf::CircleShape head(24.0f);
    head.setOrigin({24.0f, 24.0f});
    head.setPosition({anchorX + side * 12.0f, baseY - 258.0f});
    head.setFillColor({60, 64, 65});
    window.draw(head);

    sf::RectangleShape torso({58.0f, 104.0f});
    torso.setOrigin({29.0f, 12.0f});
    torso.setPosition({anchorX, baseY - 218.0f});
    torso.setRotation(sf::degrees(torsoAngle));
    torso.setFillColor(uniform);
    torso.setOutlineColor(Ink);
    torso.setOutlineThickness(2.0f);
    window.draw(torso);

    drawBatterLimb(window, rearShoulder, hands, 11.0f, trim);
    drawBatterLimb(window, frontShoulder, {hands.x - side * 8.0f, hands.y + 8.0f}, 11.0f, trim);

    sf::CircleShape handDot(7.0f);
    handDot.setOrigin({7.0f, 7.0f});
    handDot.setPosition(hands);
    handDot.setFillColor(trim);
    window.draw(handDot);

    drawBatterLimb(window,
                   {anchorX + side * 18.0f, baseY - 116.0f},
                   {anchorX + side * 34.0f, baseY - 20.0f},
                   14.0f, uniform, Ink);
    drawBatterLimb(window,
                   {anchorX - side * 18.0f, baseY - 116.0f},
                   {anchorX - side * 36.0f, baseY - 20.0f},
                   14.0f, uniform, Ink);

    const std::string label = rightHanded ? "RHB" : "LHB";
    drawText(window, font, label, {anchorX - 24.0f, baseY + 8.0f}, 16, Ink, sf::Text::Bold);
}

void drawPitchModeBatter(sf::RenderWindow& window,
                         const sf::Font& font,
                         joji::BattingSide side,
                         sf::Vector2f plateCenter,
                         BatterPose pose = BatterPose::Stance) {
    drawBatterFigure(window, font, plateCenter, side == joji::BattingSide::Right, pose);
}

void drawPitchModePitcher(sf::RenderWindow& window,
                          const sf::Font& font,
                          joji::ThrowingHand hand,
                          sf::Vector2f center,
                          sf::Vector2f releasePoint) {
    const float side = hand == joji::ThrowingHand::Right ? -1.0f : 1.0f;
    sf::CircleShape mound(58.0f);
    mound.setOrigin({58.0f, 58.0f});
    mound.setPosition({640.0f, 196.0f});
    mound.setScale({2.4f, 0.42f});
    mound.setFillColor({208, 169, 124});
    window.draw(mound);

    sf::CircleShape head(14.0f);
    head.setOrigin({14.0f, 14.0f});
    head.setPosition({center.x - side * 12.0f, center.y - 34.0f});
    head.setFillColor({60, 64, 65});
    window.draw(head);

    sf::RectangleShape body({28.0f, 52.0f});
    body.setOrigin({14.0f, 4.0f});
    body.setPosition({center.x - side * 14.0f, center.y - 20.0f});
    body.setFillColor({220, 230, 224});
    body.setOutlineColor(Ink);
    body.setOutlineThickness(1.4f);
    window.draw(body);

    // Shoulder/release circle — offset from body center toward throwing arm
    sf::CircleShape release(7.0f);
    release.setOrigin({7.0f, 7.0f});
    release.setPosition(releasePoint);
    release.setFillColor(sf::Color::White);
    release.setOutlineColor({220, 72, 66});
    release.setOutlineThickness(2.0f);
    window.draw(release);

    drawText(window, font, throwingHandLabel(hand), {center.x - 28.0f, center.y - 72.0f}, 16, Ink, sf::Text::Bold);
}

void drawLargeStrikeZone(sf::RenderWindow& window,
                         const sf::Font& font,
                         sf::Vector2f origin,
                         sf::Vector2f size) {
    drawZoneGrid(window, font, origin, size, false); // zone numbers removed; landing cell highlighted separately
}

void drawPitchModePath(sf::RenderWindow& window,
                       const joji::PitchAnimation& pitch,
                       sf::Vector2f releasePoint,
                       sf::Vector2f zoneOrigin,
                       sf::Vector2f zoneSize,
                       float elapsed) {
    const auto& points = pitch.points;
    if (points.empty()) {
        return;
    }

    std::size_t idx = 0;
    while (idx + 1 < points.size()
           && points[idx + 1].timeSeconds <= static_cast<double>(elapsed)) {
        ++idx;
    }

    const sf::Color color = pitchAnimationColor(pitch);
    sf::VertexArray path(sf::PrimitiveType::LineStrip);
    for (std::size_t i = 0; i <= idx; ++i) {
        sf::Color c = color;
        c.a = static_cast<std::uint8_t>(120 + 120 * i / std::max<std::size_t>(idx + 1, 1));
        path.append(sf::Vertex{largePitchPathPoint(pitch, points[i], releasePoint, zoneOrigin, zoneSize), c});
    }
    if (path.getVertexCount() >= 2) {
        window.draw(path);
    }

    for (std::size_t i = 1; i <= idx; ++i) {
        const sf::Vector2f dot = largePitchPathPoint(pitch, points[i], releasePoint, zoneOrigin, zoneSize);
        sf::CircleShape ghost(3.5f);
        ghost.setOrigin({3.5f, 3.5f});
        ghost.setPosition(dot);
        ghost.setFillColor({255, 255, 255, 165});
        window.draw(ghost);
    }

    const sf::Vector2f current = largePitchPathPoint(pitch, points[idx], releasePoint, zoneOrigin, zoneSize);
    sf::CircleShape ball(8.0f);
    ball.setOrigin({8.0f, 8.0f});
    ball.setPosition(current);
    ball.setFillColor(sf::Color::White);
    ball.setOutlineColor(color);
    ball.setOutlineThickness(2.2f);
    window.draw(ball);

    // Hollow ring = final landing target (distinct from white filled ball = current pos)
    const sf::Vector2f endpoint = largePitchLocationPoint(pitch.endLocationX,
                                                          pitch.endLocationZ,
                                                          zoneOrigin,
                                                          zoneSize);
    sf::CircleShape endDot(11.0f);
    endDot.setOrigin({11.0f, 11.0f});
    endDot.setPosition(endpoint);
    endDot.setFillColor(sf::Color::Transparent);
    endDot.setOutlineColor(color);
    endDot.setOutlineThickness(3.0f);
    window.draw(endDot);
}

std::string toUpper(std::string s) {
    for (char& c : s) {
        if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
    }
    return s;
}

// Determine batter pose from pitch result and animation progress.
// Before ball is halfway to plate: always Stance.
// After: Take/CalledStrike → Stance, SwingingStrike → FollowThrough, Foul/InPlay → Impact.
BatterPose pitchBatterPose(const joji::PitchAnimation& pitch, float elapsed) {
    const float triggerFrac = 0.58f;
    if (elapsed < static_cast<float>(pitch.durationSeconds) * triggerFrac) {
        return BatterPose::Stance;
    }
    if (pitch.isBall) return BatterPose::Stance;
    if (pitch.isInPlay) return BatterPose::Impact;
    const std::string r = toUpper(pitch.result);
    if (r == "SWINGING STRIKE") return BatterPose::FollowThrough;
    if (r == "FOUL")            return BatterPose::Impact;
    return BatterPose::Stance;  // called strike = take
}

void drawZoneHighlight(sf::RenderWindow& window,
                       sf::Vector2f origin,
                       sf::Vector2f size,
                       int zoneNumber,
                       sf::Color color) {
    if (zoneNumber < 1 || zoneNumber > 9) return;
    const int row = (zoneNumber - 1) / 3;
    const int col = (zoneNumber - 1) % 3;
    const float cellW = size.x / 3.0f;
    const float cellH = size.y / 3.0f;
    sf::RectangleShape cell({cellW - 2.0f, cellH - 2.0f});
    cell.setPosition({origin.x + col * cellW + 1.0f, origin.y + row * cellH + 1.0f});
    cell.setFillColor({color.r, color.g, color.b, 55});
    cell.setOutlineColor({color.r, color.g, color.b, 180});
    cell.setOutlineThickness(2.5f);
    window.draw(cell);
}

// Small base diamond: 1B=right, 2B=top, 3B=left. Occupied=gold, empty=dark.
void drawBaseDiamond(sf::RenderWindow& window,
                     const std::array<std::optional<std::string>, 3>& bases,
                     sf::Vector2f center) {
    const float gap = 17.0f;
    const float sz  = 10.0f;
    const std::array<sf::Vector2f, 3> pos = {
        sf::Vector2f{center.x + gap, center.y},      // 1B
        sf::Vector2f{center.x,       center.y - gap}, // 2B
        sf::Vector2f{center.x - gap, center.y},      // 3B
    };
    for (int i = 0; i < 3; ++i) {
        sf::RectangleShape sq({sz, sz});
        sq.setOrigin({sz * 0.5f, sz * 0.5f});
        sq.setPosition(pos[i]);
        sq.setRotation(sf::degrees(45.0f));
        if (bases[i].has_value()) {
            sq.setFillColor({230, 195, 55});
            sq.setOutlineColor({255, 225, 90});
            sq.setOutlineThickness(1.0f);
        } else {
            sq.setFillColor({35, 42, 38, 210});
            sq.setOutlineColor({120, 130, 126, 130});
            sq.setOutlineThickness(1.0f);
        }
        window.draw(sq);
    }
}

void drawIndicatorDots(sf::RenderWindow& window,
                       const sf::Font& font,
                       int filled,
                       int total,
                       sf::Color activeColor,
                       sf::Vector2f pos,
                       const std::string& label) {
    const float r = 7.0f;
    const float spacing = 20.0f;
    for (int i = 0; i < total; ++i) {
        sf::CircleShape dot(r);
        dot.setOrigin({r, r});
        dot.setPosition({pos.x + i * spacing, pos.y});
        if (i < filled) {
            dot.setFillColor(activeColor);
            dot.setOutlineThickness(0.0f);
        } else {
            dot.setFillColor({22, 28, 24, 200});
            dot.setOutlineColor({activeColor.r, activeColor.g, activeColor.b, 80});
            dot.setOutlineThickness(1.5f);
        }
        window.draw(dot);
    }
    drawText(window, font, label,
             {pos.x + total * spacing + 8.0f, pos.y - 9.0f},
             13, {128, 138, 134});
}

// panelOnLeft=true → LHB side (batter on right, panel on left)
// panelOnLeft=false → RHB side (batter on left, panel on right)
void drawPitchInfoPanel(sf::RenderWindow& window,
                        const sf::Font& font,
                        const joji::PitchAnimation& pitch,
                        sf::Color resultColor,
                        bool panelOnLeft,
                        int balls,
                        int strikes,
                        int outs,
                        const std::string& inningLabel,
                        const std::array<std::optional<std::string>, 3>& bases) {
    const float panelX = panelOnLeft ? 55.0f : 885.0f;
    const sf::Vector2f panelOrigin{panelX, 262.0f};
    const sf::Vector2f panelSize{335.0f, 268.0f};

    sf::RectangleShape panel(panelSize);
    panel.setPosition(panelOrigin);
    panel.setFillColor({14, 20, 16, 215});
    panel.setOutlineColor(resultColor);
    panel.setOutlineThickness(2.5f);
    window.draw(panel);

    const float tx = panelOrigin.x + 20.0f;
    float ty = panelOrigin.y + 13.0f;

    // Inning label + base diamond (top-right of panel)
    drawText(window, font, inningLabel, {tx, ty}, 14, {140, 150, 146});
    drawBaseDiamond(window, bases, {panelOrigin.x + panelSize.x - 46.0f, ty + 18.0f});
    ty += 22.0f;

    // Matchup: "RHP vs LHB  Opposite" / "LHP vs LHB  Same"
    {
        const bool rhp = pitch.pitcherHand == joji::ThrowingHand::Right;
        const bool rhb = pitch.batterSide  == joji::BattingSide::Right;
        const bool same = (rhp && rhb) || (!rhp && !rhb);
        const std::string handStr = std::string(rhp ? "RHP" : "LHP") + " vs " + (rhb ? "RHB" : "LHB");
        const std::string advStr  = same ? "Same" : "Opposite";
        const sf::Color advColor  = same ? sf::Color{195, 128, 72} : sf::Color{58, 178, 118};
        drawText(window, font, handStr, {tx, ty}, 12, {160, 168, 164});
        drawText(window, font, advStr,  {tx + 102.0f, ty}, 12, advColor, sf::Text::Bold);
    }
    ty += 18.0f;

    // Pitch type + speed on same line
    drawText(window, font, toUpper(pitch.pitchType), {tx, ty}, 27, {238, 232, 216}, sf::Text::Bold);
    drawText(window, font,
             std::to_string(static_cast<int>(std::round(pitch.velocity))) + " MPH",
             {tx + 164.0f, ty + 4.0f}, 18, {160, 172, 168});
    ty += 42.0f;

    // Count / Outs dot indicators
    drawIndicatorDots(window, font, balls,   4, {54, 151, 230},  {tx, ty}, "BALLS");
    ty += 22.0f;
    drawIndicatorDots(window, font, strikes, 3, {230, 200, 48},  {tx, ty}, "STRIKES");
    ty += 22.0f;
    drawIndicatorDots(window, font, outs,    3, {210, 60, 55},   {tx, ty}, "OUTS");
    ty += 30.0f;

    // Result
    const std::string result = pitch.result.empty()
        ? (pitch.isBall ? "BALL" : pitch.isInPlay ? "IN PLAY" : "STRIKE")
        : toUpper(pitch.result);
    drawText(window, font, result, {tx, ty}, 24, resultColor, sf::Text::Bold);
    ty += 34.0f;

    // Zone + movement (small)
    std::ostringstream zoneInfo;
    zoneInfo << "ZONE  " << pitch.zoneNumber
             << "    MV  " << static_cast<int>(std::round(pitch.movementX)) << "x  "
             << static_cast<int>(std::round(pitch.movementZ)) << "z";
    drawText(window, font, zoneInfo.str(), {tx, ty}, 13, {100, 110, 106});
}

void drawPitchViewMode(sf::RenderWindow& window,
                       const sf::Font& font,
                       const joji::GameEngine& engine,
                       const std::optional<joji::PlayResult>& currentPlay,
                       float elapsed,
                       bool autoPlay,
                       bool complete) {
    window.clear({202, 226, 238});

    sf::RectangleShape dirtBand({WindowWidth, 570.0f});
    dirtBand.setPosition({0.0f, 250.0f});
    dirtBand.setFillColor({228, 194, 151});
    window.draw(dirtBand);

    sf::CircleShape horizon(430.0f);
    horizon.setOrigin({430.0f, 430.0f});
    horizon.setPosition({640.0f, 430.0f});
    horizon.setScale({1.65f, 0.28f});
    horizon.setFillColor({238, 216, 184});
    window.draw(horizon);

    const std::optional<joji::AnimationPlan>& plan = engine.latestAnimationPlan();
    const joji::PitchAnimation* pitch = plan.has_value() && plan->hasPitch ? &plan->pitch : nullptr;
    const joji::ThrowingHand pitcherHand = pitch ? pitch->pitcherHand : engine.currentPitcher().throwingHand;
    const joji::BattingSide batterSide = pitch ? pitch->batterSide : joji::BattingSide::Right;
    const float pitcherX = pitcherHand == joji::ThrowingHand::Right ? 548.0f : 732.0f;
    // Body center stays at pitcherX; release point offsets to throwing-arm shoulder
    // RHP (side=-1): shoulder to the right (+X); LHP (side=+1): shoulder to the left (-X)
    const float pitcherSide = pitcherHand == joji::ThrowingHand::Right ? -1.0f : 1.0f;
    const sf::Vector2f pitcherCenter{pitcherX, 212.0f};
    const sf::Vector2f releasePoint{pitcherX - pitcherSide * 26.0f, 222.0f};
    // Zone enlarged ~28% — strike zone is the hero of this view
    const sf::Vector2f zoneOrigin{499.0f, 335.0f};
    const sf::Vector2f zoneSize{282.0f, 320.0f};
    const sf::Vector2f plateCenter{640.0f, 690.0f};

    const BatterPose batterPose = pitch ? pitchBatterPose(*pitch, elapsed) : BatterPose::Stance;

    drawPitchModePitcher(window, font, pitcherHand, pitcherCenter, releasePoint);
    drawPitchModeBatter(window, font, batterSide, plateCenter, batterPose);
    drawLargeStrikeZone(window, font, zoneOrigin, zoneSize);
    if (pitch) {
        drawZoneHighlight(window, zoneOrigin, zoneSize, pitch->zoneNumber, pitchAnimationColor(*pitch));
    }
    drawLargeHomePlate(window, plateCenter);

    // Panel on opposite side from batter: RHB (left) → panel right; LHB (right) → panel left
    const bool panelOnLeft = (batterSide == joji::BattingSide::Left);
    const float infoX = panelOnLeft ? 80.0f : 910.0f;

    const joji::GameState& gameState = engine.state();
    const bool abActive = engine.isAtBatInProgress() || engine.hasPendingAtBatResult();
    const int ballsCount   = abActive ? engine.currentAtBat().count.balls   : 0;
    const int strikesCount = abActive ? engine.currentAtBat().count.strikes : 0;
    const std::string inningLbl = halfLabel(gameState);

    if (pitch) {
        drawPitchModePath(window, *pitch, releasePoint, zoneOrigin, zoneSize, elapsed);
        drawPitchInfoPanel(window, font, *pitch, pitchAnimationColor(*pitch), panelOnLeft,
                           ballsCount, strikesCount, gameState.outs, inningLbl, gameState.bases);
    } else {
        drawText(window, font, "Pitch View",     {infoX, 350.0f}, 32, Ink, sf::Text::Bold);
        drawText(window, font, "Space to pitch", {infoX, 398.0f}, 22, {56, 76, 70});
    }

    drawText(window, font, halfLabel(engine.state()), {34.0f, 30.0f}, 28, Ink, sf::Text::Bold);
    drawText(window,
             font,
             engine.awayTeamName() + " " + std::to_string(engine.state().awayScore)
                 + "   " + engine.homeTeamName() + " " + std::to_string(engine.state().homeScore),
             {34.0f, 70.0f},
             18,
             {56, 76, 70},
             sf::Text::Bold);
    if (currentPlay.has_value()) {
        drawText(window, font, joji::toString(currentPlay->type), {34.0f, 104.0f}, 20, outcomeColor(currentPlay->type), sf::Text::Bold);
    }

    const char* hint = complete
        ? "P field  R new game"
        : (autoPlay ? "P field  Space next  A auto:on  R new game"
                    : "P field  Space next  A auto  R new game");
    drawText(window, font, hint, {34.0f, 790.0f}, 16, Ink, sf::Text::Bold);
}

void drawLog(sf::RenderWindow& window, const sf::Font& font, const std::vector<joji::GameLog>& logs) {
    drawText(window, font, "Play log", {960.0f, 773.0f}, 13, {82, 93, 88}, sf::Text::Bold);
    const std::size_t start = logs.size() > 1 ? logs.size() - 1 : 0;
    float y = 790.0f;
    for (std::size_t i = start; i < logs.size(); ++i) {
        drawText(window, font, fitLine(logs[i].text, 42), {960.0f, y}, 13, Ink);
        y += 22.0f;
    }
}

std::string baseRunningEventLabel(const joji::BaseRunningEvent& ev) {
    const auto baseName = [](int b) -> std::string {
        switch (b) {
            case 1: return "1B";
            case 2: return "2B";
            case 3: return "3B";
            case 4: return "home";
            default: return "?";
        }
    };
    switch (ev.type) {
        case joji::BaseRunningEventType::StolenBase:
            return "SB  " + ev.runnerName + " steals " + baseName(ev.toBase);
        case joji::BaseRunningEventType::CaughtStealing:
            return "CS  " + ev.runnerName + " caught at " + baseName(ev.toBase);
        case joji::BaseRunningEventType::WildPitch:
            return ev.scored ? "WP  runner scores!" : "WP  runners advance";
        case joji::BaseRunningEventType::PassedBall:
            return ev.scored ? "PB  runner scores!" : "PB  runners advance";
        case joji::BaseRunningEventType::Balk:
            return "BALK  all runners advance";
        case joji::BaseRunningEventType::PickoffAttempt:
            return "PO  " + ev.runnerName + " safe at " + baseName(ev.fromBase) + "B";
        case joji::BaseRunningEventType::PickoffOut:
            return "PO  " + ev.runnerName + " picked off at " + baseName(ev.fromBase) + "B!";
    }
    return "";
}

sf::Color baseRunningEventColor(const joji::BaseRunningEvent& ev) {
    switch (ev.type) {
        case joji::BaseRunningEventType::StolenBase:
            return {80, 190, 100};    // green
        case joji::BaseRunningEventType::CaughtStealing:
            return {220, 72, 66};     // red
        case joji::BaseRunningEventType::WildPitch:
        case joji::BaseRunningEventType::PassedBall:
            return {235, 200, 60};    // yellow
        case joji::BaseRunningEventType::Balk:
            return {235, 116, 48};    // orange
        case joji::BaseRunningEventType::PickoffAttempt:
            return {160, 160, 160};   // gray
        case joji::BaseRunningEventType::PickoffOut:
            return {220, 72, 66};     // red (same as CS)
    }
    return sf::Color::White;
}

// Draws inter-pitch base running events (SB/CS/WP/PB/Balk) in the right panel
// below the scoreboard content. Only displayed while the events are fresh
// (i.e., during the current pitch animation phase).
void drawBaseRunningEvents(sf::RenderWindow& window,
                           const sf::Font& font,
                           const std::vector<joji::BaseRunningEvent>& events,
                           float elapsed,
                           float animDuration) {
    if (events.empty()) return;

    // Fade out after the animation ends (0.6 s grace period)
    const float fadeStart = animDuration + 0.0f;
    const float fadeEnd   = animDuration + 0.6f;
    float alpha = 1.0f;
    if (elapsed > fadeEnd) return;
    if (elapsed > fadeStart)
        alpha = 1.0f - (elapsed - fadeStart) / (fadeEnd - fadeStart);

    const auto applyAlpha = [&](sf::Color c) -> sf::Color {
        c.a = static_cast<std::uint8_t>(alpha * 255.0f);
        return c;
    };

    float y = 415.0f;
    for (const auto& ev : events) {
        const std::string label = baseRunningEventLabel(ev);
        if (label.empty()) continue;
        const sf::Color col = applyAlpha(baseRunningEventColor(ev));
        drawText(window, font, fitLine(label, 38), {960.0f, y}, 14, col, sf::Text::Bold);
        y += 17.0f;
    }
}

// ── チーム選択画面 ─────────────────────────────────────────────────────────

void drawTeamSelectScreen(sf::RenderWindow& window,
                          const sf::Font& font,
                          const std::vector<joji::Team>& teams,
                          int awayIdx, int homeIdx,
                          int activeCol,   // 0=away, 1=home
                          int seriesLen) {
    window.clear({14, 22, 18});

    // タイトル
    sf::Text title(font, "JOJI  BASEBALL  ENGINE", 38);
    title.setStyle(sf::Text::Bold);
    title.setFillColor({200, 185, 80});
    {
        const sf::FloatRect b = title.getLocalBounds();
        title.setOrigin({b.position.x + b.size.x / 2.0f, b.position.y + b.size.y / 2.0f});
        title.setPosition({640.0f, 68.0f});
    }
    window.draw(title);

    drawText(window, font, "SELECT TEAMS", {640.0f - 80.0f, 114.0f}, 16, {90, 100, 96}, sf::Text::Bold);

    // カラム見出し
    const float colX[2] = {260.0f, 740.0f};
    const sf::Color colLabel[2] = {
        activeCol == 0 ? sf::Color{243, 185, 54} : sf::Color{130, 140, 136},
        activeCol == 1 ? sf::Color{243, 185, 54} : sf::Color{130, 140, 136}
    };
    drawText(window, font, "AWAY", {colX[0] - 30.0f, 148.0f}, 18, colLabel[0], sf::Text::Bold);
    drawText(window, font, "HOME", {colX[1] - 30.0f, 148.0f}, 18, colLabel[1], sf::Text::Bold);

    // 左右列のアクティブ下線
    for (int c = 0; c < 2; ++c) {
        sf::RectangleShape line({200.0f, 3.0f});
        line.setPosition({colX[c] - 100.0f, 172.0f});
        line.setFillColor(activeCol == c ? sf::Color{243, 185, 54, 200} : sf::Color{60, 70, 66, 120});
        window.draw(line);
    }

    const int n = static_cast<int>(teams.size());
    for (int i = 0; i < n; ++i) {
        const float y = 194.0f + i * 58.0f;
        const std::string& name = teams[static_cast<std::size_t>(i)].name();

        for (int c = 0; c < 2; ++c) {
            const bool selected = (c == 0) ? (i == awayIdx) : (i == homeIdx);
            const bool active   = (c == activeCol);
            const bool sameAsOther = (c == 0) ? (i == homeIdx) : (i == awayIdx);

            sf::Color bg{0, 0, 0, 0};
            sf::Color nameCol{100, 108, 104};

            if (selected) {
                bg = active ? sf::Color{40, 56, 44, 230} : sf::Color{28, 38, 32, 180};
                nameCol = active ? sf::Color{243, 185, 54} : sf::Color{200, 195, 170};

                sf::RectangleShape box({320.0f, 46.0f});
                box.setPosition({colX[c] - 160.0f, y - 4.0f});
                box.setFillColor(bg);
                box.setOutlineColor(active ? sf::Color{200, 165, 50, 200} : sf::Color{80, 90, 86, 130});
                box.setOutlineThickness(active ? 2.0f : 1.0f);
                window.draw(box);
            } else if (sameAsOther) {
                nameCol = {55, 62, 58};  // 対戦相手と同じチームは暗く
            }

            const std::string label = (selected && active ? "> " : "  ") + name;
            drawText(window, font, label, {colX[c] - 148.0f, y + 4.0f},
                     selected ? 19 : 16, nameCol,
                     selected ? sf::Text::Bold : sf::Text::Regular);

            if (selected) {
                // チーム略称バッジ
                const std::string abbr = name.size() >= 3 ? name.substr(0, 3) : name;
                drawText(window, font, abbr, {colX[c] + 110.0f, y + 6.0f}, 13,
                         active ? sf::Color{180, 140, 40} : sf::Color{90, 98, 94});
            }
        }
    }

    // シリーズ戦数セレクタ
    const float serY = 194.0f + n * 58.0f + 20.0f;
    drawText(window, font, "Series length:", {430.0f, serY}, 16, {100, 110, 106}, sf::Text::Bold);
    for (int len : {3, 5, 7}) {
        const float bx = 594.0f + (len - 3) * 44.0f;
        const bool sel = (len == seriesLen);
        sf::RectangleShape btn({36.0f, 28.0f});
        btn.setPosition({bx, serY - 2.0f});
        btn.setFillColor(sel ? sf::Color{40, 56, 44, 200} : sf::Color{20, 28, 24, 100});
        btn.setOutlineColor(sel ? sf::Color{200, 165, 50, 200} : sf::Color{60, 68, 64, 140});
        btn.setOutlineThickness(sel ? 2.0f : 1.0f);
        window.draw(btn);
        drawText(window, font, std::to_string(len), {bx + 12.0f, serY + 2.0f}, 16,
                 sel ? sf::Color{243, 185, 54} : sf::Color{110, 120, 116},
                 sel ? sf::Text::Bold : sf::Text::Regular);
    }

    // 操作ヒント
    drawText(window, font,
             "Up/Down: select  Left/Right: switch column  3/5/7: series  Space: start",
             {640.0f - 288.0f, 790.0f}, 14, {80, 90, 86});
}

// ── シリーズ中間オーバーレイ ──────────────────────────────────────────────

void drawSeriesOverlay(sf::RenderWindow& window,
                       const sf::Font& font,
                       const std::string& awayName,
                       const std::string& homeName,
                       const std::vector<std::pair<int,int>>& scores,
                       int awayWins, int homeWins,
                       int seriesLen, bool seriesOver) {
    // 半透明背景
    sf::RectangleShape bg({900.0f, 500.0f});
    bg.setPosition({190.0f, 160.0f});
    bg.setFillColor({12, 18, 14, 235});
    bg.setOutlineColor({80, 160, 100, 180});
    bg.setOutlineThickness(2.0f);
    window.draw(bg);

    const std::string awayAbbr = awayName.size() >= 3 ? awayName.substr(0, 3) : awayName;
    const std::string homeAbbr = homeName.size() >= 3 ? homeName.substr(0, 3) : homeName;
    const int needed = seriesLen / 2 + 1;

    // タイトル
    const std::string title = seriesOver
        ? (awayWins >= needed ? awayName : homeName) + "  wins the series!"
        : "Series  " + awayAbbr + " " + std::to_string(awayWins) + " - " + std::to_string(homeWins) + " " + homeAbbr;
    drawText(window, font, title, {640.0f - 200.0f, 188.0f}, 24,
             seriesOver ? sf::Color{243, 185, 54} : sf::Color{220, 215, 200}, sf::Text::Bold);

    // ゲームごとのスコア
    const float gx = 250.0f;
    float gy = 238.0f;
    drawText(window, font, "Game", {gx, gy}, 13, {100, 108, 104});
    drawText(window, font, awayAbbr, {gx + 100.0f, gy}, 13, {100, 108, 104});
    drawText(window, font, homeAbbr, {gx + 170.0f, gy}, 13, {100, 108, 104});
    drawText(window, font, "Winner", {gx + 240.0f, gy}, 13, {100, 108, 104});
    gy += 22.0f;

    for (std::size_t i = 0; i < scores.size(); ++i) {
        const auto [as, hs] = scores[i];
        const bool awayWon = as > hs;
        const sf::Color rowCol = awayWon ? sf::Color{200, 210, 255} : sf::Color{255, 200, 200};
        drawText(window, font, "Game " + std::to_string(i + 1), {gx, gy}, 14, {180, 188, 184});
        drawText(window, font, std::to_string(as), {gx + 100.0f, gy}, 15,
                 awayWon ? sf::Color{243, 185, 54} : sf::Color{140, 148, 144}, sf::Text::Bold);
        drawText(window, font, std::to_string(hs), {gx + 170.0f, gy}, 15,
                 !awayWon ? sf::Color{243, 185, 54} : sf::Color{140, 148, 144}, sf::Text::Bold);
        drawText(window, font, awayWon ? awayAbbr : homeAbbr, {gx + 240.0f, gy}, 14, rowCol);
        gy += 26.0f;
    }

    // シリーズ勝敗バー
    gy += 12.0f;
    const float barTotalW = 400.0f;
    const float awayBarW = seriesLen > 0 ? barTotalW * awayWins / needed : 0.0f;
    const float homeBarW = seriesLen > 0 ? barTotalW * homeWins / needed : 0.0f;

    drawText(window, font, awayAbbr + " wins", {gx, gy}, 14, {160, 170, 200});
    sf::RectangleShape awayBar({std::min(awayBarW, barTotalW), 14.0f});
    awayBar.setPosition({gx + 100.0f, gy + 2.0f});
    awayBar.setFillColor({80, 120, 200, 200});
    window.draw(awayBar);
    drawText(window, font, std::to_string(awayWins) + "/" + std::to_string(needed),
             {gx + 510.0f, gy}, 13, {130, 140, 160});
    gy += 24.0f;

    drawText(window, font, homeAbbr + " wins", {gx, gy}, 14, {200, 150, 150});
    sf::RectangleShape homeBar({std::min(homeBarW, barTotalW), 14.0f});
    homeBar.setPosition({gx + 100.0f, gy + 2.0f});
    homeBar.setFillColor({200, 80, 80, 200});
    window.draw(homeBar);
    drawText(window, font, std::to_string(homeWins) + "/" + std::to_string(needed),
             {gx + 510.0f, gy}, 13, {160, 120, 120});

    // ヒント
    const std::string hint = seriesOver
        ? "Space: new series  T: team select"
        : "Space: next game  T: team select";
    drawText(window, font, hint, {640.0f - 160.0f, 610.0f}, 17, {100, 180, 120}, sf::Text::Bold);
}

// ── プレー結果バナー (打席結果後1.4秒) ───────────────────────────────────

void drawPlayResultBanner(sf::RenderWindow& window,
                          const sf::Font& font,
                          const std::string& playText,
                          const std::string& batterName,
                          sf::Color color,
                          float elapsed) {
    constexpr float kFadeIn  = 0.12f;
    constexpr float kHold    = 0.85f;
    constexpr float kFadeOut = 0.43f;
    constexpr float kTotal   = kFadeIn + kHold + kFadeOut;
    if (elapsed > kTotal) return;

    float alpha;
    if (elapsed < kFadeIn)       alpha = elapsed / kFadeIn;
    else if (elapsed < kFadeIn + kHold) alpha = 1.0f;
    else alpha = 1.0f - (elapsed - kFadeIn - kHold) / kFadeOut;
    alpha = std::clamp(alpha, 0.0f, 1.0f);

    const auto a = [&](sf::Color c) -> sf::Color {
        c.a = static_cast<std::uint8_t>(c.a * alpha);
        return c;
    };

    // 背景帯
    sf::RectangleShape bg({580.0f, 82.0f});
    bg.setPosition({170.0f, 325.0f});
    bg.setFillColor(a({12, 18, 14, 210}));
    bg.setOutlineColor(a({color.r, color.g, color.b, 180}));
    bg.setOutlineThickness(3.0f);
    window.draw(bg);

    // プレー名
    sf::Text txt(font, playText, 42);
    txt.setStyle(sf::Text::Bold);
    txt.setFillColor(a(color));
    {
        const sf::FloatRect b = txt.getLocalBounds();
        txt.setOrigin({b.position.x + b.size.x / 2.0f, b.position.y + b.size.y / 2.0f});
        txt.setPosition({460.0f, 348.0f});
    }
    window.draw(txt);

    // 打者名
    drawText(window, font, batterName, {460.0f - 80.0f, 376.0f}, 16, a({200, 205, 202}));
}

} // namespace

// ピッチログ: 打席中なら現在打席、それ以外なら最後の打席
const std::vector<joji::PitchLog>& currentPitchLogs(const joji::GameEngine& engine) {
    static const std::vector<joji::PitchLog> empty;
    if (engine.isAtBatInProgress() || engine.hasPendingAtBatResult()) {
        return engine.currentAtBat().pitchLogs;
    }
    if (engine.lastAtBat().has_value()) {
        return engine.lastAtBat()->pitchLogs;
    }
    return empty;
}

int main() {
    sf::Font font;
    if (!font.openFromFile("/System/Library/Fonts/Supplemental/Arial.ttf")) {
        return 1;
    }

    // ── チーム / シリーズ状態 ──────────────────────────────────────────────
    const auto teams = joji::allTeams();
    const int numTeams = static_cast<int>(teams.size());

    int awayIdx      = 0;
    int homeIdx      = 1;
    int selectCol    = 0;   // 0=away, 1=home
    int seriesLen    = 3;
    bool inTeamSelect    = true;
    bool showSeriesSummary = false;

    std::vector<std::pair<int,int>> seriesScores;  // {away, home} per game
    int awaySeriesWins = 0;
    int homeSeriesWins = 0;
    bool seriesOver = false;

    // シリーズリセットヘルパー
    auto resetSeries = [&]() {
        seriesScores.clear();
        awaySeriesWins = homeSeriesWins = 0;
        seriesOver = false;
        showSeriesSummary = false;
    };

    // ── エンジン (チーム確定後に初期化) ───────────────────────────────────
    auto makeEngine = [&]() {
        return joji::GameEngine{
            teams.at(static_cast<std::size_t>(awayIdx)),
            teams.at(static_cast<std::size_t>(homeIdx)),
            joji::Random{std::optional<std::uint32_t>{}}
        };
    };

    // プレースホルダー (チーム選択前は teams[0] vs teams[1] で初期化)
    joji::GameEngine engine = makeEngine();
    std::optional<joji::PlayResult> currentPlay;

    sf::RenderWindow window(sf::VideoMode({static_cast<unsigned int>(WindowWidth),
                                           static_cast<unsigned int>(WindowHeight)}),
                            "Joji Baseball Engine v1.0");
    window.setVerticalSyncEnabled(true);

    sf::Clock playClock;
    float replaySeekSeconds = 0.0f;
    bool replayPaused = false;
    bool autoPlay = false;
    VisualMode visualMode = VisualMode::Field;

    // ビュー切り替えフェード
    enum class ViewTransition { None, FadingOut, FadingIn };
    ViewTransition viewTransition = ViewTransition::None;
    VisualMode     targetMode     = VisualMode::Field;
    sf::Clock      transitionClock;
    constexpr float TransitionHalf = 0.18f; // 片道フェード秒数

    // 投手交代バナー
    std::optional<joji::GameEngine::PitcherChangeEvent> bannerEvent;
    sf::Clock bannerClock;

    // プレー結果バナー
    std::string playBannerText;
    std::string playBannerSub;
    sf::Color   playBannerColor{sf::Color::White};
    sf::Clock   playBannerClock;
    bool        playBannerActive = false;

    // ゲーム状態リセットヘルパー
    auto resetGame = [&]() {
        engine = makeEngine();
        currentPlay.reset();
        bannerEvent.reset();
        replaySeekSeconds = 0.0f;
        replayPaused = false;
        autoPlay = false;
        playBannerActive = false;
        visualMode = VisualMode::Field;
        playClock.restart();
    };

    // ── 1球進める ──────────────────────────────────────────────────────────
    auto advancePitch = [&]() {
        if (engine.isComplete()) return;

        if (engine.hasPendingAtBatResult()) {
            auto result = engine.applyPendingAtBatResult();
            if (result.has_value()) {
                currentPlay = result;
                // プレー結果バナー起動
                playBannerText  = joji::toString(result->type);
                playBannerSub   = result->batterName;
                playBannerColor = outcomeColor(result->type);
                playBannerClock.restart();
                playBannerActive = true;
            }
        } else {
            engine.simulateNextPitch();
            if (!engine.hasPendingAtBatResult()) {
                currentPlay.reset();
            }
        }
        replaySeekSeconds = 0.0f;
        replayPaused = false;
        playClock.restart();
    };

    auto replayDuration = [&]() {
        if (!engine.latestAnimationPlan().has_value()) return 0.0f;
        const auto& plan = *engine.latestAnimationPlan();
        return static_cast<float>(
            std::max(plan.totalDurationSeconds, plan.replayTimeline.durationSeconds));
    };

    auto replayElapsed = [&]() {
        return replayPaused
            ? replaySeekSeconds
            : replaySeekSeconds + playClock.getElapsedTime().asSeconds();
    };

    auto toggleReplayPause = [&]() {
        if (!engine.latestAnimationPlan().has_value()) return;
        if (replayPaused) {
            replayPaused = false;
            playClock.restart();
        } else {
            const float duration = replayDuration();
            replaySeekSeconds = duration > 0.0f
                ? std::clamp(replayElapsed(), 0.0f, duration + 0.6f)
                : std::max(0.0f, replayElapsed());
            replayPaused = true;
            autoPlay = false;
        }
    };

    auto seekReplay = [&](float delta) {
        if (!engine.latestAnimationPlan().has_value()) return;
        const float duration = replayDuration();
        const float limit = duration > 0.0f ? duration + 0.6f : 0.0f;
        replaySeekSeconds = std::clamp(replayElapsed() + delta, 0.0f, limit);
        replayPaused = true;
        autoPlay = false;
        playClock.restart();
    };

    // シリーズ試合終了処理
    auto finalizeGame = [&]() {
        if (!engine.isComplete()) return;
        const int as = engine.state().awayScore;
        const int hs = engine.state().homeScore;
        if (as == hs) return;  // タイは記録しない
        seriesScores.push_back({as, hs});
        if (as > hs) awaySeriesWins++;
        else         homeSeriesWins++;
        const int needed = seriesLen / 2 + 1;
        if (awaySeriesWins >= needed || homeSeriesWins >= needed
            || static_cast<int>(seriesScores.size()) >= seriesLen) {
            seriesOver = true;
        }
        showSeriesSummary = true;
        autoPlay = false;
    };

    // ── イベントループ ─────────────────────────────────────────────────────
    while (window.isOpen()) {
        while (const std::optional event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                window.close();
            }

            if (const auto* key = event->getIf<sf::Event::KeyPressed>()) {

                // ── チーム選択画面 ──────────────────────────────────────
                if (inTeamSelect) {
                    if (key->code == sf::Keyboard::Key::Up) {
                        if (selectCol == 0) { awayIdx = (awayIdx - 1 + numTeams) % numTeams; }
                        else                { homeIdx = (homeIdx - 1 + numTeams) % numTeams; }
                        // 同じチームをスキップ
                        if (awayIdx == homeIdx) {
                            if (selectCol == 0) awayIdx = (awayIdx - 1 + numTeams) % numTeams;
                            else                homeIdx = (homeIdx - 1 + numTeams) % numTeams;
                        }
                    } else if (key->code == sf::Keyboard::Key::Down) {
                        if (selectCol == 0) { awayIdx = (awayIdx + 1) % numTeams; }
                        else                { homeIdx = (homeIdx + 1) % numTeams; }
                        if (awayIdx == homeIdx) {
                            if (selectCol == 0) awayIdx = (awayIdx + 1) % numTeams;
                            else                homeIdx = (homeIdx + 1) % numTeams;
                        }
                    } else if (key->code == sf::Keyboard::Key::Left
                            || key->code == sf::Keyboard::Key::Right) {
                        selectCol = 1 - selectCol;
                    } else if (key->code == sf::Keyboard::Key::Num3) {
                        seriesLen = 3;
                    } else if (key->code == sf::Keyboard::Key::Num5) {
                        seriesLen = 5;
                    } else if (key->code == sf::Keyboard::Key::Num7) {
                        seriesLen = 7;
                    } else if (key->code == sf::Keyboard::Key::Space
                            || key->code == sf::Keyboard::Key::Enter) {
                        inTeamSelect = false;
                        resetSeries();
                        resetGame();
                    }

                // ── シリーズサマリー画面 ────────────────────────────────
                } else if (showSeriesSummary) {
                    if (key->code == sf::Keyboard::Key::Space
                     || key->code == sf::Keyboard::Key::Enter) {
                        if (seriesOver) {
                            // シリーズ終了 → チーム選択へ
                            inTeamSelect = true;
                            showSeriesSummary = false;
                        } else {
                            // 次の試合へ
                            showSeriesSummary = false;
                            resetGame();
                        }
                    } else if (key->code == sf::Keyboard::Key::T) {
                        inTeamSelect = true;
                        showSeriesSummary = false;
                        resetSeries();
                    }

                // ── プレイ中 ───────────────────────────────────────────
                } else {
                    if (key->code == sf::Keyboard::Key::Space) {
                        if (engine.isComplete() && !showSeriesSummary) {
                            finalizeGame();
                        } else {
                            advancePitch();
                            autoPlay = false;
                        }
                    } else if (key->code == sf::Keyboard::Key::A) {
                        autoPlay = !autoPlay;
                        if (autoPlay) { replayPaused = false; playClock.restart(); }
                    } else if (key->code == sf::Keyboard::Key::R) {
                        resetGame();
                    } else if (key->code == sf::Keyboard::Key::T) {
                        inTeamSelect = true;
                        showSeriesSummary = false;
                        resetSeries();
                    } else if (key->code == sf::Keyboard::Key::P) {
                        if (viewTransition == ViewTransition::None) {
                            targetMode = (visualMode == VisualMode::Field)
                                             ? VisualMode::Pitch : VisualMode::Field;
                            viewTransition = ViewTransition::FadingOut;
                            transitionClock.restart();
                        }
                    } else if (key->code == sf::Keyboard::Key::S) {
                        toggleReplayPause();
                    } else if (key->code == sf::Keyboard::Key::Left) {
                        seekReplay(-0.25f);
                    } else if (key->code == sf::Keyboard::Key::Right) {
                        seekReplay(0.25f);
                    }
                }
            }
        }

        // ── チーム選択画面描画 ─────────────────────────────────────────────
        if (inTeamSelect) {
            drawTeamSelectScreen(window, font, teams, awayIdx, homeIdx, selectCol, seriesLen);
            window.display();
            continue;
        }

        const float elapsed = replayElapsed();
        const bool complete = engine.isComplete();

        // 投手交代バナー
        const auto& latestChange = engine.lastPitcherChange();
        if (latestChange.has_value()
            && (!bannerEvent.has_value() || bannerEvent->toName != latestChange->toName)) {
            bannerEvent = latestChange;
            bannerClock.restart();
        }
        const float bannerAge = bannerClock.getElapsedTime().asSeconds();
        const bool showBanner = bannerEvent.has_value() && bannerAge < 3.5f;

        // InPlay: Pitch View → Field View 自動遷移
        if (visualMode == VisualMode::Pitch
            && engine.hasPendingAtBatResult()
            && engine.latestAnimationPlan().has_value()
            && engine.latestAnimationPlan()->hasPitch
            && engine.latestAnimationPlan()->pitch.isInPlay
            && !replayPaused && !complete) {
            const float transitionAt = static_cast<float>(
                engine.latestAnimationPlan()->pitch.durationSeconds) + 0.28f;
            if (elapsed > transitionAt && viewTransition == ViewTransition::None) {
                auto result = engine.applyPendingAtBatResult();
                if (result.has_value()) {
                    currentPlay = result;
                    playBannerText  = joji::toString(result->type);
                    playBannerSub   = result->batterName;
                    playBannerColor = outcomeColor(result->type);
                    playBannerClock.restart();
                    playBannerActive = true;
                }
                replaySeekSeconds = 0.0f;
                replayPaused = false;
                playClock.restart();
                // フェードで Field に遷移
                targetMode     = VisualMode::Field;
                viewTransition = ViewTransition::FadingOut;
                transitionClock.restart();
            }
        }

        // オートモード
        if (autoPlay && !replayPaused && !complete) {
            const float autoDelay = engine.hasPendingAtBatResult() ? 1.8f : 0.9f;
            if (elapsed > autoDelay) advancePitch();
        }

        // ── 表示フラグ ──────────────────────────────────────────────────────
        const bool showTrajectory = currentPlay.has_value()
                                    && engine.latestAnimationPlan().has_value()
                                    && engine.latestAnimationPlan()->hasBattedBall
                                    && !engine.isAtBatInProgress()
                                    && !engine.hasPendingAtBatResult();
        const bool showRunnerMovement = currentPlay.has_value()
                                        && engine.latestAnimationPlan().has_value()
                                        && !engine.latestAnimationPlan()->runners.empty()
                                        && !engine.isAtBatInProgress()
                                        && !engine.hasPendingAtBatResult()
                                        && elapsed <= static_cast<float>(
                                            engine.latestAnimationPlan()->totalDurationSeconds + 0.45);
        const bool showPitch = engine.latestAnimationPlan().has_value()
                               && engine.latestAnimationPlan()->hasPitch
                               && !showTrajectory
                               && (engine.isAtBatInProgress() || engine.hasPendingAtBatResult())
                               && elapsed <= static_cast<float>(
                                   engine.latestAnimationPlan()->pitch.durationSeconds + 0.45);
        const bool hasInterPitchRunners = !engine.latestBaseRunningEvents().empty()
                                          && engine.latestAnimationPlan().has_value()
                                          && !engine.latestAnimationPlan()->runners.empty();
        const bool showInterPitchRunners = hasInterPitchRunners
                                           && !showRunnerMovement
                                           && elapsed <= static_cast<float>(
                                               engine.latestAnimationPlan()->totalDurationSeconds + 0.6);

        // ── 描画 ────────────────────────────────────────────────────────────
        if (visualMode == VisualMode::Pitch) {
            drawPitchViewMode(window, font, engine, currentPlay, elapsed, autoPlay, complete);
        } else {
            drawField(window, engine.ballpark());

            if (showPitch) {
                drawPitchAnimation(window, *engine.latestAnimationPlan(), elapsed);
            }
            if (showTrajectory) {
                const auto& aplan = *engine.latestAnimationPlan();
                const float throwStart = aplan.throws.empty()
                    ? 1e9f
                    : static_cast<float>(aplan.throws[0].startTimeOffset);
                drawTrajectory(window, *currentPlay, aplan, elapsed, throwStart);
                drawThrowAnimations(window, aplan, elapsed);
                drawTagFeedback(window, font, aplan, elapsed);
            }
            drawDefense(window, font, engine.currentDefenseAlignment(),
                        showTrajectory ? currentPlay : std::optional<joji::PlayResult>{},
                        showTrajectory ? engine.latestAnimationPlan() : std::optional<joji::AnimationPlan>{},
                        showTrajectory ? elapsed : 0.0f);

            const std::vector<std::string> hiddenNames =
                (showRunnerMovement || showInterPitchRunners)
                    ? animatedRunnerNames(*engine.latestAnimationPlan())
                    : std::vector<std::string>{};
            drawBases(window, engine.state(), hiddenNames);
            if (showRunnerMovement || showInterPitchRunners) {
                drawRunnerAnimations(window, font, *engine.latestAnimationPlan(), elapsed);
            }

            // プレー結果バナー
            if (playBannerActive) {
                const float bElapsed = playBannerClock.getElapsedTime().asSeconds();
                drawPlayResultBanner(window, font, playBannerText, playBannerSub,
                                     playBannerColor, bElapsed);
                if (bElapsed > 1.4f) playBannerActive = false;
            }

            // 投手交代バナー
            if (showBanner) {
                const float alpha = bannerAge > 3.0f ? (3.5f - bannerAge) / 0.5f : 1.0f;
                drawPitcherChangeBanner(window, font, *bannerEvent, alpha);
            }

            // 試合終了サマリー
            if (complete) drawGameEndSummary(window, font, engine);

            // 打順ストリップ (試合中のみ)
            if (!complete) drawBattingOrderStrip(window, font, engine);

            drawPanel(window);
            drawLineScore(window, font, engine, complete);
            drawScoreboard(window, font, engine.state(),
                           showTrajectory ? currentPlay : std::optional<joji::PlayResult>{},
                           engine, complete);

            if (hasInterPitchRunners) {
                const float animEnd = engine.latestAnimationPlan().has_value()
                    ? static_cast<float>(engine.latestAnimationPlan()->totalDurationSeconds)
                    : 0.0f;
                drawBaseRunningEvents(window, font, engine.latestBaseRunningEvents(), elapsed, animEnd);
            }
            if (!complete) {
                drawPitchView(window, font, engine.latestAnimationPlan(), currentPitchLogs(engine), elapsed);
                drawReplayTimelineStatus(window, font, engine.latestAnimationPlan(), elapsed, replayPaused);
                drawAtBatFlow(window, font, currentPitchLogs(engine));
                drawLog(window, font, engine.logs());
            } else {
                drawLog(window, font, engine.logs());
            }

            // シリーズ中間サマリーオーバーレイ
            if (showSeriesSummary) {
                drawSeriesOverlay(window, font,
                                  teams[static_cast<std::size_t>(awayIdx)].name(),
                                  teams[static_cast<std::size_t>(homeIdx)].name(),
                                  seriesScores,
                                  awaySeriesWins, homeSeriesWins,
                                  seriesLen, seriesOver);
            }

            // 操作ヒント
            const char* hint;
            if (complete && !showSeriesSummary) {
                const int needed = seriesLen / 2 + 1;
                const bool decided = awaySeriesWins >= needed || homeSeriesWins >= needed
                                  || static_cast<int>(seriesScores.size()) >= seriesLen;
                hint = decided ? "Space: series summary  T: team select  R: replay"
                               : "Space: series summary  T: team select  R: replay";
                (void)decided;
                hint = "Space: series summary  T: team select  R: replay game";
            } else if (replayPaused) {
                hint = "P pitch  S resume  <-/-> seek  Space next  T teams  R replay";
            } else if (autoPlay) {
                hint = "P pitch  Space next  A auto:on  S pause  <-/-> seek  T teams";
            } else {
                hint = "P pitch  Space next  A auto  S pause  <-/-> seek  T teams";
            }
            drawText(window, font, hint, {34.0f, 790.0f}, 13, {200, 205, 202});
        }

        // ── ビュー切り替えフェードオーバーレイ ──────────────────────────────
        if (viewTransition != ViewTransition::None) {
            const float t = transitionClock.getElapsedTime().asSeconds();
            float alpha = 0.0f;
            if (viewTransition == ViewTransition::FadingOut) {
                alpha = std::min(t / TransitionHalf, 1.0f);
                if (t >= TransitionHalf) {
                    visualMode = targetMode;
                    viewTransition = ViewTransition::FadingIn;
                    transitionClock.restart();
                    alpha = 1.0f;
                }
            } else { // FadingIn
                alpha = 1.0f - std::min(t / TransitionHalf, 1.0f);
                if (t >= TransitionHalf) {
                    viewTransition = ViewTransition::None;
                    alpha = 0.0f;
                }
            }
            if (alpha > 0.0f) {
                sf::RectangleShape veil({WindowWidth, WindowHeight});
                veil.setFillColor({0, 0, 0, static_cast<std::uint8_t>(alpha * 255.0f)});
                window.draw(veil);
            }
        }

        window.display();
    }

    return 0;
}
