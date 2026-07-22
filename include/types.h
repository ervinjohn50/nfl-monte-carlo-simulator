#pragma once
#include <string>
#include <vector>

namespace nflsim {

struct Team {
    std::string name;
    std::string conference;   // "AFC" or "NFC"
    std::string division;     // "East", "North", "South", "West"
    double elo;                // current Elo rating (mutated during a simulated season)

    // Per-season record, reset before every Monte Carlo trial
    int wins = 0;
    int losses = 0;
    int ties = 0;

    double win_pct() const {
        int games = wins + losses + ties;
        if (games == 0) return 0.0;
        return (wins + 0.5 * ties) / static_cast<double>(games);
    }
};

struct Game {
    int home_idx;   // index into the teams vector
    int away_idx;
};

// Result of simulating one full season (regular season + playoffs) once.
struct SeasonResult {
    std::vector<int> playoff_teams;      // team indices that made the playoffs (14)
    std::vector<int> division_winners;   // team indices (8)
    int afc_champion = -1;
    int nfc_champion = -1;
    int super_bowl_champion = -1;
};

// Aggregated statistics across many Monte Carlo trials.
struct AggregateStats {
    std::vector<long long> playoff_appearances;
    std::vector<long long> division_titles;
    std::vector<long long> conference_titles;
    std::vector<long long> championships;
    long long trials = 0;

    explicit AggregateStats(size_t num_teams)
        : playoff_appearances(num_teams, 0),
          division_titles(num_teams, 0),
          conference_titles(num_teams, 0),
          championships(num_teams, 0) {}

    void merge(const AggregateStats& other) {
        for (size_t i = 0; i < playoff_appearances.size(); ++i) {
            playoff_appearances[i] += other.playoff_appearances[i];
            division_titles[i] += other.division_titles[i];
            conference_titles[i] += other.conference_titles[i];
            championships[i] += other.championships[i];
        }
        trials += other.trials;
    }
};

}  // namespace nflsim
