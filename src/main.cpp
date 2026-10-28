#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

// 1. Mask Windows API macros BEFORE including Windows headers
#define Rectangle Rectangle_Win32
#define CloseWindow CloseWindow_Win32
#define ShowCursor ShowCursor_Win32
#define DrawText DrawText_Win32
#define DrawTextEx DrawTextEx_Win32
#define LoadImage LoadImage_Win32
#define PlaySound PlaySound_Win32

#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

// 2. Unmask them so Raylib can define them cleanly
#undef Rectangle
#undef CloseWindow
#undef ShowCursor
#undef DrawText
#undef DrawTextEx
#undef LoadImage
#undef PlaySound

#define SOCK_CLOSE closesocket
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <netdb.h>
#define SOCK_CLOSE close
#define SOCKET int
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#endif

#include "raylib.h"
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <random>
#include <cstring>
#include <sstream>
#include <cctype> // Required for std::toupper

// ==========================================
// CONFIGURATION
// ==========================================
const char* SERVER_IP = "obscura-server-12ot.onrender.com"; 
const int SERVER_PORT = 80;

// ==========================================
// NETWORK MANAGER (TCP Relay)
// ==========================================
class NetworkManager {
public:
    SOCKET sock = INVALID_SOCKET;
    bool isConnected = false;
    std::string myName = "";
    int myRole = -1;
    std::string receiveBuffer = "";

    bool ConnectToServer() {
#ifdef _WIN32
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) return false;
#endif
        struct addrinfo hints{}, *result = nullptr;
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        
        if (getaddrinfo(SERVER_IP, std::to_string(SERVER_PORT).c_str(), &hints, &result) != 0) return false;

        sock = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
        if (sock == INVALID_SOCKET) { freeaddrinfo(result); return false; }

        if (connect(sock, result->ai_addr, (int)result->ai_addrlen) == SOCKET_ERROR) {
            SOCK_CLOSE(sock); freeaddrinfo(result); return false;
        }
        freeaddrinfo(result);

#ifdef _WIN32
        u_long mode = 1;
        ioctlsocket(sock, FIONBIO, &mode);
#else
        int flags = fcntl(sock, F_GETFL, 0);
        fcntl(sock, F_SETFL, flags | O_NONBLOCK);
#endif
        isConnected = true;
        return true;
    }

    void Send(const std::string& msg) {
        if (isConnected) send(sock, msg.c_str(), (int)msg.length(), 0);
    }

    void ProcessMessages(std::vector<std::string>& outMessages) {
        if (!isConnected) return;
        char buffer[1024];
        int bytes = recv(sock, buffer, sizeof(buffer) - 1, 0);
        if (bytes > 0) {
            buffer[bytes] = '\0';
            receiveBuffer += buffer;
            
            size_t pos;
            while ((pos = receiveBuffer.find('\n')) != std::string::npos) {
                std::string line = receiveBuffer.substr(0, pos);
                receiveBuffer.erase(0, pos + 1);
                if (!line.empty()) outMessages.push_back(line);
            }
        }
    }

    void Close() {
        if (sock != INVALID_SOCKET) SOCK_CLOSE(sock);
#ifdef _WIN32
        WSACleanup();
#endif
    }
};

// ==========================================
// GAME CONSTANTS & STRUCTS
// ==========================================
const int SCREEN_WIDTH = 1280;
const int SCREEN_HEIGHT = 720;
const int TILE_SIZE = 40;
const int MAP_COLS = SCREEN_WIDTH / TILE_SIZE;
const int MAP_ROWS = SCREEN_HEIGHT / TILE_SIZE;

