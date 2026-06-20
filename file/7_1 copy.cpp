#include <iostream>
#include <fstream>
#include <vector>
#include <map>
#include <string>
#include <cmath>     // sqrt, sin, cos, atan2, hypot
#include <algorithm> // max, min, shuffle
#include <cstdlib>   // srand, rand
#include <ctime>     // time
#include <random>

using namespace std;

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_mixer.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Configuration */
int width = 1024;
int height = 768;
const int board_left = 50;
const int board_top = 100;
const int CELL = 60; // 60*60 pixel
const int GRID = 10;
const int BOARD_SIZE = CELL * GRID;
const int BOARD_X = board_left;
const int BOARD_Y = board_top;

/* Global Resources */
SDL_Window *window = nullptr;
SDL_Renderer *renderer = nullptr; // rendering engine for drawing graphics

// Fonts (4-tier)
TTF_Font *small_font = nullptr;
TTF_Font *medium_font = nullptr;
game.players[i].color, medium_fo