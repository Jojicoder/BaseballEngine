// draft_runner.cpp — run the offseason draft and export it as JSON.
//
// Usage:
//   ./JojiDraftRunner --season 2027 --seed 1 --rounds 7 --out-dir public/jbl/seasons/2027 [--margin 5]
//
// Writes <out-dir>/draft/draft.json, prospects.json and roster-changes.json.
// The same arguments always produce byte-identical output.

#include "DraftEngine.h"
#include "JsonExporter.h"
#include "ProgressionEngine.h"
#include "Teams.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>

namespace {

struct Options {
    int season = 2027;
    unsigned seed = 1;
    int rounds = 7;
    int margin = -1000;  // sentinel: keep DraftConfig default
    std::string outDir;
};

std::string positionLabel(joji::Position position) {
    switch (position) {
        case joji::Position::Pitcher: return "P";
        case joji::Position::Catcher: return "C";
        case joji::Position::FirstBase: return "1B";
        case joji::Position::SecondBase: return "2B";
        case joji::Position::ThirdBase: return "3B";
        case joji::Position::Shortstop: return "SS";
        case joji::Position::LeftField: return "LF";
        case joji::Position::CenterField: return "CF";
        case joji::Position::RightField: return "RF";
    }
    return "?";
}

std::string roleLabel(joji::PlayerRole role) {
    using R = joji::PlayerRole;
    switch (role) {
        case R::Leadoff: return "Leadoff";
        case R::ContactHitter: return "ContactHitter";
        case R::PowerHitter: return "PowerHitter";
        case R::CornerIF: return "CornerIF";
        case R::MiddleIF: return "MiddleIF";
        case R::CenterFielder: return "CenterFielder";
        case R::CornerOF: return "CornerOF";
        case R::Catcher: return "Catcher";
        case R::UtilityIF: return "UtilityIF";
        case R::ExtraOF: return "ExtraOF";
        case R::PinchHitter: return "PinchHitter";
        case R::BackupCatcher: return "BackupCatcher";
        case R::Ace: return "Ace";
        case R::Starter: return "Starter";
        case R::BackOfRotation: return "BackOfRotation";
        case R::Closer: return "Closer";
        case R::Setup: return "Setup";
        case R::MiddleRelief: return "MiddleRelief";
        case R::LongRelief: return "LongRelief";
        case R::Specialist: return "Specialist";
    }
    return "?";
}

std::string changeTypeLabel(joji::RosterChangeType type) {
    switch (type) {
        case joji::RosterChangeType::Promoted: return "promoted";
        case joji::RosterChangeType::Released: return "released";
        case joji::RosterChangeType::Minors: return "minors";
    }
    return "?";
}

std::string handLabel(joji::ThrowingHand hand) { return hand == joji::ThrowingHand::Left ? "L" : "R"; }

std::string sideLabel(joji::BattingSide side) {
    switch (side) {
        case joji::BattingSide::Left: return "L";
        case joji::BattingSide::Right: return "R";
        case joji::BattingSide::Switch: return "S";
    }
    return "R";
}

void writeTextFile(const std::filesystem::path& path, const std::string& contents) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path);
    if (!out) throw std::runtime_error("Unable to write " + path.string());
    out << contents;
}

std::string draftJson(const joji::DraftResult& result) {
    std::ostringstream out;
    out << "{\n  \"schemaVersion\":1,\n  \"season\":" << result.season
        << ",\n  \"seed\":" << result.seed
        << ",\n  \"rounds\":" << result.rounds
        << ",\n  \"picks\":[\n";
    for (std::size_t i = 0; i < result.picks.size(); ++i) {
        const auto& p = result.picks[i];
        out << "    {\"round\":" << p.round << ",\"pickInRound\":" << p.pickInRound
            << ",\"overall\":" << p.overall << ",\"team\":" << joji::jsonString(p.team)
            << ",\"prospectId\":" << p.prospectId << "}" << (i + 1 < result.picks.size() ? "," : "") << "\n";
    }
    out << "  ]\n}\n";
    return out.str();
}

std::string prospectsJson(const joji::DraftResult& result) {
    std::map<int, const joji::DraftPick*> pickByProspect;
    for (const auto& pick : result.picks) pickByProspect[pick.prospectId] = &pick;

    std::ostringstream out;
    out << "{\n  \"schemaVersion\":1,\n  \"season\":" << result.season << ",\n  \"prospects\":[\n";
    for (std::size_t i = 0; i < result.pool.size(); ++i) {
        const auto& d = result.pool[i];
        const auto& p = d.asPlayer;
        const auto it = pickByProspect.find(d.id);
        out << "    {\"id\":" << d.id << ",\"name\":" << joji::jsonString(d.name)
            << ",\"age\":" << d.age << ",\"position\":" << joji::jsonString(positionLabel(d.position))
            << ",\"role\":" << joji::jsonString(roleLabel(d.role))
            << ",\"potential\":" << d.potential << ",\"readiness\":" << d.readiness
            << ",\"overall\":" << d.overall
            << ",\"throws\":" << joji::jsonString(handLabel(p.throwingHand));
        if (d.position == joji::Position::Pitcher) {
            out << ",\"pitcherRole\":" << joji::jsonString(joji::toString(p.pitcherRole))
                << ",\"velocity\":" << p.pitchingVelocity << ",\"control\":" << p.pitchingControl
                << ",\"stuff\":" << p.pitchingStuff << ",\"stamina\":" << p.pitchingStamina;
        } else {
            out << ",\"bats\":" << joji::jsonString(sideLabel(p.battingSide))
                << ",\"contact\":" << p.contact << ",\"power\":" << p.power << ",\"eye\":" << p.eye
                << ",\"speed\":" << p.speed << ",\"fielding\":" << p.fielding << ",\"arm\":" << p.arm;
        }
        if (it != pickByProspect.end()) {
            out << ",\"drafted\":{\"team\":" << joji::jsonString(it->second->team)
                << ",\"round\":" << it->second->round << ",\"overall\":" << it->second->overall << "}";
        } else {
            out << ",\"drafted\":null";
        }
        out << "}" << (i + 1 < result.pool.size() ? "," : "") << "\n";
    }
    out << "  ]\n}\n";
    return out.str();
}

