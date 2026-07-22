#pragma once
#include "types.h"
#include <string>
#include <vector>

namespace nflsim {

// Loads teams from a CSV with header: name,conference,division,elo
std::vector<Team> load_teams_csv(const std::string& path);

}
