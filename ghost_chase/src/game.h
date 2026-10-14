// game.h - the turn-based chase: player AIs (incl. Minimax / Alpha-Beta), ghost turns,
//          terminal rendering and statistics.
#pragma once
#include "ai.h"
#include "pacai.h"

enum class PlayerType { Human, Flee, Random, AlphaBeta, Still };

inline const char *playerName(PlayerType p) {
    switch (p) {
        case PlayerType::Human: return "Human";
        case PlayerType::Flee: return "Greedy flee";
        case PlayerType::Random: return "Random";
        case PlayerType::AlphaBeta: return "Alpha-Beta";
        default: return "Stands still";
    }
}

struct Config {
    int maxTurns = 150;   // the player "wins" by surviving this many turns
    int rounds = 2;       // look-ahead for Minimax/Alpha-Beta, in full rounds
                          // (1 round = the player moves, then every ghost moves)
    bool prune = true;    // true = Alpha-Beta, false = plain Minimax
};

struct Stats {
    bool caught = false;
    bool quit = false;
    int turns = 0;
    long ghostNodes = 0, ghostMoves = 0, stuckMoves = 0;
    long playerNodes = 0, playerMoves = 0;
    double ms = 0;
};

// Shortest walking distance between every pair of open cells (BFS from each cell).
inline vector<int> allPairs(const Maze &m) {
    int N = m.R * m.C;
    vector<int> d((size_t)N * N, -1);
    for (int r = 0; r < m.R; r++)
        for (int c = 0; c < m.C; c++) {
            if (!m.open(r, c)) continue;
            vector<int> row = bfsDist(m, {Pos{r, c}});
            copy(row.begin(), row.end(), d.begin() + (size_t)m.id({r, c}) * N);
        }
    return d;
}

class Game {
public:
    Game(const Scenario &sc, vector<Algo> algos, PlayerType pt, Config cfg,
         const vector<int> *dist, unsigned seed)
        : sc_(sc), algos_(std::move(algos)), pt_(pt), cfg_(cfg), dist_(dist), rng_(seed) {
        player_ = sc.player;
        ghosts_ = sc.ghosts;
        while (algos_.size() < ghosts_.size()) algos_.push_back(algos_.back());
    }

    // Plays the game to the end. If show is true, draws every turn in the terminal.
    Stats run(bool show, int delayMs, bool color) {
        auto t0 = chrono::steady_clock::now();
        Stats st;
        for (turn_ = 1; turn_ <= cfg_.maxTurns; turn_++) {
            if (show) render(color, st, false);
            // ---- player's move ----
            if (pt_ == PlayerType::Human) {
                string line;
                cout << "Move (w/a/s/d + Enter, . = stay, q = quit): " << flush;
                if (!getline(cin, line) || (!line.empty() && line[0] == 'q')) { st.quit = true; break; }
                Pos n = player_;
                if (!line.empty()) {
                    if (line[0] == 'w') n.r--;
                    else if (line[0] == 's') n.r++;
                    else if (line[0] == 'a') n.c--;
                    else if (line[0] == 'd') n.c++;
                }
                if (sc_.maze.open(n)) player_ = n;
            } else if (pt_ != PlayerType::Still) {
                player_ = choosePlayerMove(st);
            }
            if (isCaught()) { st.caught = true; break; }
            // ---- ghosts' moves ----
            for (size_t i = 0; i < ghosts_.size(); i++) {
                SearchResult r = searchGrid(sc_.maze, ghosts_[i], player_, algos_[i]);
                st.ghostNodes += r.expanded;
                st.ghostMoves++;
                if (r.path.size() > 1) ghosts_[i] = r.path[1];
                else if (ghosts_[i] != player_) st.stuckMoves++;  // could not move
                if (ghosts_[i] == player_) { st.caught = true; }
            }
            if (st.caught) break;
            if (show && delayMs > 0) this_thread::sleep_for(chrono::milliseconds(delayMs));
        }
        st.turns = min(turn_, cfg_.maxTurns);
        st.ms = chrono::duration<double, milli>(chrono::steady_clock::now() - t0).count();
        if (show) {
            render(color, st, true);
            if (st.quit) cout << "You quit the game.\n";
            else if (st.caught) cout << "CAUGHT on turn " << st.turns << "!\n";
            else cout << "The player SURVIVED all " << cfg_.maxTurns << " turns.\n";
        }
        return st;
    }

private:
    const Scenario &sc_;
    vector<Algo> algos_;
    PlayerType pt_;
    Config cfg_;
    const vector<int> *dist_;
    mt19937 rng_;
    Pos player_;
    vector<Pos> ghosts_;
    int turn_ = 0;

