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

/* Hover Flags */
bool hover_start = false;
bool hover_practice = false;
bool hover_rules = false;
bool hover_settings = false;
bool hover_exit = false;

bool hover_easy = false;
bool hover_medium = false;
bool hover_hard = false;
bool hover_back_diff = false;

bool hover_2p = false;
bool hover_3p = false;
bool hover_4p = false;
bool hover_vs_cpu = false;
bool hover_back_p = false;

/* Board data */
map<int, int> snakes = {
    {16, 6}, {47, 26}, {49, 11}, {56, 53}, {62, 19}, {64, 60}, {87, 24}, {93, 73}, {95, 75}, {98, 78}};

map<int, int> ladders = {
    {2, 37}, {4, 14}, {9, 21}, {39, 57}, {28, 84}, {36, 44}, {51, 67}, {71, 91}, {80, 97}};

/* Question Database (From 70.cpp) */
struct Question
{
   string question;
   vector<string> options; // multiple options (2..6)
   int ans_index;          // index into options which is correct (0-based)
};

vector<Question> questions; // loaded from CSV or fallback

// Global setting for option count
int g_questionOptionCount = 4; // default 4

enum QuestionState
{
   NO_QUESTION,
   ASKING_QUESTION
};

int rulesScroll = 0;
int rulesScrollMax = 0;
int rulesScrollStep = 40;

/* Game Design */
struct Colors
{
   SDL_Color bg1{41, 128, 185, 255};   // Ocean Blue
   SDL_Color bg2{155, 89, 182, 255};   // Amethyst
   SDL_Color GRID{236, 240, 241, 255}; // Clouds
   SDL_Color cell1{52, 152, 219, 255}; // Bright Blue
   SDL_Color cell2{155, 89, 182, 255}; // Purple

   // Player Colors
   SDL_Color p1{231, 76, 60, 255};  // Red
   SDL_Color p2{46, 204, 113, 255}; // Green
   SDL_Color p3{52, 152, 219, 255}; // Blue
   SDL_Color p4{243, 156, 18, 255}; // Orange

   SDL_Color gold{241, 196, 15, 255}; // Gold
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

enum State
{
   WELCOME,
   DIFFICULTY_SELECT,
   PLAYER_SELECT,
   NAME_INPUT,
   SETTINGS,
   RULES,
   PLAYING,
   WINNER,
   ABOUT,
   PRACTICE_TOPIC_SELECT,
   PRACTICE_MODE
};

struct Player
{
   int pos = 1;
   SDL_Color color;
   string name;
   int offsetX, offsetY;
   bool isBot = false;
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
   State state = WELCOME;
   Player players[4];
   int current = 0;
   int total_players = 2;

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

   // CACHED UI Options (Crucial for 70.cpp logic)
   vector<string> question_ui_options;
   int question_ui_correct = -1;

   unsigned int questionStartTime = 0;
   int questionTimeLeft = 30;
   bool questionTimeout = false;
   /* Forward Declarations */
   void q_ans_handle_index(int chosenIndex);
   void dice_roll();
   void rounded_rect_draw(SDL_Rect rect, SDL_Color col, int radius);
   bool isPointInRect(int x, int y, SDL_Rect rect);

   // ADD THIS LINE HERE:
   void background_draw();
   // settings
   bool soundEnabled = true;
   bool diceSoundEnabled = true;

   // pre-game
   unsigned int preGameStartTime = 0;
   bool preGameTimerActive = false;
   int preGameCountdown = 3;
   int questionDefaultTime = 30;
   int preGameDefaultCountdown = 3;

   bool requestQuit = false;

   // Practice Mode
   int practice_topic = 0;
   vector<vector<int>> topic_questions; // Stores indices of questions
   int practice_question_index = 0;
   int practice_score = 0;
   int practice_total_answered = 0;

   // BOT Logic
   unsigned int botActionTimer = 0;
   bool botWaitingToAnswer = false;
} game;

/* Forward Declarations */
void q_ans_handle_index(int chosenIndex);
void dice_roll();
void rounded_rect_draw(SDL_Rect rect, SDL_Color col, int radius);
bool isPointInRect(int x, int y, SDL_Rect rect);

/* Utils */
float lerpf(float a, float b, float t) { return a + (b - a) * t; }

/* Drawing Helpers */
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
      font = medium_font;
   center_text_draw(text, rect.x + rect.w / 2, rect.y + rect.h / 2, fg, font);
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
void welcome_draw()
{
   // Draw background first
   background_draw();

   SDL_Rect welcome_box = {width / 2 - 300, height / 2 - 260, 600, 520};

   // Shadow
   SDL_Rect shadow = {welcome_box.x + 10, welcome_box.y + 10, welcome_box.w, welcome_box.h};
   SDL_SetRenderDrawColor(renderer, 0, 0, 0, 80);
   SDL_RenderFillRect(renderer, &shadow);

   // Main Box
   SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
   SDL_RenderFillRect(renderer, &welcome_box);
   SDL_SetRenderDrawColor(renderer, colors.dark.r, colors.dark.g, colors.dark.b, 255);
   SDL_RenderDrawRect(renderer, &welcome_box);

   // Title
   center_text_draw("THE SNADDER GAME", width / 2, welcome_box.y + 60, colors.p1, gigantic_font);
   center_text_draw("Ultimate Edition", width / 2, welcome_box.y + 110, colors.dark, large_font);

   // Buttons
   int centerX = width / 2;
   int btnY = welcome_box.y + 160;

   SDL_Rect startBtn = {centerX - 120, btnY, 280, 70};
   SDL_Rect practiceBtn = {centerX - 120, btnY + 90, 280, 60};
   SDL_Rect rulesBtn = {centerX - 120, btnY + 165, 280, 60};
   SDL_Rect settingsBtn = {centerX - 120, btnY + 240, 280, 60};
   SDL_Rect aboutBtn = {centerX - 120, btnY + 315, 280, 50};

   button_draw(startBtn, "PLAY GAME", colors.green, colors.white, hover_start, large_font);
   button_draw(practiceBtn, "PRACTICE MODE", colors.p3, colors.white, hover_practice, medium_font);
   button_draw(rulesBtn, "RULES", colors.bg1, colors.white, hover_rules, medium_font);
   button_draw(settingsBtn, "SETTINGS", colors.bg2, colors.white, hover_settings, medium_font);
   button_draw(aboutBtn, "ABOUT", colors.p1, colors.white, hover_exit, medium_font);

   center_text_draw("v1.2", welcome_box.x + welcome_box.w - 30, welcome_box.y + welcome_box.h - 20, colors.dark, small_font);
}

/* Board Helpers */
SDL_Point
get_cell_pos(int cell)
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
   if (cell < 1 || cell > 100)
      return 0;
   return cell;
}

/* CSV & Question Loading (From 70.cpp) */
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
   // Fallback questions if CSV is missing
   questions.push_back({"Which variable stores whole numbers?", {"int", "float", "double"}, 0});
   questions.push_back({"Loop for known iterations?", {"for", "while", "do"}, 0});
   questions.push_back({"Function return type for no value?", {"void", "int", "char"}, 0});
   questions.push_back({"Header for printf?", {"stdio.h", "math.h", "string.h"}, 0});
   questions.push_back({"First array index?", {"0", "1", "-1"}, 0});
   questions.push_back({"Keyword for constant variable?", {"const", "static", "volatile"}, 0});
   questions.push_back({"Exit switch statement?", {"break", "continue", "exit"}, 0});
   questions.push_back({"Allocate memory dynamically?", {"malloc", "free", "calloc"}, 0});
   questions.push_back({"Equality operator?", {"==", "=", "!="}, 0});
   questions.push_back({"Store single character?", {"char", "int", "string"}, 0});
}

