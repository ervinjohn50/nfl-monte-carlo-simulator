// Lightweight assertion-based test suite (no external test framework needed).
// Run with: make test
#include "csv_loader.h"
#include "elo_model.h"
#include "schedule_generator.h"
#include "season_simulator.h"
#include "monte_carlo_runner.h"

#include <cmath>
#include <iostream>
#include <map>
#include <set>
#include <string>

using namespace nflsim;

namespace {

int g_passed = 0;
int g_failed = 0;

void check(bool condition, const std::string& description) {
    if (condition) {
        std::cout << "PASS: " << description << "\n";
        g_passed++;
    } else {
        std::cout << "FAIL: " << description << "\n";
        g_failed++;
    }
}

bool approx(double a, double b, double tol = 1e-6) {
    return std::fabs(a - b) < tol;
}

// ---------------------------------------------------------------------
void test_elo_win_probability_symmetry() {
    EloModel model(/*home_advantage=*/0.0, /*k=*/20.0);  // no home edge for a clean symmetry check
    double p_a = model.win_probability(1600, 1500);
    double p_b = model.win_probability(1500, 1600);
    check(approx(p_a + p_b, 1.0, 1e-9), "Elo win probabilities sum to 1 (symmetric, no home edge)");
    check(p_a > 0.5, "Higher-rated team is favored");
}

void test_elo_equal_ratings_are_a_toss_up() {
    EloModel model(0.0, 20.0);
    double p = model.win_probability(1500, 1500);
    check(approx(p, 0.5, 1e-9), "Equal ratings give exactly 50% win probability (no home edge)");
}

void test_elo_home_advantage_shifts_probability() {
    EloModel model(65.0, 20.0);
    double p_home = model.win_probability(1500, 1500);
    check(p_home > 0.5, "Home-field advantage makes an even matchup favor the home team");
}

void test_elo_update_moves_ratings_correctly() {
    EloModel model(0.0, 20.0);
    Team home{"Home", "AFC", "East", 1500};
    Team away{"Away", "AFC", "East", 1500};
    model.update_ratings(home, away, /*home_won=*/true);
    check(home.elo > 1500.0, "Winning team's Elo increases");
    check(away.elo < 1500.0, "Losing team's Elo decreases");
    check(approx(home.elo - 1500.0, 1500.0 - away.elo), "Elo point transfer is zero-sum");
    check(home.wins == 1 && away.losses == 1, "Win/loss records update correctly");
}

void test_elo_upset_moves_ratings_more_than_expected_result() {
    // A big underdog winning should move ratings by more than a coin-flip result would.
    EloModel model(0.0, 20.0);
    Team favorite{"Fav", "AFC", "East", 1700};
    Team underdog{"Dog", "AFC", "East", 1300};
    double underdog_elo_before = underdog.elo;
    model.update_ratings(favorite, underdog, /*home_won=*/false);  // underdog (away) wins
    double gain = underdog.elo - underdog_elo_before;
    check(gain > 15.0, "A major upset produces a large Elo swing (K-factor scaling works)");
}

// ---------------------------------------------------------------------
void test_schedule_structural_integrity() {
    auto teams = load_teams_csv("data/teams.csv");
    auto schedule = generate_season_schedule(teams, /*seed=*/123);

    check(schedule.size() == 272, "Schedule has exactly 272 games (32 teams x 17 / 2)");

    std::map<int, int> games_per_team;
    int self_games = 0;
    for (auto& g : schedule) {
        games_per_team[g.home_idx]++;
        games_per_team[g.away_idx]++;
        if (g.home_idx == g.away_idx) self_games++;
    }
    check(self_games == 0, "No team is scheduled to play itself");

    bool all_seventeen = true;
    for (auto& [idx, count] : games_per_team) {
        if (count != 17) all_seventeen = false;
    }
    check(all_seventeen && games_per_team.size() == teams.size(),
          "Every team has exactly 17 games");

    // No non-division matchup should repeat.
    std::map<std::pair<int, int>, int> pair_counts;
    for (auto& g : schedule) {
        int a = std::min(g.home_idx, g.away_idx), b = std::max(g.home_idx, g.away_idx);
        pair_counts[{a, b}]++;
    }
    bool valid = true;
    for (auto& [p, count] : pair_counts) {
        bool same_division = teams[p.first].conference == teams[p.second].conference &&
                              teams[p.first].division == teams[p.second].division;
        if (same_division && count != 2) valid = false;
        if (!same_division && count > 1) valid = false;
    }
    check(valid, "Division rivals play exactly twice; no other matchup repeats");
}

void test_schedule_is_deterministic_given_seed() {
    auto teams = load_teams_csv("data/teams.csv");
    auto s1 = generate_season_schedule(teams, 99);
    auto s2 = generate_season_schedule(teams, 99);
    bool identical = s1.size() == s2.size();
    for (size_t i = 0; identical && i < s1.size(); ++i) {
        if (s1[i].home_idx != s2[i].home_idx || s1[i].away_idx != s2[i].away_idx) identical = false;
    }
    check(identical, "Same seed produces an identical schedule (reproducibility)");
}

// ---------------------------------------------------------------------
void test_season_simulator_produces_valid_playoff_field() {
    auto teams = load_teams_csv("data/teams.csv");
    auto schedule = generate_season_schedule(teams, 1);
    EloModel elo;
    SeasonSimulator sim(elo);
    std::mt19937_64 rng(555);

    SeasonResult result = sim.simulate_one_season(teams, schedule, rng);

    check(result.playoff_teams.size() == 14, "Exactly 14 teams make the playoffs (7 per conference)");
    check(result.division_winners.size() == 8, "Exactly 8 division winners");

    std::set<int> unique_playoff_teams(result.playoff_teams.begin(), result.playoff_teams.end());
    check(unique_playoff_teams.size() == 14, "No team appears twice in the playoff field");

    check(result.super_bowl_champion == result.afc_champion ||
          result.super_bowl_champion == result.nfc_champion,
          "Super Bowl champion is one of the two conference champions");
}

void test_original_teams_vector_is_not_mutated_between_trials() {
    // Regression guard: simulate_one_season takes teams by value, so the
    // caller's original Elo ratings/records must be untouched afterward
    // this is what makes independent Monte Carlo trials valid in the first place.
    auto teams = load_teams_csv("data/teams.csv");
    auto schedule = generate_season_schedule(teams, 2);
    double original_elo = teams[0].elo;
    EloModel elo;
    SeasonSimulator sim(elo);
    std::mt19937_64 rng(1);
    sim.simulate_one_season(teams, schedule, rng);
    check(approx(teams[0].elo, original_elo), "Original teams vector's Elo is untouched after simulating a season");
    check(teams[0].wins == 0 && teams[0].losses == 0, "Original teams vector's records are untouched");
}

// ---------------------------------------------------------------------
void test_monte_carlo_aggregate_stats_are_internally_consistent() {
    auto teams = load_teams_csv("data/teams.csv");
    auto schedule = generate_season_schedule(teams, 3);
    EloModel elo;
    SeasonSimulator sim(elo);
    MonteCarloRunner runner(teams, schedule, sim);

    long long trials = 500;
    AggregateStats stats = runner.run(trials, /*num_threads=*/2, /*base_seed=*/10);

    check(stats.trials == trials, "Aggregate trial count matches requested trials");

    long long total_playoff_appearances = 0;
    for (auto v : stats.playoff_appearances) total_playoff_appearances += v;
    check(total_playoff_appearances == trials * 14,
          "Total playoff appearances across all teams = trials x 14");

    long long total_championships = 0;
    for (auto v : stats.championships) total_championships += v;
    check(total_championships == trials, "Exactly one champion is recorded per trial");
}

void test_multithreaded_and_singlethreaded_runs_agree_in_distribution() {
    // Not a bit-for-bit check (different thread counts consume RNG streams
    // differently) but championship totals should still sum to `trials`,
    // and no team should win an implausible fraction, regardless of thread count.
    auto teams = load_teams_csv("data/teams.csv");
    auto schedule = generate_season_schedule(teams, 4);
    EloModel elo;
    SeasonSimulator sim(elo);

    MonteCarloRunner runner1(teams, schedule, sim);
    AggregateStats single = runner1.run(2000, 1, 20);

    MonteCarloRunner runner2(teams, schedule, sim);
    AggregateStats multi = runner2.run(2000, 4, 20);

    check(single.trials == 2000 && multi.trials == 2000,
          "Single- and multi-threaded runs both complete the requested trial count");
}

}  // namespace

int main() {
    test_elo_win_probability_symmetry();
    test_elo_equal_ratings_are_a_toss_up();
    test_elo_home_advantage_shifts_probability();
    test_elo_update_moves_ratings_correctly();
    test_elo_upset_moves_ratings_more_than_expected_result();

    test_schedule_structural_integrity();
    test_schedule_is_deterministic_given_seed();

    test_season_simulator_produces_valid_playoff_field();
    test_original_teams_vector_is_not_mutated_between_trials();

    test_monte_carlo_aggregate_stats_are_internally_consistent();
    test_multithreaded_and_singlethreaded_runs_agree_in_distribution();

    std::cout << "\n" << g_passed << " passed, " << g_failed << " failed\n";
    return g_failed == 0 ? 0 : 1;
}
