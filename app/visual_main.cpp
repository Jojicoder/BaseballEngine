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
    return !isGroundTrajectory(play, plan)
        && (plan.battedBall.maxHeight >= 18.0 || play.battedBall.launchAngle >= 12.0);
}

sf::Vector2f battedBallDrawPoint(const joji::AnimationPoint& point, bool airTrajectory) {
    const double visualLift = airTrajectory ? std::clamp(point.z, 0.0, 180.0) * 0.16 : 0.0;
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

void drawField(sf::RenderWindow& window) {
    window.clear(Grass);

    sf::ConvexShape infield;
    infield.setPointCount(4);
    infield.setPoint(0, fieldPoint(0.0, 0.0));
    infield.setPoint(1, fieldPoint(90.0, 90.0));
    infield.setPoint(2, fieldPoint(0.0, 180.0));
    infield.setPoint(3, fieldPoint(-90.0, 90.0));
    infield.setFillColor(Dirt);
    window.draw(infield);

    sf::VertexArray foulLines(sf::PrimitiveType::Lines, 4);
    foulLines[0].position = fieldPoint(0.0, 0.0);
    foulLines[1].position = fieldPoint(-310.0, 310.0);
    foulLines[2].position = fieldPoint(0.0, 0.0);
    foulLines[3].position = fieldPoint(310.0, 310.0);
    for (std::size_t i = 0; i < foulLines.getVertexCount(); ++i) {
        foulLines[i].color = Chalk;
    }
    window.draw(foulLines);

    sf::VertexArray fence(sf::PrimitiveType::LineStrip);
    for (int x = -300; x <= 300; x += 6) {
        const double y = 405.0 - 0.0012 * static_cast<double>(x * x);
        fence.append(sf::Vertex{fieldPoint(x, y), {22, 62, 45}});
    }
    window.draw(fence);

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
    const double amount = std::clamp((static_cast<double>(elapsed) - start.timeSeconds) / span, 0.0, 1.0);
    return fieldPoint(start.x + (end.x - start.x) * amount,
                      start.y + (end.y - start.y) * amount);
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
        sf::CircleShape runner(13.0f);
        runner.setOrigin({13.0f, 13.0f});
        runner.setPosition(current);
        runner.setFillColor(animation.scored ? sf::Color{80, 190, 100} : sf::Color{252, 203, 88});
        runner.setOutlineColor(Ink);
        runner.setOutlineThickness(2.0f);
        window.draw(runner);

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

    sf::VertexArray trail(sf::PrimitiveType::LineStrip);
    for (std::size_t i = 0; i <= idx; ++i) {
        sf::Color color = pitch.isBall ? sf::Color{54, 151, 230}
                        : pitch.isInPlay ? sf::Color{80, 190, 100}
                                         : sf::Color{220, 72, 66};
        color.a = static_cast<std::uint8_t>(120 + 105 * i / std::max<std::size_t>(idx + 1, 1));
        trail.append(sf::Vertex{pitchPoint(points[i]), color});
    }
    window.draw(trail);

    const auto& current = points[idx];
    const float height = static_cast<float>(std::clamp(current.z, 0.0, 7.0) / 7.0);
    sf::CircleShape shadow(6.0f);
    shadow.setOrigin({6.0f, 6.0f});
    shadow.setPosition(fieldPoint(current.x, current.y));
    shadow.setFillColor({10, 30, 20, 75});
    window.draw(shadow);

    sf::CircleShape ball(6.5f + height * 2.0f);
    ball.setOrigin({ball.getRadius(), ball.getRadius()});
    ball.setPosition(pitchPoint(current));
    ball.setFillColor(sf::Color::White);
    ball.setOutlineColor(pitch.isBall ? sf::Color{54, 151, 230}
                         : pitch.isInPlay ? sf::Color{80, 190, 100}
                                          : sf::Color{220, 72, 66});
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

    // After the first throw starts, use the ball's final resting point and stop the moving dot
    const bool throwStarted = elapsed >= hideAfter;
    const float drawElapsed = throwStarted ? hideAfter : elapsed;

    std::size_t idx = 0;
    while (idx + 1 < points.size()
           && points[idx + 1].timeSeconds <= static_cast<double>(drawElapsed)) {
        ++idx;
    }

    const bool groundTrajectory = isGroundTrajectory(play, plan);
    const bool airTrajectory = isAirTrajectory(play, plan);
    const sf::Color baseColor = groundTrajectory ? sf::Color{214, 142, 72}
                              : airTrajectory ? sf::Color{238, 232, 216}
                                              : outcomeColor(play.type);

    sf::VertexArray trail(sf::PrimitiveType::LineStrip);
    for (std::size_t i = 0; i <= idx; ++i) {
        sf::Color color = baseColor;
        const std::uint8_t baseAlpha = throwStarted ? 55u : 110u;
        color.a = static_cast<std::uint8_t>(baseAlpha + (throwStarted ? 0u : 145u) * i /
                                            std::max<std::size_t>(idx + 1, 1));
        trail.append(sf::Vertex{battedBallDrawPoint(points[i], airTrajectory), color});
    }
    window.draw(trail);

    if (groundTrajectory) {
        for (std::size_t i = 2; i <= idx; i += 6) {
            sf::CircleShape dust(3.0f);
            dust.setOrigin({3.0f, 3.0f});
            dust.setPosition(fieldPoint(points[i].x, points[i].y));
            dust.setFillColor({196, 126, 62, 95});
            window.draw(dust);
        }
    }

    // Hide moving ball dot once throw has started
    if (!throwStarted) {
        const auto& current = points[idx];
        const float height = static_cast<float>(std::min(current.z, 180.0) / 180.0);
        const sf::Vector2f shadowPos = fieldPoint(current.x, current.y);
        const sf::Vector2f ballPos = battedBallDrawPoint(current, airTrajectory);

        sf::CircleShape shadow(groundTrajectory ? 5.0f : 8.0f + height * 4.0f);
        shadow.setOrigin({shadow.getRadius(), shadow.getRadius()});
        shadow.setPosition(shadowPos);
        shadow.setFillColor(groundTrajectory ? sf::Color{110, 70, 38, 90}
                                             : sf::Color{10, 30, 20, 70});
        window.draw(shadow);

        if (airTrajectory && height > 0.08f) {
            sf::VertexArray heightLine(sf::PrimitiveType::Lines, 2);
            heightLine[0] = sf::Vertex{shadowPos, sf::Color{255, 255, 255, 45}};
            heightLine[1] = sf::Vertex{ballPos, sf::Color{255, 255, 255, 95}};
            window.draw(heightLine);
        }

        sf::CircleShape ball(groundTrajectory ? 5.5f : 7.0f + height * 5.0f);
        ball.setOrigin({ball.getRadius(), ball.getRadius()});
        ball.setPosition(ballPos);
        ball.setFillColor(sf::Color::White);
        ball.setOutlineColor(groundTrajectory ? sf::Color{174, 116, 64} : outcomeColor(play.type));
        ball.setOutlineThickness(groundTrajectory ? 1.5f : 2.0f);
        window.draw(ball);
    }
}

void drawThrowAnimations(sf::RenderWindow& window,
                         const joji::AnimationPlan& plan,
                         float elapsed) {
    for (const auto& thr : plan.throws) {
        if (thr.points.size() < 2) continue;
        const float startT = static_cast<float>(thr.startTimeOffset);
        const float endT   = startT + static_cast<float>(thr.durationSeconds);
        if (elapsed < startT || elapsed > endT + 0.3f) continue;

        const sf::Vector2f from = fieldPoint(thr.points[0].x, thr.points[0].y);
        const sf::Vector2f to   = fieldPoint(thr.points[1].x, thr.points[1].y);

        // Static guide line (faint white)
        sf::VertexArray line(sf::PrimitiveType::Lines, 2);
        line[0] = sf::Vertex{from, sf::Color{255, 255, 255, 55}};
        line[1] = sf::Vertex{to,   sf::Color{255, 255, 255, 55}};
        window.draw(line);

        // Thrown ball dot moving along line
        if (elapsed >= startT && elapsed <= endT) {
            const float t = std::clamp((elapsed - startT) /
                                       static_cast<float>(thr.durationSeconds), 0.0f, 1.0f);
            const sf::Vector2f pos{from.x + (to.x - from.x) * t,
                                   from.y + (to.y - from.y) * t};
            sf::CircleShape ball(5.0f);
            ball.setOrigin({5.0f, 5.0f});
            ball.setPosition(pos);
            ball.setFillColor(sf::Color::White);
            ball.setOutlineColor(sf::Color{200, 220, 255, 200});
            ball.setOutlineThickness(1.5f);
            window.draw(ball);
        }
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
    drawText(window, font, "Joji Baseball", {960.0f, 30.0f}, 30, Ink, sf::Text::Bold);
    drawText(window, font, halfLabel(state), {960.0f, 78.0f}, 22, {56, 76, 70}, sf::Text::Bold);
    if (complete)
        drawText(window, font, "Final", {1168.0f, 78.0f}, 22, {220, 72, 66}, sf::Text::Bold);

    drawText(window, font, fitLine(engine.awayTeamName(), 18), {960.0f, 130.0f}, 22, Ink);
    drawText(window, font, std::to_string(state.awayScore), {1210.0f, 130.0f}, 24, Ink, sf::Text::Bold);
    drawText(window, font, fitLine(engine.homeTeamName(), 18), {960.0f, 166.0f}, 22, Ink);
    drawText(window, font, std::to_string(state.homeScore), {1210.0f, 166.0f}, 24, Ink, sf::Text::Bold);

    drawText(window, font, "Outs", {960.0f, 220.0f}, 18, {82, 93, 88}, sf::Text::Bold);
    for (int i = 0; i < 3; ++i) {
        sf::CircleShape outLight(9.0f);
        outLight.setOrigin({9.0f, 9.0f});
        outLight.setPosition({1025.0f + i * 30.0f, 232.0f});
        outLight.setFillColor(i < state.outs ? sf::Color{220, 72, 66} : sf::Color{206, 201, 186});
        window.draw(outLight);
    }

    // ── 現在の投手 (HOT/COLD + ERA + 球数) ──────────────────────────────
    const joji::Player& pitcher = engine.currentPitcher();
    const int pitchCount = engine.currentPitcherPitchCount();
    const double pitcherForm = engine.pitcherFormValue();
    const double era = engine.currentPitcherERA();

    const sf::Color pitcherNameColor = formColor(pitcherForm);
    drawText(window, font, fitLine(pitcher.name, 18), {960.0f, 252.0f}, 16, pitcherNameColor, sf::Text::Bold);
    drawFormBadge(window, font, pitcherForm, {1160.0f, 253.0f});

    const sf::Color pitchCountColor = pitchCount > 100 ? sf::Color{220, 72, 66}
                                    : pitchCount >  75 ? sf::Color{235, 116, 48}
                                                       : sf::Color{82,  93,  88};
    std::ostringstream eraStr;
    eraStr << std::fixed << std::setprecision(2) << era;
    const std::string pitcherMeta = joji::toString(pitcher.pitcherRole)
                                  + "  " + std::to_string(pitchCount) + "P"
                                  + "  ERA " + eraStr.str();
    drawText(window, font, fitLine(pitcherMeta, 30), {960.0f, 272.0f}, 13, pitchCountColor);

    // ── 打者セクション ───────────────────────────────────────────────────
    const bool atBatActive = engine.isAtBatInProgress() || engine.hasPendingAtBatResult();
    if (atBatActive) {
        const joji::AtBatState& ab = engine.currentAtBat();
        const double batterForm = engine.batterFormValue(ab.batter.name);

        drawText(window, font, "At bat", {960.0f, 293.0f}, 14, {82, 93, 88}, sf::Text::Bold);
        const sf::Color batterColor = formColor(batterForm);
        drawText(window, font, fitLine(ab.batter.name, 22), {960.0f, 312.0f}, 22, batterColor, sf::Text::Bold);
        drawFormBadge(window, font, batterForm, {1160.0f, 314.0f});

        const std::string countStr = "Count  " + std::to_string(ab.count.balls)
                                     + " - " + std::to_string(ab.count.strikes)
                                     + "  (#" + std::to_string(ab.pitchNumber - 1) + ")";
        drawText(window, font, countStr, {960.0f, 342.0f}, 16, {56, 76, 70}, sf::Text::Bold);

        if (engine.hasPendingAtBatResult() && ab.finalOutcome.has_value()) {
            std::string outcomeStr;
            switch (*ab.finalOutcome) {
                case joji::AtBatOutcome::StrikeOut:  outcomeStr = "Strikeout K"; break;
                case joji::AtBatOutcome::Walk:        outcomeStr = "Walk BB";     break;
                case joji::AtBatOutcome::HitByPitch:  outcomeStr = "Hit By Pitch"; break;
                case joji::AtBatOutcome::InPlay:      outcomeStr = "In Play";     break;
            }
            drawText(window, font, outcomeStr, {960.0f, 370.0f}, 20,
                     *ab.finalOutcome == joji::AtBatOutcome::InPlay
                         ? sf::Color{80, 190, 100}
                         : sf::Color{220, 72, 66},
                     sf::Text::Bold);
            drawText(window, font, "Space to apply", {960.0f, 400.0f}, 14, {82, 93, 88});
        }
    } else if (currentPlay.has_value()) {
        const auto& play = *currentPlay;
        drawText(window, font, "Result", {960.0f, 293.0f}, 14, {82, 93, 88}, sf::Text::Bold);
        drawText(window, font, fitLine(play.batterName, 22), {960.0f, 312.0f}, 22, Ink, sf::Text::Bold);
        drawText(window, font, joji::toString(play.type), {960.0f, 340.0f}, 20, outcomeColor(play.type), sf::Text::Bold);

        if (play.battedBall.estimatedDistance > 0.0) {
            std::ostringstream details;
            details << static_cast<int>(std::round(play.battedBall.estimatedDistance)) << " ft  "
                    << play.battedBall.classification;
            drawText(window, font, fitLine(details.str(), 28), {960.0f, 368.0f}, 16, {47, 58, 54});

            std::ostringstream metrics;
            metrics << "EV " << static_cast<int>(std::round(play.battedBall.exitVelocity))
                    << "  LA " << static_cast<int>(std::round(play.battedBall.launchAngle))
                    << "  Spray " << static_cast<int>(std::round(play.battedBall.sprayAngle));
            drawText(window, font, fitLine(metrics.str(), 31), {960.0f, 392.0f}, 15, {47, 58, 54});

            if (play.fielderId >= 0) {
                std::ostringstream fielder;
                fielder << "Fielder " << play.fielderName
                        << "  " << std::fixed << std::setprecision(1)
                        << play.fielderTravelTime << "/" << play.fieldingAvailableTime << "s";
                drawText(window, font, fitLine(fielder.str(), 31), {960.0f, 416.0f}, 14, {47, 58, 54});
            }
        }
    } else if (!complete) {
        drawText(window, font, "Ready", {960.0f, 293.0f}, 14, {82, 93, 88}, sf::Text::Bold);
        drawText(window, font, "Space to pitch", {960.0f, 315.0f}, 22, Ink, sf::Text::Bold);
    }
}

void drawAtBatFlow(sf::RenderWindow& window, const sf::Font& font,
                   const std::vector<joji::PitchLog>& logs) {
    drawText(window, font, "At-bat flow", {960.0f, 716.0f}, 16, {82, 93, 88}, sf::Text::Bold);

    const std::size_t start = logs.size() > 1 ? logs.size() - 1 : 0;
    float y = 740.0f;

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
    const sf::Vector2f zoneOrigin{1030.0f, 560.0f};
    const sf::Vector2f zoneSize{150.0f, 135.0f};
    const sf::Vector2f pitcherPoint{1105.0f, 462.0f};

    drawText(window, font, "Pitch View", {960.0f, 432.0f}, 18, {82, 93, 88}, sf::Text::Bold);

    sf::CircleShape pitcher(8.0f);
    pitcher.setOrigin({8.0f, 8.0f});
    pitcher.setPosition(pitcherPoint);
    pitcher.setFillColor({22, 62, 45});
    pitcher.setOutlineColor(sf::Color::White);
    pitcher.setOutlineThickness(1.5f);
    window.draw(pitcher);
    drawText(window, font, "Pitcher", {1078.0f, 442.0f}, 11, {82, 93, 88});

    sf::RectangleShape moundLine({54.0f, 3.0f});
    moundLine.setOrigin({27.0f, 1.5f});
    moundLine.setPosition({pitcherPoint.x, pitcherPoint.y + 18.0f});
    moundLine.setFillColor({174, 116, 64});
    window.draw(moundLine);

    drawZoneGrid(window, font, zoneOrigin, zoneSize);
    drawText(window, font, "Batter", {1084.0f, 698.0f}, 11, {82, 93, 88});

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
        drawText(window, font, fitLine(label.str(), 25), {960.0f, 462.0f}, 13, Ink, sf::Text::Bold);

        const std::string result = pitch.isBall ? "Ball"
                                : pitch.isInPlay ? "In Play"
                                                 : "Strike";
        drawText(window, font, result, {960.0f, 482.0f}, 13, pitchAnimationColor(pitch), sf::Text::Bold);
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
    drawText(window, font, "Play log", {960.0f, 782.0f}, 15, {82, 93, 88}, sf::Text::Bold);
    const std::size_t start = logs.size() > 1 ? logs.size() - 1 : 0;
    float y = 802.0f;
    for (std::size_t i = start; i < logs.size(); ++i) {
        drawText(window, font, fitLine(logs[i].text, 42), {960.0f, y}, 14, Ink);
        y += 26.0f;
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

    auto makeEngine = [] {
        const auto teams = joji::allTeams();
        return joji::GameEngine{
            teams.at(0),
            teams.at(1),
            joji::Random{std::optional<std::uint32_t>{}}
        };
    };

    joji::GameEngine engine = makeEngine();
    std::optional<joji::PlayResult> currentPlay;

    sf::RenderWindow window(sf::VideoMode({static_cast<unsigned int>(WindowWidth), static_cast<unsigned int>(WindowHeight)}),
                            "Joji Baseball Engine v1.0");
    window.setVerticalSyncEnabled(true);

    sf::Clock playClock;
    bool autoPlay = false;
    VisualMode visualMode = VisualMode::Field;

    // 投手交代バナー管理
    std::optional<joji::GameEngine::PitcherChangeEvent> bannerEvent;
    sf::Clock bannerClock;

    // 1球進める or 打席結果を反映
    auto advancePitch = [&]() {
        if (engine.isComplete()) return;

        if (engine.hasPendingAtBatResult()) {
            // 打席終了済み: ゲーム状態に反映
            auto result = engine.applyPendingAtBatResult();
            if (result.has_value()) {
                currentPlay = result;
            }
        } else {
            // 1球投げる
            engine.simulateNextPitch();
            // pendingになっていなければ軌跡表示をリセット
            if (!engine.hasPendingAtBatResult()) {
                currentPlay.reset();
            }
        }
        playClock.restart();
    };

    while (window.isOpen()) {
        while (const std::optional event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                window.close();
            }

            if (const auto* key = event->getIf<sf::Event::KeyPressed>()) {
                if (key->code == sf::Keyboard::Key::Space) {
                    advancePitch();
                    autoPlay = false;
                } else if (key->code == sf::Keyboard::Key::A) {
                    autoPlay = !autoPlay;
                } else if (key->code == sf::Keyboard::Key::R) {
                    engine = makeEngine();
                    currentPlay.reset();
                    bannerEvent.reset();
                    playClock.restart();
                    autoPlay = false;
                } else if (key->code == sf::Keyboard::Key::P) {
                    visualMode = visualMode == VisualMode::Field ? VisualMode::Pitch : VisualMode::Field;
                }
            }
        }

        const float elapsed = playClock.getElapsedTime().asSeconds();
        const bool complete = engine.isComplete();

        // 新しい投手交代イベントを検出 → バナークロックリセット
        const auto& latestChange = engine.lastPitcherChange();
        if (latestChange.has_value()
            && (!bannerEvent.has_value() || bannerEvent->toName != latestChange->toName)) {
            bannerEvent = latestChange;
            bannerClock.restart();
        }
        const float bannerAge = bannerClock.getElapsedTime().asSeconds();
        const bool showBanner = bannerEvent.has_value() && bannerAge < 3.5f;

        // InPlay: Pitch View でアニメ終了 + 0.28s 後に自動で Field View へ遷移
        if (visualMode == VisualMode::Pitch
            && engine.hasPendingAtBatResult()
            && engine.latestAnimationPlan().has_value()
            && engine.latestAnimationPlan()->hasPitch
            && engine.latestAnimationPlan()->pitch.isInPlay
            && !complete) {
            const float transitionAt = static_cast<float>(
                engine.latestAnimationPlan()->pitch.durationSeconds) + 0.28f;
            if (elapsed > transitionAt) {
                auto result = engine.applyPendingAtBatResult();
                if (result.has_value()) currentPlay = result;
                playClock.restart();
                visualMode = VisualMode::Field;
            }
        }

        // オートモード: pending は長め(1.8s)、通常投球は短め(0.9s) で自動進行
        if (autoPlay && !complete) {
            const float autoDelay = engine.hasPendingAtBatResult() ? 1.8f : 0.9f;
            if (elapsed > autoDelay) {
                advancePitch();
            }
        }

        // 軌跡は applyPendingAtBatResult 後のみ表示し、次球で消える
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

        // Inter-pitch runner animations (SB/CS/WP/PB/Balk): show while pitch is active
        // OR right after CS-3rd-out (no pitch, but plan has runners).
        const bool hasInterPitchRunners = !engine.latestBaseRunningEvents().empty()
                                          && engine.latestAnimationPlan().has_value()
                                          && !engine.latestAnimationPlan()->runners.empty();
        const bool showInterPitchRunners = hasInterPitchRunners
                                           && !showRunnerMovement   // don't double-draw with post-play
                                           && elapsed <= static_cast<float>(
                                               engine.latestAnimationPlan()->totalDurationSeconds + 0.6);

        if (visualMode == VisualMode::Pitch) {
            drawPitchViewMode(window, font, engine, currentPlay, elapsed, autoPlay, complete);
        } else {
            drawField(window);
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
            }
            drawDefense(window, font, engine.currentDefenseAlignment(),
                        showTrajectory ? currentPlay : std::optional<joji::PlayResult>{},
                        showTrajectory ? engine.latestAnimationPlan() : std::optional<joji::AnimationPlan>{},
                        showTrajectory ? elapsed : 0.0f);
            const std::vector<std::string> hiddenRunnerNames = showRunnerMovement
                ? animatedRunnerNames(*engine.latestAnimationPlan())
                : (showInterPitchRunners
                    ? animatedRunnerNames(*engine.latestAnimationPlan())
                    : std::vector<std::string>{});
            drawBases(window, engine.state(), hiddenRunnerNames);
            if (showRunnerMovement) {
                drawRunnerAnimations(window, font, *engine.latestAnimationPlan(), elapsed);
            } else if (showInterPitchRunners) {
                drawRunnerAnimations(window, font, *engine.latestAnimationPlan(), elapsed);
            }

            // 投手交代バナー (フィールド左側オーバーレイ)
            if (showBanner) {
                // 3.5 秒でフェードアウト (最後 0.5 秒)
                const float alpha = bannerAge > 3.0f ? (3.5f - bannerAge) / 0.5f : 1.0f;
                drawPitcherChangeBanner(window, font, *bannerEvent, alpha);
            }
            // 試合終了サマリー
            if (complete) {
                drawGameEndSummary(window, font, engine);
            }

            drawPanel(window);
            drawScoreboard(window, font, engine.state(), showTrajectory ? currentPlay : std::optional<joji::PlayResult>{}, engine, complete);
            if (hasInterPitchRunners) {
                const float animEnd = engine.latestAnimationPlan().has_value()
                    ? static_cast<float>(engine.latestAnimationPlan()->totalDurationSeconds)
                    : 0.0f;
                drawBaseRunningEvents(window, font, engine.latestBaseRunningEvents(), elapsed, animEnd);
            }
            drawAtBatFlow(window, font, currentPitchLogs(engine));
            drawPitchView(window, font, engine.latestAnimationPlan(), currentPitchLogs(engine), elapsed);
            drawLog(window, font, engine.logs());

            const char* hint = complete
                ? "P pitch view  R new game"
                : (autoPlay ? "P pitch view  Space next  A auto:on  R new game"
                            : "P pitch view  Space next  A auto  R new game");
            drawText(window, font, hint, {34.0f, 790.0f}, 14, {238, 232, 216});
        }

        window.display();
    }

    return 0;
}
