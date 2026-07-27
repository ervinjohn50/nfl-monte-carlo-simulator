#include "monte_carlo_runner.h"
#include <random>
#include <thread>
#include <vector>

namespace nflsim {

namespace {

void record_result(const SeasonResult& r, AggregateStats& stats) {
    for (int idx : r.playoff_teams) stats.playoff_appearances[idx]++;
    for (int idx : r.division_winners) stats.division_titles[idx]++;
    stats.conference_titles[r.afc_champion]++;
    stats.conference_titles[r.nfc_champion]++;
    stats.championships[r.super_bowl_champion]++;
    stats.trials++;
}

// Work done by a single thread: run `trials_for_this_thread` seasons and
// return its own local AggregateStats.
AggregateStats worker(const std::vector<Team>& teams, const std::vector<Game>& schedule,
                       const SeasonSimulator& simulator, long long trials_for_this_thread,
                       unsigned int seed) {
    AggregateStats local_stats(teams.size());
    std::mt19937_64 rng(seed);  // each thread's RNG stream is independent

    for (long long i = 0; i < trials_for_this_thread; ++i) {
        SeasonResult result = simulator.simulate_one_season(teams, schedule, rng);
        record_result(result, local_stats);
    }
    return local_stats;
}

}  // namespace

AggregateStats MonteCarloRunner::run(long long num_trials, unsigned int num_threads,
                                      unsigned int base_seed) {
    if (num_threads == 0) num_threads = 1;

    // Split trials as evenly as possible across threads.
    std::vector<long long> trials_per_thread(num_threads, num_trials / num_threads);
    trials_per_thread[0] += num_trials % num_threads;  // remainder goes to thread 0

    std::vector<std::thread> threads;
    std::vector<AggregateStats> partials(num_threads, AggregateStats(teams_.size()));

    for (unsigned int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            // base_seed + t guarantees every thread gets a distinct RNG stream,
            // while the whole run stays reproducible given the same base_seed.
            partials[t] = worker(teams_, schedule_, simulator_, trials_per_thread[t],
                                  base_seed + t);
        });
    }
    for (auto& th : threads) th.join();

    AggregateStats total(teams_.size());
    for (auto& p : partials) total.merge(p);
    return total;
}

}  // namespace nflsim
