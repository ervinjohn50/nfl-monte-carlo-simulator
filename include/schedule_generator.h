#pragma once
#include "types.h"
#include <vector>

namespace nflsim {

// Builds a simplified but structurally realistic NFL regular-season schedule:
//   - Each team plays its 3 division rivals twice (home + away)   -> 6 games
//   - Each team plays all 4 teams in one other division within
//     its own conference once (rotates by division pairing)       -> 4 games
//   - Each team plays all 4 teams in one division from the other
//     conference once (rotates by division pairing)                -> 4 games
//   - 3 additional games against remaining in-conference teams
//     (randomly assigned, no repeat matchups)                       -> 3 games
// Total: 17 games per team, 272 games league-wide.
//
// This mirrors the real NFL scheduling formula closely but simplifies the
// "based on prior-year standings" rules, which require data this project
// doesn't track. Game order is not mapped to actual calendar weeks.
// standings only depend on the final win/loss totals, not sequencing.
std::vector<Game> generate_season_schedule(const std::vector<Team>& teams,
                                            unsigned int seed);

}  // namespace nflsim
