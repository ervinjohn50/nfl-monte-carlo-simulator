CXX := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -Iinclude -pthread

SRC := src/elo_model.cpp src/schedule_generator.cpp src/season_simulator.cpp \
       src/monte_carlo_runner.cpp src/csv_loader.cpp
LIB_OBJ := $(SRC:src/%.cpp=build/%.o)

.PHONY: all clean test

all: build/nfl_simulator

build/%.o: src/%.cpp
	@mkdir -p build
	$(CXX) $(CXXFLAGS) -c $< -o $@

build/nfl_simulator: $(LIB_OBJ) src/main.cpp
	$(CXX) $(CXXFLAGS) $(LIB_OBJ) src/main.cpp -o build/nfl_simulator

build/run_tests: $(LIB_OBJ) tests/test_main.cpp
	$(CXX) $(CXXFLAGS) $(LIB_OBJ) tests/test_main.cpp -o build/run_tests

test: build/run_tests
	./build/run_tests

clean:
	rm -rf build
