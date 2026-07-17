CXX := g++
CXXFLAGS := -std=c++17 -Wall

all: build/nfl_sim

build/nfl_sim: src/main.cpp
	@mkdir -p build
	$(CXX) $(CXXFLAGS) src/main.cpp -o build/nfl_sim