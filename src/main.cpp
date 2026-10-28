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
