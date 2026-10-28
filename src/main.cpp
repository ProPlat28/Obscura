#include "raylib.h"
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <random>
#include <cstring>
#include <cstdlib>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
// FIX: Undefine Windows macros that conflict with Raylib functions
#undef DrawText
#undef CloseWindow
#undef ShowCursor
#define SOCK_CLOSE closesocket
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#define SOCK_CLOSE close
#define SOCKET int
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#endif

const int NETWORK_PORT = 54000;

struct NetworkPacket {
    int type; // 0: Ping, 1: JoinReq, 2: JoinAccept, 4: PlayerMove
    char name[32];
    int timer;
    char pin[5];
    int role; // 0: Hider, 1: Hunter
    float posX, posY;
};

class NetworkManager {
public:
    SOCKET sock = INVALID_SOCKET;
    bool isHost = false;
    std::string myName = "";
    int myRole = -1;

    bool Init() {
#ifdef _WIN32
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) return false;
#endif
        sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (sock == INVALID_SOCKET) return false;
        int broadcast = 1, reuse = 1;
        setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (const char*)&broadcast, sizeof(broadcast));
        setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse));
        sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(NETWORK_PORT);
        bind(sock, (sockaddr*)&addr, sizeof(addr));
        return true;
    }

    void SendTo(const char* ip, NetworkPacket& pkt) {
        sockaddr_in dest;
        dest.sin_family = AF_INET;
        dest.sin_port = htons(NETWORK_PORT);
        inet_pton(AF_INET, ip, &dest.sin_addr);
        sendto(sock, (const char*)&pkt, sizeof(pkt), 0, (sockaddr*)&dest, sizeof(dest));
    }

    void Broadcast(NetworkPacket& pkt) { SendTo("255.255.255.255", pkt); }

    bool Receive(NetworkPacket& pkt) {
        sockaddr_in src;
        socklen_t srcLen = sizeof(src);
        return recvfrom(sock, (char*)&pkt, sizeof(pkt), 0, (sockaddr*)&src, &srcLen) > 0;
    }

    void Close() {
        if (sock != INVALID_SOCKET) SOCK_CLOSE(sock);
#ifdef _WIN32
        WSACleanup();
#endif
    }
};

const int SCREEN_WIDTH = 1280;
const int SCREEN_HEIGHT = 720;
const int TILE_SIZE = 40;
const int MAP_COLS = SCREEN_WIDTH / TILE_SIZE;
const int MAP_ROWS = SCREEN_HEIGHT / TILE_SIZE;

enum class GameState { USERNAME_INPUT, MAIN_HUB, HOST_SETUP, LOBBY_LIST, WAITING_ROOM, ROLE_REVEAL, HIDING_PHASE, SEEKING_PHASE, PAUSED, GAME_OVER };

struct Player {
    Vector2 position = {0, 0};
    float speed = 200.0f;
    float radius = 15.0f;
    Color color = WHITE;
    bool isVisible = true;
    float cheatCooldown = 0.0f;
    bool cheatActive = false;
};

struct GameMap {
    std::vector<std::vector<bool>> walls;
    void Generate() {
        walls.assign(MAP_ROWS, std::vector<bool>(MAP_COLS, false));
        std::mt19937 gen(42); 
        std::uniform_int_distribution<> dist(0, 100);
        for (int r = 0; r < MAP_ROWS; ++r) {
            for (int c = 0; c < MAP_COLS; ++c) {
                if (r == 0 || r == MAP_ROWS - 1 || c == 0 || c == MAP_COLS - 1) walls[r][c] = true;
                else if (dist(gen) < 20) walls[r][c] = true;
            }
        }
        for (int r = 1; r < 5; ++r) for (int c = 1; c < 5; ++c) walls[r][c] = false;
        for (int r = MAP_ROWS - 5; r < MAP_ROWS - 1; ++r) for (int c = MAP_COLS - 5; c < MAP_COLS - 1; ++c) walls[r][c] = false;
    }
    bool IsWall(float x, float y) const {
        int col = static_cast<int>(x) / TILE_SIZE;
        int row = static_cast<int>(y) / TILE_SIZE;
        if (row < 0 || row >= MAP_ROWS || col < 0 || col >= MAP_COLS) return true;
        return walls[row][col];
    }
};

