#include "elo_model.h"
#include <cmath>

namespace nflsim {

double EloModel::win_probability(double home_elo, double away_elo, bool neutral_site) const {
    double effective_home = home_elo + (neutral_site ? 0.0 : home_advantage_);
    double diff = away_elo - effective_home;
    return 1.0 / (1.0 + std::pow(10.0, diff / 400.0));
}

void EloModel::update_ratings(Team& home, Team& away, bool home_won) const {
    double p_home = win_probability(home.elo, away.elo);
    double actual_home = home_won ? 1.0 : 0.0;

    double delta = k_factor_ * (actual_home - p_home);
    home.elo += delta;
    away.elo -= delta;

    if (home_won) {
        home.wins++;
        away.losses++;
    } else {
        away.wins++;
        home.losses++;
    }
}

}  // namespace nflsim
