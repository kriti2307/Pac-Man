// webgame.h - the interactive game that the web page talks to.
// Everything that "thinks" lives here in C++: maze generation, the ghosts' A* / Best-First /
// Hill Climbing searches (with step-by-step traces) and Pac-Man's Minimax / Alpha-Beta.
// The browser only draws what this file reports.
#pragma once
#include "game.h"
#include <sstream>

inline const char *algoKey(Algo a) {
    return a == Algo::AStar ? "astar" : (a == Algo::BestFirst ? "best" : "hill");
}
inline Algo algoFromKey(const string &s) {
    if (s == "best") return Algo::BestFirst;
    if (s == "hill") return Algo::Hill;
    return Algo::AStar;
}

class WebGame {
public:
    static constexpr int ROWS = 15, COLS = 21;

    WebGame() { newGame("random", 7, {Algo::AStar, Algo::BestFirst, Algo::Hill}); }

    // ------------------------------------------------------------------ new game
    void newGame(const string &kind, unsigned seed, vector<Algo> algos) {
        kind_ = kind;
        seed_ = seed;
        algos_ = std::move(algos);
        while (algos_.size() < 3) algos_.push_back(Algo::AStar);
        algos_.resize(3);
        maze_ = (kind == "trap") ? trapMaze() : randomMaze(seed);
        dist_ = allPairs(maze_);

        vector<Pos> cells;
        for (int r = 0; r < ROWS; r++)
            for (int c = 0; c < COLS; c++)
                if (maze_.open(r, c)) cells.push_back({r, c});

        if (kind == "trap") {
            pac_ = {7, 5};
            ghosts_ = {{3, 15}, {7, 16}, {11, 15}};
        } else {
            pac_ = cells[0];
            long bestD = LONG_MAX;
            for (Pos p : cells) {  // start near the bottom-left corner
                long d = (long)(p.r - (ROWS - 2)) * (p.r - (ROWS - 2)) + (long)(p.c - 1) * (p.c - 1);
                if (d < bestD) { bestD = d; pac_ = p; }
            }
            vector<Pos> distant;
            for (Pos p : cells) if (dAt(pac_, p) >= 12) distant.push_back(p);
            if (distant.size() < 3) {
                distant = cells;
                sort(distant.begin(), distant.end(), [&](Pos a, Pos b) { return dAt(pac_, a) > dAt(pac_, b); });
                distant.resize(12);
            }
            mt19937 rng(seed + 99);
            ghosts_.clear();
            for (int tries = 0; tries < 200 && ghosts_.size() < 3; tries++) {
                Pos x = distant[rng() % distant.size()];
                bool ok = true;
                for (Pos y : ghosts_) if (dAt(x, y) < 6) ok = false;
                if (ok) ghosts_.push_back(x);
            }
            while (ghosts_.size() < 3) ghosts_.push_back(distant[ghosts_.size() % distant.size()]);
        }
        pacPrev_ = pac_;
        pacDir_ = {0, 1};
        hasWant_ = false;
        ghostPrev_ = ghosts_;
        ghostDir_.assign(3, Pos{0, -1});
        dots_.assign((size_t)ROWS * COLS, 0);
        dotsLeft_ = 0;
        for (Pos p : cells) {
            bool spawn = (p == pac_);
            for (Pos g : ghosts_) if (g == p) spawn = true;
            if (!spawn) { dots_[maze_.id(p)] = 1; dotsLeft_++; }
        }
        turn_ = 0; score_ = 0; over_ = false; won_ = false;
    }

    // Change which algorithm each ghost uses without restarting the game
    void setAlgos(vector<Algo> algos) {
        while (algos.size() < 3) algos.push_back(Algo::AStar);
        algos.resize(3);
        algos_ = std::move(algos);
    }

    string newJson() const {
        ostringstream o;
        o << "{\"rows\":" << ROWS << ",\"cols\":" << COLS << ",\"kind\":\"" << kind_ << "\",\"seed\":" << seed_
          << ",\"maze\":[";
        for (int r = 0; r < ROWS; r++) o << (r ? "," : "") << "\"" << maze_.g[r] << "\"";
        o << "],\"dots\":[";
        for (int r = 0; r < ROWS; r++) {
            o << (r ? "," : "") << "\"";
            for (int c = 0; c < COLS; c++) o << (dots_[maze_.id({r, c})] ? '1' : '0');
            o << "\"";
        }
        o << "],\"dotsLeft\":" << dotsLeft_ << ",\"pac\":" << pj(pac_) << ",\"ghosts\":[";
        for (int i = 0; i < 3; i++) o << (i ? "," : "") << pj(ghosts_[i]);
        o << "],\"algos\":[";
        for (int i = 0; i < 3; i++) o << (i ? ",\"" : "\"") << algoKey(algos_[i]) << "\"";
        o << "]}";
        return o.str();
    }

