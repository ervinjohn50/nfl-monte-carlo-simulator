#pragma once
#include "types.h"
#include "season_simulator.h"
#include <vector>

namespace nflsim {

class MonteCarloRunner {
public:
    MonteCarloRunner(std::vector<Team> teams, std::vector<Game> schedule,
                      SeasonSimulator simulator)
        : teams_(std::move(teams)), schedule_(std::move(schedule)),
          simulator_(std::move(simulator)) {}

    // Runs `num_trials` independent season simulations split across
    // `num_threads` worker threads, and returns aggregated statistics.
    // Each thread gets its own RNG (seeded independently) and its own local
    // AggregateStats to avoid any shared mutable state / lock contention;
    // results are merged once at the end.
    AggregateStats run(long long num_trials, unsigned int num_threads, unsigned int base_seed);

private:
    std::vector<Team> teams_;
    std::vector<Game> schedule_;
    SeasonSimulator simulator_;
};

}  // namespace nflsim
