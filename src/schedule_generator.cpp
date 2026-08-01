#include "schedule_generator.h"
#include <algorithm>
#include <map>
#include <random>
#include <set>
#include <stdexcept>

namespace nflsim {

namespace {

using TeamGroup = std::vector<int>;  // team indices

// Groups team indices by "conference-division" key, e.g. "AFC-East".
std::map<std::string, TeamGroup> group_by_division(const std::vector<Team>& teams) {
    std::map<std::string, TeamGroup> groups;
    for (int i = 0; i < static_cast<int>(teams.size()); ++i) {
        std::string key = teams[i].conference + "-" + teams[i].division;
        groups[key].push_back(i);
    }
    return groups;
}

// Adds a home-and-away pair of games between every pair of teams in `group`.
void add_round_robin_both_legs(const TeamGroup& group, std::vector<Game>& games) {
    for (size_t i = 0; i < group.size(); ++i) {
        for (size_t j = i + 1; j < group.size(); ++j) {
            games.push_back({group[i], group[j]});
            games.push_back({group[j], group[i]});
        }
    }
}

// Adds one game (single leg) between every pair across two groups of equal size,
// alternating home/away for balance.
void add_cross_group_single_leg(const TeamGroup& a, const TeamGroup& b,
                                 std::vector<Game>& games) {
    bool flip = false;
    for (int ta : a) {
        for (int tb : b) {
            if (flip) games.push_back({tb, ta});
            else games.push_back({ta, tb});
            flip = !flip;
        }
    }
}

// Builds a random 3-regular bipartite "schedule" between two equal-size groups
// (8 teams each in practice) using three randomly generated permutations that
// are pairwise non-colliding per position. Guarantees each team gets exactly
// 3 distinct opponents from the other group, with no repeated matchup.
std::vector<std::pair<int, int>> random_3_regular_bipartite(
    const TeamGroup& a, const TeamGroup& b, std::mt19937& rng) {
    size_t n = a.size();
    if (b.size() != n) throw std::runtime_error("bipartite groups must be equal size");

    std::vector<std::vector<size_t>> permutations;  // 3 permutations of [0..n)
    while (permutations.size() < 3) {
        std::vector<size_t> perm(n);
        for (size_t i = 0; i < n; ++i) perm[i] = i;
        std::shuffle(perm.begin(), perm.end(), rng);

        bool collides = false;
        for (const auto& prev : permutations) {
            for (size_t i = 0; i < n; ++i) {
                if (prev[i] == perm[i]) { collides = true; break; }
            }
            if (collides) break;
        }
        if (!collides) permutations.push_back(perm);
    }

    std::vector<std::pair<int, int>> edges;
    for (const auto& perm : permutations) {
        for (size_t i = 0; i < n; ++i) {
            edges.emplace_back(a[i], b[perm[i]]);
        }
    }
    return edges;
}

}  // namespace

std::vector<Game> generate_season_schedule(const std::vector<Team>& teams,
                                            unsigned int seed) {
    std::mt19937 rng(seed);
    std::vector<Game> games;
    games.reserve(272);

    auto divisions = group_by_division(teams);  // 8 groups of 4

    // --- 1. Division games: everyone plays division rivals twice (6 games) ---
    for (auto& [key, group] : divisions) {
        add_round_robin_both_legs(group, games);
    }

    // --- 2. Within-conference division pairing (4 games) ---
    // East<->North, South<->West, within each conference.
    add_cross_group_single_leg(divisions["AFC-East"], divisions["AFC-North"], games);
    add_cross_group_single_leg(divisions["AFC-South"], divisions["AFC-West"], games);
    add_cross_group_single_leg(divisions["NFC-East"], divisions["NFC-North"], games);
    add_cross_group_single_leg(divisions["NFC-South"], divisions["NFC-West"], games);

    // --- 3. Interconference division pairing (4 games) ---
    add_cross_group_single_leg(divisions["AFC-East"], divisions["NFC-South"], games);
    add_cross_group_single_leg(divisions["AFC-North"], divisions["NFC-West"], games);
    add_cross_group_single_leg(divisions["AFC-South"], divisions["NFC-East"], games);
    add_cross_group_single_leg(divisions["AFC-West"], divisions["NFC-North"], games);

    // --- 4. Crossover games (3 games each) ---
    // For East/North teams, the untouched divisions in-conference are South/West
    // (and vice versa). This is exactly bipartite, so a random 3-regular
    // bipartite graph gives each team 3 more distinct, non-repeated opponents.
    auto afc_group1 = divisions["AFC-East"];
    afc_group1.insert(afc_group1.end(), divisions["AFC-North"].begin(), divisions["AFC-North"].end());
    auto afc_group2 = divisions["AFC-South"];
    afc_group2.insert(afc_group2.end(), divisions["AFC-West"].begin(), divisions["AFC-West"].end());

    auto nfc_group1 = divisions["NFC-East"];
    nfc_group1.insert(nfc_group1.end(), divisions["NFC-North"].begin(), divisions["NFC-North"].end());
    auto nfc_group2 = divisions["NFC-South"];
    nfc_group2.insert(nfc_group2.end(), divisions["NFC-West"].begin(), divisions["NFC-West"].end());

    for (auto& edge : random_3_regular_bipartite(afc_group1, afc_group2, rng)) {
        games.push_back({edge.first, edge.second});
    }
    for (auto& edge : random_3_regular_bipartite(nfc_group1, nfc_group2, rng)) {
        games.push_back({edge.first, edge.second});
    }

    return games;
}

}  // namespace nflsim
