// maze.h - grid, positions and scenario builders
#pragma once
#include <bits/stdc++.h>
using namespace std;

struct Pos {
    int r = 0, c = 0;
    bool operator==(const Pos &o) const { return r == o.r && c == o.c; }
    bool operator!=(const Pos &o) const { return !(*this == o); }
};

struct Maze {
    int R = 0, C = 0;
    vector<string> g;  // '#' = wall, anything else = open floor

    bool open(int r, int c) const { return r >= 0 && r < R && c >= 0 && c < C && g[r][c] != '#'; }
    bool open(Pos p) const { return open(p.r, p.c); }
    int id(Pos p) const { return p.r * C + p.c; }
    Pos at(int id) const { return {id / C, id % C}; }

    // 4-directional neighbours (up, down, left, right) that are not walls
    vector<Pos> neighbors(Pos p) const {
        static const int dr[] = {-1, 1, 0, 0}, dc[] = {0, 0, -1, 1};
        vector<Pos> out;
        for (int k = 0; k < 4; k++) {
            Pos n{p.r + dr[k], p.c + dc[k]};
            if (open(n)) out.push_back(n);
        }
        return out;
    }
};

// A starting setup: the maze, the player's start cell and one start cell per ghost
struct Scenario {
    Maze maze;
    Pos player;
    vector<Pos> ghosts;
};

// Multi-source BFS: shortest walking distance from any source cell (-1 = unreachable)
inline vector<int> bfsDist(const Maze &m, const vector<Pos> &src) {
    vector<int> d(m.R * m.C, -1);
    queue<Pos> q;
    for (auto &s : src) { d[m.id(s)] = 0; q.push(s); }
    while (!q.empty()) {
        Pos p = q.front();
        q.pop();
        for (Pos n : m.neighbors(p))
            if (d[m.id(n)] < 0) { d[m.id(n)] = d[m.id(p)] + 1; q.push(n); }
    }
    return d;
}

// ---------------------------------------------------------------------------
// "Trap" maze: a long wall separates the player from the ghosts. The only way
// round is along the bottom row. A greedy "step closer" ghost (hill climbing)
// walks into the wall and gets stuck; A* and Best-First find the way round.
// ---------------------------------------------------------------------------
inline Scenario makeTrapScenario(int nGhosts) {
    Scenario s;
    s.maze.g = {
        "#####################",
        "#         #         #",
        "#    P    #    G    #",
        "#         #         #",
        "#         #         #",
        "#                   #",
        "#####################",
    };
    s.maze.R = (int)s.maze.g.size();
    s.maze.C = (int)s.maze.g[0].size();
    Pos gp{0, 0};
    for (int r = 0; r < s.maze.R; r++)
        for (int c = 0; c < s.maze.C; c++) {
            if (s.maze.g[r][c] == 'P') { s.player = {r, c}; s.maze.g[r][c] = ' '; }
            if (s.maze.g[r][c] == 'G') { gp = {r, c}; s.maze.g[r][c] = ' '; }
        }
    int offs[] = {0, 1, -1, 2, -2};
    for (int i = 0; i < nGhosts && i < 5; i++) s.ghosts.push_back({gp.r + offs[i], gp.c});
    return s;
}

// ---------------------------------------------------------------------------
// Random maze: random walls, then keep only the largest connected region so
// every open cell can reach every other. Ghosts start far from the player.
// ---------------------------------------------------------------------------
inline Scenario makeRandomScenario(unsigned seed, int rows, int cols, double density, int nGhosts) {
    mt19937 rng(seed);
    Scenario s;
    Maze &m = s.maze;
    m.R = rows;
    m.C = cols;
    m.g.assign(rows, string(cols, '#'));
    uniform_real_distribution<double> U(0.0, 1.0);
    for (int r = 1; r < rows - 1; r++)
        for (int c = 1; c < cols - 1; c++) m.g[r][c] = (U(rng) < density) ? '#' : ' ';

    // find the largest connected component of open cells
    vector<int> comp(rows * cols, -1);
    int bestComp = -1, bestSize = 0, nComp = 0;
    for (int r = 0; r < rows; r++)
        for (int c = 0; c < cols; c++) {
            if (!m.open(r, c) || comp[r * cols + c] >= 0) continue;
            int size = 0;
            queue<Pos> q;
            q.push({r, c});
            comp[r * cols + c] = nComp;
            while (!q.empty()) {
                Pos p = q.front();
                q.pop();
                size++;
                for (Pos n : m.neighbors(p))
                    if (comp[m.id(n)] < 0) { comp[m.id(n)] = nComp; q.push(n); }
            }
            if (size > bestSize) { bestSize = size; bestComp = nComp; }
            nComp++;
        }
    vector<Pos> cells;
    for (int r = 0; r < rows; r++)
        for (int c = 0; c < cols; c++) {
            if (m.open(r, c) && comp[r * cols + c] != bestComp) m.g[r][c] = '#';
            else if (m.open(r, c)) cells.push_back({r, c});
        }

    s.player = cells[rng() % cells.size()];
    vector<int> d = bfsDist(m, {s.player});
    int maxD = 0;
    for (Pos p : cells) maxD = max(maxD, d[m.id(p)]);
    int threshold = min(12, maxD);
    vector<Pos> cand;
    for (Pos p : cells)
        if (d[m.id(p)] >= threshold && p != s.player) cand.push_back(p);
    if ((int)cand.size() < nGhosts) {  // tiny maze fallback: take the farthest cells
        cand = cells;
        sort(cand.begin(), cand.end(), [&](Pos a, Pos b) { return d[m.id(a)] > d[m.id(b)]; });
    }
    shuffle(cand.begin(), cand.end(), rng);
    for (int i = 0; i < nGhosts && i < (int)cand.size(); i++) s.ghosts.push_back(cand[i]);
    return s;
}
