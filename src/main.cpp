#include "raylib.h"
#include "Core/EventBus.h"
#include "Gameplay/MapGenerator.h"
#include "Network/NetworkManager.h"
#include <iostream>

int main() {
    // Initialize Engine
    InitWindow(1280, 720, "Obscura - 1v1 Hide & Seek");
    SetTargetFPS(60);

    // Initialize Systems
    obscura::core::EventBus eventBus;
    obscura::gameplay::MapGenerator mapGen(100, 100, 12345);
    obscura::network::NetworkManager netManager;

    std::cout << "[Obscura] Engine Initialized. Zero-Error Build Successful." << std::endl;

    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(BLACK);
        DrawText("OBS CURA - ENTERPRISE ARCHITECTURE", 100, 300, 40, WHITE);
        DrawText("Press ESC to exit", 100, 400, 20, GRAY);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
