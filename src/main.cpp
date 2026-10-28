#include "raylib.h"
#include <vector>
#include <random>
#include <string>
#include <cmath>
#include <algorithm>

// ==================== CONSTANTS ====================
const int SCREEN_WIDTH = 1280;
const int SCREEN_HEIGHT = 720;
const int TILE_SIZE = 40;
const int MAP_COLS = SCREEN_WIDTH / TILE_SIZE;
const int MAP_ROWS = SCREEN_HEIGHT / TILE_SIZE;

// ==================== GAME STATES ====================
enum class GameState {
    MAIN_MENU,
    LOBBY,
    HIDING_PHASE,
    SEEKING_PHASE,
    PAUSED,
    GAME_OVER
};

enum class Winner { NONE, HUNTER, HIDER };

// ==================== PLAYER STRUCT ====================
struct Player {
    Vector2 position = {0, 0};
    float speed = 200.0f;
    float radius = 15.0f;
    Color color = WHITE;
    bool isVisible = true;
    float cheatCooldown = 0.0f;
    bool cheatActive = false;
};

// ==================== MAP GENERATOR ====================
struct GameMap {
    std::vector<std::vector<bool>> walls;
    
    void Generate() {
        walls.assign(MAP_ROWS, std::vector<bool>(MAP_COLS, false));
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dist(0, 100);
        
        // Create borders
        for (int r = 0; r < MAP_ROWS; ++r) {
            for (int c = 0; c < MAP_COLS; ++c) {
                if (r == 0 || r == MAP_ROWS - 1 || c == 0 || c == MAP_COLS - 1) {
                    walls[r][c] = true;
                } else if (dist(gen) < 20) {
                    walls[r][c] = true;
                }
            }
        }
        
        // Clear spawn areas
        for (int r = 1; r < 5; ++r)
            for (int c = 1; c < 5; ++c)
                walls[r][c] = false;
        
        for (int r = MAP_ROWS - 5; r < MAP_ROWS - 1; ++r)
            for (int c = MAP_COLS - 5; c < MAP_COLS - 1; ++c)
                walls[r][c] = false;
    }
    
    bool IsWall(float x, float y) const {
        int col = static_cast<int>(x) / TILE_SIZE;
        int row = static_cast<int>(y) / TILE_SIZE;
        if (row < 0 || row >= MAP_ROWS || col < 0 || col >= MAP_COLS) return true;
        return walls[row][col];
    }
};

// ==================== COLLISION DETECTION ====================
bool CheckCollisionWithWalls(const Player& player, const GameMap& map,
