// =============================================================================
//  Ghost Chase web server
//
//  Runs the C++ game (webgame.h) and serves the browser page (web/index.html).
//  The page only draws: every ghost search and Pac-Man decision is computed here.
//
//  Build (Linux / Mac):  g++ -O2 -std=c++17 -pthread src/server.cpp -o ghost_server
//  Build (Windows):      see build_windows.bat
//  Run:                  ./ghost_server          then open  http://localhost:8080
//
//  Endpoints
//    GET /                  the page
//    GET /api/new?kind=random|trap&seed=N&algos=astar,best,hill   start a new game
//    GET /api/step?mode=you|ai&prune=1&slow=1&want=dr,dc          play one turn
// =============================================================================
// Platform socket headers come first (on Windows they must precede the standard headers).
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET sock_t;
#define CLOSESOCK closesocket
static const sock_t BAD_SOCK = INVALID_SOCKET;
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
typedef int sock_t;
#define CLOSESOCK close
static const sock_t BAD_SOCK = -1;
#endif

#include "webgame.h"
#include <fstream>
#include <mutex>
#include <thread>

// Windows uses native locks and threads: statically linked MinGW programs can fail with std::thread.
#ifdef _WIN32
struct Mutex {
    CRITICAL_SECTION cs;
    Mutex() { InitializeCriticalSection(&cs); }
    void lock() { EnterCriticalSection(&cs); }
    void unlock() { LeaveCriticalSection(&cs); }
};
#else
typedef std::mutex Mutex;
#endif

static WebGame g_game;
static Mutex g_mutex;
static string g_pagePath;

static string urlDecode(const string &s) {
    string out;
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '%' && i + 2 < s.size() && isxdigit((unsigned char)s[i + 1]) && isxdigit((unsigned char)s[i + 2])) {
            out += (char)stoi(s.substr(i + 1, 2), nullptr, 16);
            i += 2;
        } else if (s[i] == '+') out += ' ';
        else out += s[i];
    }
    return out;
}

static map<string, string> parseQuery(const string &q) {
    map<string, string> m;
    size_t i = 0;
    while (i <= q.size()) {
        size_t amp = q.find('&', i);
        if (amp == string::npos) amp = q.size();
        string kv = q.substr(i, amp - i);
        size_t eq = kv.find('=');
        if (!kv.empty()) {
            if (eq == string::npos) m[urlDecode(kv)] = "";
            else m[urlDecode(kv.substr(0, eq))] = urlDecode(kv.substr(eq + 1));
        }
        i = amp + 1;
    }
    return m;
}

static string getParam(const map<string, string> &m, const string &k, const string &def) {
    auto it = m.find(k);
    return it == m.end() ? def : it->second;
}

static void sendAll(sock_t s, const string &data) {
    size_t sent = 0;
    while (sent < data.size()) {
        int n = (int)send(s, data.data() + sent, (int)min<size_t>(data.size() - sent, 1 << 16), 0);
        if (n <= 0) return;
        sent += n;
    }
}

static void respond(sock_t s, int code, const string &type, const string &body) {
    const char *msg = code == 200 ? "OK" : (code == 404 ? "Not Found" : "Bad Request");
    ostringstream h;
    h << "HTTP/1.1 " << code << " " << msg << "\r\nContent-Type: " << type
      << "\r\nContent-Length: " << body.size() << "\r\nCache-Control: no-store\r\nConnection: close\r\n\r\n";
    sendAll(s, h.str());
    sendAll(s, body);
}

static bool readFile(const string &path, string &out) {
    ifstream f(path, ios::binary);
    if (!f) return false;
    ostringstream ss;
    ss << f.rdbuf();
    out = ss.str();
    return true;
}

static void handleClient(sock_t s) {
#ifdef _WIN32
    DWORD tv = 5000;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof tv);
