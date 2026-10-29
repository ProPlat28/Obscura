#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#pragma warning(push)
#pragma warning(disable: 4005)
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
#pragma warning(pop)
#undef Rectangle
#undef CloseWindow
#undef ShowCursor
#undef DrawText
#undef DrawTextEx
#undef LoadImage
#undef PlaySound
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif

#include "raylib.h"
#include <httplib.h>
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <random>
#include <cstring>
#include <thread>
#include <mutex>
#include <queue>
#include <sstream>
#include <memory>
#include <chrono>

const char* SERVER_IP = "obscura-server-12ot.onrender.com"; 
const int SERVER_PORT = 443; 

class NetworkManager {
public:
    std::unique_ptr<httplib::SSLClient> cli;
    std::thread netThread;
    std::mutex mtx;       
    std::mutex httpMtx;   
    std::queue<std::string> msgQueue;
    std::queue<std::string> sendQueue;
    
    bool isHost = false;
    std::string roomCode = "";
    std::string role = ""; 
    int myRole = -1;       
    std::string myName = "";
    bool running = false;

    void Init() {
        cli = std::make_unique<httplib::SSLClient>(SERVER_IP, SERVER_PORT);
        cli->enable_server_certificate_verification(false); 
        cli->set_connection_timeout(15, 0); 
        cli->set_read_timeout(25, 0); 
        cli->set_keep_alive(true); 
        running = true;
        // Start thread immediately, but it will sleep until roomCode is set
        netThread = std::thread(&NetworkManager::NetworkLoop, this);
    }

