# Obscura

A fast-paced, online 1v1 Hide and Seek game built in high-performance C++, running natively Windows, Linux and MacOS.

## 🎮 Game Overview
In **Obscura**, one player takes on the role of the **Hunter** and the other is the **Hider**. Every match takes place in a large, randomly generated map, ensuring no two games are exactly the same. Built from the ground up in C++, the game guarantees smooth frame rates and precise, responsive controls.

## 🌟 Core Features
- **High-Performance C++ Engine:** Built with modern C++ and CMake, ensuring maximum performance and a strict, zero-error build process.
- **Robust Online Support:** Advanced netcode and lag-compensation provide seamless, low-latency multiplayer between Windows and Linux clients.
- **Complete UI/UX System:** 
  - **Main Menu:** Clean hub to Host, Join, or view Credits.
  - **Pause Menu:** Quick access to resume, settings, or quit during a match.
  - **Settings Menu:** Fully customizable graphics, audio, keybindings, and network options.
- **Procedurally Generated Maps:** Large, unique mazes and arenas generated randomly for every match.
- **Prematch Setup:** Customize the game in the lobby, including setting the Hunter's countdown timer (default: 60 seconds).
- **Hider Cheat Tools:** The Hider has access to special abilities (e.g., temporary invisibility, speed boosts) to evade the Hunter.

## 🕹️ Controls
- **Movement:** `WASD` or `Arrow Keys`
- **Use Cheat Tool:** `Spacebar` or `E`
- **Interact / Confirm:** `Enter`
- **Pause Game:** `Esc`
> *Note: All controls are fully remappable in the in-game Settings menu!*

## 🛠️ How to Play
1. **Main Menu:** Launch the game and select **Host** or **Join** from the Main Menu.
2. **Prematch Setup:** Both players agree on the Hunter's countdown time and other lobby settings.
3. **Hiding Phase:** The Hunter closes their eyes (countdown starts). The Hider uses this time to find a spot and use their cheat tools if necessary.
4. **Seeking Phase:** Once the countdown reaches zero, the Hunter begins searching.
5. **Win Condition:** The Hunter wins by catching the Hider. The Hider wins by surviving until the time limit expires.

## ⚙️ Building & CI/CD Workflow
Obscura is designed to compile flawlessly. The repository includes a robust **CMake** configuration and a GitHub Actions workflow (`.github/workflows/build.yml`) that guarantees **zero errors** during the build process.

### Local Build Instructions (Zero-Error Guarantee)
To ensure no errors when building locally, ensure you have the following prerequisites installed:
- **C++20 Compiler:** MSVC (Windows) or GCC/Clang (Linux)
- **CMake:** Version 3.20 or higher
- **Dependencies:** Managed automatically via CMake's `FetchContent` or `vcpkg`.

**Build Commands:**