    // ------------------------------------------------------------------ one turn
    // The player moves, then each ghost runs its search and takes one step.
    string step(const string &mode, bool prune, bool slow, bool hasWant, Pos want) {
        bool ate = false, hasAI = false;
        Pos atePos{0, 0};
        long abNodes = 0, mmNodes = 0;
        vector<string> searchJson(3, "null");
        vector<char> stuck(3, 0), rested(3, 0);

        if (!over_) {
            turn_++;
            pacPrev_ = pac_;
            ghostPrev_ = ghosts_;

            // ---- Pac-Man's move ----
            if (mode == "ai") {
                vector<int> dd = bfsDist(maze_, dotCells());
                PacAIParams p;
                p.rounds = 2; p.capMin = 6; p.capAvg = 12; p.dotWeight = 2;
                p.prune = true;
                PacAIResult ra = PacSearch(maze_, dist_, ghosts_, p, &dd).decide(pac_);
                p.prune = false;
                PacAIResult rb = PacSearch(maze_, dist_, ghosts_, p, &dd).decide(pac_);
                PacAIResult pick = prune ? ra : rb;
                abNodes = ra.nodes; mmNodes = rb.nodes; hasAI = true;
                if (pick.move != pac_) { pacDir_ = {pick.move.r - pac_.r, pick.move.c - pac_.c}; pac_ = pick.move; }
            } else {
                if (hasWant) { want_ = want; hasWant_ = true; }
                vector<Pos> tryDirs;
                bool hadWant = hasWant_;
                if (hadWant) tryDirs.push_back(want_);
                tryDirs.push_back(pacDir_);
                for (size_t i = 0; i < tryDirs.size(); i++) {
                    Pos d = tryDirs[i], n{pac_.r + d.r, pac_.c + d.c};
                    if (maze_.open(n)) {
                        if (i == 0 && hadWant) hasWant_ = false;
                        pacDir_ = d;
                        pac_ = n;
                        break;
                    }
                }
            }
            if (dots_[maze_.id(pac_)]) { dots_[maze_.id(pac_)] = 0; dotsLeft_--; score_ += 10; ate = true; atePos = pac_; }

            bool caught = false;
            for (Pos g : ghosts_) if (g == pac_) caught = true;
            if (caught) { over_ = true; won_ = false; }
            else if (dotsLeft_ == 0) { over_ = true; won_ = true; }

            // ---- ghosts: each runs its own search, then steps once ----
            if (!over_) {
                bool rest = slow && turn_ % 3 == 0;
                for (int i = 0; i < 3; i++) {
                    if (rest) { rested[i] = 1; continue; }
                    SearchResult r = searchGrid(maze_, ghosts_[i], pac_, algos_[i], true);
                    searchJson[i] = searchToJson(ghosts_[i], pac_, algos_[i], r);
                    if (r.path.size() > 1) {
                        ghostDir_[i] = {r.path[1].r - ghosts_[i].r, r.path[1].c - ghosts_[i].c};
                        ghosts_[i] = r.path[1];
                    } else stuck[i] = 1;
                }
                for (int i = 0; i < 3; i++) {
                    bool same = ghosts_[i] == pac_;
                    bool swapped = ghosts_[i] == pacPrev_ && ghostPrev_[i] == pac_;
                    if (same || swapped) { over_ = true; won_ = false; }
                }
            }
        }

        ostringstream o;
        o << "{\"turn\":" << turn_ << ",\"score\":" << score_ << ",\"dotsLeft\":" << dotsLeft_
          << ",\"over\":" << (over_ ? "true" : "false") << ",\"won\":" << (won_ ? "true" : "false")
          << ",\"pac\":{\"r\":" << pac_.r << ",\"c\":" << pac_.c << ",\"pr\":" << pacPrev_.r << ",\"pc\":" << pacPrev_.c
          << ",\"dir\":" << pj(pacDir_) << "},\"ate\":" << (ate ? pj(atePos) : string("null"))
          << ",\"ai\":" << (hasAI ? "{\"ab\":" + to_string(abNodes) + ",\"mm\":" + to_string(mmNodes) + "}" : string("null"))
          << ",\"ghosts\":[";
        for (int i = 0; i < 3; i++) {
            o << (i ? "," : "") << "{\"r\":" << ghosts_[i].r << ",\"c\":" << ghosts_[i].c << ",\"pr\":" << ghostPrev_[i].r
              << ",\"pc\":" << ghostPrev_[i].c << ",\"dir\":" << pj(ghostDir_[i]) << ",\"stuck\":" << (stuck[i] ? "true" : "false")
              << ",\"rested\":" << (rested[i] ? "true" : "false") << ",\"search\":" << searchJson[i] << "}";
        }
        o << "]}";
        return o.str();
    }

private:
    Maze maze_;
    vector<int> dist_;
    vector<char> dots_;
    int dotsLeft_ = 0, turn_ = 0, score_ = 0;
    bool over_ = false, won_ = false, hasWant_ = false;
    Pos pac_{0, 0}, pacPrev_{0, 0}, pacDir_{0, 1}, want_{0, 0};
    vector<Pos> ghosts_, ghostPrev_, ghostDir_;
    vector<Algo> algos_;
    string kind_;
    unsigned seed_ = 0;