#else
    timeval tv{5, 0};
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
#endif
    string req;
    char buf[2048];
    while (req.find("\r\n\r\n") == string::npos && req.size() < 16384) {
        int n = (int)recv(s, buf, sizeof buf, 0);
        if (n <= 0) break;
        req.append(buf, n);
    }
    size_t eol = req.find("\r\n");
    if (eol == string::npos) { CLOSESOCK(s); return; }
    istringstream line(req.substr(0, eol));
    string method, target, version;
    line >> method >> target >> version;
    string path = target, query;
    size_t qm = target.find('?');
    if (qm != string::npos) { path = target.substr(0, qm); query = target.substr(qm + 1); }
    auto params = parseQuery(query);

    if (method != "GET") {
        respond(s, 400, "text/plain", "Only GET is supported");
    } else if (path == "/" || path == "/index.html") {
        string html;
        if (readFile(g_pagePath, html)) respond(s, 200, "text/html; charset=utf-8", html);
        else respond(s, 404, "text/plain", "web/index.html not found. Run ghost_server from the project folder.");
    } else if (path == "/api/new") {
        string kind = getParam(params, "kind", "random") == "trap" ? "trap" : "random";
        unsigned seed = (unsigned)strtoul(getParam(params, "seed", "7").c_str(), nullptr, 10);
        vector<Algo> algos;
        istringstream as(getParam(params, "algos", "astar,best,hill"));
        string tok;
        while (getline(as, tok, ',')) algos.push_back(algoFromKey(tok));
        lock_guard<Mutex> lk(g_mutex);
        g_game.newGame(kind, seed, algos);
        respond(s, 200, "application/json", g_game.newJson());
    } else if (path == "/api/algos") {
        vector<Algo> algos;
        istringstream as(getParam(params, "algos", "astar,best,hill"));
        string tok;
        while (getline(as, tok, ',')) algos.push_back(algoFromKey(tok));
        lock_guard<Mutex> lk(g_mutex);
        g_game.setAlgos(algos);
        respond(s, 200, "application/json", "{\"ok\":true}");
    } else if (path == "/api/step") {
        string mode = getParam(params, "mode", "you") == "ai" ? "ai" : "you";
        bool prune = getParam(params, "prune", "1") != "0";
        bool slow = getParam(params, "slow", "1") != "0";
        bool hasWant = false;
        Pos want{0, 0};
        string w = getParam(params, "want", "");
        if (!w.empty()) {
            int dr, dc;
            if (sscanf(w.c_str(), "%d,%d", &dr, &dc) == 2 && abs(dr) + abs(dc) == 1) { want = {dr, dc}; hasWant = true; }
        }
        lock_guard<Mutex> lk(g_mutex);
        respond(s, 200, "application/json", g_game.step(mode, prune, slow, hasWant, want));
    } else if (path == "/favicon.ico") {
        respond(s, 200, "image/x-icon", "");
    } else {
        respond(s, 404, "text/plain", "Not found");
    }
    CLOSESOCK(s);
}

#ifdef _WIN32
static DWORD WINAPI clientThread(LPVOID p) {
    handleClient((sock_t)(ULONG_PTR)p);
    return 0;
}
static void spawnClient(sock_t c) {
    HANDLE h = CreateThread(nullptr, 0, clientThread, (LPVOID)(ULONG_PTR)c, 0, nullptr);
    if (h) CloseHandle(h);
    else CLOSESOCK(c);
}
#else
static void spawnClient(sock_t c) { thread(handleClient, c).detach(); }
#endif

int main(int argc, char **argv) {
    int port = 8080;
    bool openBrowser = true;
    for (int i = 1; i < argc; i++) {
        string a = argv[i];
        if (a == "--port" && i + 1 < argc) port = atoi(argv[++i]);
        else if (a == "--web" && i + 1 < argc) g_pagePath = string(argv[++i]) + "/index.html";
        else if (a == "--no-open") openBrowser = false;
        else if (a == "--help") {
            cout << "ghost_server [--port N] [--web DIR] [--no-open]\n"
                    "Run it from the project folder, then open http://localhost:8080\n";
            return 0;
        }
    }
    if (g_pagePath.empty()) {
        for (const char *p : {"web/index.html", "../web/index.html", "../../web/index.html"}) {
            ifstream f(p);
            if (f) { g_pagePath = p; break; }
        }
        if (g_pagePath.empty()) g_pagePath = "web/index.html";
    }
    if (!ifstream(g_pagePath)) {
        cerr << "Cannot find " << g_pagePath << ". Run ghost_server from the project folder (the one containing web/).\n";
        return 1;
    }

#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) { cerr << "WSAStartup failed\n"; return 1; }
#endif
    sock_t srv = socket(AF_INET, SOCK_STREAM, 0);
    if (srv == BAD_SOCK) { cerr << "Could not create socket\n"; return 1; }
    int yes = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, (const char *)&yes, sizeof yes);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);  // only this computer can connect
    bool bound = false;
    for (int p = port; p < port + 10 && !bound; p++) {
        addr.sin_port = htons((unsigned short)p);
        if (::bind(srv, (sockaddr *)&addr, (int)sizeof addr) == 0) { port = p; bound = true; }  // ::bind, not std::bind
    }
    if (!bound || ::listen(srv, 16) != 0) { cerr << "Could not listen on port " << port << " (is another copy running?)\n"; return 1; }

    string url = "http://localhost:" + to_string(port);
    cout << "Ghost Chase server is running.\nOpen " << url << " in your browser. Press Ctrl+C to stop.\n" << flush;
#if defined(_WIN32)
    if (openBrowser) system(("start " + url).c_str());
#elif defined(__APPLE__)
    if (openBrowser) system(("open " + url).c_str());
#else
    (void)openBrowser;
#endif

    while (true) {
        sock_t c = accept(srv, nullptr, nullptr);
        if (c == BAD_SOCK) continue;
        spawnClient(c);  // one thread per connection (browsers open several)
    }
    return 0;
}
