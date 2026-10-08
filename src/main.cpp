#include "raylib.h"
#include "Game.h"

int main() {
    SetTraceLogLevel(LOG_NONE);
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT);
    InitWindow(800, 640, "Thrust - BBC Micro Recreation");
    SetExitKey(KEY_NULL);

    int monitor = GetCurrentMonitor();
    int monW = GetMonitorWidth(monitor);
    int monH = GetMonitorHeight(monitor);
    if (monH > 0 && monW > 0) {
        int winH = monH / 2;
        int winW = (winH * 5) / 4; // 5:4 aspect ratio matching 320x256 logical viewport
        SetWindowSize(winW, winH);
        SetWindowPosition((monW - winW) / 2, (monH - winH) / 2);
    }

    // Default to fullscreen on launch; F11 swaps between fullscreen and the centered windowed size above
    ToggleBorderlessWindowed();
    HideCursor();

    // Rely on hardware VSync (with 240 FPS fallback cap if VSync is forced off in GPU driver)
    // so software Sleep() never oversleeps past a VSync interval and causes scroll stutter.
    SetTargetFPS(240);

    Game game;
    game.Init();

    while (!WindowShouldClose() && !game.ShouldQuit()) {
        if (IsKeyPressed(KEY_F11)) {
            ToggleBorderlessWindowed();
        }

        bool inFullscreen = IsWindowState(FLAG_BORDERLESS_WINDOWED_MODE) || IsWindowFullscreen();
        if (inFullscreen && !IsCursorHidden()) {
            HideCursor();
        } else if (!inFullscreen && IsCursorHidden()) {
            ShowCursor();
        }

        float dt = GetFrameTime();
        game.Update(dt);
        game.Draw(GetScreenWidth(), GetScreenHeight());
    }

    if (IsCursorHidden()) {
        ShowCursor();
    }
    CloseWindow();
    return 0;
}