    bool isCaught() const {
        for (auto &g : ghosts_) if (g == player_) return true;
        return false;
    }

    int D(Pos a, Pos b) const {
        int d = (*dist_)[(size_t)sc_.maze.id(a) * sc_.maze.R * sc_.maze.C + sc_.maze.id(b)];
        return d < 0 ? 99 : d;
    }
    int minGhostDist(Pos p, const vector<Pos> &gs) const {
        int m = 99;
        for (auto &g : gs) m = min(m, D(p, g));
        return m;
    }

    Pos choosePlayerMove(Stats &st) {
        vector<Pos> nb = sc_.maze.neighbors(player_);
        st.playerMoves++;
        if (nb.empty()) return player_;
        if (pt_ == PlayerType::Random) return nb[rng_() % nb.size()];
        if (pt_ == PlayerType::Flee) {
            // one-step lookahead: maximise distance to the nearest ghost, prefer open cells
            int bestScore = INT_MIN;
            vector<Pos> best;
            for (Pos n : nb) {
                bool onGhost = false;
                for (auto &g : ghosts_) if (g == n) onGhost = true;
                int score = onGhost ? -1000000
                                    : 100 * minGhostDist(n, ghosts_) + (int)sc_.maze.neighbors(n).size();
                if (score > bestScore) { bestScore = score; best = {n}; }
                else if (score == bestScore) best.push_back(n);
            }
            return best[rng_() % best.size()];
        }
        // ---- Minimax / Alpha-Beta (see pacai.h): the player is MAX, each ghost is a MIN player ----
        PacAIParams params;
        params.rounds = cfg_.rounds;
        params.prune = cfg_.prune;
        PacSearch ps(sc_.maze, *dist_, ghosts_, params, nullptr);
        PacAIResult r = ps.decide(player_);
        st.playerNodes += r.nodes;
        return r.move;
    }

    void render(bool color, const Stats &st, bool final) const {
        if (color) cout << "\033[H\033[2J";
        else cout << "\n----------------------------------------\n";
        vector<string> g = sc_.maze.g;
        auto paint = [&](Pos p, char ch) { g[p.r][p.c] = ch; };
        for (size_t i = 0; i < ghosts_.size(); i++) paint(ghosts_[i], algoChar(algos_[i]));
        paint(player_, (final && st.caught) ? 'X' : 'P');
        for (auto &row : g) {
            for (char ch : row) {
                if (!color) { cout << ch; continue; }
                switch (ch) {
                    case '#': cout << "\033[34m#\033[0m"; break;
                    case 'P': cout << "\033[1;33mP\033[0m"; break;
                    case 'X': cout << "\033[1;31mX\033[0m"; break;
                    case 'A': cout << "\033[1;31mA\033[0m"; break;
                    case 'B': cout << "\033[1;35mB\033[0m"; break;
                    case 'H': cout << "\033[1;36mH\033[0m"; break;
                    default: cout << ch;
                }
            }
            cout << "\n";
        }
        cout << "\nTurn " << min(turn_, cfg_.maxTurns) << "/" << cfg_.maxTurns
             << "   Player: " << playerName(pt_)
             << "   Ghost nodes expanded: " << st.ghostNodes << "\n";
        cout << "Ghosts: ";
        for (size_t i = 0; i < ghosts_.size(); i++)
            cout << algoChar(algos_[i]) << "=" << algoName(algos_[i]) << (i + 1 < ghosts_.size() ? ", " : "");
        cout << "\n" << flush;
    }
};
