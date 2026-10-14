CXX = g++
CXXFLAGS = -O2 -std=c++17 -Wall -Wextra

all: ghost_chase ghost_server

# terminal version: demo + simulation tables
ghost_chase: src/main.cpp src/maze.h src/ai.h src/pacai.h src/game.h
	$(CXX) $(CXXFLAGS) src/main.cpp -o ghost_chase

# web version: C++ server + browser page
ghost_server: src/server.cpp src/webgame.h src/maze.h src/ai.h src/pacai.h src/game.h web/index.html
	$(CXX) $(CXXFLAGS) -pthread src/server.cpp -o ghost_server

clean:
	rm -f ghost_chase ghost_server ghost_chase.exe ghost_server.exe
