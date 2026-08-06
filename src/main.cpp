#include "csv_loader.h"
#include "elo_model.h"
#include "schedule_generator.h"
#include "season_simulator.h"
#include "monte_carlo_runner.h"

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>

using namespace nflsim;

namespace {

struct Args {
    std::string teams_csv = "data/teams.csv";
    long long trials = 10000;
    unsigned int threads = std::max(1u, std::thread::hardware_concurrency());
    unsigned int seed = 42;
    bool json = false;
};

Args parse_args(int argc, char** argv) {
    Args args;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--trials" && i + 1 < argc) args.trials = std::stoll(argv[++i]);
        else if (arg == "--threads" && i + 1 < argc) args.threads = std::stoul(argv[++i]);
        else if (arg == "--seed" && i + 1 < argc) args.seed = std::stoul(argv[++i]);
        else if (arg == "--teams" && i + 1 < argc) args.teams_csv = argv[++i];
        else if (arg == "--json") args.json = true;
    }
    return args;
}

std::string escape_json_string(const std::string& s) {
    std::string out;
    for (char c : s) {
        if (c == '"') out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else out += c;
    }
    return out;
}

void print_json(const std::vector<Team>& teams, const AggregateStats& stats,
                double elapsed_ms) {
    double n = static_cast<double>(stats.trials);
    double seasons_per_sec = stats.trials / (elapsed_ms / 1000.0);

    std::ostringstream os;
    os << std::fixed << std::setprecision(2);
    os << "{\"trials\":" << stats.trials
       << ",\"elapsed_ms\":" << elapsed_ms
       << ",\"seasons_per_sec\":" << seasons_per_sec
       << ",\"teams\":[";

    for (size_t i = 0; i < teams.size(); ++i) {
        if (i > 0) os << ",";
        os << "{\"name\":\"" << escape_json_string(teams[i].name) << "\""
           << ",\"conference\":\"" << teams[i].conference << "\""
           << ",\"division\":\"" << teams[i].division << "\""
           << ",\"elo\":" << teams[i].elo
           << ",\"playoff_pct\":" << (100.0 * stats.playoff_appearances[i] / n)
           << ",\"division_pct\":" << (100.0 * stats.division_titles[i] / n)
           << ",\"conference_pct\":" << (100.0 * stats.conference_titles[i] / n)
           << ",\"championship_pct\":" << (100.0 * stats.championships[i] / n)
           << "}";
    }
    os << "]}";
    std::cout << os.str() << "\n";
}

void print_report(const std::vector<Team>& teams, const AggregateStats& stats) {
    struct Row {
        std::string name;
        double playoff_pct, division_pct, conference_pct, championship_pct;
    };
    std::vector<Row> rows;
    for (size_t i = 0; i < teams.size(); ++i) {
        double n = static_cast<double>(stats.trials);
        rows.push_back({
            teams[i].name,
            100.0 * stats.playoff_appearances[i] / n,
            100.0 * stats.division_titles[i] / n,
            100.0 * stats.conference_titles[i] / n,
            100.0 * stats.championships[i] / n,
        });
    }
    std::sort(rows.begin(), rows.end(),
              [](const Row& a, const Row& b) { return a.championship_pct > b.championship_pct; });

    std::cout << std::left << std::setw(26) << "Team"
              << std::right << std::setw(10) << "Playoff%"
              << std::setw(10) << "Div%"
              << std::setw(10) << "Conf%"
              << std::setw(12) << "SB Champ%" << "\n";
    std::cout << std::string(68, '-') << "\n";
    for (const auto& r : rows) {
        std::cout << std::left << std::setw(26) << r.name
                  << std::right << std::fixed << std::setprecision(1)
                  << std::setw(10) << r.playoff_pct
                  << std::setw(10) << r.division_pct
                  << std::setw(10) << r.conference_pct
                  << std::setw(12) << r.championship_pct << "\n";
    }
}

}  // namespace

int main(int argc, char** argv) {
    Args args = parse_args(argc, argv);

    if (!args.json) {
        std::cout << "Loading teams from " << args.teams_csv << "...\n";
    }
    std::vector<Team> teams = load_teams_csv(args.teams_csv);
    if (!args.json) {
        std::cout << "Loaded " << teams.size() << " teams.\n";
    }

    std::vector<Game> schedule = generate_season_schedule(teams, /*seed=*/args.seed);
    if (!args.json) {
        std::cout << "Generated schedule with " << schedule.size() << " games.\n";
        std::cout << "Running " << args.trials << " season simulations across "
                  << args.threads << " thread(s)...\n";
    }

    EloModel elo_model;
    SeasonSimulator season_sim(elo_model);
    MonteCarloRunner runner(teams, schedule, season_sim);

    auto start = std::chrono::steady_clock::now();
    AggregateStats stats = runner.run(args.trials, args.threads, args.seed);
    auto end = std::chrono::steady_clock::now();
    double elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();

    if (args.json) {
        print_json(teams, stats, elapsed_ms);
    } else {
        std::cout << "Done in " << std::fixed << std::setprecision(1) << elapsed_ms << " ms ("
                  << (args.trials / (elapsed_ms / 1000.0)) << " seasons/sec)\n\n";
        print_report(teams, stats);
    }
    return 0;
}