enum class GameState { 
    CONNECTING, NO_CONNECTION, USERNAME_INPUT, MAIN_HUB, HOST_SETUP, 
    JOIN_SETUP, WAITING_ROOM, ROLE_REVEAL, HIDING_PHASE, SEEKING_PHASE, PAUSED, GAME_OVER 
};

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
                if (r == 0 || r == MAP_ROWS - 1 || c == 0 || c == MAP_COLS - 1) {
                    walls[r][c] = true;
                } else if (dist(gen) < 20) {
                    walls[r][c] = true;
                }
            }
        }
        for (int r = 1; r < 5; ++r) {
            for (int c = 1; c < 5; ++c) walls[r][c] = false;
        }
        for (int r = MAP_ROWS - 5; r < MAP_ROWS - 1; ++r) {
            for (int c = MAP_COLS - 5; c < MAP_COLS - 1; ++c) walls[r][c] = false;
        }
    }
    
    bool IsWall(float x, float y) const {
        int col = static_cast<int>(x) / TILE_SIZE;
        int row = static_cast<int>(y) / TILE_SIZE;
        if (row < 0 || row >= MAP_ROWS || col < 0 || col >= MAP_COLS) return true;
        return walls[row][col];
    }
};

// ==========================================
// HELPER FUNCTIONS
// ==========================================
bool CheckCollision(const Player& p, const GameMap& m, Vector2 newPos) {
    float offsets[] = {-p.radius, p.radius};
    for (float ox : offsets) {
        for (float oy : offsets) {
            if (m.IsWall(newPos.x + ox, newPos.y + oy)) return true;
        }
    }
    return false;
}

std::string GetTextInput(int maxLength) {
    std::string result = "";
    int key = GetCharPressed();
    while (key > 0) {
        if ((key >= 32) && (key <= 125) && (result.length() < static_cast<size_t>(maxLength))) {
            result += static_cast<char>(key);
        }
        key = GetCharPressed();
    }
    if (IsKeyPressed(KEY_BACKSPACE) && !result.empty()) result.pop_back();
    return result;
}

std::string GenerateRoomCode() {
    const char chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    std::string code = "";
    for (int i = 0; i < 4; ++i) code += chars[std::rand() % 36];
    return code;
}

