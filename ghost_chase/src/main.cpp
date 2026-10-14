// =============================================================================
//  Ghost Chase - AI search algorithms in a Pac-Man style maze (terminal version)
//  CSE3121 Artificial Intelligence project
//
//  Ghosts chase the player using A*, Best-First Search or Hill Climbing.
//  The player can be a human, or an AI (greedy flee / random / Minimax / Alpha-Beta).
//
//  Commands:
//    ghost_chase demo       watch (or play) one game in the terminal
//    ghost_chase simulate   run many games with no display and print result tables
//    ghost_chase help
// =============================================================================
#include "game.h"

static string getOpt(const vector<string> &a, const string &key, const string &def) {
    for (size_t i = 0; i + 1 < a.size(); i++)
        if (a[i] == key) return a[i + 1];
    return def;
}
static bool hasFlag(const vector<string> &a, const string &key) {
    return find(a.begin(), a.end(), key) != a.end();
}

static void printHelp() {
    cout << "Ghost Chase - A*, Best-First, Hill Climbing, Minimax and Alpha-Beta in a maze\n\n"
            "USAGE\n"
            "  ghost_chase demo [options]\n"
            "  ghost_chase simulate [--games N] [--ghosts N] [--turns N] [--rounds N]\n\n"
            "DEMO OPTIONS\n"
            "  --scenario trap|random   maze to use                      (default random)\n"
            "  --team astar|bestfirst|hill|mixed   algorithm(s) for ghosts (default mixed)\n"
            "  --player human|flee|random|alphabeta|still               (default flee)\n"
            "  --ghosts N               number of ghosts, 1-5            (default 3)\n"
            "  --seed N                 maze seed (random scenario)      (default 1)\n"
            "  --delay MS               pause between turns in ms        (default 150)\n"
            "  --turns N                survive this many turns to win   (default 150)\n"
            "  --no-color               plain text (use if the screen looks garbled)\n\n"
            "EXAMPLES\n"
            "  ghost_chase demo --scenario trap --team hill --ghosts 1 --player still\n"
            "  ghost_chase demo --player human --team astar\n"
            "  ghost_chase simulate --games 100\n";
}

static vector<Algo> makeTeam(const string &team, int n) {
    vector<Algo> v;
    for (int i = 0; i < n; i++) {
        if (team == "astar") v.push_back(Algo::AStar);
        else if (team == "bestfirst") v.push_back(Algo::BestFirst);
        else if (team == "hill") v.push_back(Algo::Hill);
        else v.push_back(i % 3 == 0 ? Algo::AStar : (i % 3 == 1 ? Algo::BestFirst : Algo::Hill));
    }
    return v;
}

static PlayerType parsePlayer(const string &s) {
    if (s == "human") return PlayerType::Human;
    if (s == "random") return PlayerType::Random;
    if (s == "alphabeta") return PlayerType::AlphaBeta;
    if (s == "still") return PlayerType::Still;
    return PlayerType::Flee;
}

// ----------------------------------------------------------------------------
static int cmdDemo(const vector<string> &a) {
    int ng = max(1, min(5, stoi(getOpt(a, "--ghosts", "3"))));
    string scen = getOpt(a, "--scenario", "random");
    unsigned seed = (unsigned)stoul(getOpt(a, "--seed", "1"));
    Scenario sc = (scen == "trap") ? makeTrapScenario(ng) : makeRandomScenario(seed, 20, 20, 0.20, ng);
    Config cfg;
    cfg.maxTurns = stoi(getOpt(a, "--turns", "150"));
    cfg.rounds = stoi(getOpt(a, "--rounds", "2"));
    PlayerType pt = parsePlayer(getOpt(a, "--player", "flee"));
    vector<int> dist;
    if (pt == PlayerType::Flee || pt == PlayerType::AlphaBeta) dist = allPairs(sc.maze);
    Game game(sc, makeTeam(getOpt(a, "--team", "mixed"), (int)sc.ghosts.size()), pt, cfg, &dist, seed);
    int delay = stoi(getOpt(a, "--delay", "150"));
    game.run(true, pt == PlayerType::Human ? 0 : delay, !hasFlag(a, "--no-color"));
    return 0;
}

// ----------------------------------------------------------------------------
struct Agg {
    int games = 0, caught = 0;
    long turnsWhenCaught = 0, ghostNodes = 0, ghostMoves = 0, stuck = 0;
    double ms = 0;
    void add(const Stats &s) {
        games++;
        if (s.caught) { caught++; turnsWhenCaught += s.turns; }
        ghostNodes += s.ghostNodes;
        ghostMoves += s.ghostMoves;
        stuck += s.stuckMoves;
        ms += s.ms;
    }
};

