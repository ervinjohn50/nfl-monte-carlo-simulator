#pragma once
#include "types.h"

namespace nflsim {

// Elo rating model: computes win probabilities from rating differences,
// and updates ratings after a game result.
class EloModel {
public:
    // home_advantage: Elo points added to the home team's effective rating
    // k_factor: how much a single game result moves ratings (higher = more volatile)
    EloModel(double home_advantage = 65.0, double k_factor = 20.0)
        : home_advantage_(home_advantage), k_factor_(k_factor) {}

    // Probability that the home team beats the away team, given raw ratings.
    // Pass neutral_site=true for games with no home-field advantage (e.g. Super Bowl).
    double win_probability(double home_elo, double away_elo, bool neutral_site = false) const;

    // Applies one game's result to both teams' Elo ratings in place.
    // home_won: true if the home team won, false if the away team won.
    // (Ties are rare in the NFL; callers can split the update 50/50 for a tie.)
    void update_ratings(Team& home, Team& away, bool home_won) const;

private:
    double home_advantage_;
    double k_factor_;
};

}  // namespace nflsim
