#include "season_simulator.h"
#include <algorithm>
#include <map>

namespace nflsim {

int SeasonSimulator::simulate_game(std::vector<Team>& teams, int home_idx, int away_idx,
                                    std::mt19937_64& rng) const {
    std::uniform_real_distribution<double> uniform(0.0, 1.0);
    double p_home = elo_.win_probability(teams[home_idx].elo, teams[away_idx].elo);
    bool home_wins = uniform(rng) < p_home;
    return home_wins ? home_idx : away_idx;
}

void SeasonSimulator::simulate_regular_season(std::vector<Team>& teams,
                                               const std::vector<Game>& schedule,
                                               std::mt19937_64& rng) const {
    std::uniform_real_distribution<double> uniform(0.0, 1.0);
    for (const auto& g : schedule) {
        Team& home = teams[g.home_idx];
        Team& away = teams[g.away_idx];
        double p_home = elo_.win_probability(home.elo, away.elo);
        bool home_won = uniform(rng) < p_home;
        elo_.update_ratings(home, away, home_won);  // also updates win/loss counts
    }
}

std::vector<int> SeasonSimulator::seed_conference(const std::vector<Team>& teams,
                                                    const std::string& conference,
                                                    std::vector<int>& division_winners_out) const {
    // Group team indices by division, restricted to this conference.
    std::map<std::string, std::vector<int>> div_groups;
    for (int i = 0; i < static_cast<int>(teams.size()); ++i) {
        if (teams[i].conference != conference) continue;
        div_groups[teams[i].division].push_back(i);
    }

    auto better = [&](int a, int b) {
        // Tiebreak: win% first, then current Elo (a simplification of the
        // real NFL's multi-step tiebreaker procedure).
        if (teams[a].win_pct() != teams[b].win_pct())
            return teams[a].win_pct() > teams[b].win_pct();
        return teams[a].elo > teams[b].elo;
    };

    std::vector<int> division_winners;
    std::vector<int> non_winners;
    for (auto& [div, group] : div_groups) {
        std::vector<int> sorted_group = group;
        std::sort(sorted_group.begin(), sorted_group.end(), better);
        division_winners.push_back(sorted_group[0]);
        for (size_t i = 1; i < sorted_group.size(); ++i) non_winners.push_back(sorted_group[i]);
    }

    std::sort(division_winners.begin(), division_winners.end(), better);
    std::sort(non_winners.begin(), non_winners.end(), better);

    std::vector<int> seeds = division_winners;  // seeds 1-4
    for (int i = 0; i < 3 && i < static_cast<int>(non_winners.size()); ++i) {
        seeds.push_back(non_winners[i]);  // seeds 5-7 (wild cards)
    }

    division_winners_out = division_winners;
    return seeds;
}

int SeasonSimulator::simulate_conference_playoffs(std::vector<Team>& teams,
                                                    const std::vector<int>& seeds,
                                                    std::mt19937_64& rng) const {
    // seeds[0..6] = seed 1..7, better seed hosts.
    struct Ranked { int team; int rank; };

    auto play = [&](Ranked a, Ranked b) -> Ranked {
        // Better (lower) rank hosts.
        Ranked home = a.rank < b.rank ? a : b;
        Ranked away = a.rank < b.rank ? b : a;
        int winner = simulate_game(teams, home.team, away.team, rng);
        int winner_rank = (winner == home.team) ? home.rank : away.rank;
        return {winner, winner_rank};
    };

    // Wild Card round (seed 1 has a bye)
    Ranked r2{seeds[1], 2}, r7{seeds[6], 7};
    Ranked r3{seeds[2], 3}, r6{seeds[5], 6};
    Ranked r4{seeds[3], 4}, r5{seeds[4], 5};

    Ranked w27 = play(r2, r7);
    Ranked w36 = play(r3, r6);
    Ranked w45 = play(r4, r5);

    // Divisional round: 4 teams remain (seed 1 + three wild-card-round winners)
    std::vector<Ranked> remaining = {{seeds[0], 1}, w27, w36, w45};
    std::sort(remaining.begin(), remaining.end(),
              [](const Ranked& a, const Ranked& b) { return a.rank < b.rank; });
    // Best seed hosts the worst remaining seed; the middle two play each other.
    Ranked divA = play(remaining[0], remaining[3]);
    Ranked divB = play(remaining[1], remaining[2]);

    // Conference championship
    Ranked champ = play(divA, divB);
    return champ.team;
}

SeasonResult SeasonSimulator::simulate_one_season(std::vector<Team> teams,
                                                    const std::vector<Game>& schedule,
                                                    std::mt19937_64& rng) const {
    for (auto& t : teams) { t.wins = 0; t.losses = 0; t.ties = 0; }

    simulate_regular_season(teams, schedule, rng);

    SeasonResult result;
    std::vector<int> afc_div_winners, nfc_div_winners;
    std::vector<int> afc_seeds = seed_conference(teams, "AFC", afc_div_winners);
    std::vector<int> nfc_seeds = seed_conference(teams, "NFC", nfc_div_winners);

    result.playoff_teams = afc_seeds;
    result.playoff_teams.insert(result.playoff_teams.end(), nfc_seeds.begin(), nfc_seeds.end());
    result.division_winners = afc_div_winners;
    result.division_winners.insert(result.division_winners.end(),
                                    nfc_div_winners.begin(), nfc_div_winners.end());

    result.afc_champion = simulate_conference_playoffs(teams, afc_seeds, rng);
    result.nfc_champion = simulate_conference_playoffs(teams, nfc_seeds, rng);

    // Super Bowl: neutral site, no home-field advantage.
    std::uniform_real_distribution<double> uniform(0.0, 1.0);
    double p_afc = elo_.win_probability(teams[result.afc_champion].elo,
                                         teams[result.nfc_champion].elo, /*neutral_site=*/true);
    result.super_bowl_champion =
        (uniform(rng) < p_afc) ? result.afc_champion : result.nfc_champion;

    return result;
}

}  // namespace nflsim