/* Logic: Prepare Options UI (From 70.cpp) */
pair<vector<string>, int> prepare_options_for_ui(const Question &q, int displayCount)
{
   displayCount = max(2, min(6, displayCount));
   vector<string> candidates;

   // Add existing options
   for (const auto &s : q.options)
   {
      if (find(candidates.begin(), candidates.end(), s) == candidates.end())
         candidates.push_back(s);
   }

   // Fill with dummies if needed
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

   // Pad with N/A if still not enough
   while ((int)candidates.size() < displayCount)
      candidates.push_back("N/A");

   // Identify Correct Answer
   string correctStr = "";
   if (q.ans_index >= 0 && q.ans_index < (int)q.options.size())
      correctStr = q.options[q.ans_index];

   // Ensure correct answer is in candidates
   bool hadCorrect = (!correctStr.empty() && find(candidates.begin(), candidates.end(), correctStr) != candidates.end());
   if (!hadCorrect && !correctStr.empty())
   {
      if (!candidates.empty())
         candidates.back() = correctStr;
      else
         candidates.push_back(correctStr);
   }

   // Shuffle
   static std::mt19937 rng((unsigned)time(nullptr));
   shuffle(candidates.begin(), candidates.end(), rng);

   // Trim to display count
   vector<string> finalOpts;
   for (size_t i = 0; i < candidates.size() && (int)finalOpts.size() < displayCount; ++i)
   {
      finalOpts.push_back(candidates[i]);
   }

   // Find new index of correct answer
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
   // Fallback safety
   if (newAns == -1 && !correctStr.empty())
   {
      finalOpts[0] = correctStr;
      newAns = 0;
   }

   return {finalOpts, newAns};
}

/* Drawing Functions */
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
         SDL_Color c = (i == 100) ? SDL_Color{200, 0, 0, 255} : SDL_Color{60, 40, 100, 255};
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

void realistic_snake_draw(int startCell, int endCell, const snake_design &style, float timeSec, bool highlight = false)
{
   SDL_Point a = get_cell_pos(startCell);
   SDL_Point b = get_cell_pos(endCell);
   float ax = (float)(a.x + CELL / 2), ay = (float)(a.y + CELL / 2);
   float bx = (float)(b.x + CELL / 2), by = (float)(b.y + CELL / 2);
   float dx = bx - ax, dy = by - ay;
   float dist = sqrt(dx * dx + dy * dy);
   int samples = max(24, (int)(dist / 12));
   samples = min(samples, 300);
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
         bodyCol = SDL_Color{(Uint8)min(255, bodyCol.r + 40), (Uint8)min(255, bodyCol.g + 40), (Uint8)min(255, bodyCol.b + 40), 255};
      }
      SDL_Color belly = {(Uint8)(bodyCol.r / 1.2f), (Uint8)(bodyCol.g / 1.2f), (Uint8)(bodyCol.b / 1.2f), 220};
      filled_eclipse_draw((int)(cx + px * 2), (int)(cy + py * 2), (int)(r), (int)(r * 0.75f), belly);
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
      SDL_RenderDrawLine(renderer, (int)tx, (int)ty, (int)(tx + cos(forkAng) * 8), (int)(ty + sin(forkAng) * 8));
      forkAng = headAngle - 0.25f;
      SDL_RenderDrawLine(renderer, (int)tx, (int)ty, (int)(tx + cos(forkAng) * 8), (int)(ty + sin(forkAng) * 8));
   }
}

/* DYNAMIC QUESTION UI (from 70.cpp) */
void question_box_draw()
{
   if (game.questionState != ASKING_QUESTION && game.state != PRACTICE_MODE)
      return;

   // Check bounds safety for Practice Mode
   if (game.state == PRACTICE_MODE)
   {
      if (game.topic_questions[game.practice_topic].empty())
         return;
   }

   // Dim background
   SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
   SDL_SetRenderDrawColor(renderer, 0, 0, 0, 160);
   SDL_Rect overlay = {0, 0, width, height};
   SDL_RenderFillRect(renderer, &overlay);

   // FIX 1: CALCULATE POSITION RELATIVE TO BOARD, NOT SCREEN
   // Board is 600px wide. We make card 540px wide to fit nicely inside.
   int cardW = 540;
   int cardH = 380;
   // Center X = board_left + (board_size/2) - (card_width/2)
   int cardX = board_left + (BOARD_SIZE / 2) - (cardW / 2);
   int cardY = height / 2 - cardH / 2;

   SDL_Rect card = {cardX, cardY, cardW, cardH};

   rounded_rect_draw(card, colors.white, 18);

   // Title
   string title = "CHALLENGE!";
   if (game.state == PRACTICE_MODE)
      title = "PRACTICE MODE";
   else if (game.isSnake)
      title = "SNAKE CHALLENGE!";
   else
      title = "LADDER CHALLENGE!";

   center_text_draw(title, card.x + card.w / 2, card.y + 34, colors.p1, large_font);

   // Timer (if not practice)
   if (game.state == PLAYING)
   {
      string tstr = to_string(game.questionTimeLeft) + "s";
      center_text_draw(tstr, card.x + card.w - 50, card.y + 40, colors.gold, large_font);
   }
   else
   {
      string s = "Score: " + to_string(game.practice_score);
      center_text_draw(s, card.x + card.w - 90, card.y + 40, colors.gold, medium_font);
   }

   // Bot Message
   if (game.state == PLAYING && game.players[game.current].isBot)
   {
      center_text_draw("Computer is thinking...", card.x + card.w / 2, card.y + card.h / 2, colors.dark, medium_font);
      return;
   }

   // Display Question Text (Wrapped)
   const Question &q = questions[game.curr_question_index];
   int qidx = 0;
   if (game.state == PRACTICE_MODE)
   {
      // SAFETY CHECK: Ensure vector is not empty before modulo
      size_t sz = game.topic_questions[game.practice_topic].size();
      if (sz > 0)
         qidx = game.topic_questions[game.practice_topic][game.practice_question_index % sz];
      else
         qidx = 0; // Fallback

      if (game.question_ui_options.empty())
      {
         auto p = prepare_options_for_ui(questions[qidx], g_questionOptionCount);
         game.question_ui_options = p.first;
         game.question_ui_correct = p.second;
      }
   }

   int wrapX = card.x + 28;
   int wrapW = card.w - 56;
   int lineY = card.y + 80;

   string textToWrap = (game.state == PRACTICE_MODE) ? questions[qidx].question : q.question;

   // --- WORD WRAP LOGIC ---
   vector<string> words;
   string w;
   for (char ch : textToWrap)
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

   string curLine;
   int lineHeight = 28;
   for (size_t i = 0; i < words.size(); ++i)
   {
      string test = curLine.empty() ? words[i] : curLine + " " + words[i];
      int tw = 0, th = 0;
      TTF_SizeUTF8(medium_font, test.c_str(), &tw, &th);
      if (tw > wrapW && !curLine.empty())
      {
         text_draw(curLine, wrapX, lineY, colors.dark, medium_font);
         lineY += lineHeight;
         curLine = words[i];
      }
      else
      {
         curLine = test;
      }
   }
   if (!curLine.empty())
   {
      text_draw(curLine, wrapX, lineY, colors.dark, medium_font);
      lineY += lineHeight;
   }
   // -----------------------

   // Draw Options
   vector<string> &opts = game.question_ui_options;
   int optCount = (int)opts.size();
   int cols = (optCount <= 2) ? optCount : 2;
   int rows = (optCount + cols - 1) / cols;
   int optW = (card.w - 60 - (cols - 1) * 12) / cols;
   int optH = 44;
   int startY = lineY + 18;

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
         button_draw(rect, label, bg, colors.white, hover, medium_font);
         optIndex++;
      }
   }

   // FIX 2: ADD SKIP BUTTON
   SDL_Rect skipBtn = {card.x + card.w / 2 - 60, card.y + card.h - 50, 120, 35};
   int mx, my;
   SDL_GetMouseState(&mx, &my);
   bool hoverSkip = isPointInRect(mx, my, skipBtn);
   button_draw(skipBtn, "SKIP", hoverSkip ? colors.snake : colors.ladder, colors.white, hoverSkip, medium_font);
}

