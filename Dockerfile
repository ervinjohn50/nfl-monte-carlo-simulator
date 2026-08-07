FROM gcc:13-bookworm

RUN apt-get update && apt-get install -y --no-install-recommends \
    python3 python3-venv curl \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY Makefile .
COPY include/ include/
COPY src/ src/
COPY tests/ tests/
RUN make

COPY data/ data/
COPY server.py .
COPY viz/ viz/

RUN python3 -m venv .venv && .venv/bin/pip install --no-cache-dir flask

EXPOSE 5050

CMD [".venv/bin/python3", "server.py"]
