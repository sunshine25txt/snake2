// snadder_full_dynamic_v2.cpp
// Fixed Dynamic Rules, "Blur" (Dim) overlay, and Question popup isolation.

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

// SDL2 Libraries
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_mixer.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace std;

/* Configuration */
int width = 1024;
int height = 768;
const int board_left = 50;
const int board_top = 100;
const int CELL = 60;
const int GRID = 10;
const int BOARD_SIZE = CELL * GRID;
const int BOARD_X = board_left;
const int BOARD_Y = board_top;

/* Global Resources */
SDL_Window *window = nullptr;
SDL_Renderer *renderer = nullptr;

// Fonts
TTF_Font *small_font = nullptr;
TTF_Font *medium_font = nullptr;
TTF_Font *large_font = nullptr;
TTF_Font *gigantic_font = nullptr;

/* Sounds */
Mix_Chunk *snd_roll = nullptr;
Mix_Chunk *snd_move = nullptr;
Mix_Chunk *snd_win = nullptr;
Mix_Chunk *snd_snake = nullptr;
Mix_Chunk *snd_click = nullptr;
Mix_Chunk *snd_error = nullptr;
Mix_Chunk *snd_correct = nullptr;

/* UI State Flags */
bool hover_start = false;
bool hover_rules = false;
bool hover_settings = false;
bool hover_exit = false;
bool hover_easy = false;
bool hover_medium = false;
bool hover_hard = false;
bool hover_back_diff = false;

/* Board Data */
map<int, int> snakes = {
    {16, 6}, {47, 26}, {49, 11}, {56, 53}, {62, 19}, {64, 60}, {87, 24}, {93, 73}, {95, 75}, {98, 78}};

map<int, int> ladders = {
    {2, 37}, {4, 14}, {9, 21}, {39, 57}, {28, 84}, {36, 44}, {51, 67}, {71, 91}, {80, 97}};

/* Question Database */
struct Question
{
   string question;
   vector<string> options;
   int ans_index;
};

enum QuestionState
{
   NO_QUESTION,
   ASKING_QUESTION
};

// Rules Scroll State
int rulesScroll = 0;
int rulesScrollMax = 0;
int rulesScrollStep = 40;

vector<Question> questions;
int g_questionOptionCount = 4; // Default options 4

/* Colors */
struct Colors
{
   SDL_Color bg1{41, 128, 185, 255};
   SDL_Color bg2{155, 89, 182, 255};
   SDL_Color GRID{236, 240, 241, 255};
   SDL_Color cell1{52, 152, 219, 255};
   SDL_Color cell2{155, 89, 182, 255};
   SDL_Color p1{231, 76, 60, 255};
   SDL_Color p2{46, 204, 113, 255};
   SDL_Color gold{241, 196, 15, 255};
   SDL_Color white{255, 255, 255, 255};
   SDL_Color dark{44, 62, 80, 255};
   SDL_Color ladder{211, 84, 0, 255};
   SDL_Color snake{192, 57, 43, 255};
   SDL_Color green{39, 174, 96, 255};
} colors;

struct snake_design
{
   SDL_Color head;
   SDL_Color bodyA;
   SDL_Color bodyB;
};

map<int, snake_design> snake_style = {
    {16, {{200, 30, 30, 255}, {170, 20, 20, 255}, {210, 60, 60, 255}}},
    {47, {{170, 20, 170, 255}, {140, 10, 140, 255}, {190, 60, 190, 255}}},
    {49, {{30, 130, 210, 255}, {20, 100, 170, 255}, {60, 150, 220, 255}}},
    {56, {{220, 30, 30, 255}, {170, 20, 20, 255}, {210, 60, 60, 255}}},
    {62, {{220, 180, 30, 255}, {180, 150, 0, 255}, {235, 200, 60, 255}}},
    {64, {{170, 20, 170, 255}, {140, 10, 140, 255}, {190, 60, 190, 255}}},
    {87, {{30, 170, 30, 255}, {10, 120, 10, 255}, {60, 190, 60, 255}}},
    {93, {{200, 30, 30, 255}, {170, 20, 20, 255}, {210, 60, 60, 255}}},
    {95, {{220, 180, 30, 255}, {180, 150, 0, 255}, {235, 200, 60, 255}}},
    {98, {{220, 180, 30, 255}, {180, 150, 0, 255}, {235, 200, 60, 255}}}};

enum State
{
   WELCOME,
   DIFFICULTY_SELECT,
   NAME_INPUT,
   SETTINGS,
   RULES,
   PLAYING,
   WINNER,
   ABOUT
};

struct Player
{
   int pos = 1;
   SDL_Color color;
   string name;
   int offsetX, offsetY;
};

enum Difficulty
{
   EASY,
   MEDIUM,
   HARD
};

struct Game
{
   State state = WELCOME;
   Player players[2];
   int current = 0;
   Difficulty difficulty = EASY;

   // dice
   int dice = 1;
   bool rolling = false;
   bool moving = false;
   unsigned int rollStart = 0;
   vector<int> move_path;
   unsigned int last_move = 0;

   // name input
   int input_player = 0;
   string tempName = "";

   // UI State
   bool cursor_on_dice = false;
   unsigned int dice_anime_start = 0;
   bool dice_animating = false;
   int cursor_on_cell = 0;
   int cell_preview = 0;
   bool show_hints = true;
   float animation_time = 0.0f;

   // Question System
   QuestionState questionState = NO_QUESTION;
   int curr_question_index = 0;
   bool landed_special_cell = false;
   int special_cell = 0;
   bool isSnake = false;
   char select_ans = 0;
   unsigned int questionStartTime = 0;
   int questionTimeLeft = 30;
   bool questionTimeout = false;

   // Settings
   bool soundEnabled = true;
   bool diceSoundEnabled = true;

   // Timers
   unsigned int preGameStartTime = 0;
   bool preGameTimerActive = false;
   int preGameCountdown = 3;
   int questionDefaultTime = 30;
   int preGameDefaultCountdown = 3;

