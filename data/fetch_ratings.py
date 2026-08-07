import csv
import json
import os
import subprocess

STANDINGS_URL = (
    "https://site.api.espn.com/apis/v2/sports/football/nfl/standings"
    "?level=3"
)
OUTPUT_CSV = os.path.join(os.path.dirname(__file__), "teams.csv")

BASE_ELO = 1500
ELO_SPREAD = 300


def win_pct_to_elo(win_pct):
    return int(BASE_ELO + ELO_SPREAD * (win_pct - 0.5))


def fetch_standings(season=None):
    url = STANDINGS_URL
    if season:
        url += f"&season={season}"

    result = subprocess.run(
        ["curl", "-s", url],
        capture_output=True, text=True, timeout=10,
    )
    if result.returncode != 0:
        raise RuntimeError(f"curl failed: {result.stderr}")
    return json.loads(result.stdout)


def parse_teams(data):
    teams = []
    for conf in data["children"]:
        conf_name = conf["abbreviation"]
        for div in conf["children"]:
            div_short = div["name"].replace(conf_name + " ", "")
            for entry in div["standings"]["entries"]:
                stats = {
                    s["name"]: s["value"]
                    for s in entry["stats"]
                    if "value" in s
                }
                wins = int(stats.get("wins", 0))
                losses = int(stats.get("losses", 0))
                games = wins + losses
                win_pct = wins / games if games > 0 else 0.5
                teams.append({
                    "name": entry["team"]["displayName"],
                    "conference": conf_name,
                    "division": div_short,
                    "elo": win_pct_to_elo(win_pct),
                })
    return teams


def write_csv(teams, path=OUTPUT_CSV):
    with open(path, "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=["name", "conference", "division", "elo"])
        writer.writeheader()
        writer.writerows(teams)


def has_games_played(teams):
    return any(t["elo"] != BASE_ELO for t in teams)


def fetch_and_update(season=None):
    data = fetch_standings(season)
    season_year = data.get("season", {}).get("year", "unknown")
    teams = parse_teams(data)

    if not has_games_played(teams) and season is None:
        prev_year = int(season_year) - 1
        data = fetch_standings(season=prev_year)
        season_year = prev_year
        teams = parse_teams(data)

    write_csv(teams)
    return teams, season_year


if __name__ == "__main__":
    teams, year = fetch_and_update()
    print(f"Fetched {len(teams)} teams from {year} season standings")
    for t in sorted(teams, key=lambda x: x["elo"], reverse=True):
        print(f"  {t['name']:30s} {t['conference']} {t['division']:6s}  {t['elo']}")