std::string changesJson(const joji::DraftResult& result) {
    std::ostringstream out;
    out << "{\n  \"schemaVersion\":1,\n  \"season\":" << result.season << ",\n  \"changes\":[\n";
    for (std::size_t i = 0; i < result.changes.size(); ++i) {
        const auto& c = result.changes[i];
        out << "    {\"team\":" << joji::jsonString(c.team)
            << ",\"type\":" << joji::jsonString(changeTypeLabel(c.type))
            << ",\"player\":" << joji::jsonString(c.player)
            << ",\"prospectId\":" << (c.prospectId >= 0 ? std::to_string(c.prospectId) : std::string("null"))
            << ",\"slot\":" << (c.slot.empty() ? std::string("null") : joji::jsonString(c.slot))
            << ",\"replaced\":" << (c.replaced.empty() ? std::string("null") : joji::jsonString(c.replaced))
            << ",\"overall\":" << c.overall << "}" << (i + 1 < result.changes.size() ? "," : "") << "\n";
    }
    out << "  ]\n}\n";
    return out.str();
}

std::string progressionJson(const joji::ProgressionResult& result) {
    std::ostringstream out;
    out << "{\n  \"schemaVersion\":1,\n  \"season\":" << result.season
        << ",\n  \"seed\":" << result.seed << ",\n  \"changes\":[\n";
    for (std::size_t i = 0; i < result.changes.size(); ++i) {
        const auto& c = result.changes[i];
        out << "    {\"team\":" << joji::jsonString(c.team) << ",\"player\":" << joji::jsonString(c.player)
            << ",\"pitcher\":" << (c.isPitcher ? "true" : "false")
            << ",\"ageBefore\":" << c.ageBefore << ",\"ageAfter\":" << c.ageAfter
            << ",\"overallBefore\":" << c.overallBefore << ",\"overallAfter\":" << c.overallAfter
            << ",\"developing\":" << (c.developing ? "true" : "false") << "}"
            << (i + 1 < result.changes.size() ? "," : "") << "\n";
    }
    out << "  ]\n}\n";
    return out.str();
}

Options parseArgs(int argc, char* argv[]) {
    Options opts;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        const bool hasValue = i + 1 < argc;
        if (arg == "--season" && hasValue) opts.season = std::stoi(argv[++i]);
        else if (arg == "--seed" && hasValue) opts.seed = static_cast<unsigned>(std::stoul(argv[++i]));
        else if (arg == "--rounds" && hasValue) opts.rounds = std::stoi(argv[++i]);
        else if (arg == "--margin" && hasValue) opts.margin = std::stoi(argv[++i]);
        else if (arg == "--out-dir" && hasValue) opts.outDir = argv[++i];
    }
    return opts;
}

} // namespace

int main(int argc, char* argv[]) {
    try {
        const Options opts = parseArgs(argc, argv);
        if (opts.outDir.empty() || opts.rounds < 1) {
            std::cerr << "Usage: ./JojiDraftRunner --season YEAR --seed N --rounds R --out-dir DIR [--margin M]\n";
            return 2;
        }

        joji::DraftConfig config;
        config.season = opts.season;
        config.seed = opts.seed;
        config.rounds = opts.rounds;
        if (opts.margin > -1000) config.promotionMargin = opts.margin;

        // The draft runs on the rosters as they stand that offseason (earlier drafts and aging
        // included), exactly as game_export --draft builds them.
        joji::SeasonRosterOptions rosterOptions;
        rosterOptions.rounds = config.rounds;
        rosterOptions.promotionMargin = config.promotionMargin;
        rosterOptions.draftSeed = config.seed;
        joji::SeasonRosters rosters = joji::buildSeasonRosters(joji::allTeams(), opts.season, rosterOptions);
        joji::DraftResult& result = rosters.draft;

        const std::filesystem::path root = std::filesystem::path(opts.outDir) / "draft";
        writeTextFile(root / "draft.json", draftJson(result));
        writeTextFile(root / "prospects.json", prospectsJson(result));
        writeTextFile(root / "roster-changes.json", changesJson(result));
        writeTextFile(std::filesystem::path(opts.outDir) / "progression" / "progression.json",
                      progressionJson(rosters.progression));

        int promoted = 0;
        int minors = 0;
        std::map<std::string, int> perTeam;
        for (const auto& c : result.changes) {
            if (c.type == joji::RosterChangeType::Promoted) {
                ++promoted;
                ++perTeam[c.team];
            } else if (c.type == joji::RosterChangeType::Minors) {
                ++minors;
            }
        }
        std::cerr << "Draft " << result.season << " (seed " << result.seed << "): "
                  << result.pool.size() << " prospects, " << result.picks.size() << " picks, "
                  << promoted << " made a roster, " << minors << " to the minors.\n";
        for (const auto& [team, n] : perTeam) std::cerr << "  " << team << ": " << n << " promoted\n";
        std::cerr << "Wrote " << root.string() << "\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "JojiDraftRunner error: " << ex.what() << "\n";
        return 1;
    }
}