   bool requestQuit = false;
} game;

// Utils
float lerpf(float a, float b, float t) { return a + (b - a) * t; }
bool isPointInRect(int x, int y, SDL_Rect rect)
{
   return x >= rect.x && x < rect.x + rect.w && y >= rect.y && y < rect.y + rect.h;
}

// Drawing Primitives
void rounded_rect_draw(SDL_Rect rect, SDL_Color col, int radius);

void text_draw(const string &text, int x, int y, SDL_Color color, TTF_Font *font = nullptr)
{
   if (!font)
      font = small_font;
   if (!font)
      return;
   SDL_Surface *surf = TTF_RenderUTF8_Blended(font, text.c_str(), color);
   if (!surf)
      return;
   SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer, surf);
   SDL_Rect dst = {x, y, surf->w, surf->h};
   SDL_RenderCopy(renderer, tex, nullptr, &dst);
   SDL_FreeSurface(surf);
   SDL_DestroyTexture(tex);
}

void center_text_draw(const string &text, int x, int y, SDL_Color color, TTF_Font *font = nullptr)
{
   if (!font)
      font = small_font;
   if (!font)
      return;
   int w = 0, h = 0;
   TTF_SizeUTF8(font, text.c_str(), &w, &h);
   text_draw(text, x - w / 2, y - h / 2, color, font);
}

void button_draw(SDL_Rect rect, const string &text, SDL_Color bg, SDL_Color fg,
                 bool cursor_on_button = false, TTF_Font *font = nullptr)
{
   SDL_Color color = bg;
   if (cursor_on_button)
   {
      color.r = min(255, color.r + 40);
      color.g = min(255, color.g + 40);
      color.b = min(255, color.b + 40);
   }
   rounded_rect_draw(rect, color, 12);
   SDL_SetRenderDrawColor(renderer, max(0, color.r - 30), max(0, color.g - 30), max(0, color.b - 30), 200);
   SDL_Rect border = {rect.x + 1, rect.y + 1, rect.w - 2, rect.h - 2};
   SDL_RenderDrawRect(renderer, &border);
   if (!font)
      font = medium_font ? medium_font : small_font;
   center_text_draw(text, rect.x + rect.w / 2, rect.y + rect.h / 2, fg, font);
}

void circle_draw(int cx, int cy, int r, SDL_Color c)
{
   SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, c.a);
   for (int y = -r; y <= r; y++)
   {
      int w = (int)sqrt(r * r - y * y);
      SDL_RenderDrawPoint(renderer, cx - w, cy + y); // Simplified for points
      SDL_RenderDrawPoint(renderer, cx + w, cy + y);
      // To actually draw a circle outline, we need bresenham or trig,
      // but for this code's usage (dice dots), fill or small loops work.
      // Better implementation for outline:
      for (float theta = 0; theta < 2 * M_PI; theta += 0.1f)
      {
         SDL_RenderDrawPoint(renderer, cx + r * cos(theta), cy + r * sin(theta));
      }
   }
}

void filled_circle_draw(int cx, int cy, int r, SDL_Color col)
{
   SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
   SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, col.a);
   for (int dy = -r; dy <= r; ++dy)
   {
      int w = (int)sqrt((float)(r * r - dy * dy));
      SDL_RenderDrawLine(renderer, cx - w, cy + dy, cx + w, cy + dy);
   }
}

void filled_eclipse_draw(int cx, int cy, int rx, int ry, SDL_Color col)
{
   SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
   SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, col.a);
   for (int y = -ry; y <= ry; ++y)
   {
      float t = 1.0f - (float)(y * y) / (float)(ry * ry);
      int w = (int)(rx * sqrt(max(0.0f, t)));
      SDL_RenderDrawLine(renderer, cx - w, cy + y, cx + w, cy + y);
   }
}

void rounded_rect_draw(SDL_Rect rect, SDL_Color col, int radius)
{
   SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
   SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, col.a);
   SDL_Rect body = {rect.x + radius, rect.y, rect.w - 2 * radius, rect.h};
   SDL_RenderFillRect(renderer, &body);
   SDL_Rect left = {rect.x, rect.y + radius, radius, rect.h - 2 * radius};
   SDL_RenderFillRect(renderer, &left);
   SDL_Rect right = {rect.x + rect.w - radius, rect.y + radius, radius, rect.h - 2 * radius};
   SDL_RenderFillRect(renderer, &right);
   for (int w = 0; w < radius; w++)
   {
      for (int h = 0; h < radius; h++)
      {
         if (w * w + h * h <= radius * radius)
         {
            SDL_RenderDrawPoint(renderer, rect.x + radius - w, rect.y + radius - h);
            SDL_RenderDrawPoint(renderer, rect.x + rect.w - radius + w, rect.y + radius - h);
            SDL_RenderDrawPoint(renderer, rect.x + radius - w, rect.y + rect.h - radius + h);
            SDL_RenderDrawPoint(renderer, rect.x + rect.w - radius + w, rect.y + rect.h - radius + h);
         }
      }
   }
}

/* Board Logic */
SDL_Point get_cell_pos(int cell)
{
   int n = cell - 1;
   int row = n / GRID;
   int col = n % GRID;
   int drawRow = GRID - 1 - row;
   if (row % 2 == 1)
      col = GRID - 1 - col;
   int x = board_left + col * CELL;
   int y = board_top + drawRow * CELL;
   return {x, y};
}

int cursor_to_cell(int cx, int cy)
{
   if (cx < board_left || cy < board_top || cx >= board_left + BOARD_SIZE || cy >= board_top + BOARD_SIZE)
      return 0;
   int col = (cx - board_left) / CELL;
   int drawRow = (cy - board_top) / CELL;
   int row = GRID - 1 - drawRow;
   if (row < 0 || row >= GRID)
      return 0;
   int actual_col = (row % 2 == 0) ? col : (GRID - 1 - col);
   int cell = row * GRID + actual_col + 1;
   return (cell >= 1 && cell <= 100) ? cell : 0;
}