void player_select_draw()
{
   SDL_Rect box = {width / 2 - 250, height / 2 - 220, 500, 440};
   rounded_rect_draw(box, colors.white, 25);
   center_text_draw("SELECT PLAYERS", width / 2, box.y + 40, colors.bg2, gigantic_font);

   SDL_Rect btn2 = {width / 2 - 150, box.y + 90, 300, 50};
   SDL_Rect btn3 = {width / 2 - 150, box.y + 150, 300, 50};
   SDL_Rect btn4 = {width / 2 - 150, box.y + 210, 300, 50};
   SDL_Rect btnVS = {width / 2 - 150, box.y + 270, 300, 50};
   SDL_Rect backBtn = {width / 2 - 150, box.y + 350, 300, 40};

   button_draw(btn2, "2 PLAYERS", colors.green, colors.white, hover_2p, large_font);
   button_draw(btn3, "3 PLAYERS", colors.p3, colors.white, hover_3p, large_font);
   button_draw(btn4, "4 PLAYERS", colors.p4, colors.white, hover_4p, large_font);
   button_draw(btnVS, "VS COMPUTER", colors.bg1, colors.white, hover_vs_cpu, large_font);
   button_draw(backBtn, "BACK", colors.dark, colors.white, hover_back_p, medium_font);
}

void name_input_box()
{
   SDL_Rect input_box = {width / 2 - 250, height / 2 - 150, 500, 300};
   SDL_Rect shadow = {input_box.x + 5, input_box.y + 5, input_box.w, input_box.h};
   SDL_SetRenderDrawColor(renderer, 0, 0, 0, 100);
   SDL_RenderFillRect(renderer, &shadow);
   SDL_SetRenderDrawColor(renderer, colors.white.r, colors.white.g, colors.white.b, 240);
   SDL_RenderFillRect(renderer, &input_box);
   SDL_SetRenderDrawColor(renderer, colors.dark.r, colors.dark.g, colors.dark.b, 255);
   SDL_RenderDrawRect(renderer, &input_box);
   string title = "Enter Player " + to_string(game.input_player + 1) + " Name:";
   center_text_draw(title, width / 2, input_box.y + 50, colors.dark, large_font);
   SDL_Rect name_field = {width / 2 - 150, input_box.y + 100, 300, 40};
   SDL_SetRenderDrawColor(renderer, colors.GRID.r, colors.GRID.g, colors.GRID.b, 255);
   SDL_RenderFillRect(renderer, &name_field);
   SDL_SetRenderDrawColor(renderer, colors.dark.r, colors.dark.g, colors.dark.b, 255);
   SDL_RenderDrawRect(renderer, &name_field);
   string display_name = game.tempName.empty() ? "Please Enter name..." : game.tempName;
   SDL_Color nameColor = game.tempName.empty() ? SDL_Color{150, 150, 150, 255} : colors.dark;
   text_draw(display_name, name_field.x + 10, name_field.y + 10, nameColor, medium_font);
   if (SDL_GetTicks() % 1000 < 500)
   {
      int textW = 0;
      if (!game.tempName.empty())
         TTF_SizeUTF8(medium_font, game.tempName.c_str(), &textW, nullptr);
      SDL_RenderDrawLine(renderer, name_field.x + 10 + textW, name_field.y + 5, name_field.x + 10 + textW, name_field.y + 30);
   }
   SDL_Rect continue_button = {width / 2 - 75, input_box.y + 180, 150, 40};

   // LOGIC FIX: Always allow continue. If empty, we will auto-fill later.
   bool canContinue = true;
   SDL_Color btnColor = colors.green; // Always green

   button_draw(continue_button, "CONTINUE", btnColor, colors.white, false, medium_font);

   // Change instruction text to tell user they can skip
   center_text_draw("Type name or click CONTINUE for default", width / 2, input_box.y + 250, colors.dark, small_font);
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
         animOffset = (int)(sin(elapsed * 0.02f) * 5);
      else
         game.dice_animating = false;
   }
   SDL_Rect animatedRect = {dice_rect.x + animOffset, dice_rect.y + animOffset, dice_rect.w, dice_rect.h};
   for (int i = 0; i < animatedRect.h; i++)
   {
      int brightness = game.cursor_on_dice ? 255 - (i * 10 / animatedRect.h) : 255 - (i * 20 / animatedRect.h);
      SDL_SetRenderDrawColor(renderer, brightness, brightness, brightness, 255);
      SDL_RenderDrawLine(renderer, animatedRect.x, animatedRect.y + i, animatedRect.x + animatedRect.w, animatedRect.y + i);
   }
   SDL_SetRenderDrawColor(renderer, 0, 0, 0, 200);
   for (int i = 0; i < 3; i++)
   {
      SDL_Rect border = {animatedRect.x - i, animatedRect.y - i, animatedRect.w + 2 * i, animatedRect.h + 2 * i};
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
   if (game.cursor_on_dice && !game.rolling && !game.moving && game.questionState == NO_QUESTION && !game.preGameTimerActive && !game.players[game.current].isBot)
      center_text_draw("Click to Roll!", cx, animatedRect.y - 13, colors.gold, small_font);
}

void players_draw()
{
   for (int i = 0; i < game.total_players; i++)
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
      center_text_draw(to_string(i + 1), cx, cy, colors.white, medium_font);
   }
}

