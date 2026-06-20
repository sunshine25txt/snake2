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

bool hover_start = false;
bool hover_rules = false;
bool hover_settings = false;
bool hover_exit = false;

bool hover_easy = false;
bool hover_medium = false;
bool hover_hard = false;
bool hover_back_diff = false;

/* Board data */
map<int, int> snakes = {
    {16, 6}, {47, 26}, {49, 11}, {56, 53}, {62, 19}, {64, 60}, {87, 24}, {93, 73}, {95, 75}, {98, 78}};

map<int, int> ladders = {
    {2, 37}, {4, 14}, {9, 21}, {39, 57}, {28, 84}, {36, 44}, {51, 67}, {71, 91}, {80, 97}};

/* Question Challenge Database */
struct Question
{
   string question;
   vector<string> options; // multiple options (2..6)
   int ans_index;          // index into options which is correct (0-based)
};

enum QuestionState
{
   NO_QUESTION,
   ASKING_QUESTION
};

int rulesScroll = 0;
int rulesScrollMax = 0;
int rulesScrollStep = 40;

vector<Question> questions; // loaded from CSV or fallback

// how many options to display per question (user selectable 2..6)
int g_questionOptionCount = 4; // default 4

/* Game Design */
struct Colors
{
   SDL_Color bg1{41, 128, 185, 255};   // Ocean Blue
   SDL_Color bg2{155, 89, 182, 255};   // Amethyst
   SDL_Color GRID{236, 240, 241, 255}; // Clouds
   SDL_Color cell1{52, 152, 219, 255}; // Bright Blue
   SDL_Color cell2{155, 89, 182, 255}; // Purple
   SDL_Color p1{231, 76, 60, 255};     // Red
   SDL_Color p2{46, 204, 113, 255};    // Green
   SDL_Color gold{241, 196, 15, 255};  // Gold
   SDL_Color white{255, 255, 255, 255};
   SDL_Color dark{44, 62, 80, 255};   // Dark Blue
   SDL_Color ladder{211, 84, 0, 255}; // Orange
   SDL_Color snake{192, 57, 43, 255}; // Dark Red
   SDL_Color green{39, 174, 96, 255}; // Emerald Green
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

enum Statew
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
   int offsetX, offsetY; // to skip overlapping
};

enum Difficulty
{
   EASY,
   MEDIUM,
   HARD
};

/* Game Engine */
struct Game
{
   // core state
   Statew state = WELCOME;
   Player players[2];
   int current = 0;

   // difficulty
   Difficulty difficulty = EASY;

   // dice animation
   int dice = 1;
   bool rolling = false;
   bool moving = false;
   unsigned int rollStart = 0;
   vector<int> move_path;
   unsigned int last_move = 0;

   // name input
   int input_player = 0;
   string tempName = "";

   // dice UI
   bool cursor_on_dice = false;
   unsigned int dice_anime_start = 0;
   bool dice_animating = false;

   // board hover / preview
   int cursor_on_cell = 0;
   int cell_preview = 0;
   bool show_hints = true;

   float animation_time = 0.0f;

   // question system
   QuestionState questionState = NO_QUESTION;
   int curr_question_index = 0;
   bool landed_special_cell = false;
   int special_cell = 0;
   bool isSnake = false;
   char select_ans = 0;
   bool cursor_on_option = false;
   unsigned int questionStartTime = 0;
   int questionTimeLeft = 30;
   bool questionTimeout = false;

   // NEW: cached UI options for current question instance
   vector<string> question_ui_options;
   int question_ui_correct = -1;

   // settings
   bool soundEnabled = true;
   bool diceSoundEnabled = true;

   // pre-game countdown
   unsigned int preGameStartTime = 0;
   bool preGameTimerActive = false;
   int preGameCountdown = 3;

   int questionDefaultTime = 30;
   int preGameDefaultCountdown = 3;

   // exiting from UI
   bool requestQuit = false;
} game;

/* For smooth Moving */
float lerpf(float a, float b, float t) // linear interpolation
{
   return a + (b - a) * t;
}

/* Drawing Design */
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
   if (!tex)
   {
      SDL_FreeSurface(surf);
      return;
   }
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

// forward declaration
void rounded_rect_draw(SDL_Rect rect, SDL_Color col, int radius);

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

   // Fill rounded rect
   rounded_rect_draw(rect, color, 12);

   // Draw rounded border: approximate by drawing slightly inset rect corners (no square overlay)
   SDL_SetRenderDrawColor(renderer, max(0, color.r - 30), max(0, color.g - 30), max(0, color.b - 30), 200);
   SDL_Rect border = {rect.x + 1, rect.y + 1, rect.w - 2, rect.h - 2};
   SDL_RenderDrawRect(renderer, &border);

   if (!font)
      font = medium_font ? medium_font : small_font;

   center_text_draw(text,
                    rect.x + rect.w / 2,
                    rect.y + rect.h / 2,
                    fg,
                    font);
}

