// ai.h - the three ghost search algorithms (A*, Best-First, Hill Climbing)
#pragma once
#include "maze.h"

enum class Algo { AStar, BestFirst, Hill };

inline const char *algoName(Algo a) {
    switch (a) {
        case Algo::AStar: return "A*";
        case Algo::BestFirst: return "Best-First";
        default: return "Hill Climbing";
    }
}
inline char algoChar(Algo a) { return a == Algo::AStar ? 'A' : (a == Algo::BestFirst ? 'B' : 'H'); }

// One expanded cell, recorded in the order the search expanded it (used by the web viewer)
struct Expansion {
    Pos p;
    int g = 0, h = 0, f = 0;
};
struct HillInfo {                       // what hill climbing compared on its single step
    int cur = 0;                        // h of the current cell
    vector<pair<Pos, int>> cands;       // each neighbour with its h
    bool hasBest = false;
    Pos best;
};

struct SearchResult {
    bool found = false;     // A*/Best-First: reached the goal. Hill: found a better neighbour.
    vector<Pos> path;       // path[0] = start, path[1] = the next step to take
    long expanded = 0;      // nodes expanded (the "work" done by the search)
    // Filled only when searchGrid is called with trace = true:
    vector<Expansion> order;  // cells in the order they were expanded
    vector<Pos> open;         // cells discovered but not yet expanded when the search stopped
    HillInfo hill;
};

// Heuristic: Manhattan distance (admissible on a 4-direction grid)
inline int manhattan(Pos a, Pos b) { return abs(a.r - b.r) + abs(a.c - b.c); }

inline SearchResult searchGrid(const Maze &m, Pos start, Pos goal, Algo algo, bool trace = false) {
    SearchResult res;
    if (start == goal) { res.found = true; res.path = {start}; return res; }

    // ---- Hill Climbing: take ONE greedy step to the neighbour closest to the goal.
    //      If no neighbour is strictly closer, we are at a local optimum (stuck). ----
    if (algo == Algo::Hill) {
        res.expanded = 1;
        int cur = manhattan(start, goal);
        int startH = cur;
        Pos best = start;
        for (Pos n : m.neighbors(start)) {
            int h = manhattan(n, goal);
            if (trace) res.hill.cands.push_back({n, h});
            if (h < cur) { cur = h; best = n; }
        }
        res.path = {start};
        if (best != start) { res.path.push_back(best); res.found = true; }
        if (trace) {
            res.order.push_back({start, 0, startH, startH});
            res.hill.cur = startH;
            res.hill.hasBest = res.found;
            res.hill.best = best;
        }
        return res;
    }

    int N = m.R * m.C, s = m.id(start), t = m.id(goal);
    vector<int> g(N, INT_MAX), parent(N, -1);
    vector<char> done(N, 0);  // A*: closed set.  Best-First: "seen" set.
    vector<char> expandedFlag(trace ? N : 0, 0);  // only used to report the open list
    typedef tuple<int, int, int> T;  // (priority, tie-break, cell id)
    priority_queue<T, vector<T>, greater<T>> pq;
    bool reached = false;

    if (algo == Algo::AStar) {
        // ---- A*: priority f(n) = g(n) + h(n) ----
        g[s] = 0;
        pq.push({manhattan(start, goal), manhattan(start, goal), s});
        while (!pq.empty()) {
            auto [f, hh, u] = pq.top();
            pq.pop();
            if (done[u]) continue;  // stale queue entry
            done[u] = 1;
            res.expanded++;
            if (trace) { res.order.push_back({m.at(u), g[u], hh, f}); expandedFlag[u] = 1; }
            if (u == t) { reached = true; break; }
            for (Pos n : m.neighbors(m.at(u))) {
                int v = m.id(n), ng = g[u] + 1;
                if (ng < g[v]) {
                    g[v] = ng;
                    parent[v] = u;
                    int h = manhattan(n, goal);
                    pq.push({ng + h, h, v});
                }
            }
        }
    } else {
        // ---- Greedy Best-First: priority is h(n) only (ignores cost so far) ----
        done[s] = 1;
        pq.push({manhattan(start, goal), 0, s});
        while (!pq.empty()) {
            auto [h0, z, u] = pq.top();
            pq.pop();
            (void)z;
            res.expanded++;
            if (trace) { res.order.push_back({m.at(u), g[u] == INT_MAX ? 0 : g[u], h0, h0}); expandedFlag[u] = 1; }
            if (u == t) { reached = true; break; }
            for (Pos n : m.neighbors(m.at(u))) {
                int v = m.id(n);
                if (done[v]) continue;
                done[v] = 1;
                parent[v] = u;
                pq.push({manhattan(n, goal), 0, v});
            }
        }
    }

    if (trace) {  // cells that were discovered but never expanded
        for (int v = 0; v < N; v++) {
            bool discovered = (algo == Algo::AStar) ? (g[v] != INT_MAX) : (done[v] != 0);
            if (discovered && !expandedFlag[v]) res.open.push_back(m.at(v));
        }
    }

    if (reached) {
        res.found = true;
        for (int v = t; v != -1; v = parent[v]) res.path.push_back(m.at(v));
        reverse(res.path.begin(), res.path.end());
    } else {
        res.path = {start};
    }
    return res;
}
