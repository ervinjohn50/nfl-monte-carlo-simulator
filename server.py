import csv
import json
import os
import subprocess
import tempfile
from flask import Flask, request, jsonify, send_from_directory

from data.fetch_ratings import fetch_and_update

app = Flask(__name__)

SIMULATOR_BIN = "./build/nfl_simulator"
TEAMS_CSV = "data/teams.csv"

def load_teams():
    teams = []
    with open(TEAMS_CSV) as f:
        for row in csv.DictReader(f):
            teams.append({
                "name": row["name"],
                "conference": row["conference"],
                "division": row["division"],
                "elo": int(row["elo"]),
            })
    return teams

@app.route("/")
def index():
    return send_from_directory("viz", "index.html")

@app.route("/api/teams")
def get_teams():
    return jsonify(load_teams())

@app.route("/api/simulate", methods=["GET", "POST"])
def simulate():
    if request.method == "POST":
        body = request.get_json()
        trials = str(body.get("trials", 10000))
        seed = str(body.get("seed", 42))
        threads = str(body.get("threads", 4))
        custom_teams = body.get("teams")
    else:
        trials = request.args.get("trials", "10000")
        threads = request.args.get("threads", "4")
        seed = request.args.get("seed", "42")
        custom_teams = None

    teams_path = TEAMS_CSV
    tmp_file = None

    if custom_teams:
        tmp_file = tempfile.NamedTemporaryFile(
            mode="w", suffix=".csv", delete=False
        )
        writer = csv.writer(tmp_file)
        writer.writerow(["name", "conference", "division", "elo"])
        for t in custom_teams:
            writer.writerow([t["name"], t["conference"], t["division"], t["elo"]])
        tmp_file.close()
        teams_path = tmp_file.name

    try:
        result = subprocess.run(
            [SIMULATOR_BIN, "--json", "--trials", trials,
             "--threads", threads, "--seed", seed, "--teams", teams_path],
            capture_output=True, text=True, timeout=30
        )
        if result.returncode != 0:
            return jsonify({"error": result.stderr.strip()}), 500
        return app.response_class(result.stdout, mimetype="application/json")
    except subprocess.TimeoutExpired:
        return jsonify({"error": "Simulation timed out"}), 504
    except FileNotFoundError:
        return jsonify({"error": f"Simulator binary not found at {SIMULATOR_BIN}. Run 'make' first."}), 500
    finally:
        if tmp_file:
            os.unlink(tmp_file.name)

@app.route("/api/refresh", methods=["POST"])
def refresh():
    try:
        teams, season = fetch_and_update()
        return jsonify({"teams": teams, "season": season})
    except Exception as e:
        return jsonify({"error": str(e)}), 502


if __name__ == "__main__":
    print(f"Starting server - simulator binary: {SIMULATOR_BIN}")
    host = os.environ.get("HOST", "127.0.0.1")
    app.run(host=host, port=5050, debug=False)