/* Question Loading */
bool load_questions_from_csv(const string &path)
{
   ifstream f(path);
   if (!f.is_open())
      return false;
   questions.clear();
   string line;
   while (std::getline(f, line))
   {
      if (line.empty() || line[0] == '#')
         continue;
      vector<string> parts;
      string cur;
      bool inQuote = false;
      for (char ch : line)
      {
         if (ch == '"')
            inQuote = !inQuote;
         else if (ch == ',' && !inQuote)
         {
            parts.push_back(cur);
            cur.clear();
         }
         else
            cur.push_back(ch);
      }
      parts.push_back(cur);
      if (parts.size() < 3)
         continue;
      Question q;
      q.question = parts[0];
      for (size_t i = 1; i < parts.size(); ++i)
      {
         string s = parts[i];
         // simple trim
         s.erase(0, s.find_first_not_of(" \t\r\n\""));
         s.erase(s.find_last_not_of(" \t\r\n\"") + 1);
         if (!s.empty())
            q.options.push_back(s);
      }
      q.ans_index = 0;
      if (q.options.size() >= 2)
         questions.push_back(q);
   }
   f.close();
   return !questions.empty();
}

void load_default_questions()
{
   questions.clear();
   questions.push_back({"Which type of variable is used to store whole numbers in C?", {"int", "float", "double"}, 0});
   questions.push_back({"Which loop is typically used when the number of iterations is known?", {"for", "while", "do"}, 0});
   questions.push_back({"Which function return type indicates that the function does not return a value?", {"void", "int"}, 0});
   questions.push_back({"Which header file is required to use printf function?", {"stdio.h", "math.h", "string.h"}, 0});
   questions.push_back({"Which index number is used to access the first element of an array?", {"0", "1"}, 0});
   questions.push_back({"Which keyword makes a variable constant?", {"const", "static", "volatile"}, 0});
   questions.push_back({"Which keyword exits a switch case?", {"break", "continue"}, 0});
   questions.push_back({"Which function allocates memory dynamically?", {"malloc", "free", "calloc"}, 0});
   questions.push_back({"Which operator is used for equality comparison?", {"==", "=", "!="}, 0});
   questions.push_back({"Which type stores single characters?", {"char", "int"}, 0});
}

pair<vector<string>, int> prepare_options_for_ui(const Question &q, int displayCount)
{
   displayCount = max(2, min(6, displayCount));
   vector<string> candidates;
   for (const auto &s : q.options)
      if (find(candidates.begin(), candidates.end(), s) == candidates.end())
         candidates.push_back(s);

   // Fill with fillers
   if ((int)candidates.size() < displayCount)
   {
      for (const auto &oth : questions)
      {
         if (&oth == &q)
            continue;
         for (const auto &opt : oth.options)
         {
            if ((int)candidates.size() >= displayCount)
               break;
            if (find(candidates.begin(), candidates.end(), opt) == candidates.end())
               candidates.push_back(opt);
         }
         if ((int)candidates.size() >= displayCount)
            break;
      }
   }
   while ((int)candidates.size() < displayCount)
      candidates.push_back("N/A");

   // Ensure correct answer is present
   string correctStr = (q.ans_index >= 0 && q.ans_index < (int)q.options.size()) ? q.options[q.ans_index] : "";

   bool hasCorrect = false;
   for (auto &s : candidates)
      if (s == correctStr)
         hasCorrect = true;

   if (!hasCorrect && !correctStr.empty())
   {
      candidates[0] = correctStr; // Force it in
   }

   // Shuffle
   static std::mt19937 rng((unsigned)time(nullptr));
   shuffle(candidates.begin(), candidates.end(), rng);

   // Find new correct index
   int newAns = -1;
   for (int i = 0; i < (int)candidates.size(); ++i)
   {
      if (candidates[i] == correctStr)
      {
         newAns = i;
         break;
      }
   }
   return {candidates, newAns};
}

/* Rendering Helper: Dim Overlay */
void draw_dim_overlay()
{
   SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
   // Darker overlay to simulate "blur/focus"
   SDL_SetRenderDrawColor(renderer, 20, 20, 30, 200);
   SDL_Rect screen = {0, 0, width, height};
   SDL_RenderFillRect(renderer, &screen);
}

/* SCENES AND DRAWING */

void background_draw()
{
   for (int y = 0; y < height; y++)
   {
      float t = (float)y / height;
      SDL_SetRenderDrawColor(renderer,
                             (Uint8)(colors.bg1.r + (colors.bg2.r - colors.bg1.r) * t),
                             (Uint8)(colors.bg1.g + (colors.bg2.g - colors.bg1.g) * t),
                             (Uint8)(colors.bg1.b + (colors.bg2.b - colors.bg1.b) * t), 255);
      SDL_RenderDrawLine(renderer, 0, y, width, y);
   }
}

void board_draw()
{
   SDL_Rect boardBorder{board_left - 5, board_top - 5, BOARD_SIZE + 10, BOARD_SIZE + 10};
   SDL_SetRenderDrawColor(renderer, 80, 60, 100, 255);
   SDL_RenderFillRect(renderer, &boardBorder);
   SDL_Rect boardRect{board_left, board_top, BOARD_SIZE, BOARD_SIZE};
   SDL_SetRenderDrawColor(renderer, 240, 235, 245, 255);
   SDL_RenderFillRect(renderer, &boardRect);

   for (int i = 1; i <= 100; ++i)
   {
      SDL_Point p = get_cell_pos(i);
      SDL_Rect r{p.x, p.y, CELL, CELL};
      int row = (i - 1) / 10;
      int col = (i - 1) % 10;
      if (row % 2 == 1)
         col = 9 - col;
      bool even = ((row + col) % 2 == 0);

      SDL_SetRenderDrawColor(renderer, even ? 230 : 255, even ? 245 : 240, 255, 255);
      SDL_RenderFillRect(renderer, &r);
      SDL_SetRenderDrawColor(renderer, 120, 100, 160, 255);
      SDL_RenderDrawRect(renderer, &r);

      if (small_font)
      {
         SDL_Color c = (i == 100) ? SDL_Color{200, 0, 0, 255} : SDL_Color{60, 40, 100, 255};
         text_draw(to_string(i), p.x + 6, p.y + 4, c, small_font);
      }
      if (i == 100)
      {
         SDL_SetRenderDrawColor(renderer, 255, 215, 0, 255);
         SDL_Rect wr{p.x + 2, p.y + 2, CELL - 4, CELL - 4};
         SDL_RenderDrawRect(renderer, &wr);
      }
   }
}