void UI_draw()
{
   center_text_draw("THE SNADDER GAME", width / 2, 40, colors.gold, gigantic_font);
   int rightX = BOARD_X + BOARD_SIZE + 30;

   // If asking question, draw question box (popup style)
   if (game.questionState == ASKING_QUESTION)
   {
      question_box_draw();
      // Draw sidebar underneath (dimmed) or partially
   }

   SDL_Rect upper_panel = {rightX, BOARD_Y, 280, 200};
   SDL_SetRenderDrawColor(renderer, colors.dark.r, colors.dark.g, colors.dark.b, 150);
   SDL_RenderFillRect(renderer, &upper_panel);
   SDL_SetRenderDrawColor(renderer, colors.white.r, colors.white.g, colors.white.b, 100);
   SDL_RenderDrawRect(renderer, &upper_panel);

   if (game.show_hints && game.cursor_on_cell > 0)
   {
      string info;
      if (snakes.count(game.cursor_on_cell))
         info = "Snake to " + to_string(snakes[game.cursor_on_cell]);
      else if (ladders.count(game.cursor_on_cell))
         info = "Ladder to " + to_string(ladders[game.cursor_on_cell]);
      if (!info.empty())
      {
         int cx, cy;
         SDL_GetMouseState(&cx, &cy);
         SDL_Rect bg{cx, cy - 25, 150, 30};
         SDL_SetRenderDrawColor(renderer, 30, 30, 40, 240);
         SDL_RenderFillRect(renderer, &bg);
         SDL_SetRenderDrawColor(renderer, 200, 200, 100, 255);
         SDL_RenderDrawRect(renderer, &bg);
         text_draw(info, cx + 5, cy - 20, SDL_Color{255, 255, 200, 255}, small_font);
      }
   }
   center_text_draw(" QUESTION CHALLENGE  ", upper_panel.x + upper_panel.w / 2, upper_panel.y + 30, colors.gold, medium_font);
   center_text_draw("When you land on snakes or", upper_panel.x + upper_panel.w / 2, upper_panel.y + 60, colors.white, small_font);
   center_text_draw("ladders, answer questions to :", upper_panel.x + upper_panel.w / 2, upper_panel.y + 80, colors.white, small_font);
   center_text_draw(" Avoid snake slides", upper_panel.x + upper_panel.w / 2, upper_panel.y + 110, colors.white, small_font);
   center_text_draw(" Climb ladders", upper_panel.x + upper_panel.w / 2, upper_panel.y + 130, colors.white, small_font);

   SDL_Rect lower_panel = {rightX, BOARD_Y + 220, 280, 380};
   SDL_SetRenderDrawColor(renderer, colors.dark.r, colors.dark.g, colors.dark.b, 200);
   SDL_RenderFillRect(renderer, &lower_panel);
   SDL_SetRenderDrawColor(renderer, colors.white.r, colors.white.g, colors.white.b, 100);
   SDL_RenderDrawRect(renderer, &lower_panel);

   string turnText = (game.players[game.current].name.empty() ? ("Player " + to_string(game.current + 1)) : game.players[game.current].name) + "'s Turn";
   center_text_draw(turnText, lower_panel.x + lower_panel.w / 2, lower_panel.y + 30, colors.white, medium_font);

   SDL_Rect dice_rect = {lower_panel.x + 100, lower_panel.y + 70, 80, 80};
   dice_draw(dice_rect);

   if (!game.rolling && !game.moving && game.questionState == NO_QUESTION)
   {
      if (game.players[game.current].isBot)
         center_text_draw("Computer rolling...", lower_panel.x + lower_panel.w / 2, lower_panel.y + 180, colors.gold, small_font);
      else
         center_text_draw("Click dice to roll!", lower_panel.x + lower_panel.w / 2, lower_panel.y + 180, colors.white, small_font);
   }
   else if (game.rolling)
   {
      center_text_draw("Rolling...", lower_panel.x + lower_panel.w / 2, lower_panel.y + 180, colors.gold, small_font);
   }
   else if (game.moving)
   {
      center_text_draw("Moving...", lower_panel.x + lower_panel.w / 2, lower_panel.y + 180, colors.green, small_font);
   }
   else if (game.questionState == ASKING_QUESTION)
   {
      center_text_draw("Answer the question!", lower_panel.x + lower_panel.w / 2, lower_panel.y + 180, colors.gold, small_font);
   }

   center_text_draw("Positions:", lower_panel.x + lower_panel.w / 2, lower_panel.y + 220, colors.white, medium_font);

   int spacing = (game.total_players > 2) ? 25 : 30;
   int startY = (game.total_players > 2) ? 245 : 250;
   TTF_Font *listFont = (game.total_players > 2) ? small_font : medium_font;

   for (int i = 0; i < game.total_players; i++)
   {
      string score = (game.players[i].name.empty() ? ("Player " + to_string(i + 1)) : game.players[i].name) + ": " + to_string(game.players[i].pos);
      text_draw(score, lower_panel.x + 20, lower_panel.y + startY + i * spacing, game.players[i].color, listFont);
   }

   center_text_draw("Game Stats:", lower_panel.x + lower_panel.w / 2, lower_panel.y + 340, colors.white, medium_font);
   string diceText = "Last Roll: " + to_string(game.dice);
   center_text_draw(diceText, lower_panel.x + lower_panel.w / 2, lower_panel.y + 365, colors.white, small_font);
}

void rules_draw()
{
   vector<string> lines = {
       "HOW TO PLAY",
       "• Players take turns rolling the dice",
       "• Click dice or press SPACE to roll",
       "• First to reach 100 wins!",
       "",
       "QUESTION SYSTEM:",
       "• You can choose 2..6 options per question in Settings",
       "• Land on Ladder/Snake -> Answer Question",
       "• Correct: Climb Ladder / Avoid Snake",
       "• Wrong: No Climb / Slide Down Snake",
       "",
       "VS COMPUTER",
       "• Easy: Computer gets answers WRONG often",
       "• Hard: Computer answers correctly",
       "",
   };
   SDL_Rect box = {width / 2 - 350, height / 2 - 280, 700, 560};
   SDL_SetRenderDrawColor(renderer, 245, 245, 245, 255);
   SDL_RenderFillRect(renderer, &box);
   SDL_SetRenderDrawColor(renderer, 50, 50, 80, 255);
   SDL_RenderDrawRect(renderer, &box);
   center_text_draw("GAME RULES", width / 2, box.y + 50, colors.bg2, gigantic_font);
   int lineHeight = 24;
   int contentTop = box.y + 110;
   int contentBottom = box.y + box.h - 100;
   int viewHeight = contentBottom - contentTop;
   int totalContentHeight = (int)lines.size() * lineHeight;
   rulesScrollMax = max(0, totalContentHeight - viewHeight);
   if (rulesScroll < 0)
      rulesScroll = 0;
   if (rulesScroll > rulesScrollMax)
      rulesScroll = rulesScrollMax;
   int y = contentTop - rulesScroll;
   for (const string &s : lines)
   {
      center_text_draw(s, width / 2, y, colors.dark, medium_font);
      y += lineHeight;
   }
   SDL_Rect backBtn = {box.x + (box.w / 2) - 150, box.y + box.h - 60, 300, 50};
   button_draw(backBtn, "BACK", colors.p1, colors.white, false, medium_font);
}

void settings_draw()
{
   SDL_Rect box = {width / 2 - 300, height / 2 - 200, 600, 400};
   SDL_SetRenderDrawColor(renderer, 240, 240, 240, 255);
   SDL_RenderFillRect(renderer, &box);
   SDL_SetRenderDrawColor(renderer, colors.dark.r, colors.dark.g, colors.dark.b, 255);
   SDL_RenderDrawRect(renderer, &box);
   center_text_draw("SETTINGS", width / 2, box.y + 40, colors.dark, gigantic_font);
   int bx = box.x + 100;
   int by = box.y + 100;
   SDL_Rect soundBtn = {bx, by, 400, 50};
   SDL_Rect diceBtn = {bx, by + 60, 400, 50};
   SDL_Rect preBtn = {bx, by + 120, 400, 50};

   button_draw(soundBtn, string("Sound: ") + (game.soundEnabled ? "ON" : "OFF"), game.soundEnabled ? colors.green : SDL_Color{180, 180, 180, 255}, colors.white, false, medium_font);
   button_draw(diceBtn, string("Dice Sound: ") + (game.diceSoundEnabled ? "ON" : "OFF"), game.diceSoundEnabled ? colors.bg2 : SDL_Color{180, 180, 180, 255}, colors.white, false, medium_font);
   button_draw(preBtn, "Pre-Game Countdown: " + to_string(game.preGameDefaultCountdown) + " sec", colors.bg2, colors.white, false, medium_font);

   // Option Count Selector (From 70.cpp)
   int selY = by + 180;
   text_draw("Question options:", bx, selY + 20, colors.dark, medium_font);
   int optX = bx + 200;
   for (int i = 2; i <= 6; ++i)
   {
      SDL_Rect optRect = {optX + (i - 2) * 45, selY + 10, 40, 36};
      bool hover = false;
      SDL_Color bg = (g_questionOptionCount == i) ? colors.bg1 : SDL_Color{220, 220, 220, 255};
      button_draw(optRect, to_string(i), bg, colors.dark, hover, medium_font);
   }

   SDL_Rect backBtn = {bx, by + 240, 400, 50};
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
   center_text_draw("THE SNADDER GAME", width / 2, box.y + 50, colors.bg2, gigantic_font);
   center_text_draw("Version 1.2 | Merged Edition", width / 2, box.y + 90, colors.dark, small_font);
   int yy = box.y + 110;
   center_text_draw("Multiplayer, Bot Mode & Dynamic Questions", width / 2, yy, colors.dark, small_font);
   yy += 40;
   center_text_draw("Combines 4-player support with advanced settings.", width / 2, yy, colors.dark, small_font);
   yy += 32;
   center_text_draw("Select 2-6 options per question in Settings.", width / 2, yy, colors.dark, small_font);
   yy += 40;
   center_text_draw("CREDITS:", width / 2, yy, colors.bg2, medium_font);
   yy += 32;
   center_text_draw("Aditya • Koushik • Bintu", width / 2, yy, colors.dark, small_font);
   int padding_bottom = 5;
   SDL_Rect backBtn = {box.x + (box.w / 2) - 90, box.y + box.h - padding_bottom - 40, 180, 35};
   button_draw(backBtn, "BACK", colors.p1, colors.white, false, medium_font);
}

