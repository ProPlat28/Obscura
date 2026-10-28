#pragma once
#include <vector>
#include <random>

namespace obscura::gameplay {
    class MapGenerator {
    public:
        MapGenerator(int width, int height, unsigned int seed) 
            : m_width(width), m_height(height), m_rng(seed) {}
        
        void GenerateBSPMaze() {
            // BSP Tree Algorithm Implementation goes here
        }
    private:
        int m_width, m_height;
        std::mt19937 m_rng;
    };
}
