#pragma once
#include <thread>
#include <atomic>
#include <queue>
#include <mutex>

namespace obscura::network {
    class NetworkManager {
    public:
        NetworkManager() : m_isRunning(false) {}
        ~NetworkManager() { m_isRunning = false; }

        bool Host(uint16_t port) { return true; }
        bool Join(const std::string& ip, uint16_t port) { return true; }

    private:
        std::jthread m_receiveThread;
        std::atomic<bool> m_isRunning;
        std::queue<std::vector<uint8_t>> m_packetQueue;
        std::mutex m_queueMutex;
    };
}