    void NetworkLoop() {
        while (running) {
            // FIX: Only poll/send if we have successfully joined/created a room
            if (!roomCode.empty() && !role.empty()) {
                {
                    std::lock_guard<std::mutex> lock(mtx);
                    if (!sendQueue.empty()) {
                        std::string data = sendQueue.front();
                        sendQueue.pop();
                        std::lock_guard<std::mutex> httpLock(httpMtx);
                        cli->Post("/send?code=" + roomCode + "&role=" + role, data, "text/plain");
                    }
                }
                std::string path = "/poll?code=" + roomCode + "&role=" + role;
                httplib::Result res;
                {
                    std::lock_guard<std::mutex> lock(httpMtx);
                    res = cli->Post(path, "", "text/plain");
                }
                if (res && res->status == 200) {
                    std::lock_guard<std::mutex> lock(mtx);
                    std::istringstream stream(res->body);
                    std::string line;
                    while (std::getline(stream, line)) {
                        if (!line.empty() && line.back() == '\r') line.pop_back();
                        if (!line.empty()) {
                            if (msgQueue.size() >= 100) msgQueue.pop(); 
                            msgQueue.push(line);
                        }
                    }
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
    }

    void CreateRoom(const std::string& code, int time, int headstart) {
        httplib::Result res;
        {
            std::lock_guard<std::mutex> lock(httpMtx);
            res = cli->Post("/create?code=" + code + "&time=" + std::to_string(time) + "&headstart=" + std::to_string(headstart), "", "text/plain");
        }
        // FIX: Only set roomCode and role AFTER the server confirms creation
        if (res && res->status == 200) {
            roomCode = code;
            role = "host";
            isHost = true;
        } else {
            std::lock_guard<std::mutex> lock(mtx);
            msgQueue.push("ERROR:Failed to create room");
        }
    }

    void JoinRoom(const std::string& code) {
        httplib::Result res;
        {
            std::lock_guard<std::mutex> lock(httpMtx);
            res = cli->Post("/join?code=" + code, "", "text/plain");
        }
        // FIX: Only set roomCode and role AFTER the server confirms the join
        if (res && res->status == 200) {
            roomCode = code;
            role = "client";
            isHost = false;
            
            std::lock_guard<std::mutex> lock(mtx);
            std::istringstream stream(res->body);
            std::string line;
            while (std::getline(stream, line)) {
                if (!line.empty() && line.back() == '\r') line.pop_back();
                if (!line.empty()) {
                    if (msgQueue.size() >= 100) msgQueue.pop();
                    msgQueue.push(line);
                }
            }
        } else {
            std::lock_guard<std::mutex> lock(mtx);
            msgQueue.push("ERROR:Failed to join room. Check code.");
        }
    }

    void SendData(const std::string& data) {
        if (!roomCode.empty()) {
            std::lock_guard<std::mutex> lock(mtx);
            if (sendQueue.size() >= 5) sendQueue.pop(); 
            sendQueue.push(data);
        }
    }

    void GetMessages(std::vector<std::string>& out) {
        std::lock_guard<std::mutex> lock(mtx);
        while (!msgQueue.empty()) {
            out.push_back(msgQueue.front());
            msgQueue.pop();
        }
    }

    void Close() {
        running = false;
        if (netThread.joinable()) netThread.join();
    }
};

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
    Vector2 targetPos = {0, 0}; 
    float speed = 200.0f;
    float radius = 15.0f;
    Color color = WHITE;
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
    static std::mt19937 gen(std::chrono::steady_clock::now().time_since_epoch().count());
    std::uniform_int_distribution<> dist(0, 35);
    for (int i = 0; i < 4; ++i) code += chars[dist(gen)];
    return code;
}

int main() {
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Obscura - True Hide & Seek");
    SetTargetFPS(60);

    NetworkManager net;
    net.Init();

    GameState state = GameState::USERNAME_INPUT;
    GameMap map;
    Player localPlayer, opponentPlayer;
    
    std::string username = "", roomCodeInput = "", hostRoomCode = "", statusMessage = "";
    int headStartTimer = 10; 
    int seekingTime = 120;   
    
    float currentHeadStart = 0.0f;
    float phaseTimer = 0.0f;
    float revealTimer = 3.0f;
    float netSendTimer = 0.0f; 
    
    localPlayer.color = BLUE; 
    opponentPlayer.color = RED;

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        std::vector<std::string> messages;
        net.GetMessages(messages);

        for (const std::string& msg : messages) {
            if (msg.substr(0, 5) == "ROLE:") net.myRole = std::stoi(msg.substr(5));
            else if (msg.substr(0, 10) == "HEADSTART:") currentHeadStart = static_cast<float>(std::stoi(msg.substr(10)));
            else if (msg.substr(0, 6) == "TIMER:") phaseTimer = static_cast<float>(std::stoi(msg.substr(6)));
            else if (msg == "START") { map.Generate(); state = GameState::ROLE_REVEAL; revealTimer = 3.0f; }
            else if (msg.substr(0, 6) == "ERROR:") { statusMessage = "Error: " + msg.substr(6); state = (net.isHost) ? GameState::HOST_SETUP : GameState::JOIN_SETUP; }
            else if (msg == "HOST_DISCONNECTED" || msg == "CLIENT_DISCONNECTED") { statusMessage = "Opponent disconnected!"; state = GameState::MAIN_HUB; }
            else if (msg.find(',') != std::string::npos && msg.find('{') == std::string::npos) {
                size_t comma = msg.find(',');
                try {
                    float x = std::stof(msg.substr(0, comma));
                    float y = std::stof(msg.substr(comma + 1));
                    opponentPlayer.targetPos = {x, y}; 
                } catch (...) {}
            }
        }

        float lerpSpeed = 10.0f * dt;
        opponentPlayer.position.x += (opponentPlayer.targetPos.x - opponentPlayer.position.x) * lerpSpeed;
        opponentPlayer.position.y += (opponentPlayer.targetPos.y - opponentPlayer.position.y) * lerpSpeed;

        if (state == GameState::USERNAME_INPUT) {
            DrawText("ONLINE MODE", SCREEN_WIDTH/2 - 120, 150, 40, GREEN);
            DrawText("Enter Username:", SCREEN_WIDTH/2 - 100, 200, 30, WHITE);
            DrawText(username.c_str(), SCREEN_WIDTH/2 - 100, 250, 30, GREEN);
            std::string input = GetTextInput(15);
            if (!input.empty()) username += input;
            if (IsKeyPressed(KEY_BACKSPACE) && !username.empty()) username.pop_back();
            if (IsKeyPressed(KEY_ENTER) && !username.empty()) { net.myName = username; state = GameState::MAIN_HUB; }
        }
        else if (state == GameState::MAIN_HUB) {
            DrawText("MAIN HUB", SCREEN_WIDTH/2 - 100, 150, 50, WHITE);
            DrawRectangle(SCREEN_WIDTH/2 - 150, 300, 300, 60, DARKGREEN); DrawText("HOST GAME", SCREEN_WIDTH/2 - 90, 320, 30, WHITE);
            DrawRectangle(SCREEN_WIDTH/2 - 150, 400, 300, 60, DARKBLUE); DrawText("JOIN GAME", SCREEN_WIDTH/2 - 90, 420, 30, WHITE);
            Vector2 mouse = GetMousePosition();
            if (mouse.x > SCREEN_WIDTH/2 - 150 && mouse.x < SCREEN_WIDTH/2 + 150) {
                if (mouse.y > 300 && mouse.y < 360 && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { hostRoomCode = GenerateRoomCode(); state = GameState::HOST_SETUP; }
                if (mouse.y > 400 && mouse.y < 460 && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) state = GameState::JOIN_SETUP;
            }
        }
        else if (state == GameState::HOST_SETUP) {
            DrawText("HOST GAME", SCREEN_WIDTH/2 - 120, 100, 40, WHITE);
            DrawText(TextFormat("Room Code: %s", hostRoomCode.c_str()), SCREEN_WIDTH/2 - 100, 160, 50, YELLOW);
            DrawText(TextFormat("Head Start: %d sec (LEFT/RIGHT)", headStartTimer), SCREEN_WIDTH/2 - 180, 240, 20, GREEN);
            if (IsKeyPressed(KEY_LEFT)) headStartTimer = std::max(0, headStartTimer - 5);
            if (IsKeyPressed(KEY_RIGHT)) headStartTimer = std::min(60, headStartTimer + 5);
            DrawText(TextFormat("Seeking Time: %d sec (UP/DOWN)", seekingTime), SCREEN_WIDTH/2 - 180, 280, 20, YELLOW);
            if (IsKeyPressed(KEY_UP)) seekingTime = std::min(300, seekingTime + 10);
            if (IsKeyPressed(KEY_DOWN)) seekingTime = std::max(30, seekingTime - 10);
            DrawRectangle(SCREEN_WIDTH/2 - 100, 330, 200, 50, GREEN); DrawText("START WAITING", SCREEN_WIDTH/2 - 90, 345, 20, WHITE);
            Vector2 mouse = GetMousePosition();
            if (mouse.x > SCREEN_WIDTH/2 - 100 && mouse.x < SCREEN_WIDTH/2 + 100 && mouse.y > 330 && mouse.y < 380 && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                net.CreateRoom(hostRoomCode, seekingTime, headStartTimer); 
                state = GameState::WAITING_ROOM; 
                statusMessage = "Waiting for player...";
            }
            DrawText(statusMessage.c_str(), SCREEN_WIDTH/2 - 150, 420, 20, RED);
            if (IsKeyPressed(KEY_ESCAPE)) state = GameState::MAIN_HUB;
        }
        else if (state == GameState::JOIN_SETUP) {
            DrawText("JOIN GAME", SCREEN_WIDTH/2 - 120, 150, 40, WHITE);
            DrawText("Enter Room Code:", SCREEN_WIDTH/2 - 100, 220, 20, WHITE);
            std::string input = GetTextInput(4);
            for (char& c : input) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            if (!input.empty()) roomCodeInput += input;
            if (IsKeyPressed(KEY_BACKSPACE) && !roomCodeInput.empty()) roomCodeInput.pop_back();
            if (roomCodeInput.length() > 4) roomCodeInput = roomCodeInput.substr(0, 4);
            DrawText(roomCodeInput.c_str(), SCREEN_WIDTH/2 - 50, 260, 50, GREEN);
            if (IsKeyPressed(KEY_ENTER) && roomCodeInput.length() == 4) { 
                net.JoinRoom(roomCodeInput); 
                state = GameState::WAITING_ROOM; 
                statusMessage = "Joining room..."; 
            }
            DrawText(statusMessage.c_str(), SCREEN_WIDTH/2 - 150, 350, 20, RED);
            if (IsKeyPressed(KEY_ESCAPE)) state = GameState::MAIN_HUB;
        }
        else if (state == GameState::WAITING_ROOM) {
            DrawText("WAITING ROOM", SCREEN_WIDTH/2 - 120, 150, 40, WHITE);
            DrawText(statusMessage.c_str(), SCREEN_WIDTH/2 - 200, 250, 20, YELLOW);
            if (IsKeyPressed(KEY_ESCAPE)) state = GameState::MAIN_HUB;
        }
        else if (state == GameState::ROLE_REVEAL) {
            revealTimer -= dt;
            const char* roleText = (net.myRole == 0) ? "YOU ARE THE HIDER (BLUE)" : "YOU ARE THE HUNTER (RED)";
            Color roleColor = (net.myRole == 0) ? BLUE : RED;
            DrawText(roleText, SCREEN_WIDTH/2 - 250, SCREEN_HEIGHT/2 - 50, 50, roleColor);
            DrawText("The Hider is INVISIBLE to the Hunter!", SCREEN_WIDTH/2 - 220, SCREEN_HEIGHT/2 + 20, 20, GRAY);
            DrawText(TextFormat("Starts in: %.1f", revealTimer), SCREEN_WIDTH/2 - 100, SCREEN_HEIGHT/2 + 60, 30, WHITE);
            if (revealTimer <= 0) { 
                state = GameState::HIDING_PHASE; 
                localPlayer.position = {100, 100}; 
                opponentPlayer.position = {1180, 620}; 
                opponentPlayer.targetPos = {1180, 620}; 
            }
        }
        else if (state == GameState::HIDING_PHASE || state == GameState::SEEKING_PHASE) {
            if (currentHeadStart > 0) {
                currentHeadStart -= dt;
                if (currentHeadStart < 0) currentHeadStart = 0;
                
                if (net.myRole == 0) {
                    Vector2 move = {0, 0};
                    if (IsKeyDown(KEY_W)) move.y -= 1;
                    if (IsKeyDown(KEY_S)) move.y += 1;
                    if (IsKeyDown(KEY_A)) move.x -= 1;
                    if (IsKeyDown(KEY_D)) move.x += 1;
                    if (move.x != 0 || move.y != 0) {
                        float len = sqrtf(move.x * move.x + move.y * move.y); 
                        move.x /= len; move.y /= len;
                        Vector2 newPos = {localPlayer.position.x + move.x * localPlayer.speed * dt, localPlayer.position.y + move.y * localPlayer.speed * dt};
                        if (!CheckCollision(localPlayer, map, newPos)) localPlayer.position = newPos;
                    }
                }
                DrawText(TextFormat("HEAD START: %.1f", currentHeadStart), SCREEN_WIDTH/2 - 100, 50, 40, YELLOW);
                if (net.myRole == 1) DrawText("WAIT FOR THE HIDER...", SCREEN_WIDTH/2 - 150, 100, 20, RED);
                else DrawText("RUN AND HIDE!", SCREEN_WIDTH/2 - 100, 100, 20, BLUE);
            } 
            else {
                phaseTimer -= dt;
                Vector2 move = {0, 0};
                if (IsKeyDown(KEY_W)) move.y -= 1;
                if (IsKeyDown(KEY_S)) move.y += 1;
                if (IsKeyDown(KEY_A)) move.x -= 1;
                if (IsKeyDown(KEY_D)) move.x += 1;
                
                if ((IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_E)) && localPlayer.cheatCooldown <= 0) { 
                    localPlayer.cheatActive = true; localPlayer.speed = 400.0f; localPlayer.cheatCooldown = 8.0f; 
                }
                if (localPlayer.cheatActive) { 
                    localPlayer.cheatCooldown -= dt; 
                    if (localPlayer.cheatCooldown <= 5.0f) { localPlayer.cheatActive = false; localPlayer.speed = 200.0f; } 
                } else if (localPlayer.cheatCooldown > 0) localPlayer.cheatCooldown -= dt;
                
                if (move.x != 0 || move.y != 0) {
                    float len = sqrtf(move.x * move.x + move.y * move.y); 
                    move.x /= len; move.y /= len;
                    Vector2 newPos = {localPlayer.position.x + move.x * localPlayer.speed * dt, localPlayer.position.y + move.y * localPlayer.speed * dt};
                    if (!CheckCollision(localPlayer, map, newPos)) localPlayer.position = newPos;
                }
                
                if (net.myRole == 1) { 
                    float dx = localPlayer.position.x - opponentPlayer.position.x;
                    float dy = localPlayer.position.y - opponentPlayer.position.y;
                    float dist = sqrtf(dx * dx + dy * dy);
                    if (dist < 30.0f) { state = GameState::GAME_OVER; statusMessage = "HUNTER WINS! CAUGHT THE HIDER!"; }
                }
                
                if (phaseTimer <= 0) { state = GameState::GAME_OVER; statusMessage = "HIDER WINS! SURVIVED!"; }
            }
            
            netSendTimer += dt;
            if (netSendTimer >= 0.2f) {
                std::string posMsg = std::to_string(localPlayer.position.x) + "," + std::to_string(localPlayer.position.y);
                net.SendData(posMsg);
                netSendTimer = 0.0f;
            }
            if (IsKeyPressed(KEY_ESCAPE)) state = GameState::PAUSED;
        }
        else if (state == GameState::PAUSED) {
            DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, ColorAlpha(BLACK, 0.8f));
            DrawText("PAUSED", SCREEN_WIDTH/2 - 80, SCREEN_HEIGHT/2 - 50, 40, WHITE);
            if (IsKeyPressed(KEY_ESCAPE)) state = GameState::HIDING_PHASE;
            if (IsKeyPressed(KEY_Q)) state = GameState::MAIN_HUB;
        }
        else if (state == GameState::GAME_OVER) {
            DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, ColorAlpha(BLACK, 0.85f));
            DrawText(statusMessage.c_str(), SCREEN_WIDTH/2 - 250, SCREEN_HEIGHT/2 - 30, 40, GOLD);
            DrawText("Press ENTER for Main Menu", SCREEN_WIDTH/2 - 150, SCREEN_HEIGHT/2 + 50, 20, WHITE);
            if (IsKeyPressed(KEY_ENTER)) state = GameState::MAIN_HUB;
        }

