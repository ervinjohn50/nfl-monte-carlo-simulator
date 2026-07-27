#include "csv_loader.h"
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace nflsim {

namespace {
std::vector<std::string> split_csv_line(const std::string& line) {
    std::vector<std::string> fields;
    std::stringstream ss(line);
    std::string field;
    while (std::getline(ss, field, ',')) fields.push_back(field);
    return fields;
}
}  // namespace

std::vector<Team> load_teams_csv(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open teams CSV: " + path);
    }

    std::vector<Team> teams;
    std::string line;
    bool first_line = true;
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        if (first_line) { first_line = false; continue; }  // skip header

        auto fields = split_csv_line(line);
        if (fields.size() < 4) continue;

        Team t;
        t.name = fields[0];
        t.conference = fields[1];
        t.division = fields[2];
        t.elo = std::stod(fields[3]);
        teams.push_back(t);
    }

    if (teams.empty()) {
        throw std::runtime_error("No teams loaded from CSV (file may be empty or malformed): " + path);
    }
    
    return teams;
}

}  // namespace nflsim
