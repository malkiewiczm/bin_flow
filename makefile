MAKEFLAGS += Rr

CXX := g++
CXXFLAGS := -std=c++11 -Wall -Wextra -Wpedantic -Wshadow -O3

main: main.cpp
	$(CXX) $(CXXFLAGS) $< -o $@

