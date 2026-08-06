import json
import subprocess
import sys
from flask import Flask, request, jsonify, send_from_directory

app = Flask(__name__)

SIMULATOR_BIN = "./build/nfl_simulator"

@app.route("/")
def index():
    return send_from_directory("viz", "index.html")

@app.route("/api/simulate")
def simulate():
    trials = request.args.get("trials", "10000")
    threads = request.args.get("threads", "4")
    seed = request.args.get("seed", "42")

    try:
        result = subprocess.run(
            [SIMULATOR_BIN, "--json", "--trials", trials, "--threads", threads, "--seed", seed],
            capture_output=True, text=True, timeout=30
        )
        if result.returncode != 0:
            return jsonify({"error": result.stderr.strip()}), 500
        return app.response_class(result.stdout, mimetype="application/json")
    except subprocess.TimeoutExpired:
        return jsonify({"error": "Simulation timed out"}), 504
    except FileNotFoundError:
        return jsonify({"error": f"Simulator binary not found at {SIMULATOR_BIN}. Run 'make' first."}), 500

if __name__ == "__main__":
    print(f"Starting server — simulator binary: {SIMULATOR_BIN}")
    app.run(host="127.0.0.1", port=5050, debug=False)