void ladder_draw(int startCell, int endCell)
{
   SDL_Point a = get_cell_pos(startCell);
   SDL_Point b = get_cell_pos(endCell);
   int ax = a.x + CELL / 2, ay = a.y + CELL / 2;
   int bx = b.x + CELL / 2, by = b.y + CELL / 2;
   double vx = bx - ax, vy = by - ay;
   double len = hypot(vx, vy);
   if (len <= 0)
      return;
   vx /= len;
   vy /= len;
   double px = -vy, py = vx;
   int dist = 12;
   SDL_SetRenderDrawColor(renderer, 200, 140, 60, 255);
   for (int t = 0; t < 3; ++t)
   {
      SDL_RenderDrawLine(renderer, (int)(ax + px * dist) + t, (int)(ay + py * dist), (int)(bx + px * dist) + t, (int)(by + py * dist));
      SDL_RenderDrawLine(renderer, (int)(ax - px * dist) + t, (int)(ay - py * dist), (int)(bx - px * dist) + t, (int)(by - py * dist));
   }
   int rungs = max(5, (int)(len / 35));
   SDL_SetRenderDrawColor(renderer, 180, 120, 40, 255);
   for (int i = 1; i < rungs; ++i)
   {
      double t = (double)i / rungs;
      SDL_RenderDrawLine(renderer, (int)(ax + px * dist + (bx - ax) * t), (int)(ay + py * dist + (by - ay) * t),
                         (int)(ax - px * dist + (bx - ax) * t), (int)(ay - py * dist + (by - ay) * t));
   }
}

void realistic_snake_draw(int startCell, int endCell, const snake_design &style, float timeSec, bool highlight)
{
   SDL_Point a = get_cell_pos(startCell);
   SDL_Point b = get_cell_pos(endCell);
   float ax = a.x + CELL / 2.0f, ay = a.y + CELL / 2.0f;
   float bx = b.x + CELL / 2.0f, by = b.y + CELL / 2.0f;
   float dx = bx - ax, dy = by - ay;
   float dist = sqrt(dx * dx + dy * dy);
   int samples = max(24, (int)(dist / 12));
   float ux = dx / dist, uy = dy / dist;
   float px = -uy, py = ux;

   for (int i = 0; i <= samples; ++i)
   {
      float t = (float)i / samples;
      float wobble = sin((t * M_PI * 4.0f) + timeSec * 2.0f) * 10.0f;
      float cx = ax + dx * t + px * wobble;
      float cy = ay + dy * t + py * wobble;
      float r = lerpf(18.0f, 6.0f, t);
      SDL_Color col = ((i / 2) % 2 == 0) ? style.bodyA : style.bodyB;
      if (highlight)
      {
         col.r = min(255, col.r + 40);
         col.g = min(255, col.g + 40);
         col.b = min(255, col.b + 40);
      }
      filled_circle_draw((int)cx, (int)cy, (int)r, col);
   }
   float hx = ax + px * sin(timeSec * 2.0f) * 2.0f;
   float hy = ay + py * sin(timeSec * 2.0f) * 2.0f;
   filled_circle_draw((int)hx, (int)hy, 20, style.head); // Head
   // Eyes
   filled_circle_draw((int)(hx - 5), (int)(hy - 5), 4, colors.white);
   filled_circle_draw((int)(hx + 5), (int)(hy - 5), 4, colors.white);
}

void dice_draw(SDL_Rect r)
{
   rounded_rect_draw(r, colors.white, 8);
   SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
   SDL_RenderDrawRect(renderer, &r);

   int cx = r.x + r.w / 2, cy = r.y + r.h / 2;
   int off = 18;
   int s = 5;
   SDL_Color d = colors.dark;

   // Simple dice logic
   int val = game.dice;
   if (val % 2 != 0)
      filled_circle_draw(cx, cy, s, d);
   if (val > 1)
   {
      filled_circle_draw(cx - off, cy - off, s, d);
      filled_circle_draw(cx + off, cy + off, s, d);
   }
   if (val > 3)
   {
      filled_circle_draw(cx + off, cy - off, s, d);
      filled_circle_draw(cx - off, cy + off, s, d);
   }
   if (val == 6)
   {
      filled_circle_draw(cx - off, cy, s, d);
      filled_circle_draw(cx + off, cy, s, d);
   }
}

void players_draw()
{
   for (int i = 0; i < 2; i++)
   {
      SDL_Point p = get_cell_pos(game.players[i].pos);
      int cx = p.x + CELL / 2 + game.players[i].offsetX;
      int cy = p.y + CELL / 2 + game.players[i].offsetY;
      filled_circle_draw(cx + 2, cy + 2, 16, SDL_Color{0, 0, 0, 90});
      filled_circle_draw(cx, cy, 14, game.players[i].color);
      center_text_draw(to_string(i + 1), cx, cy, colors.white, small_font);
   }
}

/* UI Logic */
// Rect Helpers
SDL_Rect welcome_box_rect() { return {width / 2 - 300, height / 2 - 200, 600, 400}; }
SDL_Rect welcome_start_btn()
{
   SDL_Rect b = welcome_box_rect();
   return {width / 2 - 100, b.y + 180, 200, 60};
}
SDL_Rect welcome_rules_btn()
{
   SDL_Rect b = welcome_box_rect();
   return {width / 2 - 100, b.y + 250, 200, 50};
}
SDL_Rect welcome_settings_btn()
{
   SDL_Rect b = welcome_box_rect();
   return {width / 2 - 100, b.y + 310, 200, 50};
}
SDL_Rect welcome_about_btn()
{
   SDL_Rect b = welcome_box_rect();
   return {width / 2 - 100, b.y + 370, 200, 40};
}

