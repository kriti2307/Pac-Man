# Ghost Chase - AI Search in a Pac-Man Style Maze (C++)

CSE3121 Artificial Intelligence project. Ghosts chase Pac-Man through a maze using classic
search algorithms from the syllabus, and Pac-Man can use game-tree search to run away.

| Algorithm | Where it is used | Syllabus topic |
|---|---|---|
| A* | Ghost: finds the shortest path (f = g + h) | Heuristic search |
| Best-First (greedy) | Ghost: follows h only, ignores cost so far | Heuristic search |
| Hill Climbing | Ghost: steps to the neighbour closest to Pac-Man, gets stuck behind walls | Heuristic search |
| Minimax | Pac-Man AI: looks ahead, ghosts are the MIN players | Game playing |
| Alpha-Beta pruning | Same as Minimax but skips useless branches | Game playing |

Heuristic for all ghosts: Manhattan distance to Pac-Man.

There are two ways to run it. Both use the same C++ algorithms.

## 1. Web version (play it, watch the search step by step)

A C++ server runs every search and every Pac-Man decision. The browser page only draws
what the server reports (look at `web/index.html`: it has no search code, only `fetch` calls).

Build:

    Windows (MinGW / MSYS2):  double-click build_windows.bat   (or run it in the terminal)
    Mac / Linux:              make ghost_server

Run from the project folder (the one that contains `web/`):

    Windows:      .\ghost_server.exe
    Mac / Linux:  ./ghost_server

It prints an address and opens your browser. If it does not, open http://localhost:8080
yourself. Keep the terminal window open while you play; press Ctrl+C to stop.

In the page:
- Arrow keys / WASD move Pac-Man. Play runs in real time, Step (or N) advances one turn.
- Each ghost can use A*, Best-First or Hill Climbing (pick it in the Ghosts panel).
- "Watch this ghost think" shades the cells that ghost's search explored, outlines the cells
  waiting to be explored and draws the route. "Replay search" then Back / Next steps through
  the C++ search one expansion at a time. "Show numbers" prints f = g + h on each cell.
- Pac-Man can be set to AI: it uses Alpha-Beta and the panel shows how many nodes it visited
  compared with plain Minimax.
- "Trap maze" has a wall in the middle: the Hill Climbing ghost walks into it and gets stuck.

The page header chip says "C++ engine connected" when it can reach the server.
If the page was opened as a file (double-clicked) it cannot work: always use the address above.

## 2. Terminal version (demo and result tables for the report)

Build:

    Windows:      g++ -O2 -std=c++17 -static src/main.cpp -o ghost_chase.exe
    Mac / Linux:  make ghost_chase

Run:

    ./ghost_chase help
    ./ghost_chase simulate --games 100        # result tables for the report
    ./ghost_chase demo                         # watch an AI player flee from mixed ghosts
    ./ghost_chase demo --player human --team astar    # you play: w/a/s/d + Enter
    ./ghost_chase demo --scenario trap --team hill --ghosts 1 --player still

On Windows use `.\ghost_chase.exe` instead of `./ghost_chase`.

Demo legend: `P` player, `A` A* ghost, `B` Best-First ghost, `H` Hill Climbing ghost,
`X` caught, `#` wall. If the screen looks garbled add `--no-color`.

`simulate` measures three things:
1. Trap maze: a wall separates ghost and player, the player stands still. Hill climbing walks
   into the wall and never gets round.
2. Random mazes: every ghost team against three player types (catch rate, average turns to
   catch, nodes expanded per move, how often ghosts were stuck).
3. Minimax vs Alpha-Beta: the same decisions with far fewer nodes visited.

## How the game works

Turn based on a grid. Each turn Pac-Man moves one cell, then every ghost runs its search from
its cell to Pac-Man's cell and takes the first step of the result. Pac-Man is caught when a ghost
reaches him. In the web version he wins by eating every dot. In the terminal version he wins
by surviving `--turns` turns.

## Files

    src/maze.h      grid, BFS distances, maze builders for the terminal version
    src/ai.h        A*, Best-First and Hill Climbing (with optional step-by-step trace)
    src/pacai.h     Minimax and Alpha-Beta for Pac-Man
    src/game.h      terminal game loop, player AIs, rendering
    src/main.cpp    command line for the terminal version and the simulation experiments
    src/webgame.h   the web game: mazes, turns, JSON replies (all AI runs here)
    src/server.cpp  small HTTP server that serves web/index.html and the /api calls
    web/index.html  the page: drawing, controls and the search replay (no AI code)
    Makefile, build_windows.bat

## Notes for the report

- On the trap maze A* and Best-First both find the 16-move route; hill climbing never does.
- Hill climbing does 1 node of work per move, so it is the cheapest but the least reliable.
- A* is optimal; Best-First is not guaranteed to give the shortest path on mazes with more detours.
- Alpha-Beta gives exactly the same moves as Minimax with far fewer nodes (shown live in the web
  version and in `simulate`, experiment 3).
- Results depend on the random mazes. Change `--games` to average over more.
