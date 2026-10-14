// pacai.h - Pac-Man's decision making: Minimax and Alpha-Beta pruning.
// The player is the MAX agent and every ghost is its own MIN agent.
// Used by both the terminal simulation (game.h) and the web server (server.cpp).
#pragma once
#include "maze.h"

struct PacAIParams {
    int rounds = 2;        // look-ahead depth in full rounds (player move + every ghost move)
    bool prune = true;     // true = Alpha-Beta, false = plain Minimax
    int capMin = 99;       // cap on the "nearest ghost" distance in the evaluation (99 = no cap)
    int capAvg = 99;       // cap on the average ghost distance
    int dotWeight = 0;     // reward for being close to a dot (0 = ignore dots)
};

struct PacAIResult {
    Pos move;              // chosen cell
    long nodes = 0;        // game-tree nodes visited while deciding
};

class PacSearch {
public:
    PacSearch(const Maze &m, const vector<int> &dist, vector<Pos> ghosts, const PacAIParams &p,
              const vector<int> *dotDist)
        : m_(m), dist_(dist), gs_(std::move(ghosts)), p_(p), dotDist_(dotDist) {}

    PacAIResult decide(Pos player) {
        PacAIResult out;
        out.move = player;
        vector<Pos> nb = m_.neighbors(player);
        if (nb.empty()) return out;
        int plies = p_.rounds * (1 + (int)gs_.size());
        int alpha = -100000, beta = 100000, bestV = INT_MIN;
        out.move = nb[0];
        for (Pos n : nb) {
            int v = search(n, 1, plies - 1, alpha, beta);
            if (v > bestV) { bestV = v; out.move = n; }
            if (p_.prune) alpha = max(alpha, bestV);
        }
        out.nodes = nodes_;
        return out;
    }

private:
    const Maze &m_;
    const vector<int> &dist_;
    vector<Pos> gs_;
    PacAIParams p_;
    const vector<int> *dotDist_;
    long nodes_ = 0;

    int D(Pos a, Pos b) const {
        int d = dist_[(size_t)m_.id(a) * m_.R * m_.C + m_.id(b)];
        return d < 0 ? 99 : d;
    }

    int evaluate(Pos p) const {
        int sum = 0, mn = 99;
        for (auto &g : gs_) { int d = D(p, g); mn = min(mn, d); sum += d; }
        int avg = sum / (int)gs_.size();
        int score = 10 * min(mn, p_.capMin) + min(avg, p_.capAvg);
        if (dotDist_ && p_.dotWeight) {
            int dd = (*dotDist_)[m_.id(p)];
            score -= p_.dotWeight * (dd < 0 ? 0 : dd);
        }
        return score;
    }

    // phase 0 = player to move, phase i = ghost (i-1) to move
    int search(Pos p, int phase, int depth, int alpha, int beta) {
        nodes_++;
        for (auto &g : gs_) if (g == p) return -1000 - depth;  // caught: sooner = worse
        if (depth == 0) return evaluate(p);
        int ng = (int)gs_.size();
        if (phase == 0) {
            int best = INT_MIN;
            for (Pos n : m_.neighbors(p)) {
                best = max(best, search(n, 1, depth - 1, alpha, beta));
                alpha = max(alpha, best);
                if (p_.prune && beta <= alpha) break;  // prune
            }
            return best == INT_MIN ? evaluate(p) : best;
        }
        int i = phase - 1, next = (phase == ng) ? 0 : phase + 1;
        Pos old = gs_[i];
        int best = INT_MAX;
        for (Pos n : m_.neighbors(old)) {
            gs_[i] = n;
            best = min(best, search(p, next, depth - 1, alpha, beta));
            beta = min(beta, best);
            if (p_.prune && beta <= alpha) break;  // prune
        }
        gs_[i] = old;
        return best == INT_MAX ? evaluate(p) : best;
    }
};