void welcome_draw()
{
   SDL_Rect box = welcome_box_rect();
   rounded_rect_draw(box, colors.white, 25);
   center_text_draw("THE SNADDER GAME", width / 2, box.y + 50, colors.bg2, gigantic_font);
   text_draw("Debug your path - climb ladders, avoid snakes.", box.x + 30, box.y + 110, colors.dark, medium_font);
   button_draw(welcome_start_btn(), "TAP TO START", colors.green, colors.white, hover_start, large_font);
   button_draw(welcome_rules_btn(), "RULES", colors.bg1, colors.white, hover_rules, medium_font);
   button_draw(welcome_settings_btn(), "SETTINGS", colors.bg2, colors.white, hover_settings, medium_font);
   button_draw(welcome_about_btn(), "ABOUT", colors.p1, colors.white, hover_exit, medium_font);
}

// Rules Draw - Dynamic
void rules_draw()
{
   // 1. Draw Dim Background
   draw_dim_overlay();

   // 2. Dynamic Box Calculation
   int boxW = max(600, (int)(width * 0.72));
   int boxH = max(360, (int)(height * 0.75));
   if (boxW > width - 40)
      boxW = width - 40;
   if (boxH > height - 40)
      boxH = height - 40;
   SDL_Rect box = {(width - boxW) / 2, (height - boxH) / 2, boxW, boxH};

   rounded_rect_draw(box, colors.white, 16);
   SDL_SetRenderDrawColor(renderer, 50, 50, 80, 255);
   SDL_RenderDrawRect(renderer, &box);

   center_text_draw("GAME RULES", width / 2, box.y + 30, colors.bg2, gigantic_font);

   // 3. Content Area
   int pad = 30;
   int cx = box.x + pad;
   int cy = box.y + 80;
   int cw = box.w - pad * 2 - 20; // -20 for scrollbar
   int ch = box.h - 140;          // Space for header and footer

   // Content Text
   vector<string> lines = {
       "HOW TO PLAY:",
       "• Two players take turns rolling the dice.",
       "• Click the dice or press SPACE to roll.",
       "• First player to reach cell 100 wins.",
       "",
       "SNAKES & LADDERS:",
       "• Landing on ladders gives upward movement.",
       "• Landing on snakes pulls you downward.",
       "• But each special cell triggers a question!",
       "",
       "QUESTION SYSTEM:",
       "• Click an option button or press A/B/C... to answer.",
       "• Correct Ladder: Climb up.",
       "• Correct Snake: Avoid slide.",
       "• Wrong Ladder: No climb.",
       "• Wrong Snake: Slide down.",
       "",
       "DIFFICULTY MODES:",
       "• Easy    = 30 sec timer",
       "• Medium = 15 sec timer",
       "• Hard    =  7 sec timer",
       "",
       "CONTROLS:",
       "• SPACE = Roll dice, H = Hints, A/B/C = Answer",
       "• Use Mouse Wheel to scroll this page."};

   // Calculate total height dynamically
   int lineHeight = 28;
   int totalH = lines.size() * lineHeight;

   // Scroll clamping
   rulesScrollMax = max(0, totalH - ch);
   if (rulesScroll > rulesScrollMax)
      rulesScroll = rulesScrollMax;
   if (rulesScroll < 0)
      rulesScroll = 0;

   // Clipping
   SDL_Rect clip = {cx, cy, cw, ch};
   SDL_RenderSetClipRect(renderer, &clip);

   int y = cy - rulesScroll;
   for (const auto &l : lines)
   {
      if (y + lineHeight > cy && y < cy + ch)
      {
         SDL_Color c = (!l.empty()) ? colors.p1 : colors.dark;

         text_draw(l, cx, y, c, medium_font);
      }
      y += lineHeight;
   }
   SDL_RenderSetClipRect(renderer, nullptr);

   // Scrollbar
   if (rulesScrollMax > 0)
   {
      int trackH = ch;
      int barH = max(30, (int)((float)ch / totalH * trackH));
      int barY = cy + (int)((float)rulesScroll / rulesScrollMax * (trackH - barH));
      SDL_Rect track = {box.x + box.w - 20, cy, 10, trackH};
      SDL_SetRenderDrawColor(renderer, 220, 220, 220, 255);
      SDL_RenderFillRect(renderer, &track);
      SDL_Rect bar = {box.x + box.w - 20, barY, 10, barH};
      SDL_SetRenderDrawColor(renderer, 150, 150, 150, 255);
      SDL_RenderFillRect(renderer, &bar);
   }

   SDL_Rect backBtn = {width / 2 - 70, box.y + box.h - 50, 140, 40};
   button_draw(backBtn, "BACK", colors.p1, colors.white, false, medium_font);
}

