#include "raylib.h"
#include <vector>
#include <cmath>
#include <algorithm>
#include <random>

const int SCREEN_WIDTH = 1280;
const int SCREEN_HEIGHT = 720;
const int TILE_SIZE = 40;
const int MAP_COLS = SCREEN_WIDTH / TILE_SIZE;
const int MAP_ROWS = SCREEN_HEIGHT / TILE_SIZE;

enum class GameState { MAIN_MENU, LOBBY, HIDING_PHASE, SEEKING_PHASE, PAUSED, GAME_OVER };
enum class Winner { NONE, HUNTER, HIDER };

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
            for (int c = 1; c < 5; ++c) {
                walls[r][c] = false;
            }
        }
        for (int r = MAP_ROWS - 5; r < MAP_ROWS - 1; ++r) {
            for (int c = MAP_COLS - 5; c < MAP_COLS - 1; ++c) {
                walls[r][c] = false;
            }
        }
    }
    
    bool IsWall(float x, float y) const {
        int col = static_cast<int>(x) / TILE_SIZE;
        int row = static_cast<int>(y) / TILE_SIZE;
        if (row < 0 || row >= MAP_ROWS || col < 0 || col >= MAP_COLS) return true;
        return walls[row][col];
    }
};

bool CheckCollisionWithWalls(const Player& player, const GameMap& map, Vector2 newPos) {
    float offsets[] = {-player.radius, player.radius};
    for (float ox : offsets) {
        for (float oy : offsets) {
            if (map.IsWall(newPos.x + ox, newPos.y + oy)) return true;
        }
    }
    return false;
}

float Distance(const Player& a, const Player& b) {
    float dx = a.position.x - b.position.x;
    float dy = a.position.y - b.position.y;
    return sqrtf(dx * dx + dy * dy);
}

