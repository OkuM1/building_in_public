#pragma once

struct Player {
    int x, y; // Grid position
    int hp;
    Player(int startX, int startY, int startHP) : x(startX), y(startY), hp(startHP) {}
};