/* LOGIC */

void start_question_challenge(int pos)
{
   game.questionState = ASKING_QUESTION;
   game.special_cell = pos;
   game.isSnake = snakes.count(pos) > 0;

   // Select random question
   game.curr_question_index = rand() % questions.size();
   game.landed_special_cell = true;
   game.questionStartTime = SDL_GetTicks();
   game.questionTimeLeft = game.questionDefaultTime;
   game.questionTimeout = false;
   game.botWaitingToAnswer = true;

   // PREPARE OPTIONS ONCE (Shuffle and cache)
   auto p = prepare_options_for_ui(questions[game.curr_question_index], g_questionOptionCount);
   game.question_ui_options = p.first;
   game.question_ui_correct = p.second;
}

void q_ans_handle_index(int chosenIndex)
{
   if (game.questionState != ASKING_QUESTION)
      return;

   // Use cached correct index
   bool correct = (chosenIndex == game.question_ui_correct);
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
      game.current = (game.current + 1) % game.total_players;
      game.botActionTimer = SDL_GetTicks();
   }
   game.questionState = NO_QUESTION;
   game.landed_special_cell = false;

   // Clear cache
   game.question_ui_options.clear();
   game.question_ui_correct = -1;
}

void initGame()
{
   // Player 1 (Red, Top Left)
   game.players[0] = {1, colors.p1, game.players[0].name.empty() ? "Player 1" : game.players[0].name, -15, -15, false};

   // Player 2 (Green, Top Right)
   string p2name = game.players[1].name.empty() ? "Player 2" : game.players[1].name;
   if (game.players[1].isBot)
      p2name = "Computer";
   game.players[1] = {1, colors.p2, p2name, 15, -15, game.players[1].isBot};

   // Player 3 (Blue, Bottom Left)
   game.players[2] = {1, colors.p3, game.players[2].name.empty() ? "Player 3" : game.players[2].name, -15, 15, false};
   // Player 4 (Orange, Bottom Right)
   game.players[3] = {1, colors.p4, game.players[3].name.empty() ? "Player 4" : game.players[3].name, 15, 15, false};

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

   game.botActionTimer = SDL_GetTicks();
}

void dice_roll()
{
   if (game.rolling || game.moving || game.questionState == ASKING_QUESTION || game.preGameTimerActive)
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
         game.current = (game.current + 1) % game.total_players;
         game.botActionTimer = SDL_GetTicks(); // Reset bot timer
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
            start_question_challenge(pos);
            game.botActionTimer = SDL_GetTicks(); // Reset for bot delay before answering
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
               game.current = (game.current + 1) % game.total_players;
               game.botActionTimer = SDL_GetTicks(); // Reset bot timer
            }
         }
         game.moving = false;
      }
   }
}

void handle_name_input(const string &input)
{
   // Skip bot names
   if (game.players[game.input_player].isBot)
   {
      game.input_player++;
      if (game.input_player >= game.total_players)
      {
         game.preGameStartTime = SDL_GetTicks();
         game.preGameTimerActive = true;
         game.preGameCountdown = game.preGameDefaultCountdown;
         initGame();
      }
      return;
   }

   if (input == "BACKSPACE" && !game.tempName.empty())
   {
      game.tempName.pop_back();
   }
   else if (input == "ENTER" || input == "CONTINUE")
   {
      // LOGIC FIX: Even if tempName is empty, proceed and set default
      string finalName = game.tempName;
      if (finalName.empty())
      {
         finalName = "Player " + to_string(game.input_player + 1);
      }

      game.players[game.input_player].name = finalName;
      game.tempName.clear();
      game.input_player++;

      // If next player is bot, skip input
      while (game.input_player < game.total_players && game.players[game.input_player].isBot)
      {
         game.input_player++;
      }

      if (game.input_player >= game.total_players)
      {
         game.preGameStartTime = SDL_GetTicks();
         game.preGameTimerActive = true;
         game.preGameCountdown = game.preGameDefaultCountdown;
         initGame();
      }
   }
   else if (input.length() == 1 && game.tempName.length() < 15)
   {
      game.tempName += input;
   }
}
/* Updated Bot Brain Logic */
void bot_update()
{
   if (game.state != PLAYING || game.preGameTimerActive)
      return;
   if (!game.players[game.current].isBot)
      return;

   // 1. Roll Dice
   if (!game.rolling && !game.moving && game.questionState == NO_QUESTION)
   {
      if (SDL_GetTicks() - game.botActionTimer > 1000)
      {
         dice_roll();
      }
   }

   // 2. Answer Question
   if (game.questionState == ASKING_QUESTION && game.botWaitingToAnswer)
   {
      if (SDL_GetTicks() - game.botActionTimer > 2000)
      {
         game.botWaitingToAnswer = false;

         int pickedIndex;
         int correctIdx = game.question_ui_correct;
         int totalOpts = (int)game.question_ui_options.size();

         if (game.difficulty == EASY)
         {
            // Easy: Force a wrong answer if possible
            pickedIndex = (correctIdx + 1) % totalOpts;
         }
         else if (game.difficulty == MEDIUM)
         {
            // Medium: 50/50
            if (rand() % 100 < 50)
               pickedIndex = correctIdx;
            else
               pickedIndex = (correctIdx + 1) % totalOpts;
         }
         else
         {
            // Hard: Correct
            pickedIndex = correctIdx;
         }
         q_ans_handle_index(pickedIndex);
      }
   }
}

bool isPointInRect(int x, int y, SDL_Rect rect)
{
   return x >= rect.x && x <= rect.x + rect.w && y >= rect.y && y <= rect.y + rect.h;
}
void background_draw();
void difficulty_select_draw()
{
   SDL_Rect box = {width / 2 - 250, height / 2 - 180, 500, 360};
   rounded_rect_draw(box, colors.white, 25);
   center_text_draw("SELECT DIFFICULTY", width / 2, box.y + 40, colors.bg2, gigantic_font);
   SDL_Rect easyBtn = {width / 2 - 150, box.y + 100, 300, 60};
   SDL_Rect mediumBtn = {width / 2 - 150, box.y + 170, 300, 60};
   SDL_Rect hardBtn = {width / 2 - 150, box.y + 240, 300, 60};
   SDL_Rect backBtn = {width / 2 - 150, box.y + 310, 300, 40};
   button_draw(easyBtn, "EASY", colors.green, colors.white, hover_easy, large_font);
   button_draw(mediumBtn, "MEDIUM", colors.bg1, colors.white, hover_medium, large_font);
   button_draw(hardBtn, "HARD", colors.p1, colors.white, hover_hard, large_font);
   button_draw(backBtn, "BACK", colors.dark, colors.white, hover_back_diff, medium_font);
}