static int cmdSimulate(const vector<string> &a) {
    int games = stoi(getOpt(a, "--games", "100"));
    int ng = max(1, min(5, stoi(getOpt(a, "--ghosts", "3"))));
    Config cfg;
    cfg.maxTurns = stoi(getOpt(a, "--turns", "150"));
    cfg.rounds = stoi(getOpt(a, "--rounds", "2"));

    cout << "==========================================================================\n"
            " GHOST CHASE - SIMULATION RESULTS\n"
            "==========================================================================\n";

    // ---- Experiment 1: the trap maze, player stands still ----
    cout << "\nEXPERIMENT 1: Trap maze (wall between ghost and player, player stands still)\n"
            "One ghost per run. Shortest possible route is 16 moves.\n\n";
    printf("%-15s %-9s %-14s %-12s\n", "Ghost algorithm", "Caught?", "Turns to catch", "Nodes expanded");
    printf("%s\n", string(54, '-').c_str());
    Scenario trap = makeTrapScenario(1);
    Config tcfg = cfg;
    tcfg.maxTurns = 100;
    for (Algo al : {Algo::AStar, Algo::BestFirst, Algo::Hill}) {
        Game g(trap, {al}, PlayerType::Still, tcfg, nullptr, 1);
        Stats s = g.run(false, 0, false);
        printf("%-15s %-9s %-14s %-12ld\n", algoName(al), s.caught ? "yes" : "NO (stuck)",
               s.caught ? to_string(s.turns).c_str() : "-", s.ghostNodes);
    }

    // ---- Experiment 2: random mazes, every ghost team vs every player ----
    cout << "\nEXPERIMENT 2: " << games << " random 20x20 mazes, " << ng << " ghosts, "
         << cfg.maxTurns << " turns max\n"
            "(same mazes for every row; stuck% = ghost turns where the ghost could not move)\n\n";
    printf("%-22s %-12s %-8s %-11s %-12s %-8s %-8s\n", "Ghost team", "Player", "Caught%",
           "Avg turns", "Nodes/move", "Stuck%", "ms/game");
    printf("%s\n", string(88, '-').c_str());

    vector<Scenario> scens;
    vector<vector<int>> dists;
    for (int i = 0; i < games; i++) {
        scens.push_back(makeRandomScenario(1000 + i, 20, 20, 0.20, ng));
        dists.push_back(allPairs(scens.back().maze));
    }
    struct Team { const char *label, *key; } teams[] = {
        {"A* (all)", "astar"}, {"Best-First (all)", "bestfirst"},
        {"Hill Climbing (all)", "hill"}, {"Mixed A*/BF/Hill", "mixed"}};
    PlayerType players[] = {PlayerType::Random, PlayerType::Flee, PlayerType::AlphaBeta};
    for (auto &t : teams) {
        for (PlayerType pt : players) {
            Agg ag;
            for (int i = 0; i < games; i++) {
                Game g(scens[i], makeTeam(t.key, ng), pt, cfg, &dists[i], 500 + i);
                ag.add(g.run(false, 0, false));
            }
            printf("%-22s %-12s %-8.0f %-11.1f %-12.1f %-8.1f %-8.2f\n", t.label, playerName(pt),
                   100.0 * ag.caught / ag.games,
                   ag.caught ? (double)ag.turnsWhenCaught / ag.caught : 0.0,
                   ag.ghostMoves ? (double)ag.ghostNodes / ag.ghostMoves : 0.0,
                   ag.ghostMoves ? 100.0 * ag.stuck / ag.ghostMoves : 0.0, ag.ms / ag.games);
        }
        printf("\n");
    }

    // ---- Experiment 3: Minimax vs Alpha-Beta for the player ----
    int cg = max(5, games / 5);
    cout << "EXPERIMENT 3: Player look-ahead - Minimax vs Alpha-Beta (" << cg
         << " games, A* ghosts, " << cfg.rounds << " rounds look-ahead)\n\n";
    printf("%-14s %-14s %-14s %-10s %-10s\n", "Player search", "Total nodes", "Nodes/move", "Caught%", "ms/game");
    printf("%s\n", string(66, '-').c_str());
    long nodesBy[2] = {0, 0};
    vector<pair<bool, int>> outcome[2];
    for (int mode = 0; mode < 2; mode++) {
        Config c2 = cfg;
        c2.prune = (mode == 1);
        long nodes = 0, moves = 0;
        int caught = 0;
        double ms = 0;
        for (int i = 0; i < cg; i++) {
            Game g(scens[i], makeTeam("astar", ng), PlayerType::AlphaBeta, c2, &dists[i], 500 + i);
            Stats s = g.run(false, 0, false);
            nodes += s.playerNodes;
            moves += s.playerMoves;
            caught += s.caught;
            ms += s.ms;
            outcome[mode].push_back({s.caught, s.turns});
        }
        nodesBy[mode] = nodes;
        printf("%-14s %-14ld %-14.0f %-10.0f %-10.1f\n", mode ? "Alpha-Beta" : "Minimax", nodes,
               moves ? (double)nodes / moves : 0.0, 100.0 * caught / cg, ms / cg);
    }
    int same = 0;
    for (int i = 0; i < cg; i++) same += (outcome[0][i] == outcome[1][i]);
    printf("\nAlpha-Beta visited %.1f%% fewer nodes and gave identical results in %d/%d games.\n",
           nodesBy[0] ? 100.0 * (nodesBy[0] - nodesBy[1]) / nodesBy[0] : 0.0, same, cg);
    return 0;
}

int main(int argc, char **argv) {
    vector<string> args(argv + 1, argv + argc);
    if (args.empty() || args[0] == "help" || args[0] == "--help") { printHelp(); return 0; }
    string cmd = args[0];
    args.erase(args.begin());
    try {
        if (cmd == "demo") return cmdDemo(args);
        if (cmd == "simulate") return cmdSimulate(args);
    } catch (const exception &e) {
        cerr << "Invalid option value: " << e.what() << "\n";
        return 1;
    }
    printHelp();
    return 1;
}