// Question Popup - Centered & Isolated
void question_box_draw()
{
   if (game.questionState != ASKING_QUESTION)
      return;

   // Dim Overlay
   draw_dim_overlay();

   int cardW = min(800, width - 100);
   int cardH = min(400, height - 100);
   SDL_Rect card = {(width - cardW) / 2, (height - cardH) / 2, cardW, cardH};

   rounded_rect_draw(card, colors.white, 20);
   SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
   SDL_RenderDrawRect(renderer, &card);

   // Header
   string title = game.isSnake ? "SNAKE CHALLENGE!" : "LADDER CHALLENGE!";
   SDL_Color headCol = game.isSnake ? colors.snake : colors.ladder;
   center_text_draw(title, width / 2, card.y + 40, headCol, large_font);

   // Timer
   string timeStr = "Time: " + to_string(game.questionTimeLeft) + "s";
   text_draw(timeStr, card.x + card.w - 140, card.y + 40, colors.gold, medium_font);

   // Question Text Wrapping
   const Question &q = questions[game.curr_question_index];
   int wrapW = card.w - 60;
   int lineY = card.y + 90;

   // Very simple word wrap
   string full = q.question;
   string line;
   int spaceW;
   TTF_SizeUTF8(medium_font, " ", &spaceW, nullptr);

   vector<string> words;
   size_t pos = 0;
   while ((pos = full.find(' ')) != string::npos)
   {
      words.push_back(full.substr(0, pos));
      full.erase(0, pos + 1);
   }
   words.push_back(full);

   for (const auto &w : words)
   {
      int lw, lh;
      string test = line + (line.empty() ? "" : " ") + w;
      TTF_SizeUTF8(medium_font, test.c_str(), &lw, &lh);
      if (lw > wrapW)
      {
         center_text_draw(line, width / 2, lineY, colors.dark, medium_font);
         line = w;
         lineY += lh + 5;
      }
      else
      {
         line = test;
      }
   }
   center_text_draw(line, width / 2, lineY, colors.dark, medium_font);
   lineY += 40;

   // Options
   auto prep = prepare_options_for_ui(q, g_questionOptionCount);
   vector<string> opts = prep.first;
   int rows = (opts.size() > 2) ? 2 : opts.size();
   int cols = (opts.size() + 1) / 2;
   if (opts.size() <= 3)
   {
      rows = opts.size();
      cols = 1;
   } // Vertical stack for few
   else
   {
      rows = (opts.size() + 1) / 2;
      cols = 2;
   } // Grid for many

   int btnH = 45;
   int btnW = (cardW - 80 - (cols - 1) * 20) / cols;

   int idx = 0;
   for (int r = 0; r < rows; ++r)
   {
      for (int c = 0; c < cols; ++c)
      {
         if (idx >= opts.size())
            break;
         int bx = card.x + 40 + c * (btnW + 20);
         int by = lineY + r * (btnH + 15);
         SDL_Rect b = {bx, by, btnW, btnH};

         // Hover logic inline
         int mx, my;
         SDL_GetMouseState(&mx, &my);
         bool h = isPointInRect(mx, my, b);

         button_draw(b, opts[idx], h ? colors.green : colors.bg2, colors.white, h, small_font);
         idx++;
      }
   }
}

/* Control Logic */

void initGame()
{
   game.players[0] = {1, colors.p1, game.players[0].name.empty() ? "P1" : game.players[0].name, -15, -15};
   game.players[1] = {1, colors.p2, game.players[1].name.empty() ? "P2" : game.players[1].name, 15, 15};
   game.current = 0;
   game.state = PLAYING;
   game.moving = false;
   game.rolling = false;
   game.questionState = NO_QUESTION;

   if (game.difficulty == EASY)
      game.questionDefaultTime = 30;
   else if (game.difficulty == MEDIUM)
      game.questionDefaultTime = 15;
   else
      game.questionDefaultTime = 7;
}

void dice_roll()
{
   if (game.rolling || game.moving || game.questionState == ASKING_QUESTION || game.preGameTimerActive)
      return;
   game.rolling = true;
   game.rollStart = SDL_GetTicks();
   game.dice_animating = true;
   game.dice_anime_start = SDL_GetTicks();
   if (game.soundEnabled && snd_roll)
      Mix_PlayChannel(-1, snd_roll, 0);
}

void q_ans_handle(int chosenIdx)
{
   if (game.questionState != ASKING_QUESTION)
      return;
   Question &q = questions[game.curr_question_index];
   auto prep = prepare_options_for_ui(q, g_questionOptionCount);
   bool correct = (chosenIdx == prep.second);

   int pos = game.players[game.current].pos;
   if (correct)
   {
      if (!game.isSnake && ladders.count(pos))
         game.players[game.current].pos = ladders[pos];
      if (game.soundEnabled && snd_correct)
         Mix_PlayChannel(-1, snd_correct, 0);
   }
   else
   {
      if (game.isSnake && snakes.count(pos))
         game.players[game.current].pos = snakes[pos];
      if (game.soundEnabled && snd_error)
         Mix_PlayChannel(-1, snd_error, 0);
   }

   if (game.players[game.current].pos == 100)
   {
      game.state = WINNER;
      if (game.soundEnabled && snd_win)
         Mix_PlayChannel(-1, snd_win, 0);
   }
   else
   {
      game.current = 1 - game.current;
   }
   game.questionState = NO_QUESTION;
}

void update_logic()
{
   if (game.state == PLAYING)
   {
      // Pre-game timer
      if (game.preGameTimerActive)
      {
         unsigned int el = (SDL_GetTicks() - game.preGameStartTime) / 1000;
         game.preGameCountdown = max(0, game.preGameDefaultCountdown - (int)el);
         if (game.preGameCountdown <= 0)
            game.preGameTimerActive = false;
         return;
      }

      // Question Timer
      if (game.questionState == ASKING_QUESTION)
      {
         unsigned int el = (SDL_GetTicks() - game.questionStartTime) / 1000;
         game.questionTimeLeft = max(0, game.questionDefaultTime - (int)el);
         if (game.questionTimeLeft == 0 && !game.questionTimeout)
         {
            game.questionTimeout = true;
            q_ans_handle(-1); // Fail
         }
         return; // STOP all other logic
      }

      // Dice
      if (game.rolling)
      {
         if (SDL_GetTicks() - game.rollStart > 1000)
         {
            game.rolling = false;
            game.dice = (rand() % 6) + 1;
            int target = game.players[game.current].pos + game.dice;
            if (target <= 100)
            {
               game.moving = true;
               game.move_path.clear();
               for (int i = game.players[game.current].pos + 1; i <= target; ++i)
                  game.move_path.push_back(i);
               game.last_move = SDL_GetTicks();
            }
            else
            {
               game.current = 1 - game.current;
            }
         }
         else
         {
            game.dice = (rand() % 6) + 1; // Animate
         }
      }

      // Movement
      if (game.moving && SDL_GetTicks() - game.last_move > 300)
      {
         game.players[game.current].pos = game.move_path[0];
         game.move_path.erase(game.move_path.begin());
         game.last_move = SDL_GetTicks();
         if (game.soundEnabled && snd_move)
            Mix_PlayChannel(-1, snd_move, 0);

         if (game.move_path.empty())
         {
            int pos = game.players[game.current].pos;
            if (pos == 100)
            {
               game.state = WINNER;
               if (game.soundEnabled && snd_win)
                  Mix_PlayChannel(-1, snd_win, 0);
            }
            else if (snakes.count(pos) || ladders.count(pos))
            {
               game.questionState = ASKING_QUESTION;
               game.isSnake = snakes.count(pos);
               game.curr_question_index = rand() % questions.size();
               game.questionStartTime = SDL_GetTicks();
               game.questionTimeLeft = game.questionDefaultTime;
               game.questionTimeout = false;
            }
            else
            {
               game.current = 1 - game.current;
            }
            game.moving = false;
         }
      }
   }
}

