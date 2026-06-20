#include "JsonExporter.h"
#include "GameEngine.h"

#include <iomanip>
#include <sstream>

namespace joji {

namespace {

std::string q(const std::string& s) {
    return "\"" + s + "\"";
}

std::string dbl(double v, int prec = 3) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(prec) << v;
    return ss.str();
}

std::string intArray(const std::vector<int>& v) {
    std::string out = "[";
    for (std::size_t i = 0; i < v.size(); ++i) {
        if (i > 0) out += ",";
        out += (v[i] < 0 ? "null" : std::to_string(v[i]));
    }
    return out + "]";
}

std::string resultTypeName(GameResultType t) {
    switch (t) {
        case GameResultType::AwayWin: return "AwayWin";
        case GameResultType::HomeWin: return "HomeWin";
        case GameResultType::Tie:     return "Tie";
    }
    return "Unknown";
}

double safeRate(int num, int den) {
    return den > 0 ? static_cast<double>(num) / den : 0.0;
}

std::string playerJson(const PlayerBoxScore& p, const std::string& indent) {
    const int pa  = p.atBats + p.walks;
    const double avg = safeRate(p.hits, p.atBats);
    const double obp = safeRate(p.hits + p.walks, pa);
    const double slg = safeRate(p.totalBases, p.atBats);

    std::string s = indent + "{\n";
    s += indent + "  " + q("name")       + ": " + q(p.name)                + ",\n";
    s += indent + "  " + q("atBats")     + ": " + std::to_string(p.atBats)     + ",\n";
    s += indent + "  " + q("hits")       + ": " + std::to_string(p.hits)       + ",\n";
    s += indent + "  " + q("doubles")    + ": " + std::to_string(p.doubles)    + ",\n";
    s += indent + "  " + q("triples")    + ": " + std::to_string(p.triples)    + ",\n";
    s += indent + "  " + q("homeRuns")   + ": " + std::to_string(p.homeRuns)   + ",\n";
    s += indent + "  " + q("walks")      + ": " + std::to_string(p.walks)      + ",\n";
    s += indent + "  " + q("strikeouts") + ": " + std::to_string(p.strikeouts) + ",\n";
    s += indent + "  " + q("totalBases") + ": " + std::to_string(p.totalBases) + ",\n";
    s += indent + "  " + q("rbi")        + ": " + std::to_string(p.rbi)        + ",\n";
    s += indent + "  " + q("avg")        + ": " + dbl(avg) + ",\n";
    s += indent + "  " + q("obp")        + ": " + dbl(obp) + ",\n";
    s += indent + "  " + q("slg")        + ": " + dbl(slg) + "\n";
    s += indent + "}";
    return s;
}

std::string playerArrayJson(const std::vector<PlayerBoxScore>& players,
                            const std::string& indent) {
    std::string s = "[\n";
    for (std::size_t i = 0; i < players.size(); ++i) {
        s += playerJson(players[i], indent + "  ");
        if (i + 1 < players.size()) s += ",";
        s += "\n";
    }
    return s + indent + "]";
}

} // namespace

std::string exportGameToJson(const GameEngine& engine) {
    const GameResult r = engine.result();

    std::string s = "{\n";
    s += "  " + q("awayTeam")   + ": " + q(engine.awayTeamName()) + ",\n";
    s += "  " + q("homeTeam")   + ": " + q(engine.homeTeamName()) + ",\n";
    s += "  " + q("awayScore")  + ": " + std::to_string(r.awayScore)  + ",\n";
    s += "  " + q("homeScore")  + ": " + std::to_string(r.homeScore)  + ",\n";
    s += "  " + q("innings")    + ": " + std::to_string(r.innings)    + ",\n";
    s += "  " + q("result")     + ": " + q(resultTypeName(r.type))    + ",\n";

    s += "  " + q("awayLineScore") + ": " + intArray(engine.awayLineScore()) + ",\n";
    s += "  " + q("homeLineScore") + ": " + intArray(engine.homeLineScore()) + ",\n";

    s += "  " + q("awayPlayerStats") + ": " + playerArrayJson(engine.awayPlayerStats(), "  ") + ",\n";
    s += "  " + q("homePlayerStats") + ": " + playerArrayJson(engine.homePlayerStats(), "  ") + "\n";

    s += "}\n";
    return s;
}

} // namespace joji