int main() {
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Obscura - 1v1 Hide & Seek");
    SetTargetFPS(60);
    
    GameState state = GameState::MAIN_MENU;
    GameMap map;
    Player hider, hunter;
    Winner winner = Winner::NONE;
    
    float lobbyTimer = 60.0f;
    float phaseTimer = 0.0f;
    float totalGameTime = 120.0f;
    
    hider.position = {100, 100}; 
    hider.color = BLUE;
    hunter.position = {SCREEN_WIDTH - 100, SCREEN_HEIGHT - 100}; 
    hunter.color = RED;
    
    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        
        if (state == GameState::MAIN_MENU) {
            if (IsKeyPressed(KEY_ENTER)) { 
                map.Generate(); 
                state = GameState::LOBBY; 
            }
        } 
        else if (state == GameState::LOBBY) {
            if (IsKeyDown(KEY_UP)) lobbyTimer = std::min(180.0f, lobbyTimer + dt * 10);
            if (IsKeyDown(KEY_DOWN)) lobbyTimer = std::max(10.0f, lobbyTimer - dt * 10);
            if (IsKeyPressed(KEY_ENTER)) { 
                phaseTimer = lobbyTimer; 
                state = GameState::HIDING_PHASE; 
            }
            if (IsKeyPressed(KEY_ESCAPE)) state = GameState::MAIN_MENU;
        } 
        else if (state == GameState::HIDING_PHASE || state == GameState::SEEKING_PHASE) {
            phaseTimer -= dt;
            
            Vector2 hiderMove = {0, 0};
            if (IsKeyDown(KEY_W)) hiderMove.y -= 1;
            if (IsKeyDown(KEY_S)) hiderMove.y += 1;
            if (IsKeyDown(KEY_A)) hiderMove.x -= 1;
            if (IsKeyDown(KEY_D)) hiderMove.x += 1;
            
            if ((IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_E)) && hider.cheatCooldown <= 0) {
                hider.cheatActive = true; 
                hider.isVisible = false; 
                hider.speed = 350.0f; 
                hider.cheatCooldown = 8.0f;
            }
            if (hider.cheatActive) {
                hider.cheatCooldown -= dt;
                if (hider.cheatCooldown <= 5.0f) { 
                    hider.cheatActive = false; 
                    hider.isVisible = true; 
                    hider.speed = 200.0f; 
                }
            } else if (hider.cheatCooldown > 0) { 
                hider.cheatCooldown -= dt; 
            }
            
            if (hiderMove.x != 0 || hiderMove.y != 0) {
                float len = sqrtf(hiderMove.x * hiderMove.x + hiderMove.y * hiderMove.y);
                hiderMove.x /= len; 
                hiderMove.y /= len;
                Vector2 newPos = {hider.position.x + hiderMove.x * hider.speed * dt, hider.position.y + hiderMove.y * hider.speed * dt};
                if (!CheckCollisionWithWalls(hider, map, newPos)) hider.position = newPos;
            }
            
            if (state == GameState::SEEKING_PHASE) {
                Vector2 hunterMove = {0, 0};
                if (IsKeyDown(KEY_UP)) hunterMove.y -= 1;
                if (IsKeyDown(KEY_DOWN)) hunterMove.y += 1;
                if (IsKeyDown(KEY_LEFT)) hunterMove.x -= 1;
                if (IsKeyDown(KEY_RIGHT)) hunterMove.x += 1;
                
                if (hunterMove.x != 0 || hunterMove.y != 0) {
                    float len = sqrtf(hunterMove.x * hunterMove.x + hunterMove.y * hunterMove.y);
                    hunterMove.x /= len; 
                    hunterMove.y /= len;
                    Vector2 newPos = {hunter.position.x + hunterMove.x * hunter.speed * dt, hunter.position.y + hunterMove.y * hunter.speed * dt};
                    if (!CheckCollisionWithWalls(hunter, map, newPos)) hunter.position = newPos;
                }
                
                if (!hider.cheatActive && Distance(hider, hunter) < (hider.radius + hunter.radius)) { 
                    winner = Winner::HUNTER; 
                    state = GameState::GAME_OVER; 
                }
                if (phaseTimer <= 0) { 
                    winner = Winner::HIDER; 
                    state = GameState::GAME_OVER; 
                }
            } else {
                if (phaseTimer <= 0) { 
                    state = GameState::SEEKING_PHASE; 
                    phaseTimer = totalGameTime; 
                }
            }
            
            if (IsKeyPressed(KEY_ESCAPE)) state = GameState::PAUSED;
        } 
        else if (state == GameState::PAUSED) {
            if (IsKeyPressed(KEY_ESCAPE)) {
                if (phaseTimer < totalGameTime) state = GameState::SEEKING_PHASE;
                else state = GameState::HIDING_PHASE;
            }
            if (IsKeyPressed(KEY_Q)) state = GameState::MAIN_MENU;
        } 
        else if (state == GameState::GAME_OVER) {
            if (IsKeyPressed(KEY_ENTER)) { 
                state = GameState::MAIN_MENU; 
                winner = Winner::NONE; 
                hider.position = {100, 100}; 
                hunter.position = {SCREEN_WIDTH - 100, SCREEN_HEIGHT - 100}; 
            }
        }

        BeginDrawing();
        ClearBackground(BLACK);
        
        // FIX: Only draw the map if it has actually been generated!
        if (!map.walls.empty()) {
            for (int r = 0; r < MAP_ROWS; ++r) {
                for (int c = 0; c < MAP_COLS; ++c) {
                    if (map.walls[r][c]) DrawRectangle(c * TILE_SIZE, r * TILE_SIZE, TILE_SIZE, TILE_SIZE, DARKGRAY);
                }
            }
        }
        
        if (state == GameState::MAIN_MENU) {
            DrawText("OBS CURA", SCREEN_WIDTH/2 - 180, 200, 80, WHITE);
            DrawText("1v1 Hide and Seek", SCREEN_WIDTH/2 - 140, 300, 30, GRAY);
            DrawText("Press ENTER to Start", SCREEN_WIDTH/2 - 140, 450, 30, GREEN);
        } 
        else if (state == GameState::LOBBY) {
            DrawText("PREMATCH LOBBY", SCREEN_WIDTH/2 - 160, 150, 50, WHITE);
            DrawText(TextFormat("Hunter Countdown: %.0f seconds", lobbyTimer), SCREEN_WIDTH/2 - 200, 250, 30, YELLOW);
            DrawText("UP/DOWN to adjust timer", SCREEN_WIDTH/2 - 150, 320, 20, GRAY);
            DrawText("Press ENTER to start", SCREEN_WIDTH/2 - 120, 360, 20, GREEN);
        } 
        else if (state == GameState::HIDING_PHASE || state == GameState::SEEKING_PHASE) {
            if (hider.isVisible || state == GameState::HIDING_PHASE) {
                DrawCircleV(hider.position, hider.radius, hider.cheatActive ? ColorAlpha(BLUE, 0.3f) : BLUE);
            }
            DrawCircleV(hunter.position, hunter.radius, RED);
            
            DrawText(TextFormat("Phase: %s", state == GameState::HIDING_PHASE ? "HIDING" : "SEEKING"), 10, 10, 20, WHITE);
            DrawText(TextFormat("Time: %.1f", phaseTimer), 10, 40, 20, YELLOW);
            DrawText(TextFormat("Cheat CD: %.1f", hider.cheatCooldown), 10, 70, 20, hider.cheatActive ? GREEN : GRAY);
            DrawText("Hider: WASD | Space/E (Cheat)", 10, SCREEN_HEIGHT - 50, 15, DARKGRAY);
            DrawText("Hunter: Arrow Keys", 10, SCREEN_HEIGHT - 30, 15, DARKGRAY);
        } 
        else if (state == GameState::PAUSED) {
            DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, ColorAlpha(BLACK, 0.8f));
            DrawText("PAUSED", SCREEN_WIDTH/2 - 80, SCREEN_HEIGHT/2 - 50, 40, WHITE);
            DrawText("ESC to Resume", SCREEN_WIDTH/2 - 100, SCREEN_HEIGHT/2 + 20, 20, GRAY);
            DrawText("Q to Quit to Menu", SCREEN_WIDTH/2 - 110, SCREEN_HEIGHT/2 + 50, 20, RED);
        } 
        else if (state == GameState::GAME_OVER) {
            DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, ColorAlpha(BLACK, 0.85f));
            const char* winnerText = (winner == Winner::HUNTER) ? "HUNTER WINS!" : "HIDER WINS!";
            Color winnerColor = (winner == Winner::HUNTER) ? RED : BLUE;
            DrawText(winnerText, SCREEN_WIDTH/2 - 150, SCREEN_HEIGHT/2 - 30, 50, winnerColor);
            DrawText("Press ENTER for Main Menu", SCREEN_WIDTH/2 - 150, SCREEN_HEIGHT/2 + 50, 20, WHITE);
        }
        
        EndDrawing();
    }
    
    CloseWindow();
    return 0;
}