// Global Mouse Click Handle
void mouse_click(int mx, int my)
{
   if (game.state == WELCOME)
   {
      if (isPointInRect(mx, my, welcome_start_btn()))
         game.state = DIFFICULTY_SELECT;
      else if (isPointInRect(mx, my, welcome_rules_btn()))
      {
         game.state = RULES;
         rulesScroll = 0;
      }
      // ... settings/about logic omitted for brevity, similar structure
   }
   else if (game.state == RULES)
   {
      // Back button is dynamic
      int boxW = max(600, (int)(width * 0.72));
      int boxH = max(360, (int)(height * 0.75));
      if (boxW > width - 40)
         boxW = width - 40;
      if (boxH > height - 40)
         boxH = height - 40;
      SDL_Rect box = {(width - boxW) / 2, (height - boxH) / 2, boxW, boxH};
      SDL_Rect backBtn = {width / 2 - 70, box.y + box.h - 50, 140, 40};
      if (isPointInRect(mx, my, backBtn))
         game.state = WELCOME;
   }
   else if (game.state == PLAYING)
   {
      if (game.questionState == ASKING_QUESTION)
      {
         // Re-calculate button positions to find click
         int cardW = min(800, width - 100);
         int cardH = min(400, height - 100);
         SDL_Rect card = {(width - cardW) / 2, (height - cardH) / 2, cardW, cardH};

         // Header height approx
         int lineY = card.y + 130; // Approx based on draw

         Question &q = questions[game.curr_question_index];
         auto prep = prepare_options_for_ui(q, g_questionOptionCount);
         int rows, cols;
         if (prep.first.size() <= 3)
         {
            rows = prep.first.size();
            cols = 1;
         }
         else
         {
            rows = (prep.first.size() + 1) / 2;
            cols = 2;
         }

         int btnH = 45;
         int btnW = (cardW - 80 - (cols - 1) * 20) / cols;

         int idx = 0;
         for (int r = 0; r < rows; ++r)
         {
            for (int c = 0; c < cols; ++c)
            {
               if (idx >= prep.first.size())
                  break;
               int bx = card.x + 40 + c * (btnW + 20);
               int by = lineY + r * (btnH + 15);
               SDL_Rect b = {bx, by, btnW, btnH};
               if (isPointInRect(mx, my, b))
               {
                  q_ans_handle(idx);
                  return;
               }
               idx++;
            }
         }
      }
      else
      {
         // Dice click
         SDL_Rect lp = {BOARD_X + BOARD_SIZE + 30, BOARD_Y + 220, 280, 380};
         SDL_Rect dr = {lp.x + 100, lp.y + 70, 80, 80};
         if (isPointInRect(mx, my, dr))
            dice_roll();
      }
   }
   else if (game.state == DIFFICULTY_SELECT)
   {
      SDL_Rect box = {width / 2 - 250, height / 2 - 180, 500, 360};
      SDL_Rect easy = {width / 2 - 150, box.y + 100, 300, 60};
      SDL_Rect med = {width / 2 - 150, box.y + 170, 300, 60};
      SDL_Rect hard = {width / 2 - 150, box.y + 240, 300, 60};
      if (isPointInRect(mx, my, easy))
      {
         game.difficulty = EASY;
         game.state = NAME_INPUT;
      }
      else if (isPointInRect(mx, my, med))
      {
         game.difficulty = MEDIUM;
         game.state = NAME_INPUT;
      }
      else if (isPointInRect(mx, my, hard))
      {
         game.difficulty = HARD;
         game.state = NAME_INPUT;
      }
   }
   else if (game.state == NAME_INPUT)
   {
      SDL_Rect btn = {width / 2 - 75, height / 2 + 30, 150, 40};
      if (isPointInRect(mx, my, btn) && !game.tempName.empty())
      {
         game.players[game.input_player].name = game.tempName;
         game.tempName = "";
         game.input_player++;
         if (game.input_player >= 2)
         {
            game.preGameTimerActive = true;
            game.preGameStartTime = SDL_GetTicks();
            initGame();
         }
      }
   }
   else if (game.state == WINNER)
   {
      game.state = WELCOME;
   }
}