// ==========================================
// MAIN GAME
// ==========================================
int main() {
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Obscura - Online 1v1 Hide & Seek");
    SetTargetFPS(60);

    NetworkManager net;
    bool connected = net.ConnectToServer();

    GameState state = connected ? GameState::USERNAME_INPUT : GameState::NO_CONNECTION;
    GameMap map;
    Player localPlayer, opponentPlayer;
    
    std::string username = "", roomCodeInput = "", hostRoomCode = "", statusMessage = "";
    int lobbyTimer = 60;
    float revealTimer = 3.0f, phaseTimer = 0.0f;
    localPlayer.color = BLUE; 
    opponentPlayer.color = RED;

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        std::vector<std::string> messages;
        net.ProcessMessages(messages);

        for (const std::string& msg : messages) {
            if (msg == "PLAYER_JOINED") {
                net.myRole = std::rand() % 2;
                phaseTimer = static_cast<float>(lobbyTimer);
                map.Generate();
                state = GameState::ROLE_REVEAL;
                revealTimer = 3.0f;
            } else if (msg == "HOST_DISCONNECTED" || msg == "CLIENT_DISCONNECTED") {
                statusMessage = "Opponent disconnected!";
                state = GameState::MAIN_HUB;
            } else if (msg.substr(0, 5) == "DATA:") {
                std::string data = msg.substr(5);
                size_t xPos = data.find("\"x\":");
                size_t yPos = data.find("\"y\":");
                if (xPos != std::string::npos && yPos != std::string::npos) {
                    float x = std::stof(data.substr(xPos + 4));
                    float y = std::stof(data.substr(yPos + 4));
                    opponentPlayer.position = {x, y};
                }
            }
        }

        if (state == GameState::NO_CONNECTION) {
            DrawText("CANNOT CONNECT TO SERVER", SCREEN_WIDTH/2 - 200, SCREEN_HEIGHT/2 - 50, 30, RED);
            DrawText("Check SERVER_IP in main.cpp", SCREEN_WIDTH/2 - 150, SCREEN_HEIGHT/2, 20, GRAY);
            if (IsKeyPressed(KEY_ESCAPE)) break;
        }
        else if (state == GameState::USERNAME_INPUT) {
            DrawText("ONLINE MODE (Room Codes)", SCREEN_WIDTH/2 - 180, 150, 40, GREEN);
            DrawText("Enter Username:", SCREEN_WIDTH/2 - 100, 200, 30, WHITE);
            DrawText(username.c_str(), SCREEN_WIDTH/2 - 100, 250, 30, GREEN);
            DrawText("Press ENTER to confirm", SCREEN_WIDTH/2 - 150, 350, 20, GRAY);
            std::string input = GetTextInput(15);
            if (!input.empty()) username += input;
            if (IsKeyPressed(KEY_BACKSPACE) && !username.empty()) username.pop_back();
            if (IsKeyPressed(KEY_ENTER) && !username.empty()) { 
                net.myName = username; 
                state = GameState::MAIN_HUB; 
            }
        }
        else if (state == GameState::MAIN_HUB) {
            DrawText("MAIN HUB", SCREEN_WIDTH/2 - 100, 150, 50, WHITE);
            DrawText(TextFormat("Player: %s", net.myName.c_str()), SCREEN_WIDTH/2 - 100, 220, 20, GREEN);
            
            DrawRectangle(SCREEN_WIDTH/2 - 150, 300, 300, 60, DARKGREEN);
            DrawText("HOST GAME", SCREEN_WIDTH/2 - 90, 320, 30, WHITE);
            
            DrawRectangle(SCREEN_WIDTH/2 - 150, 400, 300, 60, DARKBLUE);
            DrawText("JOIN GAME", SCREEN_WIDTH/2 - 90, 420, 30, WHITE);
            
            Vector2 mouse = GetMousePosition();
            if (mouse.x > SCREEN_WIDTH/2 - 150 && mouse.x < SCREEN_WIDTH/2 + 150) {
                if (mouse.y > 300 && mouse.y < 360 && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                    hostRoomCode = GenerateRoomCode();
                    state = GameState::HOST_SETUP;
                }
                if (mouse.y > 400 && mouse.y < 460 && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                    state = GameState::JOIN_SETUP;
                }
            }
        }
        else if (state == GameState::HOST_SETUP) {
            DrawText("HOST GAME", SCREEN_WIDTH/2 - 120, 150, 40, WHITE);
            DrawText(TextFormat("Room Code: %s", hostRoomCode.c_str()), SCREEN_WIDTH/2 - 100, 220, 50, YELLOW);
            DrawText("Share this code with your friend!", SCREEN_WIDTH/2 - 180, 290, 20, GRAY);
            
            DrawText(TextFormat("Timer: %d seconds (UP/DOWN)", lobbyTimer), SCREEN_WIDTH/2 - 150, 340, 20, YELLOW);
            if (IsKeyDown(KEY_UP)) lobbyTimer = std::min(180, lobbyTimer + 1);
            if (IsKeyDown(KEY_DOWN)) lobbyTimer = std::max(10, lobbyTimer - 1);
            
            DrawRectangle(SCREEN_WIDTH/2 - 100, 400, 200, 50, GREEN);
            DrawText("START WAITING", SCREEN_WIDTH/2 - 90, 415, 20, WHITE);
            
            Vector2 mouse = GetMousePosition();
            if (mouse.x > SCREEN_WIDTH/2 - 100 && mouse.x < SCREEN_WIDTH/2 + 100 && 
                mouse.y > 400 && mouse.y < 450 && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                net.Send("HOST:" + hostRoomCode + "\n");
                state = GameState::WAITING_ROOM;
                statusMessage = "Waiting for player to join code...";
            }
            
            DrawText(statusMessage.c_str(), SCREEN_WIDTH/2 - 150, 500, 20, RED);
            if (IsKeyPressed(KEY_ESCAPE)) state = GameState::MAIN_HUB;
        }
        else if (state == GameState::JOIN_SETUP) {
            DrawText("JOIN GAME", SCREEN_WIDTH/2 - 120, 150, 40, WHITE);
            DrawText("Enter 4-Letter Room Code:", SCREEN_WIDTH/2 - 150, 220, 20, WHITE);
            
            std::string input = GetTextInput(4);
            // FIX: static_cast to satisfy MSVC /W4 warning
            for (char& c : input) {
                c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            }
            if (!input.empty()) roomCodeInput += input;
            if (IsKeyPressed(KEY_BACKSPACE) && !roomCodeInput.empty()) roomCodeInput.pop_back();
            if (roomCodeInput.length() > 4) roomCodeInput = roomCodeInput.substr(0, 4);
            
            DrawText(roomCodeInput.c_str(), SCREEN_WIDTH/2 - 50, 260, 50, GREEN);
            DrawText("Press ENTER to Join", SCREEN_WIDTH/2 - 120, 350, 20, GRAY);
            
            if (IsKeyPressed(KEY_ENTER) && roomCodeInput.length() == 4) {
                net.Send("JOIN:" + roomCodeInput + "\n");
                state = GameState::WAITING_ROOM;
                statusMessage = "Joining room...";
            }
            
            DrawText(statusMessage.c_str(), SCREEN_WIDTH/2 - 150, 400, 20, RED);
            if (IsKeyPressed(KEY_ESCAPE)) state = GameState::MAIN_HUB;
        }
        else if (state == GameState::WAITING_ROOM) {
            DrawText("WAITING ROOM", SCREEN_WIDTH/2 - 120, 150, 40, WHITE);
            DrawText(statusMessage.c_str(), SCREEN_WIDTH/2 - 200, 250, 20, YELLOW);
            if (IsKeyPressed(KEY_ESCAPE)) {
                net.Send("DATA:{\"type\":\"quit\"}\n");
                state = GameState::MAIN_HUB;
            }
        }
        else if (state == GameState::ROLE_REVEAL) {
            revealTimer -= dt;
            const char* roleText = (net.myRole == 0) ? "YOU ARE THE HIDER" : "YOU ARE THE HUNTER";
            Color roleColor = (net.myRole == 0) ? BLUE : RED;
            DrawText(roleText, SCREEN_WIDTH/2 - 200, SCREEN_HEIGHT/2 - 50, 60, roleColor);
            DrawText(TextFormat("Game starts in: %.1f", revealTimer), SCREEN_WIDTH/2 - 100, SCREEN_HEIGHT/2 + 50, 30, WHITE);
            if (revealTimer <= 0) {
                state = (net.myRole == 0) ? GameState::HIDING_PHASE : GameState::SEEKING_PHASE;
                localPlayer.position = {100, 100};
            }
        }
        else if (state == GameState::HIDING_PHASE || state == GameState::SEEKING_PHASE) {
            phaseTimer -= dt;
            Vector2 move = {0, 0};
            
            if (IsKeyDown(KEY_W)) move.y -= 1;
            if (IsKeyDown(KEY_S)) move.y += 1;
            if (IsKeyDown(KEY_A)) move.x -= 1;
            if (IsKeyDown(KEY_D)) move.x += 1;
            
            if ((IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_E)) && localPlayer.cheatCooldown <= 0) {
                localPlayer.cheatActive = true; 
                localPlayer.isVisible = false; 
                localPlayer.speed = 350.0f; 
                localPlayer.cheatCooldown = 8.0f;
            }
            if (localPlayer.cheatActive) {
                localPlayer.cheatCooldown -= dt;
                if (localPlayer.cheatCooldown <= 5.0f) { 
                    localPlayer.cheatActive = false; 
                    localPlayer.isVisible = true; 
                    localPlayer.speed = 200.0f; 
                }
            } else if (localPlayer.cheatCooldown > 0) { 
                localPlayer.cheatCooldown -= dt; 
            }
            
            if (move.x != 0 || move.y != 0) {
                float len = sqrtf(move.x * move.x + move.y * move.y);
                move.x /= len; 
                move.y /= len;
                Vector2 newPos = {
                    localPlayer.position.x + move.x * localPlayer.speed * dt, 
                    localPlayer.position.y + move.y * localPlayer.speed * dt
                };
                if (!CheckCollision(localPlayer, map, newPos)) {
                    localPlayer.position = newPos;
                }
            }
            
            std::string posMsg = "DATA:{\"type\":4,\"x\":" + 
                                 std::to_string(localPlayer.position.x) + 
                                 ",\"y\":" + 
                                 std::to_string(localPlayer.position.y) + 
                                 "}\n";
            net.Send(posMsg);
            
            if (state == GameState::SEEKING_PHASE) {
                if (phaseTimer <= 0) { 
                    state = GameState::GAME_OVER; 
                    statusMessage = "HIDER WINS!"; 
                }
            } else {
                if (phaseTimer <= 0) { 
                    state = GameState::SEEKING_PHASE; 
                    phaseTimer = 120.0f; 
                }
            }
            if (IsKeyPressed(KEY_ESCAPE)) state = GameState::PAUSED;
        }
        else if (state == GameState::PAUSED) {
            DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, ColorAlpha(BLACK, 0.8f));
            DrawText("PAUSED", SCREEN_WIDTH/2 - 80, SCREEN_HEIGHT/2 - 50, 40, WHITE);
            DrawText("ESC to Resume", SCREEN_WIDTH/2 - 100, SCREEN_HEIGHT/2 + 20, 20, GRAY);
            DrawText("Q to Quit to Menu", SCREEN_WIDTH/2 - 110, SCREEN_HEIGHT/2 + 50, 20, RED);
            if (IsKeyPressed(KEY_ESCAPE)) state = GameState::SEEKING_PHASE;
            if (IsKeyPressed(KEY_Q)) state = GameState::MAIN_HUB;
        }
        else if (state == GameState::GAME_OVER) {
            DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, ColorAlpha(BLACK, 0.85f));
            DrawText(statusMessage.c_str(), SCREEN_WIDTH/2 - 150, SCREEN_HEIGHT/2 - 30, 50, GOLD);
            DrawText("Press ENTER for Main Menu", SCREEN_WIDTH/2 - 150, SCREEN_HEIGHT/2 + 50, 20, WHITE);
            if (IsKeyPressed(KEY_ENTER)) state = GameState::MAIN_HUB;
        }

        BeginDrawing();
        ClearBackground(BLACK);
        
        if (state == GameState::HIDING_PHASE || state == GameState::SEEKING_PHASE || state == GameState::GAME_OVER) {
            if (!map.walls.empty()) {
                for (int r = 0; r < MAP_ROWS; ++r) {
                    for (int c = 0; c < MAP_COLS; ++c) {
                        if (map.walls[r][c]) {
                            DrawRectangle(c * TILE_SIZE, r * TILE_SIZE, TILE_SIZE, TILE_SIZE, DARKGRAY);
                        }
                    }
                }
            }
            
            if (net.myRole == 0) {
                if (localPlayer.isVisible || state == GameState::HIDING_PHASE) {
                    DrawCircleV(localPlayer.position, localPlayer.radius, BLUE);
                }
                DrawCircleV(opponentPlayer.position, 15, RED);
            } else {
                DrawCircleV(localPlayer.position, 15, RED);
                if (opponentPlayer.isVisible || state == GameState::HIDING_PHASE) {
                    DrawCircleV(opponentPlayer.position, 15, BLUE);
                }
            }
            
            DrawText(TextFormat("Time: %.1f", phaseTimer), 10, 10, 20, YELLOW);
        }
        
        EndDrawing();
    }
    
    net.Close();
    CloseWindow();
    return 0;
}