        BeginDrawing();
        ClearBackground(BLACK);
        
        if (state >= GameState::HIDING_PHASE) {
            if (!map.walls.empty()) {
                for (int r = 0; r < MAP_ROWS; ++r) {
                    for (int c = 0; c < MAP_COLS; ++c) {
                        if (map.walls[r][c]) DrawRectangle(c * TILE_SIZE, r * TILE_SIZE, TILE_SIZE, TILE_SIZE, DARKGRAY);
                    }
                }
            }
            
            if (net.myRole == 0) { 
                DrawCircleV(localPlayer.position, localPlayer.radius, BLUE);
                DrawCircleV(opponentPlayer.position, 15, RED); 
            } else { 
                DrawCircleV(localPlayer.position, 15, RED);
            }
            
            if (currentHeadStart <= 0) {
                DrawText(TextFormat("Time: %.1f", phaseTimer), 10, 10, 20, YELLOW);
                if (net.myRole == 0 && localPlayer.cheatCooldown > 0) {
                    DrawText(TextFormat("Sprint CD: %.1f", localPlayer.cheatCooldown), 10, 40, 20, localPlayer.cheatActive ? GREEN : GRAY);
                }
            }
        }
        EndDrawing();
    }
    net.Close();
    CloseWindow();
    return 0;
}