int main(int argc, char **argv)
{
   srand((unsigned int)time(nullptr));
   if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0)
      return 1;
   if (TTF_Init() != 0)
      return 1;
   if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) != 0)
      return 1;

   window = SDL_CreateWindow("Snadder V2", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
   renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

   // Load fonts (assumed assets exist)
   small_font = TTF_OpenFont("assets/font/newspaper.ttf", 16);
   medium_font = TTF_OpenFont("assets/font/good_timing_bd.ttf", 22);
   large_font = TTF_OpenFont("assets/font/Pixel_Game.ttf", 30);
   gigantic_font = TTF_OpenFont("assets/font/KnightWarrior.ttf", 70);

   // Load sounds
   snd_roll = Mix_LoadWAV("assets/sound/roll.wav");
   snd_move = Mix_LoadWAV("assets/sound/move.wav");
   snd_win = Mix_LoadWAV("assets/sound/win.wav");
   snd_snake = Mix_LoadWAV("assets/sound/snake.wav");
   snd_correct = Mix_LoadWAV("assets/sound/correct.wav");
   snd_click = Mix_LoadWAV("assets/sound/click.wav");
   snd_error = Mix_LoadWAV("assets/sound/error.wav");

   if (!load_questions_from_csv("questions.csv"))
      load_default_questions();

   bool quit = false;
   SDL_Event e;
   SDL_StartTextInput();
   unsigned int last_time = SDL_GetTicks();

   while (!quit)
   {
      unsigned int curr_time = SDL_GetTicks();
      float delta = (curr_time - last_time) / 1000.0f;
      last_time = curr_time;
      game.animation_time += delta;

      while (SDL_PollEvent(&e))
      {
         if (e.type == SDL_QUIT)
            quit = true;
         if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
         {
            width = e.window.data1;
            height = e.window.data2;
         }

         if (e.type == SDL_MOUSEWHEEL)
         {
            if (game.state == RULES)
            {
               rulesScroll -= e.wheel.y * rulesScrollStep;
               // Clamp happens in draw for dynamic reasons, but good to hint here
            }
         }

         if (e.type == SDL_MOUSEBUTTONDOWN)
         {
            mouse_click(e.button.x, e.button.y);
         }

         if (e.type == SDL_KEYDOWN)
         {
            if (game.state == NAME_INPUT)
            {
               if (e.key.keysym.sym == SDLK_BACKSPACE && !game.tempName.empty())
                  game.tempName.pop_back();
               else if (e.key.keysym.sym == SDLK_RETURN && !game.tempName.empty())
               {
                  // Auto-click continue
                  mouse_click(width / 2, height / 2 + 50);
               }
            }
            else if (game.state == PLAYING)
            {
               if (e.key.keysym.sym == SDLK_SPACE)
                  dice_roll();
               if (game.questionState == ASKING_QUESTION)
               {
                  if (e.key.keysym.sym >= SDLK_a && e.key.keysym.sym <= SDLK_f)
                     q_ans_handle(e.key.keysym.sym - SDLK_a);
               }
            }
         }

         if (e.type == SDL_TEXTINPUT && game.state == NAME_INPUT)
         {
            if (game.tempName.length() < 12)
               game.tempName += e.text.text;
         }

         // Hover logic omitted for brevity in single file, handled inside buttons usually
      }

      update_logic();

      // DRAW PHASE
      background_draw();

      if (game.state == WELCOME)
         welcome_draw();
      else if (game.state == DIFFICULTY_SELECT)
      {
         // Draw simple box
         SDL_Rect box = {width / 2 - 250, height / 2 - 180, 500, 360};
         rounded_rect_draw(box, colors.white, 20);
         center_text_draw("DIFFICULTY", width / 2, box.y + 40, colors.bg2, large_font);

         int mx, my;
         SDL_GetMouseState(&mx, &my);
         SDL_Rect easy = {width / 2 - 150, box.y + 100, 300, 60};
         SDL_Rect med = {width / 2 - 150, box.y + 170, 300, 60};
         SDL_Rect hard = {width / 2 - 150, box.y + 240, 300, 60};
         button_draw(easy, "EASY", colors.green, colors.white, isPointInRect(mx, my, easy), large_font);
         button_draw(med, "MEDIUM", colors.bg1, colors.white, isPointInRect(mx, my, med), large_font);
         button_draw(hard, "HARD", colors.p1, colors.white, isPointInRect(mx, my, hard), large_font);
      }
      else if (game.state == NAME_INPUT)
      {
         SDL_Rect box = {width / 2 - 200, height / 2 - 100, 400, 200};
         rounded_rect_draw(box, colors.white, 20);
         string t = "Player " + to_string(game.input_player + 1) + " Name:";
         center_text_draw(t, width / 2, box.y + 40, colors.dark, large_font);

         SDL_Rect field = {width / 2 - 150, box.y + 80, 300, 40};
         SDL_SetRenderDrawColor(renderer, 230, 230, 230, 255);
         SDL_RenderFillRect(renderer, &field);
         center_text_draw(game.tempName + "_", width / 2, box.y + 100, colors.dark, medium_font);

         SDL_Rect btn = {width / 2 - 75, box.y + 150, 150, 40};
         int mx, my;
         SDL_GetMouseState(&mx, &my);
         button_draw(btn, "CONTINUE", game.tempName.empty() ? colors.dark : colors.green, colors.white, isPointInRect(mx, my, btn), medium_font);
      }
      else if (game.state == PLAYING || game.state == WINNER)
      {
         board_draw();
         for (auto &l : ladders)
            ladder_draw(l.first, l.second);
         for (auto &s : snakes)
            realistic_snake_draw(s.first, s.second, snake_style[s.first], game.animation_time, false);
         players_draw();

         // UI Panel
         SDL_Rect lp = {BOARD_X + BOARD_SIZE + 30, BOARD_Y + 220, 280, 380};
         rounded_rect_draw(lp, colors.white, 10);
         center_text_draw(game.players[game.current].name + "'s Turn", lp.x + lp.w / 2, lp.y + 30, colors.dark, medium_font);
         SDL_Rect dr = {lp.x + 100, lp.y + 70, 80, 80};
         dice_draw(dr);

         if (game.questionState == ASKING_QUESTION)
            question_box_draw();

         if (game.state == WINNER)
         {
            draw_dim_overlay();
            center_text_draw("WINNER!", width / 2, height / 2, colors.gold, gigantic_font);
            center_text_draw(game.players[game.current].name, width / 2, height / 2 + 80, colors.white, large_font);
         }
      }
      else if (game.state == RULES)
      {
         rules_draw();
      }

      SDL_RenderPresent(renderer);
      SDL_Delay(16);
   }

   SDL_DestroyRenderer(renderer);
   SDL_DestroyWindow(window);
   Mix_CloseAudio();
   TTF_Quit();
   SDL_Quit();
   return 0;
}