    static string pj(Pos p) { return "[" + to_string(p.r) + "," + to_string(p.c) + "]"; }
    int dAt(Pos a, Pos b) const {
        int d = dist_[(size_t)maze_.id(a) * ROWS * COLS + maze_.id(b)];
        return d < 0 ? 99 : d;
    }
    vector<Pos> dotCells() const {
        vector<Pos> v;
        for (int r = 0; r < ROWS; r++)
            for (int c = 0; c < COLS; c++)
                if (dots_[maze_.id({r, c})]) v.push_back({r, c});
        return v;
    }

    // Everything the page needs to replay a search cell by cell
    static string searchToJson(Pos start, Pos goal, Algo a, const SearchResult &r) {
        ostringstream o;
        o << "{\"algo\":\"" << algoKey(a) << "\",\"start\":" << pj(start) << ",\"goal\":" << pj(goal)
          << ",\"found\":" << (r.found ? "true" : "false") << ",\"expanded\":" << r.expanded << ",\"path\":[";
        for (size_t i = 0; i < r.path.size(); i++) o << (i ? "," : "") << pj(r.path[i]);
        o << "],\"order\":[";
        for (size_t i = 0; i < r.order.size(); i++) {
            const Expansion &e = r.order[i];
            o << (i ? "," : "") << "[" << e.p.r << "," << e.p.c << "," << e.g << "," << e.h << "," << e.f << "]";
        }
        o << "],\"open\":[";
        for (size_t i = 0; i < r.open.size(); i++) o << (i ? "," : "") << pj(r.open[i]);
        o << "],\"hill\":";
        if (a == Algo::Hill) {
            o << "{\"cur\":" << r.hill.cur << ",\"cands\":[";
            for (size_t i = 0; i < r.hill.cands.size(); i++)
                o << (i ? "," : "") << "[" << r.hill.cands[i].first.r << "," << r.hill.cands[i].first.c << "," << r.hill.cands[i].second << "]";
            o << "],\"best\":" << (r.hill.hasBest ? pj(r.hill.best) : string("null")) << "}";
        } else o << "null";
        o << "}";
        return o.str();
    }

    // ---- mazes (21 x 15) ----
    static Maze trapMaze() {
        Maze m;
        m.R = ROWS; m.C = COLS;
        m.g.assign(ROWS, string(COLS, '#'));
        for (int r = 1; r < ROWS - 1; r++)
            for (int c = 1; c < COLS - 1; c++) m.g[r][c] = ' ';
        for (int r = 1; r <= ROWS - 4; r++) m.g[r][10] = '#';  // wall with a gap along the bottom
        return m;
    }

    // Recursive-backtracker maze, then ~30% of the separating walls are removed to create loops.
    static Maze randomMaze(unsigned seed) {
        Maze m;
        m.R = ROWS; m.C = COLS;
        m.g.assign(ROWS, string(COLS, '#'));
        mt19937 rng(seed);
        vector<Pos> st{{1, 1}};
        m.g[1][1] = ' ';
        static const int dr[] = {-2, 2, 0, 0}, dc[] = {0, 0, -2, 2};
        while (!st.empty()) {
            Pos p = st.back();
            vector<Pos> opts;
            for (int k = 0; k < 4; k++) {
                Pos n{p.r + dr[k], p.c + dc[k]};
                if (n.r > 0 && n.r < ROWS - 1 && n.c > 0 && n.c < COLS - 1 && m.g[n.r][n.c] == '#') opts.push_back(n);
            }
            if (opts.empty()) { st.pop_back(); continue; }
            Pos n = opts[rng() % opts.size()];
            m.g[(p.r + n.r) / 2][(p.c + n.c) / 2] = ' ';
            m.g[n.r][n.c] = ' ';
            st.push_back(n);
        }
        vector<Pos> cand;
        for (int r = 1; r < ROWS - 1; r++)
            for (int c = 1; c < COLS - 1; c++) {
                if (m.g[r][c] != '#') continue;
                bool vert = m.g[r - 1][c] == ' ' && m.g[r + 1][c] == ' ';
                bool horiz = m.g[r][c - 1] == ' ' && m.g[r][c + 1] == ' ';
                if (vert || horiz) cand.push_back({r, c});
            }
        shuffle(cand.begin(), cand.end(), rng);
        size_t cut = cand.size() * 3 / 10;
        for (size_t i = 0; i < cut; i++) m.g[cand[i].r][cand[i].c] = ' ';
        return m;
    }
};