bool CheckCollision(const Player& p, const GameMap& m, Vector2 newPos) {
    float offsets[] = {-p.radius, p.radius};
    for (float ox : offsets) for (float oy : offsets) if (m.IsWall(newPos.x + ox, newPos.y + oy)) return true;
    return false;
}

float Distance(const Player& a, const Player& b) {
    float dx = a.position.x - b.position.x;
    float dy = a.position.y - b.position.y;
    return sqrtf(dx * dx + dy * dy);
}

std::string GetTextInput(int maxLength) {
    std::string result = "";
    int key = GetCharPressed();
    while (key > 0) {
        // FIX: static_cast to prevent sign-compare warning on Linux
        if ((key >= 32) && (key <= 125) && (result.length() < static_cast<size_t>(maxLength))) {
            result += (char)key;
        }
        key = GetCharPressed();
    }
    if (IsKeyPressed(KEY_BACKSPACE) && !result.empty()) result.pop_back();
    return result;
}

int main() {
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Obscura - Online 1v1");
    SetTargetFPS(60);
    NetworkManager net;
    net.Init();

    GameState state = GameState::USERNAME_INPUT;
    GameMap map;
    Player localPlayer, opponentPlayer;
    std::string username = "", pinInput = "", hostPin = "", statusMessage = "";
    int lobbyTimer = 60;
    bool usePin = false;
    float revealTimer = 3.0f, phaseTimer = 0.0f;
    localPlayer.color = BLUE; opponentPlayer.color = RED;

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        NetworkPacket pkt;
        bool gotPacket = net.Receive(pkt);

        if (state == GameState::USERNAME_INPUT) {
            DrawText("Enter Username:", SCREEN_WIDTH/2 - 100, 200, 30, WHITE);
            DrawText(username.c_str(), SCREEN_WIDTH/2 - 100, 250, 30, GREEN);
            DrawText("Press ENTER to confirm", SCREEN_WIDTH/2 - 150, 350, 20, GRAY);
            std::string input = GetTextInput(15);
            if (!input.empty()) username += input;
            if (IsKeyPressed(KEY_BACKSPACE) && !username.empty()) username.pop_back();
            if (IsKeyPressed(KEY_ENTER) && !username.empty()) { net.myName = username; state = GameState::MAIN_HUB; }
        }
        else if (state == GameState::MAIN_HUB) {
            DrawText("MAIN HUB", SCREEN_WIDTH/2 - 100, 150, 50, WHITE);
            DrawText(TextFormat("Logged in as: %s", net.myName.c_str()), SCREEN_WIDTH/2 - 150, 220, 20, GREEN);
            DrawRectangle(SCREEN_WIDTH/2 - 150, 300, 300, 60, DARKGREEN);
            DrawText("HOST GAME", SCREEN_WIDTH/2 - 90, 320, 30, WHITE);
            DrawRectangle(SCREEN_WIDTH/2 - 150, 400, 300, 60, DARKBLUE);
            DrawText("JOIN GAME", SCREEN_WIDTH/2 - 90, 420, 30, WHITE);
            Vector2 mouse = GetMousePosition();
            if (mouse.x > SCREEN_WIDTH/2 - 150 && mouse.x < SCREEN_WIDTH/2 + 150) {
                if (mouse.y > 300 && mouse.y < 360 && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) state = GameState::HOST_SETUP;
                if (mouse.y > 400 && mouse.y < 460 && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) state = GameState::LOBBY_LIST;
            }
        }
        else if (state == GameState::HOST_SETUP) {
            DrawText("HOST SETUP", SCREEN_WIDTH/2 - 120, 150, 40, WHITE);
            DrawText(TextFormat("Timer: %d seconds (UP/DOWN)", lobbyTimer), SCREEN_WIDTH/2 - 150, 22
