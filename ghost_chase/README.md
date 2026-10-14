# Ghost Chase
 
**Classic AI search algorithms in a Pac-Man style maze, written in C++.**
 
Three ghosts chase Pac-Man using A\*, Best-First Search and Hill Climbing, and Pac-Man can run away using Minimax with Alpha-Beta pruning. You can play it in the browser, step through each ghost's search one expansion at a time, or run the terminal version to produce result tables.
 
Course project for **CSE3121: Artificial Intelligence** (Heuristic Search and Game Playing units).
 
---
 
## Contents
 
- [Algorithms](#algorithms)
- [Two ways to run it](#two-ways-to-run-it)
- [Web version](#web-version)
- [Terminal version](#terminal-version)
- [How it works](#how-it-works)
- [Results](#results)
- [Project structure](#project-structure)
- [Troubleshooting](#troubleshooting)
---
 
## Algorithms
 
| Algorithm | Used by | Idea |
|---|---|---|
| **A\*** | Ghost | Expands the cell with the lowest `f = g + h`. Always finds the shortest route. |
| **Best-First (greedy)** | Ghost | Expands the cell with the lowest `h` only. Fast, but can take a longer route. |
| **Hill Climbing** | Ghost | Steps to the neighbouring cell closest to Pac-Man. If none is closer, it is stuck. |
| **Minimax** | Pac-Man AI | Looks ahead; Pac-Man is the MAX player and each ghost is a MIN player. |
| **Alpha-Beta pruning** | Pac-Man AI | Minimax that skips branches that cannot change the result. Same moves, far fewer nodes. |
 
All ghosts use the **Manhattan distance** to Pac-Man as the heuristic `h`.
 
---
 
## Two ways to run it
 
| Version | What it is for | Needs |
|---|---|---|
| **Web version** | Playing the game, live demos, watching each search step by step | A C++ compiler and a browser |
| **Terminal version** | Text demo and the simulation tables for your report | A C++ compiler |
 
Both versions use the same C++ algorithm code (`src/ai.h`, `src/pacai.h`).
 
---
 
## Web version
 
A small C++ server (`ghost_server`) runs every ghost search and every Pac-Man decision. The browser page (`web/index.html`) only draws what the server reports. The page contains no search or AI code, only `fetch` calls to the server.
 
### Build
 
**Windows (MinGW / MSYS2 g++)**
 
```
g++ -O2 -std=c++17 -static src/server.cpp -o ghost_server.exe -lws2_32
```
 
or run `build_windows.bat`, which builds both versions.
 
**Mac / Linux**
 
```
make ghost_server
```
 
### Run
 
Run it from the project folder, the one that contains `src` and `web`.
 
```
.\ghost_server.exe      # Windows
./ghost_server          # Mac / Linux
```
 
Open **http://127.0.0.1:8080** in your browser (it may open by itself). Keep the terminal open while you play and press `Ctrl+C` to stop.
 
Options: `--port 8090` to use another port, `--no-open` to stop it opening the browser.
 
> Do not double-click `index.html`. The page needs the server, and the header chip will say "C++ server not reachable" if it cannot find it.
 
### Using the page
 
| Control | What it does |
|---|---|
| Arrow keys / WASD | Move Pac-Man |
| **Play** / `Space` | Real-time play, pause |
| **Step** / `N` | Advance exactly one turn |
| Speed slider | How fast the game ticks |
| Ghost algorithm menus | Switch any ghost between A\*, Best-First and Hill Climbing |
| **Watch this ghost think** | Shades the cells that ghost's search explored, outlines the cells still waiting, draws the chosen route |
| **Replay search**, Back, Next | Step through the last search one expansion at a time |
| **Show numbers** | Prints `f = g + h` (A\*) or `h` on each cell |
| Pac-Man: AI | Pac-Man uses Alpha-Beta; the panel compares its node count with plain Minimax |
| **Trap maze** | A wall splits the board. The Hill Climbing ghost walks into it and gets stuck (red `!`) |
 
The header chip shows **C++ engine connected** and the reply time of the last move.
 
Pac-Man wins by eating every dot. He loses when a ghost reaches him.
 
---
 
## Terminal version
 
### Build
 
**Windows**
 
```
g++ -O2 -std=c++17 -static src/main.cpp -o ghost_chase.exe
```
 
**Mac / Linux**
 
```
make ghost_chase
```
 
### Run
 
```
./ghost_chase help
./ghost_chase simulate --games 100
./ghost_chase demo
./ghost_chase demo --player human --team astar
./ghost_chase demo --scenario trap --team hill --ghosts 1 --player still
```
 
On Windows use `.\ghost_chase.exe` instead of `./ghost_chase`.
 
Demo legend: `P` Pac-Man, `A` A\* ghost, `B` Best-First ghost, `H` Hill Climbing ghost, `X` caught, `#` wall. Add `--no-color` if the screen looks garbled.
 
Demo options: `--scenario trap|random`, `--team astar|bestfirst|hill|mixed`, `--player human|flee|random|alphabeta|still`, `--ghosts N`, `--seed N`, `--delay MS`, `--turns N`.
 
`simulate` runs many games with no display and prints three experiments:
 
1. **Trap maze:** a wall separates the ghost from Pac-Man, who stands still.
2. **Random mazes:** every ghost team against three kinds of Pac-Man.
3. **Minimax vs Alpha-Beta:** how many nodes Pac-Man's look-ahead visits.
---
 
## How it works
 
The game is turn based on a grid. Each turn:
 
1. Pac-Man moves one cell (you, or the Alpha-Beta AI).
2. Each ghost runs its search from its cell to Pac-Man's cell and takes the first step of the result.
3. If a ghost lands on Pac-Man, he is caught.
**Pac-Man's AI** searches two full rounds ahead (his move, then every ghost's move, twice). The evaluation rewards being far from the nearest ghost and, in the web version, being close to a dot. Alpha-Beta and plain Minimax choose identical moves; the difference is the number of nodes visited.
 
**Search traces.** `searchGrid(..., trace = true)` in `src/ai.h` records the cells in the order they were expanded, with their `g`, `h` and `f` values, plus the cells left waiting. The server sends that trace to the page, which is what the replay draws.
 
---
 
## Results
 
Measured with `./ghost_chase simulate --games 100` (random 20x20 mazes, 3 ghosts, 150 turns maximum). Exact numbers depend on the mazes, so yours may differ slightly.
 
**Trap maze (player stands still, shortest route is 16 moves)**
 
| Ghost algorithm | Caught? | Turns to catch | Nodes expanded |
|---|---|---|---|
| A\* | yes | 16 | 228 |
| Best-First | yes | 16 | 217 |
| Hill Climbing | **no (stuck)** | - | 100 |
 
**Ghost teams against a fleeing Pac-Man (greedy flee)**
 
| Ghost team | Caught | Avg turns | Nodes per move | Stuck |
|---|---|---|---|---|
| A\* | 71% | 22.6 | 8.9 | 0% |
| Best-First | 61% | 25.8 | 11.3 | 0% |
| Hill Climbing | 5% | 17.4 | 1.0 | 60% |
 
**Pac-Man's look-ahead: Minimax vs Alpha-Beta (20 games)**
 
| Search | Total nodes | Result |
|---|---|---|
| Minimax | 10,522,238 | same moves and outcomes |
| Alpha-Beta | 3,696,900 | **64.9% fewer nodes** |
 
**What the results show**
 
- A\* is the most reliable chaser and always finds the shortest route.
- Best-First does a similar amount of work, but it is not guaranteed to find the shortest route when a maze has more detours.
- Hill Climbing does the least work (one node per move) but gets trapped by any wall between it and Pac-Man. This is the local-optimum problem.
- Alpha-Beta gives exactly the same decisions as Minimax with far fewer nodes.
---
 
## Project structure
 
```
ghost_chase/
|-- src/
|   |-- maze.h        grid, BFS distances, maze builders (terminal version)
|   |-- ai.h          A*, Best-First, Hill Climbing (with optional search trace)
|   |-- pacai.h       Minimax and Alpha-Beta for Pac-Man
|   |-- game.h        terminal game loop, player AIs, rendering
|   |-- main.cpp      terminal command line and simulation experiments
|   |-- webgame.h     web game: mazes, turns, JSON replies (all AI runs here)
|   `-- server.cpp    small HTTP server for the page and the /api calls
|-- web/
|   `-- index.html    the page: drawing, controls, search replay (no AI code)
|-- Makefile
|-- build_windows.bat
`-- README.md
```
 
The server exposes three endpoints: `GET /api/new` (start a game), `GET /api/step` (play one turn) and `GET /api/algos` (change a ghost's algorithm).
 
---
 
## Troubleshooting
 
| Problem | Fix |
|---|---|
| `'.\ghost_server.exe' is not recognized` | The program is not built in this folder. Run the build command first, and check that `dir` shows `src` and `web`. |
| `cc1plus.exe: fatal error: src/main.cpp: No such file` | You are in the wrong folder, often one level too high. Run `cd ghost_chase`, then `dir`. |
| The program prints nothing and exits | Windows could not find the MinGW runtime. Rebuild with `-static`. |
| `Cannot find web/index.html` | Run `ghost_server` from the folder that contains `web`. |
| `Could not listen on port 8080` | Another copy is running. Close it, or use `--port 8090`. |
| The game feels laggy | Use `http://127.0.0.1:8080` instead of `localhost`, and press `Ctrl+F5`. |
| "Play again" box is stuck on screen | Use the latest `web/index.html` and press `Ctrl+F5`. |
| Screen looks garbled in the terminal demo | Add `--no-color`. |
 
---
 
## Author
 
Your name, roll number and section.