void circle_draw(int cx, int cy, int r, SDL_Color c)
{
   for (int y = -r; y <= r; y++)
   {
      int w = (int)sqrt(r * r - y * y);
      SDL_RenderDrawLine(renderer, cx - w, cy + y, cx + w, cy + y);
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

/* Get Position */
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
   if (cx < board_left || cy < board_top ||
       cx >= board_left + BOARD_SIZE || cy >= board_top + BOARD_SIZE)
      return 0;

   int col = (cx - board_left) / CELL;
   int drawRow = (cy - board_top) / CELL;
   int row = GRID - 1 - drawRow;
   if (row < 0 || row >= GRID)
      return 0;
   int actual_col = (row % 2 == 0) ? col : (GRID - 1 - col);
   int cell = row * GRID + actual_col + 1;
   if (cell < 1 || cell > 100)
      return 0;
   return cell;
}

/* Drawing Function */
void background_draw()
{
   for (int y = 0; y < height; y++)
   {
      float t = (float)y / height;
      SDL_SetRenderDrawColor(renderer,
                             colors.bg1.r + (colors.bg2.r - colors.bg1.r) * t,
                             colors.bg1.g + (colors.bg2.g - colors.bg1.g) * t,
                             colors.bg1.b + (colors.bg2.b - colors.bg1.b) * t,
                             255);
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
      if (even)
         SDL_SetRenderDrawColor(renderer, 230, 245, 255, 255);
      else
         SDL_SetRenderDrawColor(renderer, 255, 240, 245, 255);

      SDL_RenderFillRect(renderer, &r);
      SDL_SetRenderDrawColor(renderer, 120, 100, 160, 255);
      SDL_RenderDrawRect(renderer, &r);

      if (small_font)
      {
         SDL_Color c = (i == 100) ? SDL_Color{200, 0, 0, 255}
                                  : SDL_Color{60, 40, 100, 255};
         text_draw(to_string(i), p.x + 6, p.y + 4, c, small_font);
      }

      if (i == 100)
      {
         SDL_SetRenderDrawColor(renderer, 255, 215, 0, 255);
         for (int k = 0; k < 3; ++k)
         {
            SDL_Rect wr{p.x + k, p.y + k, CELL - 2 * k, CELL - 2 * k};
            SDL_RenderDrawRect(renderer, &wr);
         }
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
   if (len <= 0.0)
      return;

   vx /= len;
   vy /= len;
   double px = -vy, py = vx;
   int distance = 12;

   int ax1 = (int)(ax + px * distance), ay1 = (int)(ay + py * distance);
   int ax2 = (int)(ax - px * distance), ay2 = (int)(ay - py * distance);
   int bx1 = (int)(bx + px * distance), by1 = (int)(by + py * distance);
   int bx2 = (int)(bx - px * distance), by2 = (int)(by - py * distance);

   SDL_SetRenderDrawColor(renderer, 200, 140, 60, 255);
   for (int t = 0; t < 3; ++t)
   {
      SDL_RenderDrawLine(renderer, ax1 + t, ay1, bx1 + t, by1);
      SDL_RenderDrawLine(renderer, ax2 + t, ay2, bx2 + t, by2);
   }

   int rungs = max(5, (int)(len / 35));
   SDL_SetRenderDrawColor(renderer, 180, 120, 40, 255);
   for (int i = 1; i < rungs; ++i)
   {
      double tt = (double)i / (double)rungs;
      int rx1 = (int)lerpf((float)ax1, (float)bx1, (float)tt);
      int ry1 = (int)lerpf((float)ay1, (float)by1, (float)tt);
      int rx2 = (int)lerpf((float)ax2, (float)bx2, (float)tt);
      int ry2 = (int)lerpf((float)ay2, (float)by2, (float)tt);
      for (int th = 0; th < 2; ++th)
         SDL_RenderDrawLine(renderer, rx1, ry1 + th, rx2, ry2 + th);
   }
}

void realistic_snake_draw(int startCell, int endCell,
                          const snake_design &style, float timeSec,
                          bool highlight = false)
{
   SDL_Point a = get_cell_pos(startCell);
   SDL_Point b = get_cell_pos(endCell);
   float ax = (float)(a.x + CELL / 2), ay = (float)(a.y + CELL / 2);
   float bx = (float)(b.x + CELL / 2), by = (float)(b.y + CELL / 2);

   float dx = bx - ax, dy = by - ay;
   float dist = sqrt(dx * dx + dy * dy);
   int samples = max(24, (int)(dist / 12));
   samples = min(samples, 300); // cap
   float ux = (dist > 0) ? dx / dist : 1.0f;
   float uy = (dist > 0) ? dy / dist : 0.0f;
   float px = -uy, py = ux;

   float baseR = 18.0f;
   float tailR = 6.0f;

   for (int i = 0; i <= samples; ++i)
   {
      float t = (float)i / (float)samples;
      float wobble = sin((t * M_PI * 4.0f) + timeSec * 2.0f) * 10.0f;
      float cx = ax + dx * t + px * wobble;
      float cy = ay + dy * t + py * wobble;
      float r = lerpf(baseR, tailR, t);

      bool alt = ((i / 2) % 2) == 0;
      SDL_Color bodyCol = alt ? style.bodyA : style.bodyB;

      if (highlight)
      {
         bodyCol = SDL_Color{
             (Uint8)min(255, bodyCol.r + 40),
             (Uint8)min(255, bodyCol.g + 40),
             (Uint8)min(255, bodyCol.b + 40),
             255};
      }

      SDL_Color belly = {
          (Uint8)(bodyCol.r / 1.2f),
          (Uint8)(bodyCol.g / 1.2f),
          (Uint8)(bodyCol.b / 1.2f),
          220};

      filled_eclipse_draw((int)(cx + px * 2), (int)(cy + py * 2),
                          (int)(r), (int)(r * 0.75f), belly);
      filled_circle_draw((int)cx, (int)cy, (int)r, bodyCol);
   }

   float headAngle = atan2(dy, dx);
   float hx = ax + px * sin(timeSec * 2.0f) * 2.0f;
   float hy = ay + py * sin(timeSec * 2.0f) * 2.0f;

   filled_circle_draw((int)hx, (int)hy, (int)(baseR + 2), style.head);

   float eyeOff = baseR * 0.4f;
   float ex1 = hx + cos(headAngle + M_PI / 2.8f) * eyeOff;
   float ey1 = hy + sin(headAngle + M_PI / 2.8f) * eyeOff;
   float ex2 = hx + cos(headAngle - M_PI / 2.8f) * eyeOff;
   float ey2 = hy + sin(headAngle - M_PI / 2.8f) * eyeOff;

   filled_circle_draw((int)ex1, (int)ey1, 4, SDL_Color{255, 255, 255, 255});
   filled_circle_draw((int)ex2, (int)ey2, 4, SDL_Color{255, 255, 255, 255});
   filled_circle_draw((int)ex1, (int)ey1, 2, SDL_Color{0, 0, 0, 255});
   filled_circle_draw((int)ex2, (int)ey2, 2, SDL_Color{0, 0, 0, 255});

   float flick = (sin(timeSec * 12.0f) > 0.6f) ? 1.0f : 0.0f;
   if (flick > 0.5f)
   {
      float tx = hx + cos(headAngle) * (baseR + 10);
      float ty = hy + sin(headAngle) * (baseR + 10);
      SDL_SetRenderDrawColor(renderer, 220, 50, 50, 255);
      SDL_RenderDrawLine(renderer, (int)hx, (int)hy, (int)tx, (int)ty);
      float forkAng = headAngle + 0.25f;
      SDL_RenderDrawLine(renderer, (int)tx, (int)ty,
                         (int)(tx + cos(forkAng) * 8),
                         (int)(ty + sin(forkAng) * 8));
      forkAng = headAngle - 0.25f;
      SDL_RenderDrawLine(renderer, (int)tx, (int)ty,
                         (int)(tx + cos(forkAng) * 8),
                         (int)(ty + sin(forkAng) * 8));
   }
}

/* UTIL: read CSV (simple) */
bool load_questions_from_csv(const string &path)
{
   ifstream f(path);
   if (!f.is_open())
      return false;

   questions.clear();
   string line;
   while (std::getline(f, line))
   {
      if (line.empty())
         continue;
      if (line.size() > 0 && line[0] == '#')
         continue;
      vector<string> parts;
      string cur;
      for (size_t i = 0; i < line.size(); ++i)
      {
         char ch = line[i];
         if (ch == ',')
         {
            parts.push_back(cur);
            cur.clear();
         }
         else
            cur.push_back(ch);
      }
      parts.push_back(cur);
      if (parts.size() < 3) // need question + at least 2 options
         continue;

      Question q;
      q.question = parts[0];
      q.options.clear();
      for (size_t i = 1; i < parts.size() && q.options.size() < 6; ++i)
      {
         string s = parts[i];
         while (!s.empty() && (s.front() == ' ' || s.front() == '\t'))
            s.erase(s.begin());
         while (!s.empty() && (s.back() == ' ' || s.back() == '\t'))
            s.pop_back();
         if (!s.empty())
            q.options.push_back(s);
      }
      q.ans_index = 0; // assume first is correct
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

/* Prepare options to show on UI */
pair<vector<string>, int> prepare_options_for_ui(const Question &q, int displayCount)
{
   displayCount = max(2, min(6, displayCount));
   vector<string> candidates;

   for (const auto &s : q.options)
   {
      if (find(candidates.begin(), candidates.end(), s) == candidates.end())
         candidates.push_back(s);
   }

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
   {
      for (size_t i = 0; i < candidates.size() && (int)candidates.size() < displayCount; ++i)
         candidates.push_back(candidates[i]);
      if (candidates.empty())
      {
         candidates.push_back("N/A");
         candidates.push_back("N/A2");
      }
   }

   string correctStr = "";
   if (q.ans_index >= 0 && q.ans_index < (int)q.options.size())
      correctStr = q.options[q.ans_index];

   bool hadCorrect = (!correctStr.empty() && find(candidates.begin(), candidates.end(), correctStr) != candidates.end());
   if (!hadCorrect && !correctStr.empty())
   {
      if (!candidates.empty())
         candidates.back() = correctStr;
      else
         candidates.push_back(correctStr);
   }

   static std::mt19937 rng((unsigned)time(nullptr));
   shuffle(candidates.begin(), candidates.end(), rng);

   vector<string> finalOpts;
   finalOpts.reserve(displayCount);
   for (size_t i = 0; i < candidates.size() && (int)finalOpts.size() < displayCount; ++i)
   {
      if (find(finalOpts.begin(), finalOpts.end(), candidates[i]) == finalOpts.end())
         finalOpts.push_back(candidates[i]);
   }

   if ((int)finalOpts.size() < 2)
   {
      for (const auto &s : q.options)
      {
         if ((int)finalOpts.size() >= 2)
            break;
         if (find(finalOpts.begin(), finalOpts.end(), s) == finalOpts.end())
            finalOpts.push_back(s);
      }
   }
   while ((int)finalOpts.size() < displayCount)
      finalOpts.push_back(string("N/A"));

   int newAns = -1;
   if (!correctStr.empty())
   {
      for (int i = 0; i < (int)finalOpts.size(); ++i)
      {
         if (finalOpts[i] == correctStr)
         {
            newAns = i;
            break;
         }
      }
   }
   if (newAns == -1)
   {
      if (!correctStr.empty())
      {
         finalOpts[0] = correctStr;
         newAns = 0;
      }
   }

   return {finalOpts, newAns};
}

/* Drawing question popup UI dynamically (center floating card with overlay) */
void question_box_draw()
{
   if (game.questionState != ASKING_QUESTION)
      return;

   SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
   SDL_SetRenderDrawColor(renderer, 0, 0, 0, 160);
   SDL_Rect overlay = {0, 0, width, height};
   SDL_RenderFillRect(renderer, &overlay);

   int cardW = min(760, width - 200);
   int cardH = min(380, height - 200);
   SDL_Rect card = {width / 2 - cardW / 2, height / 2 - cardH / 2, cardW, cardH};

   rounded_rect_draw(card, colors.white, 18);

   string title = game.isSnake ? "SNAKE CHALLENGE!" : "LADDER CHALLENGE!";
   center_text_draw(title, card.x + card.w / 2, card.y + 34, colors.p1, large_font ? large_font : medium_font);

   string tstr = to_string(game.questionTimeLeft) + "s";
   int timerX = card.x + card.w - 90;
   center_text_draw(tstr, timerX, card.y + 40, colors.gold, large_font ? large_font : medium_font);

   int wrapX = card.x + 28;
   int wrapW = card.w - 56;
   int lineY = card.y + 80;

   const Question &q = questions[game.curr_question_index];

   // Build wrapped lines
   vector<string> words;
   {
      string w;
      for (char ch : q.question)
      {
         if (ch == ' ')
         {
            if (!w.empty())
            {
               words.push_back(w);
               w.clear();
            }
         }
         else
            w.push_back(ch);
      }
      if (!w.empty())
         words.push_back(w);
   }

   string curLine;
   // determine an approximate line height using small/medium font metrics
   int tmpw = 0, tmph = 0;
   TTF_SizeUTF8(medium_font ? medium_font : small_font, "Ay", &tmpw, &tmph);
   int lineHeight = max(18, tmph + 6);

   for (size_t i = 0; i < words.size(); ++i)
   {
      string test = curLine.empty() ? words[i] : curLine + " " + words[i];
      int tw = 0, th = 0;
      if (small_font)
         TTF_SizeUTF8(small_font, test.c_str(), &tw, &th);
      if (tw > wrapW && !curLine.empty())
      {
         text_draw(curLine, wrapX, lineY, colors.dark, medium_font ? medium_font : small_font);
         lineY += lineHeight;
         curLine = words[i];
      }
      else
         curLine = test;
   }
   if (!curLine.empty())
   {
      text_draw(curLine, wrapX, lineY, colors.dark, medium_font ? medium_font : small_font);
      lineY += lineHeight;
   }

   // Use cached options prepared when the question started
   vector<string> opts = game.question_ui_options;
   int correctIndex = game.question_ui_correct;

   // Safety fallback: if cache missing, prepare once now and store
   if (opts.empty() && !questions.empty())
   {
      auto prep = prepare_options_for_ui(q, g_questionOptionCount);
      opts = prep.first;
      correctIndex = prep.second;
      game.question_ui_options = opts;
      game.question_ui_correct = correctIndex;
   }

   int optCount = (int)opts.size();
   int cols = (optCount <= 2) ? optCount : 2;
   int rows = (optCount + cols - 1) / cols;
   int optW = (card.w - 60 - (cols - 1) * 12) / cols;
   int optH = 44;
   int startY = lineY + 8;

   // draw options and hover
   int optIndex = 0;
   for (int r = 0; r < rows; ++r)
   {
      int y = startY + r * (optH + 12);
      for (int c = 0; c < cols; ++c)
      {
         if (optIndex >= optCount)
            break;
         int x = card.x + 30 + c * (optW + 12);
         SDL_Rect rect = {x, y, optW, optH};
         int mx, my;
         SDL_GetMouseState(&mx, &my);
         bool hover = (mx >= rect.x && mx < rect.x + rect.w && my >= rect.y && my < rect.y + rect.h);
         SDL_Color bg = hover ? colors.green : colors.bg2;
         string label = string(1, 'A' + optIndex) + string(": ") + opts[optIndex];
         button_draw(rect, label, bg, colors.white, hover, medium_font ? medium_font : small_font);
         optIndex++;
      }
   }

   center_text_draw("Click option or press A..", card.x + card.w / 2, card.y + card.h - 28, colors.dark, small_font);
}

/* Rounded rect implementation */
void rounded_rect_draw(SDL_Rect rect, SDL_Color col, int radius)
{
   SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
   SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, col.a);

   SDL_Rect body = {rect.x + radius, rect.y, rect.w - 2 * radius, rect.h};
   SDL_RenderFillRect(renderer, &body);

   SDL_Rect left = {rect.x, rect.y + radius, radius, rect.h - 2 * radius};
   SDL_RenderFillRect(renderer, &left);

   SDL_Rect right = {rect.x + rect.w - radius, rect.y + radius,
                     radius, rect.h - 2 * radius};
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

// Return rect helpers (same as before)
SDL_Rect welcome_box_rect() { return SDL_Rect{width / 2 - 300, height / 2 - 200, 600, 400}; }
SDL_Rect welcome_start_btn()
{
   SDL_Rect b = welcome_box_rect();
   return SDL_Rect{width / 2 - 100, b.y + 180, 200, 60};
}
SDL_Rect welcome_rules_btn()
{
   SDL_Rect b = welcome_box_rect();
   return SDL_Rect{width / 2 - 100, b.y + 250, 200, 50};
}
SDL_Rect welcome_settings_btn()
{
   SDL_Rect b = welcome_box_rect();
   return SDL_Rect{width / 2 - 100, b.y + 310, 200, 50};
}
SDL_Rect welcome_about_btn()
{
   SDL_Rect b = welcome_box_rect();
   return SDL_Rect{width / 2 - 100, b.y + 370, 200, 40};
}

// Name input rects
SDL_Rect name_input_box_rect() { return SDL_Rect{width / 2 - 250, height / 2 - 150, 500, 300}; }
SDL_Rect name_continue_button_rect()
{
   SDL_Rect ib = name_input_box_rect();
   return SDL_Rect{width / 2 - 75, ib.y + 180, 150, 40};
}
SDL_Rect name_field_rect()
{
   SDL_Rect ib = name_input_box_rect();
   return SDL_Rect{width / 2 - 150, ib.y + 100, 300, 40};
}

// For playing UI
SDL_Rect playing_lower_panel_rect()
{
   int rightX = BOARD_X + BOARD_SIZE + 30;
   return SDL_Rect{rightX, BOARD_Y + 220, 280, 380};
}
SDL_Rect playing_dice_rect()
{
   SDL_Rect lp = playing_lower_panel_rect();
   return SDL_Rect{lp.x + 100, lp.y + 70, 80, 80};
}

/* WELCOME with ABOUT button */
void welcome_draw()
{
   SDL_Rect welcome_box = welcome_box_rect();
   rounded_rect_draw(welcome_box, colors.white, 25);

   center_text_draw("THE SNADDER GAME", width / 2,
                    welcome_box.y + 50, colors.bg2, gigantic_font ? gigantic_font : medium_font);

   string subtitle = "Debug your path — climb ladders, avoid snakes.";
   int sx = welcome_box.x + 30;
   text_draw(subtitle, sx, welcome_box.y + 110, colors.dark, medium_font ? medium_font : small_font);

   SDL_Rect startBtn = welcome_start_btn();
   SDL_Rect rulesBtn = welcome_rules_btn();
   SDL_Rect settingsBtn = welcome_settings_btn();
   SDL_Rect aboutBtn = welcome_about_btn();

   button_draw(startBtn, "TAP TO START", colors.green, colors.white, hover_start, large_font ? large_font : medium_font);
   button_draw(rulesBtn, "RULES", colors.bg1, colors.white, hover_rules, medium_font);
   button_draw(settingsBtn, "SETTINGS", colors.bg2, colors.white, hover_settings, medium_font);
   button_draw(aboutBtn, "ABOUT", colors.p1, colors.white, hover_exit, medium_font);
}

void name_input_box()
{
   SDL_Rect input_box = name_input_box_rect();
   SDL_Rect shadow = {input_box.x + 5, input_box.y + 5, input_box.w, input_box.h};
   SDL_SetRenderDrawColor(renderer, 0, 0, 0, 100);
   SDL_RenderFillRect(renderer, &shadow);

   SDL_SetRenderDrawColor(renderer, colors.white.r, colors.white.g,
                          colors.white.b, 240);
   SDL_RenderFillRect(renderer, &input_box);
   SDL_SetRenderDrawColor(renderer, colors.dark.r, colors.dark.g,
                          colors.dark.b, 255);
   SDL_RenderDrawRect(renderer, &input_box);

   string title = "Enter Player " + to_string(game.input_player + 1) + " Name:";
   center_text_draw(title, width / 2, input_box.y + 50, colors.dark, large_font ? large_font : medium_font);

   SDL_Rect name_field = name_field_rect();
   SDL_SetRenderDrawColor(renderer, colors.GRID.r, colors.GRID.g, colors.GRID.b, 255);
   SDL_RenderFillRect(renderer, &name_field);
   SDL_SetRenderDrawColor(renderer, colors.dark.r, colors.dark.g, colors.dark.b, 255);
   SDL_RenderDrawRect(renderer, &name_field);

   string display_name = game.tempName.empty() ? "Please Enter name..." : game.tempName;
   SDL_Color nameColor = game.tempName.empty() ? SDL_Color{150, 150, 150, 255} : colors.dark;
   text_draw(display_name, name_field.x + 10, name_field.y + 10, nameColor, medium_font ? medium_font : small_font);

   if (SDL_GetTicks() % 1000 < 500)
   {
      int textW = 0;
      if (!game.tempName.empty() && medium_font)
         TTF_SizeUTF8(medium_font, game.tempName.c_str(), &textW, nullptr);
      SDL_RenderDrawLine(renderer,
                         name_field.x + 10 + textW, name_field.y + 5,
                         name_field.x + 10 + textW, name_field.y + 30);
   }

   SDL_Rect continue_button = name_continue_button_rect();
   bool canContinue = !game.tempName.empty();
   SDL_Color btnColor = canContinue ? colors.green : SDL_Color{100, 100, 100, 255};
   button_draw(continue_button, "CONTINUE", btnColor, colors.white, false, medium_font);

   center_text_draw("Type player name and press CONTINUE",
                    width / 2, input_box.y + 250,
                    colors.dark, small_font);
}

void dice_draw(SDL_Rect dice_rect)
{
   SDL_Rect shadow = {dice_rect.x + 3, dice_rect.y + 3, dice_rect.w, dice_rect.h};
   SDL_SetRenderDrawColor(renderer, 0, 0, 0, 100);
   SDL_RenderFillRect(renderer, &shadow);

   int animOffset = 0;
   if (game.dice_animating)
   {
      unsigned int elapsed = SDL_GetTicks() - game.dice_anime_start;
      if (elapsed < 500)
      {
         animOffset = (int)(sin(elapsed * 0.02f) * 5);
      }
      else
      {
         game.dice_animating = false;
      }
   }
   SDL_Rect animatedRect = {dice_rect.x + animOffset, dice_rect.y + animOffset,
                            dice_rect.w, dice_rect.h};

   for (int i = 0; i < animatedRect.h; i++)
   {
      int brightness = game.cursor_on_dice
                           ? 255 - (i * 10 / animatedRect.h)
                           : 255 - (i * 20 / animatedRect.h);
      SDL_SetRenderDrawColor(renderer, brightness, brightness, brightness, 255);
      SDL_RenderDrawLine(renderer,
                         animatedRect.x,
                         animatedRect.y + i,
                         animatedRect.x + animatedRect.w,
                         animatedRect.y + i);
   }

   SDL_SetRenderDrawColor(renderer, 0, 0, 0, 200);
   for (int i = 0; i < 3; i++)
   {
      SDL_Rect border = {animatedRect.x - i, animatedRect.y - i,
                         animatedRect.w + 2 * i, animatedRect.h + 2 * i};
      SDL_RenderDrawRect(renderer, &border);
   }

   int dot_size = 6;
   SDL_SetRenderDrawColor(renderer, colors.dark.r, colors.dark.g, colors.dark.b, 255);
   int cx = animatedRect.x + animatedRect.w / 2;
   int cy = animatedRect.y + animatedRect.h / 2;

   switch (game.dice)
   {
   case 1:
      circle_draw(cx, cy, dot_size, colors.dark);
      break;
   case 2:
      circle_draw(cx - 15, cy - 15, dot_size, colors.dark);
      circle_draw(cx + 15, cy + 15, dot_size, colors.dark);
      break;
   case 3:
      circle_draw(cx - 15, cy - 15, dot_size, colors.dark);
      circle_draw(cx, cy, dot_size, colors.dark);
      circle_draw(cx + 15, cy + 15, dot_size, colors.dark);
      break;
   case 4:
      circle_draw(cx - 15, cy - 15, dot_size, colors.dark);
      circle_draw(cx + 15, cy - 15, dot_size, colors.dark);
      circle_draw(cx - 15, cy + 15, dot_size, colors.dark);
      circle_draw(cx + 15, cy + 15, dot_size, colors.dark);
      break;
   case 5:
      circle_draw(cx - 15, cy - 15, dot_size, colors.dark);
      circle_draw(cx + 15, cy - 15, dot_size, colors.dark);
      circle_draw(cx, cy, dot_size, colors.dark);
      circle_draw(cx - 15, cy + 15, dot_size, colors.dark);
      circle_draw(cx + 15, cy + 15, dot_size, colors.dark);
      break;
   case 6:
      circle_draw(cx - 15, cy - 20, dot_size, colors.dark);
      circle_draw(cx + 15, cy - 20, dot_size, colors.dark);
      circle_draw(cx - 15, cy, dot_size, colors.dark);
      circle_draw(cx + 15, cy, dot_size, colors.dark);
      circle_draw(cx - 15, cy + 20, dot_size, colors.dark);
      circle_draw(cx + 15, cy + 20, dot_size, colors.dark);
      break;
   }

   if (game.cursor_on_dice &&
       !game.rolling && !game.moving &&
       game.questionState == NO_QUESTION &&
       !game.preGameTimerActive)
   {
      center_text_draw("Click to Roll!", cx, animatedRect.y - 13,
                       colors.gold, small_font);
   }
}

void players_draw()
{
   for (int i = 0; i < 2; i++)
   {
      SDL_Point p = get_cell_pos(game.players[i].pos);
      int cx = p.x + CELL / 2 + game.players[i].offsetX;
      int cy = p.y + CELL / 2 + game.players[i].offsetY;

      filled_circle_draw(cx + 3, cy + 3, 18, SDL_Color{0, 0, 0, 90});
      filled_circle_draw(cx, cy, 15, game.players[i].color);

      if (i == game.current && game.state == PLAYING)
      {
         SDL_SetRenderDrawColor(renderer, 255, 255, 255, 200);
         for (int a = 0; a < 360; a += 10)
         {
            double rad = a * M_PI / 180.0;
            int x = cx + (int)(18 * cos(rad));
            int y = cy + (int)(18 * sin(rad));
            SDL_RenderDrawPoint(renderer, x, y);
         }
      }
      center_text_draw(to_string(i + 1), cx, cy, colors.white, medium_font ? medium_font : small_font);
   }
}

void UI_draw()
{
   center_text_draw("THE SNADDER GAME", width / 2, 40,
                    colors.gold, gigantic_font ? gigantic_font : medium_font);

   int rightX = BOARD_X + BOARD_SIZE + 30;

   if (game.questionState == ASKING_QUESTION)
   {
      question_box_draw();
   }
   else
   {
      SDL_Rect upper_panel = {rightX, BOARD_Y, 280, 200};
      SDL_SetRenderDrawColor(renderer, colors.dark.r, colors.dark.g, colors.dark.b, 150);
      SDL_RenderFillRect(renderer, &upper_panel);
      SDL_SetRenderDrawColor(renderer, colors.white.r, colors.white.g, colors.white.b, 100);
      SDL_RenderDrawRect(renderer, &upper_panel);

      if (game.show_hints && game.cursor_on_cell > 0)
      {
         string info;
         if (snakes.count(game.cursor_on_cell))
         {
            info = "Snake to " + to_string(snakes[game.cursor_on_cell]);
         }
         else if (ladders.count(game.cursor_on_cell))
         {
            info = "Ladder to " + to_string(ladders[game.cursor_on_cell]);
         }
         if (!info.empty())
         {
            int cx, cy;
            SDL_GetMouseState(&cx, &cy);
            SDL_Rect bg{cx, cy - 25, 150, 30};
            SDL_SetRenderDrawColor(renderer, 30, 30, 40, 240);
            SDL_RenderFillRect(renderer, &bg);
            SDL_SetRenderDrawColor(renderer, 200, 200, 100, 255);
            SDL_RenderDrawRect(renderer, &bg);
            text_draw(info, cx + 5, cy - 20,
                      SDL_Color{255, 255, 200, 255}, small_font);
         }
      }

      center_text_draw(" QUESTION CHALLENGE  ",
                       upper_panel.x + upper_panel.w / 2,
                       upper_panel.y + 30,
                       colors.gold, medium_font);
      center_text_draw("When you land on snakes or",
                       upper_panel.x + upper_panel.w / 2,
                       upper_panel.y + 60,
                       colors.white, small_font);
      center_text_draw("ladders, answer questions to :",
                       upper_panel.x + upper_panel.w / 2,
                       upper_panel.y + 80,
                       colors.white, small_font);
      center_text_draw(" Avoid snake slides",
                       upper_panel.x + upper_panel.w / 2,
                       upper_panel.y + 110,
                       colors.white, small_font);
      center_text_draw(" Climb ladders",
                       upper_panel.x + upper_panel.w / 2,
                       upper_panel.y + 130,
                       colors.white, small_font);
   }

   SDL_Rect lower_panel = playing_lower_panel_rect();
   SDL_SetRenderDrawColor(renderer, colors.dark.r, colors.dark.g, colors.dark.b, 200);
   SDL_RenderFillRect(renderer, &lower_panel);
   SDL_SetRenderDrawColor(renderer, colors.white.r, colors.white.g, colors.white.b, 100);
   SDL_RenderDrawRect(renderer, &lower_panel);

   string turnText =
       (game.players[game.current].name.empty()
            ? ("Player " + to_string(game.current + 1))
            : game.players[game.current].name) +
       "'s Turn";
   center_text_draw(turnText,
                    lower_panel.x + lower_panel.w / 2,
                    lower_panel.y + 30,
                    colors.white, medium_font);
   SDL_Rect dice_rect = playing_dice_rect();
   dice_draw(dice_rect);

   if (!game.rolling && !game.moving && game.questionState == NO_QUESTION)
   {
      center_text_draw("Click dice to roll!",
                       lower_panel.x + lower_panel.w / 2,
                       lower_panel.y + 180,
                       colors.white, small_font);
   }
   else if (game.rolling)
   {
      center_text_draw("Rolling...",
                       lower_panel.x + lower_panel.w / 2,
                       lower_panel.y + 180,
                       colors.gold, small_font);
   }
   else if (game.moving)
   {
      center_text_draw("Moving...",
                       lower_panel.x + lower_panel.w / 2,
                       lower_panel.y + 180,
                       colors.green, small_font);
   }
   else if (game.questionState == ASKING_QUESTION)
   {
      center_text_draw("Answer the question!",
                       lower_panel.x + lower_panel.w / 2,
                       lower_panel.y + 180,
                       colors.gold, small_font);
   }

   center_text_draw("Positions:",
                    lower_panel.x + lower_panel.w / 2,
                    lower_panel.y + 220,
                    colors.white, medium_font);

   for (int i = 0; i < 2; i++)
   {
      string score =
          (game.players[i].name.empty()
               ? ("Player " + to_string(i + 1))
               : game.players[i].name) +
          ": " + to_string(game.players[i].pos);

      text_draw(score,
                lower_panel.x + 20,
                lower_panel.y + 250 + i * 30,
                game.players[i].color, medium_font ? medium_font : small_font);
   }

   center_text_draw("Game Stats:",
                    lower_panel.x + lower_panel.w / 2,
                    lower_panel.y + 320,
                    colors.white, medium_font);

   string diceText = "Last Roll: " + to_string(game.dice);
   center_text_draw(diceText,
                    lower_panel.x + lower_panel.w / 2,
                    lower_panel.y + 350,
                    colors.white, small_font);
}

/* RULES – dynamic responsive box */
void rules_draw()
{
   // read current window size (in case of resize)
   int winW = width, winH = height;
   SDL_GetWindowSize(window, &winW, &winH);
   width = winW;
   height = winH;

   // content lines
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
       "• Easy   = 30 sec timer",
       "• Medium = 15 sec timer",
       "• Hard   =  7 sec timer",

       "",
       "QUESTION OPTIONS:",
       "• You can choose 2..6 options per question from Settings.",
       "• UI will adapt (grid) and ensure correct option present.",

       "",
       "CONTROLS:",
       "• Mouse Left Click = Buttons, Dice, Answer Choices",
       "• SPACE = Roll dice",
       "• H = Toggle cell hints",
       "• A/B/C.. = Answer question",
       "• R = Restart after win",

       "",
       "MENUS:",
       "• Tap to Start → Begin game",
       "• Rules → Show this page",
       "• Settings → Sound & timers & option count",
       "• Difficulty → Select mode",
       "• Exit → Quit game"};

   // dynamic sizing: box takes 72% width, 70% height (clamped)
   int boxW = max(600, (int)(winW * 0.72));
   int boxH = max(360, (int)(winH * 0.7));

   if (boxW > winW - 80)
      boxW = winW - 80;
   if (boxH > winH - 80)
      boxH = winH - 80;
   int boxX = (winW - boxW) / 2;
   int boxY = (winH - boxH) / 2;
   SDL_Rect box = {boxX, boxY, boxW, boxH};

   // draw box and border
   SDL_SetRenderDrawColor(renderer, 245, 245, 245, 255);
   SDL_RenderFillRect(renderer, &box);
   SDL_SetRenderDrawColor(renderer, 50, 50, 80, 255);
   SDL_RenderDrawRect(renderer, &box);

   center_text_draw("GAME RULES", box.x + box.w / 2, box.y + 36, colors.bg2, gigantic_font ? gigantic_font : medium_font);

   // content area inside box (with padding)
   int pad = 28;
   int contentLeft = box.x + pad;
   int contentTop = box.y + 80;
   int contentRight = box.x + box.w - pad - 20; // leave space for scrollbar
   int contentBottom = box.y + box.h - 80;      // leave space for back button and hint
   int contentW = contentRight - contentLeft;
   int viewHeight = contentBottom - contentTop;

   // Determine text line height using font metrics
   int tmpw = 0, tmph = 0;
   TTF_SizeUTF8(medium_font ? medium_font : small_font, "Ay", &tmpw, &tmph);
   int lineHeight = max(18, tmph + 8);

   // total content height
   int totalContentHeight = (int)lines.size() * lineHeight;
   rulesScrollMax = max(0, totalContentHeight - viewHeight);

   if (rulesScroll < 0)
      rulesScroll = 0;
   if (rulesScroll > rulesScrollMax)
      rulesScroll = rulesScrollMax;

   // DO clipping so content never draws outside the content area
   SDL_Rect clipRect = {contentLeft, contentTop, contentW, viewHeight};
   SDL_RenderSetClipRect(renderer, &clipRect);

   int y = contentTop - rulesScroll;
   for (const string &s : lines)
   {
      text_draw(s, contentLeft, y, colors.dark, medium_font);
      y += lineHeight;
   }

   // remove clip
   SDL_RenderSetClipRect(renderer, nullptr);

   // scrollbar track
   if (rulesScrollMax > 0)
   {
      int barWidth = 12;
      int trackX = box.x + box.w - pad - barWidth;
      int trackY = contentTop;
      int trackH = viewHeight;
      SDL_Rect track = {trackX, trackY, barWidth, trackH};
      SDL_SetRenderDrawColor(renderer, 220, 220, 220, 200);
      SDL_RenderFillRect(renderer, &track);

      float ratio = (float)viewHeight / (float)totalContentHeight;
      int barH = max((int)(trackH * ratio), 30);
      float posRatio = (rulesScrollMax > 0) ? (float)rulesScroll / (float)rulesScrollMax : 0.0f;
      int barY = trackY + (int)((trackH - barH) * posRatio);
      SDL_Rect bar = {trackX, barY, barWidth, barH};
      SDL_SetRenderDrawColor(renderer, 140, 140, 140, 255);
      SDL_RenderFillRect(renderer, &bar);
   }

   // BACK button positioned inside box at bottom-center, below content area
   SDL_Rect backBtn = {box.x + (box.w / 2) - 90, box.y + box.h - 56, 180, 44};
   button_draw(backBtn, "BACK", colors.p1, colors.white, false, medium_font);

   // small note/hint below back button (if space)
   // center_text_draw("Use mouse wheel or drag to scroll", box.x + box.w / 2, box.y + box.h - 20, colors.dark, small_font);
}

/* SETTINGS, ABOUT, etc (mostly same) */
void settings_draw()
{
   SDL_Rect box = {width / 2 - 300, height / 2 - 200, 600, 400};
   SDL_SetRenderDrawColor(renderer, 240, 240, 240, 255);
   SDL_RenderFillRect(renderer, &box);
   SDL_SetRenderDrawColor(renderer, colors.dark.r, colors.dark.g, colors.dark.b, 255);
   SDL_RenderDrawRect(renderer, &box);

   center_text_draw("SETTINGS", width / 2, box.y + 40,
                    colors.dark, gigantic_font ? gigantic_font : medium_font);

   int bx = box.x + 100;
   int by = box.y + 100;

   SDL_Rect soundBtn = {bx, by, 400, 50};
   SDL_Rect diceBtn = {bx, by + 60, 400, 50};
   SDL_Rect preBtn = {bx, by + 120, 400, 50};
   SDL_Rect backBtn = {bx, by + 200, 400, 50};

   button_draw(soundBtn,
               string("Sound: ") + (game.soundEnabled ? "ON" : "OFF"),
               game.soundEnabled ? colors.green : SDL_Color{180, 180, 180, 255},
               colors.white, false, medium_font);

   button_draw(diceBtn,
               string("Dice Sound: ") + (game.diceSoundEnabled ? "ON" : "OFF"),
               game.diceSoundEnabled ? colors.bg2 : SDL_Color{180, 180, 180, 255},
               colors.white, false, medium_font);

   // Question option count selectors (2..6)
   int selY = by + 180;
   text_draw("Question options:", bx, selY + 70, colors.dark, medium_font);
   int optX = bx + 200;
   for (int i = 2; i <= 6; ++i)
   {
      SDL_Rect optRect = {optX + (i - 2) * 52, selY + 60, 44, 36};
      bool hover = false;
      SDL_Color bg = (g_questionOptionCount == i) ? colors.bg1 : SDL_Color{220, 220, 220, 255};
      button_draw(optRect, to_string(i), bg, colors.dark, hover, medium_font);
   }

   button_draw(preBtn,
               "Pre-Game Countdown: " + to_string(game.preGameDefaultCountdown) + " sec",
               colors.bg2, colors.white, false, medium_font);

   button_draw(backBtn, "BACK", colors.p1, colors.white, false, medium_font);
}

void about_draw()
{
   SDL_Rect box = {width / 2 - 350, height / 2 - 280, 700, 560};
   SDL_Rect shadow = {box.x + 8, box.y + 8, box.w, box.h};
   SDL_SetRenderDrawColor(renderer, 0, 0, 0, 100);
   SDL_RenderFillRect(renderer, &shadow);

   SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
   SDL_RenderFillRect(renderer, &box);

   SDL_SetRenderDrawColor(renderer, 255, 100, 50, 255);
   for (int i = 0; i < 8; i++)
   {
      SDL_Rect b = {box.x - i, box.y - i, box.w + 2 * i, box.h + 2 * i};
      SDL_RenderDrawRect(renderer, &b);
   }

   center_text_draw("THE SNADDER GAME", width / 2,
                    box.y + 50, colors.bg2, gigantic_font ? gigantic_font : medium_font);
   center_text_draw("Version 1.0 | November 2025",
                    width / 2, box.y + 90,
                    colors.dark, small_font);

   int yy = box.y + 110;

   center_text_draw("A modern educational take on the classic Snakes & Ladders board game.",
                    width / 2, yy, colors.dark, small_font);
   yy += 32;
   center_text_draw("Combines luck with programming knowledge.",
                    width / 2, yy, colors.dark, small_font);
   yy += 32;
   center_text_draw("Land on a snake or ladder? Answer MCQs to avoid penalties or climb faster!",
                    width / 2, yy, colors.dark, small_font);
   yy += 40;

   center_text_draw("KEY FEATURES:", width / 2, yy,
                    colors.bg2, medium_font);
   yy += 32;

   center_text_draw("• Select 2..6 options per question in Settings", width / 2, yy, colors.dark, small_font);
   yy += 28;
   center_text_draw("• Questions load from questions.csv (fallback DB included)", width / 2, yy, colors.dark, small_font);
   yy += 28;
   center_text_draw("• Realistic snakes, smooth movement, sound effects", width / 2, yy, colors.dark, small_font);
   yy += 28;
   center_text_draw("© 2025 Aditya • Koushik • Bintu . All rights reserved.", width / 2, yy, colors.dark, small_font);

   SDL_Rect backBtn = {box.x + (box.w / 2) - 90, box.y + box.h - 60, 180, 40};
   button_draw(backBtn, "BACK", colors.p1, colors.white, false, medium_font);
}

/* Control functions */

void start_question_challenge(int pos)
{
   // Centralized: prepare and cache options ONCE when question starts
   game.questionState = ASKING_QUESTION;
   game.special_cell = pos;
   game.isSnake = snakes.count(pos) > 0;
   if (questions.empty())
      return;
   game.curr_question_index = rand() % questions.size();
   game.select_ans = 0;
   game.landed_special_cell = true;

   game.questionStartTime = SDL_GetTicks();
   game.questionTimeLeft = game.questionDefaultTime;
   game.questionTimeout = false;

   // Prepare options ONCE and cache them for the UI and answer handling
   auto prep = prepare_options_for_ui(questions[game.curr_question_index], g_questionOptionCount);
   game.question_ui_options = prep.first;
   game.question_ui_correct = prep.second;
}

void q_ans_handle_index(int chosenIndex) // chosenIndex is 0-based in prepared options
{
   if (game.questionState != ASKING_QUESTION)
      return;
   if (questions.empty())
      return;

   Question &q = questions[game.curr_question_index];

   // Use cached options/correct index prepared earlier
   int correctIdxInFinal = game.question_ui_correct;

   bool correct = (chosenIndex >= 0 && chosenIndex == correctIdxInFinal);
   int pos = game.players[game.current].pos;

   if (!correct)
   {
      if (game.isSnake)
      {
         if (snakes.count(pos))
            game.players[game.current].pos = snakes[pos];

         if (game.soundEnabled && snd_snake)
            Mix_PlayChannel(-1, snd_snake, 0);
      }
      else
      {
         if (game.soundEnabled && snd_error)
            Mix_PlayChannel(-1, snd_error, 0);
      }
   }
   else
   {
      if (!game.isSnake)
      {
         if (ladders.count(pos))
            game.players[game.current].pos = ladders[pos];
      }
      if (game.soundEnabled && snd_correct)
         Mix_PlayChannel(-1, snd_correct, 0);
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

   // Clear question state and cached UI options so next question reshuffles
   game.questionState = NO_QUESTION;
   game.landed_special_cell = false;
   game.question_ui_options.clear();
   game.question_ui_correct = -1;
}

void initGame()
{
   game.players[0] = {1, colors.p1,
                      game.players[0].name.empty() ? "Player 1" : game.players[0].name,
                      -15, -15};
   game.players[1] = {1, colors.p2,
                      game.players[1].name.empty() ? "Player 2" : game.players[1].name,
                      15, 15};

   game.current = 0;
   game.state = PLAYING;
   game.moving = false;
   game.rolling = false;
   game.cell_preview = 0;
   game.cursor_on_cell = 0;
   game.questionState = NO_QUESTION;
   game.landed_special_cell = false;

   if (game.difficulty == EASY)
      game.questionDefaultTime = 30;
   else if (game.difficulty == MEDIUM)
      game.questionDefaultTime = 15;
   else
      game.questionDefaultTime = 7;
}

void dice_roll()
{
   if (game.rolling || game.moving ||
       game.questionState == ASKING_QUESTION ||
       game.preGameTimerActive)
      return;

   game.rolling = true;
   game.rollStart = SDL_GetTicks();
   game.dice_animating = true;
   game.dice_anime_start = SDL_GetTicks();

   if (game.soundEnabled && game.diceSoundEnabled && snd_roll)
      Mix_PlayChannel(-1, snd_roll, 0);
}

void dice_update()
{
   if (!game.rolling)
      return;

   unsigned int elapsed = SDL_GetTicks() - game.rollStart;
   if (elapsed < 1500)
   {
      game.dice = (rand() % 6) + 1;
   }
   else
   {
      game.rolling = false;
      game.dice = (rand() % 6) + 1;
      int target = game.players[game.current].pos + game.dice;
      if (target <= 100)
      {
         game.move_path.clear();
         for (int i = game.players[game.current].pos + 1; i <= target; i++)
         {
            game.move_path.push_back(i);
         }
         game.moving = true;
         game.last_move = SDL_GetTicks();
      }
      else
      {
         game.current = 1 - game.current;
      }
   }
}

void movement_update()
{
   if (!game.moving || game.move_path.empty())
      return;

   if (SDL_GetTicks() - game.last_move > 300)
   {
      game.players[game.current].pos = game.move_path[0];
      game.move_path.erase(game.move_path.begin());
      game.last_move = SDL_GetTicks();

      if (game.soundEnabled && snd_move)
         Mix_PlayChannel(-1, snd_move, 0);

      if (game.move_path.empty())
      {
         int pos = game.players[game.current].pos;
         if (snakes.count(pos) || ladders.count(pos))
         {
            // centralized: start question which will cache options once
            start_question_challenge(pos);
         }
         else
         {
            if (pos == 100)
            {
               game.state = WINNER;
               if (game.soundEnabled && snd_win)
                  Mix_PlayChannel(-1, snd_win, 0);
            }
            else
            {
               game.current = 1 - game.current;
            }
         }
         game.moving = false;
      }
   }
}

void handle_name_input(const string &input)
{
   if (input == "BACKSPACE" && !game.tempName.empty())
   {
      game.tempName.pop_back();
   }
   else if (input == "ENTER" || input == "CONTINUE")
   {
      if (!game.tempName.empty())
      {
         game.players[game.input_player].name = game.tempName;
         game.tempName.clear();
         game.input_player++;

         if (game.input_player >= 2)
         {
            game.preGameStartTime = SDL_GetTicks();
            game.preGameTimerActive = true;
            game.preGameCountdown = game.preGameDefaultCountdown;
            initGame();
         }
      }
   }
   else if (input.length() == 1 && game.tempName.length() < 15)
   {
      game.tempName += input;
   }
}

bool isPointInRect(int x, int y, SDL_Rect rect)
{
   return x >= rect.x && x < rect.x + rect.w &&
          y >= rect.y && y < rect.y + rect.h;
}

void difficulty_select_draw()
{
   SDL_Rect box = {width / 2 - 250, height / 2 - 180, 500, 360};
   rounded_rect_draw(box, colors.white, 25);

   center_text_draw("SELECT DIFFICULTY", width / 2,
                    box.y + 40, colors.bg2, gigantic_font ? gigantic_font : medium_font);

   SDL_Rect easyBtn = {width / 2 - 150, box.y + 100, 300, 60};
   SDL_Rect mediumBtn = {width / 2 - 150, box.y + 170, 300, 60};
   SDL_Rect hardBtn = {width / 2 - 150, box.y + 240, 300, 60};
   SDL_Rect backBtn = {width / 2 - 150, box.y + 310, 300, 40};

   button_draw(easyBtn, "EASY", colors.green, colors.white, hover_easy, large_font ? large_font : medium_font);
   button_draw(mediumBtn, "MEDIUM", colors.bg1, colors.white, hover_medium, large_font ? large_font : medium_font);
   button_draw(hardBtn, "HARD", colors.p1, colors.white, hover_hard, large_font ? large_font : medium_font);
   button_draw(backBtn, "BACK", colors.dark, colors.white, hover_back_diff, medium_font);
}

/* Mouse handling (updated: options selection in settings and question popup) */
void mouse_click_handle(int mx, int my)
{
   auto play_click = []()
   {
      if (game.soundEnabled && snd_click)
         Mix_PlayChannel(-1, snd_click, 0);
   };

   switch (game.state)
   {
   case WELCOME:
   {
      SDL_Rect startBtn = welcome_start_btn();
      SDL_Rect rulesBtn = welcome_rules_btn();
      SDL_Rect settingsBtn = welcome_settings_btn();
      SDL_Rect aboutBtn = welcome_about_btn();

      if (isPointInRect(mx, my, startBtn))
      {
         play_click();
         game.state = DIFFICULTY_SELECT;
         return;
      }
      if (isPointInRect(mx, my, rulesBtn))
      {
         play_click();
         game.state = RULES;
         rulesScroll = 0; // reset scroll on open
         return;
      }
      if (isPointInRect(mx, my, settingsBtn))
      {
         play_click();
         game.state = SETTINGS;
         return;
      }
      if (isPointInRect(mx, my, aboutBtn))
      {
         play_click();
         game.state = ABOUT;
         return;
      }
      break;
   }

   case NAME_INPUT:
   {
      SDL_Rect continue_button = name_continue_button_rect();
      if (isPointInRect(mx, my, continue_button) && !game.tempName.empty())
      {
         if (game.soundEnabled && snd_click)
            Mix_PlayChannel(-1, snd_click, 0);
         handle_name_input("CONTINUE");
      }
      break;
   }

   case DIFFICULTY_SELECT:
   {
      SDL_Rect box = {width / 2 - 250, height / 2 - 180, 500, 360};

      SDL_Rect easyBtn = {width / 2 - 150, box.y + 100, 300, 60};
      SDL_Rect mediumBtn = {width / 2 - 150, box.y + 170, 300, 60};
      SDL_Rect hardBtn = {width / 2 - 150, box.y + 240, 300, 60};
      SDL_Rect backBtn = {width / 2 - 150, box.y + 310, 300, 40};

      if (isPointInRect(mx, my, easyBtn))
      {
         play_click();
         game.difficulty = EASY;
         game.state = NAME_INPUT;
         return;
      }
      if (isPointInRect(mx, my, mediumBtn))
      {
         play_click();
         game.difficulty = MEDIUM;
         game.state = NAME_INPUT;
         return;
      }
      if (isPointInRect(mx, my, hardBtn))
      {
         play_click();
         game.difficulty = HARD;
         game.state = NAME_INPUT;
         return;
      }
      if (isPointInRect(mx, my, backBtn))
      {
         play_click();
         game.state = WELCOME;
         return;
      }
      break;
   }

   case SETTINGS:
   {
      SDL_Rect box = {width / 2 - 300, height / 2 - 200, 600, 400};
      int bx = box.x + 100;
      int by = box.y + 100;

      SDL_Rect soundBtn = {bx, by, 400, 50};
      SDL_Rect diceBtn = {bx, by + 60, 400, 50};
      SDL_Rect preBtn = {bx, by + 120, 400, 50};

      // Move BACK further down so it doesn't overlap options
      SDL_Rect backBtn = {bx, by + 240, 400, 50}; // <- moved from by+200 to by+240

      if (isPointInRect(mx, my, soundBtn))
      {
         play_click();
         game.soundEnabled = !game.soundEnabled;
         return;
      }
      if (isPointInRect(mx, my, diceBtn))
      {
         play_click();
         game.diceSoundEnabled = !game.diceSoundEnabled;
         return;
      }

      // MATCH the drawing Y: drawing uses selY = by + 120, options at selY + 60 => by + 180
      int optBaseY = by + 180; // <-- FIXED: match settings_draw()
      for (int i = 2; i <= 6; ++i)
      {
         SDL_Rect optRect = {bx + 200 + (i - 2) * 52, optBaseY, 44, 36};
         if (isPointInRect(mx, my, optRect))
         {
            play_click();
            g_questionOptionCount = i;
            return;
         }
      }

      if (isPointInRect(mx, my, preBtn))
      {
         play_click();
         int &v = game.preGameDefaultCountdown;
         if (v == 3)
            v = 5;
         else if (v == 5)
            v = 10;
         else if (v == 10)
            v = 15;
         else
            v = 3;
         return;
      }
      if (isPointInRect(mx, my, backBtn))
      {
         play_click();
         game.state = WELCOME;
         return;
      }
      break;
   }

   case PLAYING:
   {
      if (game.questionState == ASKING_QUESTION)
      {
         // Use cached UI options to determine which option was clicked
         auto &finalOpts = game.question_ui_options;
         int optCount = (int)finalOpts.size();
         if (optCount > 0)
         {
            int cols = (optCount <= 2) ? optCount : 2;
            int rows = (optCount + cols - 1) / cols;
            int cardW = min(760, width - 200);
            int cardH = min(380, height - 200);
            SDL_Rect card = {width / 2 - cardW / 2, height / 2 - cardH / 2, cardW, cardH};
            int optW = (card.w - 60 - (cols - 1) * 12) / cols;
            int optH = 44;
            int startY = card.y + 80 + 28 + 8;

            int idx2 = 0;
            for (int rr = 0; rr < rows; ++rr)
            {
               int yy = startY + rr * (optH + 12);
               for (int cc = 0; cc < cols; ++cc)
               {
                  if (idx2 >= optCount)
                     break;
                  int xx = card.x + 30 + cc * (optW + 12);
                  SDL_Rect rect = {xx, yy, optW, optH};
                  if (isPointInRect(mx, my, rect))
                  {
                     if (game.soundEnabled && snd_click)
                        Mix_PlayChannel(-1, snd_click, 0);
                     q_ans_handle_index(idx2);
                     return;
                  }
                  idx2++;
               }
            }
         }
      }

      SDL_Rect dice_rect = playing_dice_rect();
      if (isPointInRect(mx, my, dice_rect))
      {
         if (game.soundEnabled && snd_click)
            Mix_PlayChannel(-1, snd_click, 0);
         dice_roll();
      }

      int c = cursor_to_cell(mx, my);
      if (c > 0)
      {
         if (snakes.count(c) || ladders.count(c))
         {
            game.cell_preview = (game.cell_preview == c) ? 0 : c;
         }
         else
         {
            game.cell_preview = 0;
         }
      }
      break;
   }

   case RULES:
   {
      // dynamic box coordinates (same as rules_draw)
      int winW = width, winH = height;
      SDL_GetWindowSize(window, &winW, &winH);
      int boxW = max(600, (int)(width * 0.72));
      int boxH = max(360, (int)(height * 0.75));
      if (boxW > winW - 80)
         boxW = winW - 80;
      if (boxH > winH - 80)
         boxH = winH - 80;
      int boxX = (winW - boxW) / 2;
      int boxY = (winH - boxH) / 2;
      SDL_Rect backBtn = {boxX + (boxW / 2) - 90, boxY + boxH - 56, 180, 44};

      if (isPointInRect(mx, my, backBtn))
      {
         if (game.soundEnabled && snd_click)
            Mix_PlayChannel(-1, snd_click, 0);
         game.state = WELCOME;
         return;
      }
      break;
   }

   case ABOUT:
   {
      SDL_Rect box = {width / 2 - 350, height / 2 - 280, 700, 560};
      SDL_Rect backBtn = {box.x + (box.w / 2) - 150, box.y + box.h - 60, 300, 50};

      if (isPointInRect(mx, my, backBtn))
      {
         if (game.soundEnabled && snd_click)
            Mix_PlayChannel(-1, snd_click, 0);
         game.state = WELCOME;
      }
      break;
   }

   case WINNER:
   {
      if (game.soundEnabled && snd_click)
         Mix_PlayChannel(-1, snd_click, 0);
      game.state = WELCOME;
      game.input_player = 0;
      game.players[0].name.clear();
      game.players[1].name.clear();
      break;
   }
   }
}

void mouse_motion_handel(int x, int y)
{
   switch (game.state)
   {
   case WELCOME:
   {
      SDL_Rect startBtn = welcome_start_btn();
      SDL_Rect rulesBtn = welcome_rules_btn();
      SDL_Rect settingsBtn = welcome_settings_btn();
      SDL_Rect aboutBtn = welcome_about_btn();

      hover_start = isPointInRect(x, y, startBtn);
      hover_rules = isPointInRect(x, y, rulesBtn);
      hover_settings = isPointInRect(x, y, settingsBtn);
      hover_exit = isPointInRect(x, y, aboutBtn);
      break;
   }

   case DIFFICULTY_SELECT:
   {
      SDL_Rect box = {width / 2 - 250, height / 2 - 180, 500, 360};

      SDL_Rect easyBtn = {width / 2 - 150, box.y + 100, 300, 60};
      SDL_Rect mediumBtn = {width / 2 - 150, box.y + 170, 300, 60};
      SDL_Rect hardBtn = {width / 2 - 150, box.y + 240, 300, 60};
      SDL_Rect backBtn = {width / 2 - 150, box.y + 310, 300, 40};

      hover_easy = isPointInRect(x, y, easyBtn);
      hover_medium = isPointInRect(x, y, mediumBtn);
      hover_hard = isPointInRect(x, y, hardBtn);
      hover_back_diff = isPointInRect(x, y, backBtn);
      break;
   }

   case PLAYING:
   {
      SDL_Rect dice_rect = playing_dice_rect();
      game.cursor_on_dice = isPointInRect(x, y, dice_rect);

      game.cursor_on_cell = cursor_to_cell(x, y);

      break;
   }

   default:
      break;
   }
}

int main(int argc, char **argv)
{
   srand((unsigned int)time(nullptr));

   if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0)
   {
      fprintf(stderr, "SDL_Init Error: %s\n", SDL_GetError());
      return 1;
   }
   if (TTF_Init() != 0)
   {
      fprintf(stderr, "TTF_Init Error: %s\n", TTF_GetError());
      SDL_Quit();
      return 1;
   }
   if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) != 0)
   {
      fprintf(stderr, "Mix_OpenAudio Error: %s\n", Mix_GetError());
      TTF_Quit();
      SDL_Quit();
      return 1;
   }

   window = SDL_CreateWindow(
       "Aditya - Snadder Game",
       SDL_WINDOWPOS_CENTERED,
       SDL_WINDOWPOS_CENTERED,
       width,
       height,
       SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
   if (!window)
   {
      fprintf(stderr, "SDL_CreateWindow Error: %s\n", SDL_GetError());
      Mix_CloseAudio();
      TTF_Quit();
      SDL_Quit();
      return 1;
   }

   renderer = SDL_CreateRenderer(window, -1,
                                 SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
   if (!renderer)
   {
      fprintf(stderr, "SDL_CreateRenderer Error: %s\n", SDL_GetError());
      SDL_DestroyWindow(window);
      Mix_CloseAudio();
      TTF_Quit();
      SDL_Quit();
      return 1;
   }

   small_font = TTF_OpenFont("assets/font/newspaper.ttf", 16);
   medium_font = TTF_OpenFont("assets/font/good_timing_bd.ttf", 22);
   large_font = TTF_OpenFont("assets/font/Pixel_Game.ttf", 30);
   gigantic_font = TTF_OpenFont("assets/font/KnightWarrior.ttf", 70);

   if (!small_font)
      fprintf(stderr, "Warning: small_font failed to load: %s\n", TTF_GetError());
   if (!medium_font)
      fprintf(stderr, "Warning: medium_font failed to load: %s\n", TTF_GetError());
   if (!large_font)
      fprintf(stderr, "Warning: large_font failed to load: %s\n", TTF_GetError());
   if (!gigantic_font)
      fprintf(stderr, "Warning: gigantic_font failed to load: %s\n", TTF_GetError());

   snd_roll = Mix_LoadWAV("assets/sound/roll.wav");
   snd_move = Mix_LoadWAV("assets/sound/move.wav");
   snd_win = Mix_LoadWAV("assets/sound/win.wav");
   snd_snake = Mix_LoadWAV("assets/sound/snake.wav");
   snd_correct = Mix_LoadWAV("assets/sound/correct.wav");
   snd_click = Mix_LoadWAV("assets/sound/click.wav");
   snd_error = Mix_LoadWAV("assets/sound/error.wav");

   // load questions from CSV if present
   if (!load_questions_from_csv("questions.csv"))
   {
      load_default_questions();
   }

   if (!snd_roll)
      fprintf(stderr, "Warning: snd_roll failed to load: %s\n", Mix_GetError());
   if (!snd_move)
      fprintf(stderr, "Warning: snd_move failed to load: %s\n", Mix_GetError());
   if (!snd_win)
      fprintf(stderr, "Warning: snd_win failed to load: %s\n", Mix_GetError());
   if (!snd_snake)
      fprintf(stderr, "Warning: snd_snake failed to load: %s\n", Mix_GetError());
   if (!snd_correct)
      fprintf(stderr, "Warning: snd_correct failed to load: %s\n", Mix_GetError());
   if (!snd_click)
      fprintf(stderr, "Warning: snd_click failed to load: %s\n", Mix_GetError());
   if (!snd_error)
      fprintf(stderr, "Warning: snd_error failed to load: %s\n", Mix_GetError());

   bool quit = false;
   SDL_Event e;
   unsigned int last_time = SDL_GetTicks();

   SDL_StartTextInput();

   while (!quit)
   {
      unsigned int curr_time = SDL_GetTicks();
      float delta_time = (curr_time - last_time) / 1000.0f;
      last_time = curr_time;
      game.animation_time += delta_time;

      while (SDL_PollEvent(&e))
      {
         if (e.type == SDL_QUIT)
            quit = true;

         if (e.type == SDL_WINDOWEVENT)
         {
            if (e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
            {
               width = e.window.data1;
               height = e.window.data2;
            }
         }

         if (e.type == SDL_KEYDOWN)
         {
            if (game.state == NAME_INPUT)
            {
               if (e.key.keysym.sym == SDLK_BACKSPACE)
                  handle_name_input("BACKSPACE");
               else if (e.key.keysym.sym == SDLK_RETURN)
                  handle_name_input("ENTER");
            }
            else if (game.state == PLAYING)
            {
               if (e.key.keysym.sym == SDLK_SPACE)
                  dice_roll();
               if (e.key.keysym.sym == SDLK_h)
                  game.show_hints = !game.show_hints;
               if (game.questionState == ASKING_QUESTION)
               {
                  if (e.key.keysym.sym >= SDLK_a && e.key.keysym.sym <= SDLK_f)
                  {
                     int idx = e.key.keysym.sym - SDLK_a;
                     q_ans_handle_index(idx);
                  }
               }
            }
            else if (game.state == WINNER)
            {
               if (e.key.keysym.sym == SDLK_r)
               {
                  game.state = WELCOME;
                  game.input_player = 0;
                  game.players[0].name.clear();
                  game.players[1].name.clear();
               }
            }

            if (e.key.keysym.sym == SDLK_ESCAPE)
            {
               game.state = WELCOME;
            }
         }

         if (e.type == SDL_TEXTINPUT && game.state == NAME_INPUT)
         {
            handle_name_input(e.text.text);
         }
         if (e.type == SDL_MOUSEBUTTONDOWN)
         {
            mouse_click_handle(e.button.x, e.button.y);
         }
         if (e.type == SDL_MOUSEMOTION)
         {
            mouse_motion_handel(e.motion.x, e.motion.y);
         }
         if (e.type == SDL_MOUSEWHEEL && (game.state == RULES))
         {
            // vertical wheel positive = up, negative = down
            if (e.wheel.y > 0)
               rulesScroll -= rulesScrollStep;
            if (e.wheel.y < 0)
               rulesScroll += rulesScrollStep;
            if (rulesScroll < 0)
               rulesScroll = 0;
            if (rulesScroll > rulesScrollMax)
               rulesScroll = rulesScrollMax;
         }
      }

      if (game.requestQuit)
      {
         quit = true;
      }

      if (game.state == PLAYING)
      {
         if (game.preGameTimerActive)
         {
            unsigned int elapsed = (SDL_GetTicks() - game.preGameStartTime) / 1000;
            game.preGameCountdown = max(0, game.preGameDefaultCountdown - (int)elapsed);

            if (game.preGameCountdown == 0)
            {
               game.preGameTimerActive = false;
            }
         }

         if (game.questionState == ASKING_QUESTION)
         {
            unsigned int elapsed = (SDL_GetTicks() - game.questionStartTime) / 1000;
            game.questionTimeLeft = max(0, game.questionDefaultTime - (int)elapsed);

            if (game.questionTimeLeft == 0 && !game.questionTimeout)
            {
               game.questionTimeout = true;
               q_ans_handle_index(-1); // invalid index => wrong
            }
         }

         dice_update();
         movement_update();
      }

      background_draw();

      switch (game.state)
      {
      case WELCOME:
         welcome_draw();
         break;
      case DIFFICULTY_SELECT:
         difficulty_select_draw();
         break;
      case NAME_INPUT:
         name_input_box();
         break;
      case SETTINGS:
         settings_draw();
         break;
      case RULES:
         rules_draw();
         break;
      case ABOUT:
         about_draw();
         break;
      case PLAYING:
      {
         board_draw();
         for (auto &l : ladders)
            ladder_draw(l.first, l.second);
         for (auto &s : snakes)
         {
            const snake_design &vis = snake_style[s.first];
            bool highlight =
                (game.show_hints && (game.cursor_on_cell == s.first)) ||
                (game.cell_preview == s.first);
            realistic_snake_draw(s.first, s.second, vis,
                                 game.animation_time, highlight);
         }
         players_draw();
         UI_draw();

         if (game.preGameTimerActive)
         {
            SDL_Rect overlay = {0, 0, width, height};
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 160);
            SDL_RenderFillRect(renderer, &overlay);

            center_text_draw(to_string(game.preGameCountdown),
                             width / 2, height / 2,
                             colors.gold, gigantic_font ? gigantic_font : medium_font);
         }
         break;
      }
      case WINNER:
      {
         board_draw();
         for (auto &l : ladders)
            ladder_draw(l.first, l.second);
         for (auto &s : snakes)
         {
            const snake_design &vis = snake_style[s.first];
            realistic_snake_draw(s.first, s.second, vis,
                                 game.animation_time, false);
         }
         players_draw();
         UI_draw();

         SDL_Rect overlay = {0, 0, width, height};
         SDL_SetRenderDrawColor(renderer, 0, 0, 0, 150);
         SDL_RenderFillRect(renderer, &overlay);

         SDL_Rect win_box = {width / 2 - 200, height / 2 - 100, 400, 200};
         SDL_SetRenderDrawColor(renderer,
                                colors.white.r, colors.white.g, colors.white.b, 255);
         SDL_RenderFillRect(renderer, &win_box);
         SDL_SetRenderDrawColor(renderer,
                                colors.gold.r, colors.gold.g, colors.gold.b, 255);
         for (int i = 0; i < 5; i++)
         {
            SDL_Rect border = {win_box.x - i, win_box.y - i,
                               win_box.w + 2 * i, win_box.h + 2 * i};
            SDL_RenderDrawRect(renderer, &border);
         }

         string win_text =
             (game.players[game.current].name.empty()
                  ? ("Player " + to_string(game.current + 1))
                  : game.players[game.current].name) +
             " WINS!";
         center_text_draw(win_text, width / 2,
                          height / 2 - 20, colors.gold, gigantic_font ? gigantic_font : medium_font);
         center_text_draw("CONGRATULATIONS!",
                          width / 2, height / 2 + 20,
                          colors.dark, medium_font);
         center_text_draw("Click anywhere or press R to play again",
                          width / 2, height / 2 + 60,
                          colors.dark, small_font);
         break;
      }
      }

      SDL_RenderPresent(renderer);
      SDL_Delay(16);
   }

   SDL_StopTextInput();

   if (small_font)
      TTF_CloseFont(small_font);
   if (medium_font)
      TTF_CloseFont(medium_font);
   if (large_font)
      TTF_CloseFont(large_font);
   if (gigantic_font)
      TTF_CloseFont(gigantic_font);

   if (snd_roll)
      Mix_FreeChunk(snd_roll);
   if (snd_move)
      Mix_FreeChunk(snd_move);
   if (snd_win)
      Mix_FreeChunk(snd_win);
   if (snd_snake)
      Mix_FreeChunk(snd_snake);
   if (snd_correct)
      Mix_FreeChunk(snd_correct);
   if (snd_click)
      Mix_FreeChunk(snd_click);
   if (snd_error)
      Mix_FreeChunk(snd_error);

   SDL_DestroyRenderer(renderer);
   SDL_DestroyWindow(window);
   Mix_CloseAudio();
   TTF_Quit();
   SDL_Quit();
   return 0;
}
