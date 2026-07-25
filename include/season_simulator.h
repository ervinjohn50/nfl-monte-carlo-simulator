#pragma once
#include "types.h"
#include "elo_model.h"
#include <random>
#include <vector>

namespace nflsim {

class SeasonSimulator {
public:
    explicit SeasonSimulator(const EloModel& elo_model) : elo_(elo_model) {}

    // Simulates one full season (regular season + playoffs) on a *copy* of the
    // teams vector, so ratings/records don't leak between Monte Carlo trials.
    // `schedule` is the fixed list of regular-season games (order doesn't
    // affect final standings, only intermediate Elo drift within the season).
    SeasonResult simulate_one_season(std::vector<Team> teams,
                                      const std::vector<Game>& schedule,
                                      std::mt19937_64& rng) const;

private:
    EloModel elo_;

    void simulate_regular_season(std::vector<Team>& teams,
                                  const std::vector<Game>& schedule,
                                  std::mt19937_64& rng) const;

    // Returns the 7 playoff seeds for one conference, ranked 1-7:
    // seeds 1-4 are division winners (best division winner = 1 seed),
    // seeds 5-7 are the next-best non-division-winning teams.
    std::vector<int> seed_conference(const std::vector<Team>& teams,
                                      const std::string& conference,
                                      std::vector<int>& division_winners_out) const;

    // Single-elimination bracket for one conference's 7 seeds -> returns champion index.
    int simulate_conference_playoffs(std::vector<Team>& teams,
                                      const std::vector<int>& seeds,
                                      std::mt19937_64& rng) const;

    // Simulates a single game and returns the winner's team index.
    int simulate_game(std::vector<Team>& teams, int home_idx, int away_idx,
                       std::mt19937_64& rng) const;
};

}  // namespace nflsim