void practice_topic_draw()
{
   SDL_Rect box = {width / 2 - 300, height / 2 - 250, 600, 500};
   rounded_rect_draw(box, colors.white, 25);
   center_text_draw("PRACTICE MODE", width / 2, box.y + 40, colors.bg2, gigantic_font);
   center_text_draw("Select a Topic Set", width / 2, box.y + 100, colors.dark, large_font);
   vector<string> topics = {
       "Set 1 (Questions 1-10)",
       "Set 2 (Questions 11-20)",
       "Set 3 (Questions 21-30)",
       "Set 4 (Mixed Bag)"};
   for (int i = 0; i < topics.size(); i++)
   {
      SDL_Rect btn = {width / 2 - 200, box.y + 160 + i * 70, 400, 55};
      bool hover = (game.practice_topic == i);
      SDL_Color col = hover ? colors.green : colors.bg1;
      button_draw(btn, topics[i], col, colors.white, hover, medium_font);
   }
   SDL_Rect backBtn = {width / 2 - 100, box.y + 480, 200, 50};
   button_draw(backBtn, "BACK", colors.p1, colors.white, false, medium_font);
}

void mouse_click_handle(int x, int y)
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
      SDL_Rect welcome_box = {width / 2 - 300, height / 2 - 260, 600, 520};
      int centerX = width / 2;
      int btnY = welcome_box.y + 160;
      SDL_Rect startBtn = {centerX - 120, btnY, 280, 70};
      SDL_Rect practiceBtn = {centerX - 120, btnY + 90, 280, 60};
      SDL_Rect rulesBtn = {centerX - 120, btnY + 165, 280, 60};
      SDL_Rect settingsBtn = {centerX - 120, btnY + 240, 280, 60};
      SDL_Rect aboutBtn = {centerX - 120, btnY + 315, 280, 50};
      if (isPointInRect(x, y, startBtn))
      {
         play_click();
         game.state = DIFFICULTY_SELECT;
         return;
      }
      if (isPointInRect(x, y, practiceBtn))
      {
         play_click();
         game.state = PRACTICE_TOPIC_SELECT;
         game.practice_score = 0;
         game.practice_total_answered = 0;
         return;
      }
      if (isPointInRect(x, y, rulesBtn))
      {
         play_click();
         game.state = RULES;
         return;
      }
      if (isPointInRect(x, y, settingsBtn))
      {
         play_click();
         game.state = SETTINGS;
         return;
      }
      if (isPointInRect(x, y, aboutBtn))
      {
         play_click();
         game.state = ABOUT;
         return;
      }
      break;
   }
   case NAME_INPUT:
   {
      SDL_Rect continue_button = {width / 2 - 75, height / 2 + 30, 150, 40};
      if (isPointInRect(x, y, continue_button) && !game.tempName.empty())
      {
         play_click();
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
      if (isPointInRect(x, y, easyBtn))
      {
         play_click();
         game.difficulty = EASY;
         game.state = PLAYER_SELECT;
         return;
      }
      if (isPointInRect(x, y, mediumBtn))
      {
         play_click();
         game.difficulty = MEDIUM;
         game.state = PLAYER_SELECT;
         return;
      }
      if (isPointInRect(x, y, hardBtn))
      {
         play_click();
         game.difficulty = HARD;
         game.state = PLAYER_SELECT;
         return;
      }
      if (isPointInRect(x, y, backBtn))
      {
         play_click();
         game.state = WELCOME;
         return;
      }
      break;
   }
   case PLAYER_SELECT:
   {
      SDL_Rect box = {width / 2 - 250, height / 2 - 220, 500, 440};
      SDL_Rect btn2 = {width / 2 - 150, box.y + 90, 300, 50};
      SDL_Rect btn3 = {width / 2 - 150, box.y + 150, 300, 50};
      SDL_Rect btn4 = {width / 2 - 150, box.y + 210, 300, 50};
      SDL_Rect btnVS = {width / 2 - 150, box.y + 270, 300, 50};
      SDL_Rect backBtn = {width / 2 - 150, box.y + 350, 300, 40};

      // LOGIC FIX: Create a lambda to handle reset cleanly
      auto setup_new_game = [&](int count, bool vs_cpu)
      {
         game.total_players = count;
         game.state = NAME_INPUT;
         game.input_player = 0; // CRITICAL: Reset index to 0
         game.tempName = "";    // Clear temp buffer
         for (int i = 0; i < 4; i++)
         {
            game.players[i].name = ""; // CRITICAL: Wipe old names so defaults work
            game.players[i].isBot = false;
         }
         if (vs_cpu)
         {
            game.players[1].isBot = true;
         }
      };

      if (isPointInRect(x, y, btn2))
      {
         play_click();
         setup_new_game(2, false);
         return;
      }
      if (isPointInRect(x, y, btn3))
      {
         play_click();
         setup_new_game(3, false);
         return;
      }
      if (isPointInRect(x, y, btn4))
      {
         play_click();
         setup_new_game(4, false);
         return;
      }
      if (isPointInRect(x, y, btnVS))
      {
         play_click();
         setup_new_game(2, true);
         return;
      }
      if (isPointInRect(x, y, backBtn))
      {
         play_click();
         game.state = DIFFICULTY_SELECT;
         return;
      }
      break;
   }
   case PRACTICE_TOPIC_SELECT:
   {
      for (int i = 0; i < 4; i++)
      {
         SDL_Rect btn = {width / 2 - 200, height / 2 - 250 + 160 + i * 70, 400, 55};
         if (isPointInRect(x, y, btn))
         {
            play_click();
            game.practice_topic = i;
            game.practice_question_index = 0;
            // Clear cache for fresh start
            game.question_ui_options.clear();
            game.state = PRACTICE_MODE;
            return;
         }
      }
      SDL_Rect backBtn = {width / 2 - 100, height / 2 + 230, 200, 50};
      if (isPointInRect(x, y, backBtn))
      {
         play_click();
         game.state = WELCOME;
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
      SDL_Rect backBtn = {bx, by + 240, 400, 50};
      if (isPointInRect(x, y, soundBtn))
      {
         play_click();
         game.soundEnabled = !game.soundEnabled;
         return;
      }
      if (isPointInRect(x, y, diceBtn))
      {
         play_click();
         game.diceSoundEnabled = !game.diceSoundEnabled;
         return;
      }
      if (isPointInRect(x, y, preBtn))
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
      // Option selector clicks
      int selY = by + 180;
      int optX = bx + 200;
      for (int i = 2; i <= 6; ++i)
      {
         SDL_Rect optRect = {optX + (i - 2) * 45, selY + 10, 40, 36};
         if (isPointInRect(x, y, optRect))
         {
            play_click();
            g_questionOptionCount = i;
            return;
         }
      }

      if (isPointInRect(x, y, backBtn))
      {
         play_click();
         game.state = WELCOME;
         return;
      }
      break;
   }
   case PLAYING:
   {
      // PREVENT clicking during Bot's Turn
      if (game.players[game.current].isBot)
         return;

      if (game.questionState == ASKING_QUESTION)
      {
         // DYNAMIC BUTTON CLICK CHECK (Mirrors drawing logic)
         // FIX: Use board relative positioning just like question_box_draw
         int cardW = 540;
         int cardH = 380;
         int cardX = board_left + (BOARD_SIZE / 2) - (cardW / 2);
         int cardY = height / 2 - cardH / 2;
         SDL_Rect card = {cardX, cardY, cardW, cardH};

         // Re-calculate layout to find button positions
         vector<string> &opts = game.question_ui_options;
         int optCount = (int)opts.size();
         int cols = (optCount <= 2) ? optCount : 2;
         int rows = (optCount + cols - 1) / cols;
         int optW = (card.w - 60 - (cols - 1) * 12) / cols;
         int optH = 44;

         int lineY = card.y + 80;
         string textToWrap = questions[game.curr_question_index].question;
         int wrapW = card.w - 56;
         vector<string> words;
         string w;
         for (char ch : textToWrap)
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
         string curLine;
         int lineHeight = 28;
         for (size_t i = 0; i < words.size(); ++i)
         {
            string test = curLine.empty() ? words[i] : curLine + " " + words[i];
            int tw = 0, th = 0;
            TTF_SizeUTF8(medium_font, test.c_str(), &tw, &th);
            if (tw > wrapW && !curLine.empty())
            {
               lineY += lineHeight;
               curLine = words[i];
            }
            else
               curLine = test;
         }
         if (!curLine.empty())
            lineY += lineHeight;

         int startY = lineY + 18;
         int optIndex = 0;
         for (int r = 0; r < rows; ++r)
         {
            int y = startY + r * (optH + 12);
            for (int c = 0; c < cols; ++c)
            {
               if (optIndex >= optCount)
                  break;
               int xx = card.x + 30 + c * (optW + 12);
               SDL_Rect rect = {xx, y, optW, optH};
               if (isPointInRect(x, y, rect))
               {
                  if (game.soundEnabled && snd_click)
                     Mix_PlayChannel(-1, snd_click, 0);
                  q_ans_handle_index(optIndex);
                  return;
               }
               optIndex++;
            }
         }
         // FIX: CHECK FOR SKIP BUTTON CLICK
         SDL_Rect skipBtn = {card.x + card.w / 2 - 60, card.y + card.h - 50, 120, 35};
         if (isPointInRect(x, y, skipBtn))
         {
            if (game.soundEnabled && snd_click)
               Mix_PlayChannel(-1, snd_click, 0);
            // Skip counts as wrong or just closes
            q_ans_handle_index(-1);
            return;
         }

         return; // Don't allow clicking board if question is active
      }

      SDL_Rect dice_rect = {BOARD_X + BOARD_SIZE + 130, BOARD_Y + 290, 80, 80};
      if (isPointInRect(x, y, dice_rect))
      {
         if (game.soundEnabled && snd_click)
            Mix_PlayChannel(-1, snd_click, 0);
         dice_roll();
      }
      int c = cursor_to_cell(x, y);
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
   case PRACTICE_MODE:
   {
      if (game.topic_questions[game.practice_topic].empty())
         return;

      // Same click detection logic as PLAYING case, but for practice questions
      int cardW = 540;
      int cardH = 380;
      int cardX = board_left + (BOARD_SIZE / 2) - (cardW / 2);
      int cardY = height / 2 - cardH / 2;
      SDL_Rect card = {cardX, cardY, cardW, cardH};

      size_t sz = game.topic_questions[game.practice_topic].size();
      int qidx = game.topic_questions[game.practice_topic][game.practice_question_index % sz];
      string textToWrap = questions[qidx].question;
      int wrapW = card.w - 56;
      int lineY = card.y + 80;
      vector<string> words;
      string w;
      for (char ch : textToWrap)
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
      string curLine;
      int lineHeight = 28;
      for (size_t i = 0; i < words.size(); ++i)
      {
         string test = curLine.empty() ? words[i] : curLine + " " + words[i];
         int tw = 0, th = 0;
         TTF_SizeUTF8(medium_font, test.c_str(), &tw, &th);
         if (tw > wrapW && !curLine.empty())
         {
            lineY += lineHeight;
            curLine = words[i];
         }
         else
            curLine = test;
      }
      if (!curLine.empty())
         lineY += lineHeight;

      int startY = lineY + 18;
      int optIndex = 0;
      vector<string> &opts = game.question_ui_options;
      int optCount = (int)opts.size();
      int cols = (optCount <= 2) ? optCount : 2;
      int rows = (optCount + cols - 1) / cols;
      int optW = (card.w - 60 - (cols - 1) * 12) / cols;
      int optH = 44;

      for (int r = 0; r < rows; ++r)
      {
         int y = startY + r * (optH + 12);
         for (int c = 0; c < cols; ++c)
         {
            if (optIndex >= optCount)
               break;
            int xx = card.x + 30 + c * (optW + 12);
            SDL_Rect rect = {xx, y, optW, optH};
            if (isPointInRect(x, y, rect))
            {
               play_click();
               game.practice_total_answered++;
               if (optIndex == game.question_ui_correct)
               {
                  game.practice_score++;
                  if (game.soundEnabled)
                     Mix_PlayChannel(-1, snd_correct, 0);
               }
               else
               {
                  if (game.soundEnabled)
                     Mix_PlayChannel(-1, snd_error, 0);
               }
               // Next question
               game.practice_question_index++;
               game.question_ui_options.clear(); // clear cache
               return;
            }
            optIndex++;
         }
      }

      // FIX: Add logic for SKIP in practice mode (just skips to next)
      SDL_Rect skipBtn = {card.x + card.w / 2 - 60, card.y + card.h - 50, 120, 35};
      if (isPointInRect(x, y, skipBtn))
      {
         play_click();
         game.practice_question_index++;
         game.question_ui_options.clear();
         return;
      }
      break;
   }
   case RULES:
   {
      SDL_Rect box = {width / 2 - 350, height / 2 - 280, 700, 560};
      SDL_Rect backBtn = {box.x + (box.w / 2) - 150, box.y + box.h - 60, 300, 50};
      if (isPointInRect(x, y, backBtn))
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
      if (isPointInRect(x, y, backBtn))
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
      for (int i = 0; i < 4; i++)
      {
         game.players[i].name.clear(); // Reset all names
         game.players[i].isBot = false;
      }
      break;
   }
   }
}

void mouse_motion_handle(int x, int y)
{
   switch (game.state)
   {
   case WELCOME:
   {
      SDL_Rect welcome_box = {width / 2 - 300, height / 2 - 260, 600, 520};
      int centerX = width / 2;
      int btnY = welcome_box.y + 160;
      SDL_Rect startBtn = {centerX - 120, btnY, 280, 70};
      SDL_Rect practiceBtn = {centerX - 120, btnY + 90, 280, 60};
      SDL_Rect rulesBtn = {centerX - 120, btnY + 165, 280, 60};
      SDL_Rect settingsBtn = {centerX - 120, btnY + 240, 280, 60};
      SDL_Rect aboutBtn = {centerX - 120, btnY + 315, 280, 50};
      hover_start = isPointInRect(x, y, startBtn);
      hover_practice = isPointInRect(x, y, practiceBtn);
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
   case PLAYER_SELECT:
   {
      SDL_Rect box = {width / 2 - 250, height / 2 - 220, 500, 440};
      SDL_Rect btn2 = {width / 2 - 150, box.y + 90, 300, 50};
      SDL_Rect btn3 = {width / 2 - 150, box.y + 150, 300, 50};
      SDL_Rect btn4 = {width / 2 - 150, box.y + 210, 300, 50};
      SDL_Rect btnVS = {width / 2 - 150, box.y + 270, 300, 50};
      SDL_Rect backBtn = {width / 2 - 150, box.y + 350, 300, 40};
      hover_2p = isPointInRect(x, y, btn2);
      hover_3p = isPointInRect(x, y, btn3);
      hover_4p = isPointInRect(x, y, btn4);
      hover_vs_cpu = isPointInRect(x, y, btnVS);
      hover_back_p = isPointInRect(x, y, backBtn);
      break;
   }
   case PLAYING:
   {
      // Don't highlight dice for bot
      if (game.players[game.current].isBot)
      {
         game.cursor_on_dice = false;
         return;
      }

      SDL_Rect dice_rect = {BOARD_X + BOARD_SIZE + 130, BOARD_Y + 290, 80, 80};
      game.cursor_on_dice = isPointInRect(x, y, dice_rect);
      game.cursor_on_cell = cursor_to_cell(x, y);
      break;
   }
   default:
      break;
   }
}

void practice_mode_draw()
{
   background_draw();

   // FIX: Handle Empty Topics safely
   if (game.topic_questions[game.practice_topic].empty())
   {
      center_text_draw("No questions in this topic!", width / 2, height / 2, colors.p1, large_font);
      // Fallback Back Button logic
      SDL_Rect backBtn = {width / 2 - 100, height / 2 + 60, 200, 50};
      int mx, my;
      SDL_GetMouseState(&mx, &my);
      bool hover = isPointInRect(mx, my, backBtn);
      button_draw(backBtn, "BACK", colors.p1, colors.white, hover, medium_font);
      return;
   }

   // Reuse question_box_draw logic but adapted
   question_box_draw();
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

   // FIX: Turned off Resizable because your board logic is hardcoded to (50, 100).
   window = SDL_CreateWindow("Aditya - Snadder Game Ultimate", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, SDL_WINDOW_SHOWN);
   if (!window)
   {
      fprintf(stderr, "SDL_CreateWindow Error: %s\n", SDL_GetError());
      Mix_CloseAudio();
      TTF_Quit();
      SDL_Quit();
      return 1;
   }
   renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
   if (!renderer)
   {
      fprintf(stderr, "SDL_CreateRenderer Error: %s\n", SDL_GetError());
      SDL_DestroyWindow(window);
      Mix_CloseAudio();
      TTF_Quit();
      SDL_Quit();
      return 1;
   }

   // Load fonts (paths from 70.cpp seem more reliable for standard assets)
   small_font = TTF_OpenFont("assets/font/newspaper.ttf", 16);
   medium_font = TTF_OpenFont("assets/font/good_timing_bd.ttf", 22);
   large_font = TTF_OpenFont("assets/font/Pixel_Game.ttf", 30);
   gigantic_font = TTF_OpenFont("assets/font/KnightWarrior.ttf", 70);

   if (!small_font)
      fprintf(stderr, "Warning: small_font failed to load: %s\n", TTF_GetError());

   snd_roll = Mix_LoadWAV("assets/sound/roll.wav");
   snd_move = Mix_LoadWAV("assets/sound/move.wav");
   snd_win = Mix_LoadWAV("assets/sound/win.wav");
   snd_snake = Mix_LoadWAV("assets/sound/snake.wav");
   snd_correct = Mix_LoadWAV("assets/sound/correct.wav");
   snd_click = Mix_LoadWAV("assets/sound/click.wav");
   snd_error = Mix_LoadWAV("assets/sound/error.wav");

   // LOAD QUESTIONS
   if (!load_questions_from_csv("questions.csv"))
   {
      load_default_questions();
   }

   // Organize for Practice Mode (Distribution logic)
   game.topic_questions.resize(4);
   for (size_t i = 0; i < questions.size(); ++i)
   {
      // Simple distribution since we don't have categories in CSV
      if (i < 10)
         game.topic_questions[0].push_back(i);
      else if (i < 20)
         game.topic_questions[1].push_back(i);
      else if (i < 30)
         game.topic_questions[2].push_back(i);
      else
         game.topic_questions[3].push_back(i);
   }

   // FIX: Ensure no empty vectors to prevent modulo crash
   for (int i = 0; i < 4; i++)
   {
      if (game.topic_questions[i].empty())
      {
         // If we have any questions at all, fill it with question 0
         if (!questions.empty())
            game.topic_questions[i].push_back(0);
      }
   }

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

         // Resizing disabled to prevent logic breakage
         /*
         if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
         {
            width = e.window.data1;
            height = e.window.data2;
         }
         */

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
               // Only allow Human to use Spacebar
               if (!game.players[game.current].isBot)
               {
                  if (e.key.keysym.sym == SDLK_SPACE)
                     dice_roll();
               }
               if (e.key.keysym.sym == SDLK_h)
                  game.show_hints = !game.show_hints;
            }
            else if (game.state == WINNER)
            {
               if (e.key.keysym.sym == SDLK_r)
               {
                  game.state = WELCOME;
                  game.input_player = 0;
                  for (int i = 0; i < 4; i++)
                  {
                     game.players[i].name.clear();
                     game.players[i].isBot = false;
                  }
               }
            }
            if (e.key.keysym.sym == SDLK_ESCAPE)
            {
               game.state = WELCOME;
            }
         }
         if (e.type == SDL_TEXTINPUT && game.state == NAME_INPUT)
            handle_name_input(e.text.text);
         if (e.type == SDL_MOUSEBUTTONDOWN)
            mouse_click_handle(e.button.x, e.button.y);
         if (e.type == SDL_MOUSEMOTION)
            mouse_motion_handle(e.motion.x, e.motion.y);
         if (e.type == SDL_MOUSEWHEEL && game.state == RULES)
         {
            if (e.wheel.y > 0)
               rulesScroll -= rulesScrollStep;
            if (e.wheel.y < 0)
               rulesScroll += rulesScrollStep;
         }
      }
      if (game.requestQuit)
         quit = true;

      if (game.state == PLAYING)
      {
         if (game.preGameTimerActive)
         {
            unsigned int elapsed = (SDL_GetTicks() - game.preGameStartTime) / 1000;
            game.preGameCountdown = max(0, game.preGameDefaultCountdown - (int)elapsed);
            if (game.preGameCountdown == 0)
               game.preGameTimerActive = false;
         }

         // Bot Update
         bot_update();

         if (game.questionState == ASKING_QUESTION)
         {
            unsigned int elapsed = (SDL_GetTicks() - game.questionStartTime) / 1000;
            game.questionTimeLeft = max(0, game.questionDefaultTime - (int)elapsed);
            if (game.questionTimeLeft == 0 && !game.questionTimeout)
            {
               game.questionTimeout = true;
               q_ans_handle_index(-1); // wrong
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
      case PLAYER_SELECT:
         player_select_draw();
         break;
      case NAME_INPUT:
         name_input_box();
         break;
      case PRACTICE_TOPIC_SELECT:
         practice_topic_draw();
         break;
      case PRACTICE_MODE:
         practice_mode_draw();
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
            bool highlight = (game.show_hints && (game.cursor_on_cell == s.first)) || (game.cell_preview == s.first);
            realistic_snake_draw(s.first, s.second, vis, game.animation_time, highlight);
         }
         players_draw();
         UI_draw();
         if (game.preGameTimerActive)
         {
            SDL_Rect overlay = {0, 0, width, height};
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 160);
            SDL_RenderFillRect(renderer, &overlay);
            center_text_draw(to_string(game.preGameCountdown), width / 2, height / 2, colors.gold, gigantic_font);
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
            realistic_snake_draw(s.first, s.second, vis, game.animation_time, false);
         }
         players_draw();
         UI_draw();
         SDL_Rect overlay = {0, 0, width, height};
         SDL_SetRenderDrawColor(renderer, 0, 0, 0, 150);
         SDL_RenderFillRect(renderer, &overlay);
         SDL_Rect win_box = {width / 2 - 200, height / 2 - 100, 400, 200};
         SDL_SetRenderDrawColor(renderer, colors.white.r, colors.white.g, colors.white.b, 255);
         SDL_RenderFillRect(renderer, &win_box);
         SDL_SetRenderDrawColor(renderer, colors.gold.r, colors.gold.g, colors.gold.b, 255);
         for (int i = 0; i < 5; i++)
         {
            SDL_Rect border = {win_box.x - i, win_box.y - i, win_box.w + 2 * i, win_box.h + 2 * i};
            SDL_RenderDrawRect(renderer, &border);
         }
         string win_text = (game.players[game.current].name.empty() ? ("Player " + to_string(game.current + 1)) : game.players[game.current].name) + " WINS!";
         center_text_draw(win_text, width / 2, height / 2 - 20, colors.gold, gigantic_font);
         center_text_draw("CONGRATULATIONS!", width / 2, height / 2 + 20, colors.dark, medium_font);
         center_text_draw("Click anywhere or press R to play again", width / 2, height / 2 + 60, colors.dark, small_font);
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