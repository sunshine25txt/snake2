#include <iostream>
#include <fstream>
#include <vector>
#include <map>
#include <string>
#include <cmath>     // sqrt, sin, cos, atan2, hypot
#include <algorithm> // max, min, shuffle
#include <random>

using namespace std;

#ifdef __ANDROID__
#include <SDL.h>
#include <SDL_ttf.h>
#include <SDL_mixer.h>
#else
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_mixer.h>
#endif
#include "features.hpp"

/* Android Compatibility */
#ifdef __ANDROID__
#include <android/log.h>
#define LOG_TAG "SnadderGame"
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ==================== GameConfig Namespace (Audit 3.4) ==================== */
namespace GameConfig {
     // Window
     constexpr int WINDOW_WIDTH = 1024;
     constexpr int WINDOW_HEIGHT = 768;

     // Board Layout
     constexpr int BOARD_CELL_SIZE = 60;
     constexpr int BOARD_GRID_SIZE = 10;
     constexpr int BOARD_POSITION_X = 50;
     constexpr int BOARD_POSITION_Y = 100;
     constexpr int BOARD_TOTAL_SIZE = BOARD_CELL_SIZE * BOARD_GRID_SIZE;

     // Timing (ms)
     constexpr int DICE_ROLL_DURATION = 1500;
     constexpr int PLAYER_MOVE_INTERVAL = 300;
     constexpr int BOT_ROLL_DELAY = 1000;
     constexpr int BOT_ANSWER_DELAY = 2000;
     constexpr int DICE_WOBBLE_DURATION = 500;
     constexpr int TARGET_FPS = 60;
     constexpr int FRAME_DELAY_MS = 1000 / TARGET_FPS; // ~16ms

     // Animation
     constexpr float DICE_WOBBLE_FREQUENCY = 0.02f;
     constexpr float DICE_WOBBLE_AMPLITUDE = 5.0f;

     // UI
     constexpr int DICE_DOT_RADIUS = 6;
     constexpr int BUTTON_CORNER_RADIUS = 12;
     constexpr int CARD_WIDTH = 540;
     constexpr int CARD_HEIGHT = 380;
#ifdef __ANDROID__
     constexpr int OPTION_HEIGHT = 54;   // P1: Larger touch targets for mobile
#else
     constexpr int OPTION_HEIGHT = 44;
#endif
     constexpr int LINE_HEIGHT = 28;
     constexpr int CARD_PADDING = 28;

     // Game Constants
     constexpr int MAX_PLAYERS = 4;
     constexpr int BOARD_MAX_CELL = 100;
     constexpr int MAX_NAME_LENGTH = 15;

     // Settings file
     constexpr const char* SETTINGS_FILE = "settings.dat";
     constexpr const char* SAVE_FILE = "savegame.dat";
     constexpr int PRACTICE_MAX_SKIPS = 3;
     constexpr int DEFAULT_QUESTION_TIME = 30;
     constexpr int DEFAULT_PRE_GAME_COUNTDOWN = 3;
     constexpr int DEFAULT_OPTION_COUNT = 4;
     constexpr int NUM_TOPICS = 10;
}

/* Configuration (using GameConfig) */
int width = GameConfig::WINDOW_WIDTH;
int height = GameConfig::WINDOW_HEIGHT;

/* Scaling for Android */
float scaleX = 1.0f;
float scaleY = 1.0f;
int screen_physical_w = GameConfig::WINDOW_WIDTH;
int screen_physical_h = GameConfig::WINDOW_HEIGHT;

const int board_left = GameConfig::BOARD_POSITION_X;
const int board_top = GameConfig::BOARD_POSITION_Y;
const int CELL = GameConfig::BOARD_CELL_SIZE;
const int GRID = GameConfig::BOARD_GRID_SIZE;
const int BOARD_SIZE = GameConfig::BOARD_TOTAL_SIZE;
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

/* Random Number Generation */
std::mt19937 g_rng;
std::uniform_int_distribution<int> g_dice_dist(1, 6);
std::uniform_int_distribution<int> g_coin_dist(0, 1);

/* Feature System Instances (features.hpp) */
DarkModeManager g_darkMode;
OfflineManager g_offlineMode;
NotificationManager g_notificationMgr;
CloudSaveManager g_cloudSave;
AchievementManager g_achievements;
LeaderboardManager g_leaderboard;
UpdateChecker g_updateChecker;
ControllerManager g_controllerMgr;
unsigned int g_gameStartTime = 0;  // Track game start for speedrun achievement

/* P0: App Lifecycle — pause/resume timer management */
bool app_paused = false;
unsigned int pause_start_time = 0;
unsigned int total_pause_offset = 0;  // Accumulated pause duration

/* P0: IME state tracking */
bool ime_active = false;

/* P1: Settings persistence path */
char *pref_path = nullptr;

// Helper function for random range
int random_range(int min_val, int max_val) {
     std::uniform_int_distribution<int> dist(min_val, max_val);
     return dist(g_rng);
}

/* Hover Flags */
bool hover_start = false;
bool hover_practice = false;
bool hover_rules = false;

// Practice topic scrolling
int practice_topic_scroll = 0;
const int TOPIC_ITEM_HEIGHT = 62;
const int VISIBLE_TOPICS = 8;
bool practice_scrollbar_dragging = false;
int practice_scrollbar_drag_start = 0;
int practice_scrollbar_offset_start = 0;

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
int g_questionOptionCount = GameConfig::DEFAULT_OPTION_COUNT;

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
     bool usedSkip = false; // Has this player used their one-time skip for the whole game
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

     // Answer Feedback (Professional: show result before proceeding)
     bool showingAnswerResult = false;
     bool lastAnswerCorrect = false;
     unsigned int answerResultTime = 0;
     string answerFeedbackText;
     int answeredCorrectCount = 0;
     int answeredWrongCount = 0;

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
     int preGameCountdown = GameConfig::DEFAULT_PRE_GAME_COUNTDOWN;
     int questionDefaultTime = GameConfig::DEFAULT_QUESTION_TIME;
     int preGameDefaultCountdown = GameConfig::DEFAULT_PRE_GAME_COUNTDOWN;

     bool requestQuit = false;

     // Practice Mode
     int practice_topic = 0;
     vector<vector<int>> topic_questions; // Stores indices of questions
     int practice_question_index = 0;
     int practice_score = 0;
     int practice_total_answered = 0;
     bool practice_current_question_skipped = false; // Track if current question was skipped
     int practice_skips_remaining = GameConfig::PRACTICE_MAX_SKIPS;
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

/* Text Wrapping Utility (Audit 3.6 — extracted from 3 duplicate blocks) */
struct WrappedText {
     vector<string> lines;
     int total_height;
};

WrappedText wrap_text(const string &text, int max_width, TTF_Font *font, int line_height = GameConfig::LINE_HEIGHT)
{
     WrappedText result;
     vector<string> words;
     string current_word;

     for (char ch : text)
     {
          if (ch == ' ')
          {
               if (!current_word.empty())
               {
                    words.push_back(current_word);
                    current_word.clear();
               }
          }
          else
          {
               current_word.push_back(ch);
          }
     }
     if (!current_word.empty())
          words.push_back(current_word);

     string current_line;
     for (size_t i = 0; i < words.size(); ++i)
     {
          string test = current_line.empty() ? words[i] : current_line + " " + words[i];
          int tw = 0, th = 0;
          TTF_SizeUTF8(font, test.c_str(), &tw, &th);

          if (tw > max_width && !current_line.empty())
          {
               result.lines.push_back(current_line);
               current_line = words[i];
          }
          else
          {
               current_line = test;
          }
     }
     if (!current_line.empty())
          result.lines.push_back(current_line);

     result.total_height = (int)result.lines.size() * line_height;
     return result;
}

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
     rounded_rect_draw(rect, color, GameConfig::BUTTON_CORNER_RADIUS);
     // Border removed for cleaner look

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

          // Split by pipe '|' character
          vector<string> parts;
          string cur;
          for (size_t i = 0; i < line.size(); ++i)
          {
               char ch = line[i];
               if (ch == '|')
               {
                    parts.push_back(cur);
                    cur.clear();
               }
               else
                    cur.push_back(ch);
          }
          parts.push_back(cur);

          // Need: question | opt1 | opt2 | ... | optN | correct_index
          // Minimum: question | opt1 | opt2 | correct_index (4 parts)
          if (parts.size() < 4)
               continue;

          Question q;
          q.question = parts[0];
          q.options.clear();

          // Extract options (all parts except first and last)
          for (size_t i = 1; i < parts.size() - 1; ++i)
          {
               string s = parts[i];
               // Trim whitespace
               while (!s.empty() && (s.front() == ' ' || s.front() == '\t'))
                    s.erase(s.begin());
               while (!s.empty() && (s.back() == ' ' || s.back() == '\t'))
                    s.pop_back();
               if (!s.empty() && q.options.size() < 6)
                    q.options.push_back(s);
          }

          // Extract correct answer index from last part
          string indexStr = parts[parts.size() - 1];
          // Trim whitespace
          while (!indexStr.empty() && (indexStr.front() == ' ' || indexStr.front() == '\t'))
               indexStr.erase(indexStr.begin());
          while (!indexStr.empty() && (indexStr.back() == ' ' || indexStr.back() == '\t'))
               indexStr.pop_back();

          try
          {
               int idx = stoi(indexStr);
               q.ans_index = idx >= 0 && idx < (int)q.options.size() ? idx : 0;
          }
          catch (...)
          {
               q.ans_index = 0; // default to first if parse fails
          }

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

     // **FIX: TRIM TO EXACT DISPLAY COUNT**
     if ((int)candidates.size() > displayCount)
          candidates.resize(displayCount);

     // Find new index of correct answer
     int newAns = -1;
     if (!correctStr.empty())
     {
          for (int i = 0; i < (int)candidates.size(); ++i)
          {
               if (candidates[i] == correctStr)
               {
                    newAns = i;
                    break;
               }
          }
     }
     // Fallback safety
     if (newAns == -1 && !correctStr.empty() && !candidates.empty())
     {
          candidates[0] = correctStr;
          newAns = 0;
     }

     return {candidates, newAns};
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
               bodyCol.r = (Uint8)min(255, bodyCol.r + 40);
               bodyCol.g = (Uint8)min(255, bodyCol.g + 40);
               bodyCol.b = (Uint8)min(255, bodyCol.b + 40);
          }
          SDL_Color belly;
          belly.r = (Uint8)(bodyCol.r / 1.2f);
          belly.g = (Uint8)(bodyCol.g / 1.2f);
          belly.b = (Uint8)(bodyCol.b / 1.2f);
          belly.a = 220;
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
     SDL_Color white = {255, 255, 255, 255};
     SDL_Color black = {0, 0, 0, 255};
     filled_circle_draw((int)ex1, (int)ey1, 4, white);
     filled_circle_draw((int)ex2, (int)ey2, 4, white);
     filled_circle_draw((int)ex1, (int)ey1, 2, black);
     filled_circle_draw((int)ex2, (int)ey2, 2, black);
     float flick = (sin(timeSec * 12.0f) > 0.6f) ? 1.0f : 0.0f;
     if (flick > 0.5f)
     {
          float tx = hx + cos(headAngle) * (baseR + 10);
          float ty = hy + sin(headAngle) * (baseR + 10);
          SDL_SetRenderDrawColor(renderer, 220, 50, 50, 255);
          SDL_RenderDrawLine(renderer, (int)hx, (int)hy, (int)tx, (int)ty);
          float forkAng1 = headAngle + 0.25f;
          SDL_RenderDrawLine(renderer, (int)tx, (int)ty,
               (int)(tx + cos(forkAng1) * 8), (int)(ty + sin(forkAng1) * 8));
          float forkAng2 = headAngle - 0.25f;
          SDL_RenderDrawLine(renderer, (int)tx, (int)ty,
               (int)(tx + cos(forkAng2) * 8), (int)(ty + sin(forkAng2) * 8));
     }
}

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

     // Card Positioning
     int cardW = GameConfig::CARD_WIDTH;
     int cardH = GameConfig::CARD_HEIGHT;
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
          size_t sz = game.topic_questions[game.practice_topic].size();
          if (sz > 0)
               qidx = game.topic_questions[game.practice_topic][game.practice_question_index % sz];
          else
               qidx = 0;

          if (game.question_ui_options.empty())
          {
               auto p = prepare_options_for_ui(questions[qidx], g_questionOptionCount);
               game.question_ui_options = p.first;
               game.question_ui_correct = p.second;
          }
     }

     int wrapX = card.x + GameConfig::CARD_PADDING;
     int wrapW = card.w - GameConfig::CARD_PADDING * 2;
     int lineY = card.y + 80;

     string textToWrap = (game.state == PRACTICE_MODE) ? questions[qidx].question : q.question;

     // Use extracted wrap_text utility (Audit 3.6)
     WrappedText wrapped = wrap_text(textToWrap, wrapW, medium_font);
     for (const string &line : wrapped.lines)
     {
          text_draw(line, wrapX, lineY, colors.dark, medium_font);
          lineY += GameConfig::LINE_HEIGHT;
     }

     // Draw Options
     vector<string> &opts = game.question_ui_options;
     int optCount = (int)opts.size();
     int cols = (optCount <= 2) ? optCount : 2;
     int rows = (optCount + cols - 1) / cols;
     int optW = (card.w - 60 - (cols - 1) * 12) / cols;
     int optH = GameConfig::OPTION_HEIGHT;
     // Guard: prevent options from overflowing card bounds
     int maxStartY = card.y + card.h - (rows * (optH + 12)) - 60;
     int startY = min(lineY + 18, maxStartY);

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

     // --- SKIP BUTTON ---
     SDL_Rect skipBtn = {card.x + card.w / 2 - 60, card.y + card.h - 50, 120, 35};
     int mx, my;
     SDL_GetMouseState(&mx, &my);
     bool hoverSkip = isPointInRect(mx, my, skipBtn);

     if (game.state == PRACTICE_MODE)
     {
          string btnText;
          SDL_Color btnColor;
          if (game.practice_skips_remaining > 0)
          {
               btnText = "SKIP (" + to_string(game.practice_skips_remaining) + ")";
               btnColor = colors.ladder;
          }
          else
          {
               btnText = "NO SKIPS LEFT";
               btnColor = {120, 120, 120, 255};
          }
          button_draw(skipBtn, btnText, btnColor, colors.white, hoverSkip, medium_font);
     }
     else
     {
          bool curUsedSkip = false;
          if (game.current >= 0 && game.current < 4)
               curUsedSkip = game.players[game.current].usedSkip;
          string btnText = curUsedSkip ? "SURRENDER" : "SKIP ONCE";
          SDL_Color btnColor = curUsedSkip ? colors.snake : colors.gold;
          button_draw(skipBtn, btnText, btnColor, colors.white, hoverSkip, medium_font);
     }

     // --- Answer Feedback Overlay (Professional: brief flash) ---
     if (game.showingAnswerResult)
     {
          unsigned int fbElapsed = SDL_GetTicks() - game.answerResultTime;
          if (fbElapsed < 1200)
          {
               // Fade alpha over time
               Uint8 alpha = (Uint8)(255 - (fbElapsed * 255 / 1200));
               SDL_Color fbColor = game.lastAnswerCorrect
                    ? SDL_Color{39, 174, 96, alpha}
                    : SDL_Color{231, 76, 60, alpha};
               SDL_Rect fbRect = {card.x + 20, card.y + card.h / 2 - 30, card.w - 40, 60};
               rounded_rect_draw(fbRect, fbColor, 10);
               center_text_draw(game.answerFeedbackText,
                    card.x + card.w / 2, card.y + card.h / 2,
                    colors.white, large_font);
          }
          else
          {
               game.showingAnswerResult = false;
          }
     }
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
          if (elapsed < GameConfig::DICE_WOBBLE_DURATION)
               animOffset = (int)(sin(elapsed * GameConfig::DICE_WOBBLE_FREQUENCY) * GameConfig::DICE_WOBBLE_AMPLITUDE);
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
     int dot_size = GameConfig::DICE_DOT_RADIUS;
     SDL_SetRenderDrawColor(renderer, colors.dark.r, colors.dark.g, colors.dark.b, 255);
     int cx = animatedRect.x + animatedRect.w / 2;
     int cy = animatedRect.y + animatedRect.h / 2;
     // Dice dots: use filled_circle for professional look
     switch (game.dice)
     {
     case 1:
          filled_circle_draw(cx, cy, dot_size, colors.dark);
          break;
     case 2:
          filled_circle_draw(cx - 15, cy - 15, dot_size, colors.dark);
          filled_circle_draw(cx + 15, cy + 15, dot_size, colors.dark);
          break;
     case 3:
          filled_circle_draw(cx - 15, cy - 15, dot_size, colors.dark);
          filled_circle_draw(cx, cy, dot_size, colors.dark);
          filled_circle_draw(cx + 15, cy + 15, dot_size, colors.dark);
          break;
     case 4:
          filled_circle_draw(cx - 15, cy - 15, dot_size, colors.dark);
          filled_circle_draw(cx + 15, cy - 15, dot_size, colors.dark);
          filled_circle_draw(cx - 15, cy + 15, dot_size, colors.dark);
          filled_circle_draw(cx + 15, cy + 15, dot_size, colors.dark);
          break;
     case 5:
          filled_circle_draw(cx - 15, cy - 15, dot_size, colors.dark);
          filled_circle_draw(cx + 15, cy - 15, dot_size, colors.dark);
          filled_circle_draw(cx, cy, dot_size, colors.dark);
          filled_circle_draw(cx - 15, cy + 15, dot_size, colors.dark);
          filled_circle_draw(cx + 15, cy + 15, dot_size, colors.dark);
          break;
     case 6:
          filled_circle_draw(cx - 15, cy - 20, dot_size, colors.dark);
          filled_circle_draw(cx + 15, cy - 20, dot_size, colors.dark);
          filled_circle_draw(cx - 15, cy, dot_size, colors.dark);
          filled_circle_draw(cx + 15, cy, dot_size, colors.dark);
          filled_circle_draw(cx - 15, cy + 20, dot_size, colors.dark);
          filled_circle_draw(cx + 15, cy + 20, dot_size, colors.dark);
          break;
     }
     bool canRoll = game.cursor_on_dice && !game.rolling && !game.moving
               && game.questionState == NO_QUESTION && !game.preGameTimerActive
               && !game.players[game.current].isBot;
     if (canRoll)
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
     int upperCenterX = upper_panel.x + upper_panel.w / 2;
     center_text_draw(" QUESTION CHALLENGE  ", upperCenterX, upper_panel.y + 30, colors.gold, medium_font);
     center_text_draw("When you land on snakes or", upperCenterX, upper_panel.y + 60, colors.white, small_font);
     center_text_draw("ladders, answer questions to :", upperCenterX, upper_panel.y + 80, colors.white, small_font);
     center_text_draw(" Avoid snake slides", upperCenterX, upper_panel.y + 110, colors.white, small_font);
     center_text_draw(" Climb ladders", upperCenterX, upper_panel.y + 130, colors.white, small_font);

     SDL_Rect lower_panel = {rightX, BOARD_Y + 220, 280, 380};
     SDL_SetRenderDrawColor(renderer, colors.dark.r, colors.dark.g, colors.dark.b, 200);
     SDL_RenderFillRect(renderer, &lower_panel);
     SDL_SetRenderDrawColor(renderer, colors.white.r, colors.white.g, colors.white.b, 100);
     SDL_RenderDrawRect(renderer, &lower_panel);

     int lowerCenterX = lower_panel.x + lower_panel.w / 2;
     string playerName = game.players[game.current].name.empty()
          ? ("Player " + to_string(game.current + 1))
          : game.players[game.current].name;
     string turnText = playerName + "'s Turn";
     center_text_draw(turnText, lowerCenterX, lower_panel.y + 30, colors.white, medium_font);

     SDL_Rect dice_rect = {lower_panel.x + 100, lower_panel.y + 70, 80, 80};
     dice_draw(dice_rect);

     if (!game.rolling && !game.moving && game.questionState == NO_QUESTION)
     {
          if (game.players[game.current].isBot)
               center_text_draw("Computer rolling...", lowerCenterX, lower_panel.y + 180, colors.gold, small_font);
          else
               center_text_draw("Click dice to roll!", lowerCenterX, lower_panel.y + 180, colors.white, small_font);
     }
     else if (game.rolling)
     {
          center_text_draw("Rolling...", lowerCenterX, lower_panel.y + 180, colors.gold, small_font);
     }
     else if (game.moving)
     {
          center_text_draw("Moving...", lowerCenterX, lower_panel.y + 180, colors.green, small_font);
     }
     else if (game.questionState == ASKING_QUESTION)
     {
          center_text_draw("Answer the question!", lowerCenterX, lower_panel.y + 180, colors.gold, small_font);
     }

     center_text_draw("Positions:", lowerCenterX, lower_panel.y + 220, colors.white, medium_font);

     int spacing = (game.total_players > 2) ? 25 : 30;
     int startY = (game.total_players > 2) ? 245 : 250;
     TTF_Font *listFont = (game.total_players > 2) ? small_font : medium_font;

     for (int i = 0; i < game.total_players; i++)
     {
          string pName = game.players[i].name.empty() ? ("Player " + to_string(i + 1)) : game.players[i].name;
          string score = pName + ": " + to_string(game.players[i].pos);
          text_draw(score, lower_panel.x + 20, lower_panel.y + startY + i * spacing, game.players[i].color, listFont);
     }

     center_text_draw("Game Stats:", lower_panel.x + lower_panel.w / 2, lower_panel.y + 340, colors.white, medium_font);
     string diceText = "Last Roll: " + to_string(game.dice);
     center_text_draw(diceText, lower_panel.x + lower_panel.w / 2, lower_panel.y + 365, colors.white, small_font);

     // --- QUIT GAME and BACK TO DIFFICULTY buttons ---
     SDL_Rect quitBtn = {lower_panel.x + 10, lower_panel.y + 410, 125, 45};
     button_draw(quitBtn, "BACK", colors.snake, colors.white, false, medium_font);

     SDL_Rect backBtn = {lower_panel.x + 145, lower_panel.y + 410, 125, 45};
     button_draw(backBtn, "QUIT GAME", colors.gold, colors.dark, false, medium_font);
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
     SDL_Rect box = {width / 2 - 300, height / 2 - 260, 600, 520};
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

     string soundLabel = string("Sound: ") + (game.soundEnabled ? "ON" : "OFF");
     SDL_Color soundColor = game.soundEnabled ? colors.green : SDL_Color{180, 180, 180, 255};
     button_draw(soundBtn, soundLabel, soundColor, colors.white, false, medium_font);

     string diceLabel = string("Dice Sound: ") + (game.diceSoundEnabled ? "ON" : "OFF");
     SDL_Color diceColor = game.diceSoundEnabled ? colors.bg2 : SDL_Color{180, 180, 180, 255};
     button_draw(diceBtn, diceLabel, diceColor, colors.white, false, medium_font);

     string preLabel = "Pre-Game Countdown: " + to_string(game.preGameDefaultCountdown) + " sec";
     button_draw(preBtn, preLabel, colors.bg2, colors.white, false, medium_font);

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

     // --- Feature: Dark Mode Toggle ---
     SDL_Rect darkModeBtn = {bx, by + 240, 400, 50};
     string dmLabel = string("Dark Mode: ") + (g_darkMode.isDarkMode() ? "ON" : "OFF");
     SDL_Color dmColor = g_darkMode.isDarkMode() ? SDL_Color{60, 60, 90, 255} : SDL_Color{180, 180, 200, 255};
     button_draw(darkModeBtn, dmLabel, dmColor, colors.white, false, medium_font);

     // --- Feature: Update Checker Status ---
     string updateStatus = "Version: " + g_updateChecker.getVersionString() + " | " + g_updateChecker.getStatusString();
     center_text_draw(updateStatus, width / 2, by + 310, colors.dark, small_font);

     // --- Feature: Achievements Summary ---
     string achText = "Achievements: " + to_string(g_achievements.getUnlockedCount()) + "/" + to_string(g_achievements.getTotalCount());
     center_text_draw(achText, width / 2, by + 335, colors.bg2, small_font);

     SDL_Rect backBtn = {bx, by + 360, 400, 50};
     button_draw(backBtn, "BACK", colors.p1, colors.white, false, medium_font);
}

void about_draw()
{
     SDL_Rect box = {width / 2 - 350, height / 2 - 280, 700, 560};
     SDL_Rect shadow = {box.x + 5, box.y + 5, box.w, box.h};
     SDL_SetRenderDrawColor(renderer, 0, 0, 0, 80);
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
     if (questions.empty())
     {
          SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "No questions available, skipping challenge");
          return;
     }

     game.questionState = ASKING_QUESTION;
     game.special_cell = pos;
     game.isSnake = snakes.count(pos) > 0;
     game.showingAnswerResult = false;

     // Select random question (with bounds safety)
     game.curr_question_index = random_range(0, (int)questions.size() - 1);
     if (game.curr_question_index < 0 || game.curr_question_index >= (int)questions.size())
          game.curr_question_index = 0;

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

     // Track stats
     if (correct)
     {
          game.answeredCorrectCount++;
          g_achievements.consecutiveCorrect++;
          if (g_achievements.consecutiveCorrect >= 10)
               if (g_achievements.checkAndUnlock(AchievementManager::PERFECT_GAME))
                    g_notificationMgr.sendNotification("Achievement!", "Perfect Game unlocked!", NotificationManager::ACHIEVEMENT);
     }
     else
     {
          game.answeredWrongCount++;
          g_achievements.consecutiveCorrect = 0;
     }

     // Show feedback overlay
     game.showingAnswerResult = true;
     game.lastAnswerCorrect = correct;
     game.answerResultTime = SDL_GetTicks();

     if (!correct)
     {
          game.answerFeedbackText = "WRONG!";
          if (game.isSnake)
          {
               if (snakes.count(pos))
               {
                    game.players[game.current].pos = snakes[pos];
                    game.answerFeedbackText = "WRONG! Sliding down...";
               }
               if (game.soundEnabled && snd_snake)
                    Mix_PlayChannel(-1, snd_snake, 0);
          }
          else
          {
               game.answerFeedbackText = "WRONG! No climb.";
               if (game.soundEnabled && snd_error)
                    Mix_PlayChannel(-1, snd_error, 0);
          }
     }
     else
     {
          if (!game.isSnake)
          {
               if (ladders.count(pos))
               {
                    game.players[game.current].pos = ladders[pos];
                    game.answerFeedbackText = "CORRECT! Climbing up!";
                    g_achievements.laddersClimbed++;
                    g_achievements.updateProgress(AchievementManager::LADDER_CLIMBER, g_achievements.laddersClimbed * 10);
                    if (g_achievements.laddersClimbed >= 10)
                         g_achievements.checkAndUnlock(AchievementManager::LADDER_CLIMBER);
               }
               else
                    game.answerFeedbackText = "CORRECT!";
          }
          else
          {
               game.answerFeedbackText = "CORRECT! Snake avoided!";
               g_achievements.snakesSurvived++;
               g_achievements.updateProgress(AchievementManager::SNAKE_MASTER, g_achievements.snakesSurvived * 20);
               if (g_achievements.snakesSurvived >= 5)
                    if (g_achievements.checkAndUnlock(AchievementManager::SNAKE_MASTER))
                         g_notificationMgr.sendNotification("Achievement!", "Snake Master unlocked!", NotificationManager::ACHIEVEMENT);
          }
          if (game.soundEnabled && snd_correct)
               Mix_PlayChannel(-1, snd_correct, 0);
     }

     if (game.players[game.current].pos == 100)
     {
          game.state = WINNER;
          if (game.soundEnabled && snd_win)
               Mix_PlayChannel(-1, snd_win, 0);

          // --- Feature Integration: Win Events ---
          string winnerName = game.players[game.current].name;
          if (winnerName.empty()) winnerName = "Player " + to_string(game.current + 1);

          // Achievement: First Win
          if (!game.players[game.current].isBot)
               g_achievements.checkAndUnlock(AchievementManager::FIRST_WIN);

          // Achievement: Speedrun (win in under 5 min)
          if (!game.players[game.current].isBot && g_gameStartTime > 0)
          {
               unsigned int elapsed = SDL_GetTicks() - g_gameStartTime;
               if (elapsed < 300000) // 5 minutes
                    if (g_achievements.checkAndUnlock(AchievementManager::SPEEDRUN))
                         g_notificationMgr.sendNotification("Achievement!", "Speed Runner unlocked!", NotificationManager::ACHIEVEMENT);
          }

          // Leaderboard: add scores for all players
          for (int i = 0; i < game.total_players; i++)
          {
               string pn = game.players[i].name.empty() ? ("Player " + to_string(i + 1)) : game.players[i].name;
               g_leaderboard.addPlayerScore(pn, game.players[i].pos, (i == game.current));
          }

          // Cloud Save: update stats
          g_cloudSave.updateStats(winnerName, true);

          // Achievement: Consistency (games played)
          g_achievements.updateProgress(AchievementManager::CONSISTENCY, g_cloudSave.getGamesPlayed() * 10);
          if (g_cloudSave.getGamesPlayed() >= 10)
               g_achievements.checkAndUnlock(AchievementManager::CONSISTENCY);

          // Notification
          g_notificationMgr.sendNotification("Game Over!", winnerName + " wins!", NotificationManager::MILESTONE);
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
     game.players[0] = {1, colors.p1, game.players[0].name.empty() ? "Player 1" : game.players[0].name, -15, -15, false, false};

     // Player 2 (Green, Top Right)
     string p2name = game.players[1].name.empty() ? "Player 2" : game.players[1].name;
     if (game.players[1].isBot)
          p2name = "Computer";
     game.players[1] = {1, colors.p2, p2name, 15, -15, game.players[1].isBot, false};

     // Player 3 (Blue, Bottom Left)
     game.players[2] = {1, colors.p3, game.players[2].name.empty() ? "Player 3" : game.players[2].name, -15, 15, false, false};
     // Player 4 (Orange, Bottom Right)
     game.players[3] = {1, colors.p4, game.players[3].name.empty() ? "Player 4" : game.players[3].name, 15, 15, false, false};

     game.current = 0;
     game.state = PLAYING;
     game.moving = false;
     game.rolling = false;
     game.cell_preview = 0;
     game.cursor_on_cell = 0;
     game.questionState = NO_QUESTION;
     game.landed_special_cell = false;
     game.showingAnswerResult = false;
     game.answeredCorrectCount = 0;
     game.answeredWrongCount = 0;
     if (game.difficulty == EASY)
          game.questionDefaultTime = GameConfig::DEFAULT_QUESTION_TIME;
     else if (game.difficulty == MEDIUM)
          game.questionDefaultTime = 15;
     else
          game.questionDefaultTime = 7;

     game.botActionTimer = SDL_GetTicks();

     // --- Feature Integration: Game Start ---
     g_gameStartTime = SDL_GetTicks();
     g_achievements.resetGameCounters();

     // Achievement: Social Butterfly (4 players)
     if (game.total_players == 4)
          g_achievements.checkAndUnlock(AchievementManager::SOCIAL_BUTTERFLY);

     // Offline mode: cache initial state
     int positions[4] = {game.players[0].pos, game.players[1].pos, game.players[2].pos, game.players[3].pos};
     g_offlineMode.cacheGameState(positions, game.current, game.total_players, game.dice);
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
     if (elapsed < GameConfig::DICE_ROLL_DURATION)
     {
          game.dice = g_dice_dist(g_rng);
     }
     else
     {
          game.rolling = false;
          game.dice = g_dice_dist(g_rng);
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
     if (SDL_GetTicks() - game.last_move > GameConfig::PLAYER_MOVE_INTERVAL)
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
          if ((SDL_GetTicks() - total_pause_offset) - game.botActionTimer > GameConfig::BOT_ROLL_DELAY)
          {
               dice_roll();
          }
     }

     // 2. Answer Question
     if (game.questionState == ASKING_QUESTION && game.botWaitingToAnswer)
     {
          if ((SDL_GetTicks() - total_pause_offset) - game.botActionTimer > GameConfig::BOT_ANSWER_DELAY)
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
                    if (g_coin_dist(g_rng) == 1)
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
     SDL_Rect box = {width / 2 - 300, height / 2 - 280, 600, 560};
     rounded_rect_draw(box, colors.white, 25);
     center_text_draw("PRACTICE MODE - SELECT CHAPTER", width / 2, box.y + 40, colors.bg2, large_font);

     // 10 Chapters with individual names
     vector<string> topics = {
         "1. Variables & Data Types",
         "2. Operators & Expressions",
         "3. Control Structures",
         "4. Functions",
         "5. Arrays & Strings",
         "6. Pointers",
         "7. Structures & Unions",
         "8. Dynamic Memory",
         "9. File Handling",
         "10. Preprocessor & Macros"};

     int total_topics = (int)topics.size();
     int max_scroll = max(0, total_topics - VISIBLE_TOPICS);
     if (practice_topic_scroll < 0)
          practice_topic_scroll = 0;
     if (practice_topic_scroll > max_scroll)
          practice_topic_scroll = max_scroll;

     int mx, my;
     SDL_GetMouseState(&mx, &my);

     int start_y = box.y + 120;
     for (int i = 0; i < total_topics; ++i)
     {
          int draw_y = start_y + (i - practice_topic_scroll) * TOPIC_ITEM_HEIGHT;
          if (draw_y < box.y + 100 || draw_y > box.y + box.h - 100)
               continue;

          SDL_Rect btn = {width / 2 - 300, draw_y, 600, 56};
          bool hovered = isPointInRect(mx, my, btn);
          SDL_Color bg = hovered ? colors.green : colors.bg1;
          button_draw(btn, topics[i], bg, colors.white, hovered, medium_font);
     }

     // Scrollbar
     if (max_scroll > 0)
     {
          SDL_Rect track = {box.x + box.w - 30, box.y + 120, 20, 500};
          SDL_SetRenderDrawColor(renderer, 0, 0, 0, 100);
          SDL_RenderFillRect(renderer, &track);

          float ratio = (float)VISIBLE_TOPICS / total_topics;
          int bar_h = (int)(500 * ratio);
          bar_h = max(bar_h, 50);
          int bar_y = box.y + 120 + (int)((500 - bar_h) * (float)practice_topic_scroll / max_scroll);

          SDL_Rect bar = {track.x + 2, bar_y, 16, bar_h};
          SDL_SetRenderDrawColor(renderer, colors.gold.r, colors.gold.g, colors.gold.b, 255);
          SDL_RenderFillRect(renderer, &bar);
     }

     SDL_Rect backBtn = {width / 2 - 150, box.y + box.h - 80, 300, 60};
     bool backHovered = isPointInRect(mx, my, backBtn);
     button_draw(backBtn, "BACK", colors.p1, colors.white, backHovered, large_font);
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
                    game.players[i].usedSkip = false;
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
          SDL_Rect box = {width / 2 - 300, height / 2 - 280, 600, 560};

          // 10 Chapters with individual names (SAME AS DRAW FUNCTION)
          vector<string> topics = {
              "1. Variables & Data Types",
              "2. Operators & Expressions",
              "3. Control Structures",
              "4. Functions",
              "5. Arrays & Strings",
              "6. Pointers",
              "7. Structures & Unions",
              "8. Dynamic Memory",
              "9. File Handling",
              "10. Preprocessor & Macros"};

          int total_topics = (int)topics.size();
          int max_scroll = max(0, total_topics - VISIBLE_TOPICS);

          int start_y = box.y + 120;
          for (int i = 0; i < total_topics; ++i)
          {
               int draw_y = start_y + (i - practice_topic_scroll) * TOPIC_ITEM_HEIGHT;
               if (draw_y < box.y + 100 || draw_y > box.y + box.h - 100)
                    continue;

               SDL_Rect btn = {width / 2 - 300, draw_y, 600, 56};
               if (isPointInRect(x, y, btn))
               {
                    play_click();
                    game.practice_topic = i;
                    game.practice_question_index = 0;
                    game.practice_skips_remaining = GameConfig::PRACTICE_MAX_SKIPS;  // Reset skips
                    game.question_ui_options.clear();
                    game.practice_current_question_skipped = false;
                    game.state = PRACTICE_MODE;
                    return;
               }
          }

          // Scrollbar click detection
          if (max_scroll > 0)
          {
               SDL_Rect track = {box.x + box.w - 30, box.y + 120, 20, 500};
               if (isPointInRect(x, y, track))
               {
                    float ratio = (float)VISIBLE_TOPICS / total_topics;
                    int bar_h = (int)(500 * ratio);
                    bar_h = max(bar_h, 50);
                    int bar_y = box.y + 120 + (int)((500 - bar_h) * (float)practice_topic_scroll / max_scroll);

                    SDL_Rect bar = {track.x + 2, bar_y, 16, bar_h};
                    if (isPointInRect(x, y, bar))
                    {
                         play_click();
                         practice_scrollbar_dragging = true;
                         practice_scrollbar_drag_start = y;
                         practice_scrollbar_offset_start = practice_topic_scroll;
                    }
               }
          }

          SDL_Rect backBtn = {width / 2 - 150, box.y + box.h - 80, 300, 60};
          if (isPointInRect(x, y, backBtn))
          {
               play_click();
               practice_scrollbar_dragging = false; // Reset drag state
               game.state = WELCOME;
          }
          break;
     }
     case SETTINGS:
     {
          SDL_Rect box = {width / 2 - 300, height / 2 - 260, 600, 520};
          int bx = box.x + 100;
          int by = box.y + 100;
          SDL_Rect soundBtn = {bx, by, 400, 50};
          SDL_Rect diceBtn = {bx, by + 60, 400, 50};
          SDL_Rect preBtn = {bx, by + 120, 400, 50};
          SDL_Rect darkModeBtn = {bx, by + 240, 400, 50};
          SDL_Rect backBtn = {bx, by + 360, 400, 50};
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

          // --- Feature: Dark Mode Toggle Click ---
          if (isPointInRect(x, y, darkModeBtn))
          {
               play_click();
               g_darkMode.toggleTheme();
               // Apply dark mode colors to the game's color scheme
               if (g_darkMode.isDarkMode())
               {
                    colors.bg1 = g_darkMode.getBackgroundPrimary();
                    colors.bg2 = g_darkMode.getBackgroundSecondary();
               }
               else
               {
                    colors.bg1 = {41, 128, 185, 255};   // Original Ocean Blue
                    colors.bg2 = {155, 89, 182, 255};    // Original Amethyst
               }
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
     case PLAYING:
     {
          // PREVENT clicking during Bot's Turn
          if (game.players[game.current].isBot)
               return;

          if (game.questionState == ASKING_QUESTION)
          {
               // DYNAMIC BUTTON CLICK CHECK
               int cardW = GameConfig::CARD_WIDTH;
               int cardH = GameConfig::CARD_HEIGHT;
               int cardX = board_left + (BOARD_SIZE / 2) - (cardW / 2);
               int cardY = height / 2 - cardH / 2;
               SDL_Rect card = {cardX, cardY, cardW, cardH};

               vector<string> &opts = game.question_ui_options;
               int optCount = (int)opts.size();
               int cols = (optCount <= 2) ? optCount : 2;
               int rows = (optCount + cols - 1) / cols;
               int optW = (card.w - 60 - (cols - 1) * 12) / cols;
               int optH = GameConfig::OPTION_HEIGHT;

               // Use wrap_text utility (Audit 3.6)
               string textToWrap = questions[game.curr_question_index].question;
               int wrapW = card.w - GameConfig::CARD_PADDING * 2;
               WrappedText wrapped = wrap_text(textToWrap, wrapW, medium_font);
               int lineY = card.y + 80 + wrapped.total_height;

               int startY = lineY + 18;
               int optIndex = 0;
               for (int r = 0; r < rows; ++r)
               {
                    int yRect = startY + r * (optH + 12);
                    for (int c = 0; c < cols; ++c)
                    {
                         if (optIndex >= optCount)
                              break;
                         int xx = card.x + 30 + c * (optW + 12);
                         SDL_Rect rect = {xx, yRect, optW, optH};
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

               // --- SKIP / NEW QUESTION LOGIC ---
               SDL_Rect skipBtn = {card.x + card.w / 2 - 60, card.y + card.h - 50, 120, 35};
               if (isPointInRect(x, y, skipBtn))
               {
                    if (game.soundEnabled && snd_click)
                         Mix_PlayChannel(-1, snd_click, 0);

                    // Per-player single use skip: if the current player hasn't used it yet, give a new question and mark used.
                    if (game.current >= 0 && game.current < 4)
                    {
                         Player &p = game.players[game.current];
                         if (!p.usedSkip)
                         {
                              // mark used for whole game for this player
                              p.usedSkip = true;

                              // load a new question
                              game.curr_question_index = random_range(0, questions.size() - 1);
                              game.questionStartTime = SDL_GetTicks();
                              game.questionTimeLeft = game.questionDefaultTime;
                              game.questionTimeout = false;

                              // rebuild options
                              game.question_ui_options.clear();
                              auto newopts = prepare_options_for_ui(questions[game.curr_question_index], g_questionOptionCount);
                              game.question_ui_options = newopts.first;
                              game.question_ui_correct = newopts.second;

                              // return without penalizing
                              return;
                         }
                         else
                         {
                              // already used: count as wrong answer
                              q_ans_handle_index(-1);
                              return;
                         }
                    }
               }
               return;
          }

          // --- QUIT GAME and BACK TO DIFFICULTY buttons in lower_panel ---
          int rightX = BOARD_X + BOARD_SIZE + 30;
          SDL_Rect quitBtn = {rightX + 10, BOARD_Y + 220 + 410, 125, 45};
          if (isPointInRect(x, y, quitBtn))
          {
               if (game.soundEnabled && snd_click)
                    Mix_PlayChannel(-1, snd_click, 0);
               // **FIX: BACK button - Return to difficulty with full reset**
               game.state = DIFFICULTY_SELECT;
               game.input_player = 0;
               game.rolling = false;
               game.moving = false;
               game.questionState = NO_QUESTION;
               game.move_path.clear();
               game.question_ui_options.clear();
               game.question_ui_correct = -1;
               return;
          }

          SDL_Rect backBtn = {rightX + 145, BOARD_Y + 220 + 410, 125, 45};
          if (isPointInRect(x, y, backBtn))
          {
               if (game.soundEnabled && snd_click)
                    Mix_PlayChannel(-1, snd_click, 0);
               // **FIX: QUIT button - Return to main menu with full reset**
               game.state = WELCOME;
               game.input_player = 0;
               game.rolling = false;
               game.moving = false;
               game.questionState = NO_QUESTION;
               game.move_path.clear();
               game.question_ui_options.clear();
               game.question_ui_correct = -1;
               // Clear all player data for next game
               for (int i = 0; i < 4; i++)
               {
                    game.players[i].name = "";
                    game.players[i].pos = 1;
                    game.players[i].isBot = false;
                    game.players[i].usedSkip = false;
               }
               return;
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
                    game.cell_preview = (game.cell_preview == c) ? 0 : c;
               else
                    game.cell_preview = 0;
          }
          break;
     }
     case PRACTICE_MODE:
     {
          // Scoreboard view when user quits (practice_question_index == -1)
          if (game.practice_question_index == -1)
          {
               SDL_Rect box = {width / 2 - 320, height / 2 - 260, 640, 520};
               SDL_Rect backBtn = {width / 2 - 150, box.y + 450, 300, 60};
               if (isPointInRect(x, y, backBtn))
               {
                    if (game.soundEnabled && snd_click)
                         Mix_PlayChannel(-1, snd_click, 0);
                    // Reset practice state and go back to topic select
                    game.practice_question_index = 0;
                    game.practice_score = 0;
                    game.practice_total_answered = 0;
                    game.question_ui_options.clear();
                    game.question_ui_correct = -1;
                    game.state = PRACTICE_TOPIC_SELECT;
               }
               return;
          }

          // If topic has no questions, ignore clicks (UI shows BACK)
          if (game.topic_questions.empty() || game.practice_topic < 0 || game.practice_topic >= (int)game.topic_questions.size())
               return;
          if (game.topic_questions[game.practice_topic].empty())
               return;

          // Card layout (must match drawing logic)
          int cardW = GameConfig::CARD_WIDTH, cardH = GameConfig::CARD_HEIGHT;
          int cardX = board_left + (BOARD_SIZE / 2) - (cardW / 2);
          int cardY = height / 2 - cardH / 2;
          SDL_Rect card = {cardX, cardY, cardW, cardH};

          // Which question index (safety: ensure non-empty vector)
          size_t sz = game.topic_questions[game.practice_topic].size();
          if (sz == 0)
               return;
          int qidx = game.topic_questions[game.practice_topic][game.practice_question_index % sz];

          // Ensure options are prepared & cached (same logic used in drawing)
          if (game.question_ui_options.empty())
          {
               auto p = prepare_options_for_ui(questions[qidx], g_questionOptionCount);
               game.question_ui_options = p.first;
               game.question_ui_correct = p.second;
          }

          // Layout params (must mirror question_box_draw)
          vector<string> &opts = game.question_ui_options;
          int optCount = (int)opts.size();
          int cols = (optCount <= 2) ? optCount : 2;
          int rows = (optCount + cols - 1) / cols;
          int optW = (card.w - 60 - (cols - 1) * 12) / cols;
          int optH = GameConfig::OPTION_HEIGHT;

          // Use wrap_text utility (Audit 3.6)
          string textToWrap = questions[qidx].question;
          int wrapW = card.w - GameConfig::CARD_PADDING * 2;
          WrappedText wrapped = wrap_text(textToWrap, wrapW, medium_font);
          int lineY = card.y + 80 + wrapped.total_height;

          int startY = lineY + 18;

          // Click detection: options laid out in rows/cols
          int optIndex = 0;
          for (int r = 0; r < rows; ++r)
          {
               int yRect = startY + r * (optH + 12);
               for (int c = 0; c < cols; ++c)
               {
                    if (optIndex >= optCount)
                         break;
                    int xx = card.x + 30 + c * (optW + 12);
                    SDL_Rect rect = {xx, yRect, optW, optH};
                    if (isPointInRect(x, y, rect))
                    {
                         // play click sound
                         if (game.soundEnabled && snd_click)
                              Mix_PlayChannel(-1, snd_click, 0);

                         // Evaluate
                         game.practice_total_answered++;
                         if (optIndex == game.question_ui_correct)
                         {
                              game.practice_score++;
                              if (game.soundEnabled && snd_correct)
                                   Mix_PlayChannel(-1, snd_correct, 0);
                         }
                         else
                         {
                              if (game.soundEnabled && snd_error)
                                   Mix_PlayChannel(-1, snd_error, 0);
                         }

                         // advance question and clear cache so next question rebuilds options
                         game.practice_question_index++;
                         game.question_ui_options.clear();
                         game.question_ui_correct = -1;
                         return;
                    }
                    optIndex++;
               }
          }

          // SKIP button (replace with different random question in same chapter)
          SDL_Rect skipBtn = {card.x + card.w / 2 - 60, card.y + card.h - 50, 120, 35};
          if (isPointInRect(x, y, skipBtn))
          {
               if (game.soundEnabled && snd_click)
                    Mix_PlayChannel(-1, snd_click, 0);

               // **FIX: SKIP LOGIC WITH LIMIT (3 skips per session)**
               if (game.practice_skips_remaining > 0)
               {
                    bool validTopic = !game.topic_questions.empty()
                         && game.practice_topic >= 0
                         && game.practice_topic < (int)game.topic_questions.size();

                    if (validTopic && !game.topic_questions[game.practice_topic].empty())
                    {
                         size_t sz = game.topic_questions[game.practice_topic].size();

                         // Get new random question (different from current if possible)
                         int currentIdx = game.practice_question_index % sz;
                         int newIdx = currentIdx;

                         if (sz > 1)
                         {
                              do
                              {
                                   newIdx = random_range(0, sz - 1);
                              } while (newIdx == currentIdx);
                         }

                         game.practice_question_index = newIdx;
                         game.practice_current_question_skipped = true;
                         game.question_ui_options.clear();
                         game.question_ui_correct = -1;
                         game.practice_skips_remaining--;  // **DECREMENT SKIP COUNT**

                         if (game.soundEnabled && snd_move)
                              Mix_PlayChannel(-1, snd_move, 0);
                    }
               }
               else
               {
                    // Out of skips - treat as wrong answer
                    if (game.soundEnabled && snd_error)
                         Mix_PlayChannel(-1, snd_error, 0);

                    // Advance to next question without scoring
                    game.practice_question_index++;
                    game.question_ui_options.clear();
                    game.question_ui_correct = -1;
               }
               return;
          }

          // QUIT PRACTICE button
          SDL_Rect quitBtn = {width / 2 - 180, height - 70, 360, 55};
          if (isPointInRect(x, y, quitBtn))
          {
               if (game.soundEnabled && snd_click)
                    Mix_PlayChannel(-1, snd_click, 0);
               game.practice_question_index = -1; // Show scoreboard
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
     case PRACTICE_TOPIC_SELECT:
     {
          // Handle scrollbar dragging
          if (practice_scrollbar_dragging)
          {

               int total_topics = (int)10; // We have 10 chapters now
               int max_scroll = max(0, total_topics - VISIBLE_TOPICS);

               if (max_scroll > 0)
               {
                    int track_height = 500;

                    float ratio = (float)VISIBLE_TOPICS / total_topics;
                    int bar_h = (int)(track_height * ratio);
                    bar_h = max(bar_h, 50);

                    // Calculate new scroll position based on mouse movement
                    int mouse_delta = y - practice_scrollbar_drag_start;
                    int usable_track = track_height - bar_h;

                    // Safety check: avoid division by zero
                    if (usable_track > 0)
                    {
                         float scroll_delta = (float)mouse_delta * max_scroll / usable_track;
                         practice_topic_scroll = practice_scrollbar_offset_start + (int)scroll_delta;
                         practice_topic_scroll = max(0, min(practice_topic_scroll, max_scroll));
                    }
               }
          }
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
          SDL_Rect backBtn = {width / 2 - 100, height / 2 + 60, 200, 50};
          int mx, my;
          SDL_GetMouseState(&mx, &my);
          bool hover = isPointInRect(mx, my, backBtn);
          button_draw(backBtn, "BACK", colors.p1, colors.white, hover, medium_font);
          return;
     }

     // Quit হলে Scoreboard দেখানো (final) - even if 0 questions answered
     if (game.practice_question_index == -1)
     {
          // Final Scoreboard (styled)
          SDL_Rect overlay = {0, 0, width, height};
          SDL_SetRenderDrawColor(renderer, 0, 0, 0, 190);
          SDL_RenderFillRect(renderer, &overlay);

          SDL_Rect box = {width / 2 - 320, height / 2 - 260, 640, 520};
          rounded_rect_draw(box, colors.white, 35);

          center_text_draw("PRACTICE SESSION OVER", width / 2, box.y + 60, colors.gold, gigantic_font);

          if (game.practice_total_answered == 0)
          {
               // No answers - show motivation message
               center_text_draw("You didn't answer any questions!", width / 2, box.y + 180, colors.dark, large_font);
               center_text_draw("", width / 2, box.y + 230, colors.white, large_font);
               center_text_draw("Come back and practice more!", width / 2, box.y + 280, colors.gold, medium_font);
               center_text_draw("Keep learning, you got this!", width / 2, box.y + 330, colors.green, medium_font);
          }
          else
          {
               // Show stats and motivation
               float percent = (float)game.practice_score * 100.0f / max(1, game.practice_total_answered);
               string acc = to_string((int)percent) + "%";

               center_text_draw("Total Questions: " + to_string(game.practice_total_answered),
                                width / 2, box.y + 160, colors.dark, large_font);
               center_text_draw("Correct: " + to_string(game.practice_score),
                                width / 2, box.y + 210, colors.green, large_font);
               center_text_draw("Wrong: " + to_string(game.practice_total_answered - game.practice_score),
                                width / 2, box.y + 260, colors.p1, large_font);
               center_text_draw("Accuracy: " + acc,
                                width / 2, box.y + 330,
                                percent >= 70 ? colors.green : (percent >= 40 ? colors.gold : colors.p1),
                                gigantic_font);

               if (percent >= 80)
                    center_text_draw("Excellent!", width / 2, box.y + 390, colors.green, large_font);
               else if (percent >= 60)
                    center_text_draw("Good Job!", width / 2, box.y + 390, colors.gold, large_font);
               else if (percent >= 40)
                    center_text_draw("Keep Practicing!", width / 2, box.y + 390, colors.bg2, medium_font);
               else
                    center_text_draw("Never Give Up!", width / 2, box.y + 390, colors.p1, medium_font);
          }

          SDL_Rect backBtn = {width / 2 - 150, box.y + 450, 300, 60};
          int mx, my;
          SDL_GetMouseState(&mx, &my);
          bool hover = isPointInRect(mx, my, backBtn);
          button_draw(backBtn, "BACK TO MENU", colors.p1, colors.white, hover, large_font);
          return;
     }

     // Normal question view reuses dynamic question UI
     // Reset skip flag for new questions (when question index changes)
     static int last_question_index = -1;
     if (game.practice_question_index != last_question_index)
     {
          game.practice_current_question_skipped = false;
          last_question_index = game.practice_question_index;
     }

     question_box_draw();

     // Quit button at bottom
     SDL_Rect quitBtn = {width / 2 - 180, height - 70, 360, 55};
     int mx, my;
     SDL_GetMouseState(&mx, &my);
     bool hoverQuit = isPointInRect(mx, my, quitBtn);
     button_draw(quitBtn, "QUIT PRACTICE", colors.p1, colors.white, hoverQuit, large_font);
}

/* ==================== Feature HUD: Notification Toast Overlay ==================== */
void notification_hud_draw()
{
     // Clean up old notifications
     g_notificationMgr.clearOldNotifications(4000);

     auto active = g_notificationMgr.getActiveNotifications(3500);
     if (active.empty()) return;

     int startY = 10;
     unsigned int now = SDL_GetTicks();
     for (size_t i = 0; i < active.size() && i < 3; i++)
     {
          unsigned int age = now - active[i].timestamp;
          Uint8 alpha = 255;
          if (age > 2500) alpha = (Uint8)(255 * (3500 - age) / 1000); // Fade out

          SDL_Color bgColor;
          switch (active[i].type)
          {
               case NotificationManager::ACHIEVEMENT: bgColor = {241, 196, 15, alpha}; break;  // Gold
               case NotificationManager::MILESTONE: bgColor = {39, 174, 96, alpha}; break;      // Green
               case NotificationManager::TURN_ALERT: bgColor = {52, 152, 219, alpha}; break;    // Blue
               default: bgColor = {44, 62, 80, alpha}; break;                                    // Dark
          }

          int notifW = 280, notifH = 50;
          SDL_Rect notifRect = {width - notifW - 15, startY + (int)i * (notifH + 8), notifW, notifH};
          rounded_rect_draw(notifRect, bgColor, 10);
          SDL_Color textCol = {255, 255, 255, alpha};
          text_draw(active[i].title, notifRect.x + 12, notifRect.y + 6, textCol, small_font);
          text_draw(active[i].message, notifRect.x + 12, notifRect.y + 26, textCol, small_font);
     }
}

/* ==================== Feature HUD: Controller Status Indicator ==================== */
void controller_status_draw()
{
     if (!g_controllerMgr.isControllerConnected()) return;
     SDL_Color green = {39, 174, 96, 180};
     SDL_Rect badge = {width - 160, height - 30, 150, 22};
     rounded_rect_draw(badge, green, 8);
     center_text_draw("Controller Connected", badge.x + badge.w / 2, badge.y + badge.h / 2, {255, 255, 255, 200}, small_font);
}

/* ========================================================================
   P1: Settings Persistence — save/load user preferences via SDL_GetPrefPath
   ======================================================================== */

void save_settings()
{
     if (!pref_path) return;
     string path = string(pref_path) + GameConfig::SETTINGS_FILE;
     FILE *fp = fopen(path.c_str(), "wb");
     if (!fp) { SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Cannot save settings to %s", path.c_str()); return; }

     // Write a simple binary format: magic + version + fields
     const char magic[] = "SSET";
     int version = 1;
     fwrite(magic, 1, 4, fp);
     fwrite(&version, sizeof(int), 1, fp);
     fwrite(&game.soundEnabled, sizeof(bool), 1, fp);
     fwrite(&game.diceSoundEnabled, sizeof(bool), 1, fp);
     fwrite(&game.difficulty, sizeof(int), 1, fp);
     fwrite(&g_questionOptionCount, sizeof(int), 1, fp);
     fwrite(&game.preGameDefaultCountdown, sizeof(int), 1, fp);
     fclose(fp);
     SDL_Log("Settings saved to %s", path.c_str());
}

void load_settings()
{
     if (!pref_path) return;
     string path = string(pref_path) + GameConfig::SETTINGS_FILE;
     FILE *fp = fopen(path.c_str(), "rb");
     if (!fp) return;  // No saved settings yet, use defaults

     char magic[5] = {0};
     int version = 0;
     fread(magic, 1, 4, fp);
     if (string(magic) != "SSET") { fclose(fp); return; }  // Corrupt/wrong file
     fread(&version, sizeof(int), 1, fp);
     if (version >= 1)
     {
          fread(&game.soundEnabled, sizeof(bool), 1, fp);
          fread(&game.diceSoundEnabled, sizeof(bool), 1, fp);
          fread(&game.difficulty, sizeof(int), 1, fp);
          fread(&g_questionOptionCount, sizeof(int), 1, fp);
          fread(&game.preGameDefaultCountdown, sizeof(int), 1, fp);
          // Validate loaded values
          if (g_questionOptionCount < 2 || g_questionOptionCount > 6) g_questionOptionCount = 4;
          if (game.preGameDefaultCountdown < 0 || game.preGameDefaultCountdown > 10) game.preGameDefaultCountdown = 3;
     }
     fclose(fp);
     SDL_Log("Settings loaded from %s", path.c_str());
}

/* ========================================================================
   P1: Save/Restore In-Progress Game State
   ======================================================================== */

void save_game_state()
{
     if (!pref_path) return;
     // Only save if we're actually in a game
     if (game.state != PLAYING && game.state != WINNER) return;

     string path = string(pref_path) + GameConfig::SAVE_FILE;
     FILE *fp = fopen(path.c_str(), "wb");
     if (!fp) return;

     const char magic[] = "SGAM";
     int version = 1;
     fwrite(magic, 1, 4, fp);
     fwrite(&version, sizeof(int), 1, fp);

     // Save core game state
     int state_int = (int)game.state;
     fwrite(&state_int, sizeof(int), 1, fp);
     fwrite(&game.total_players, sizeof(int), 1, fp);
     fwrite(&game.current, sizeof(int), 1, fp);
     fwrite(&game.dice, sizeof(int), 1, fp);
     fwrite(&game.answeredCorrectCount, sizeof(int), 1, fp);
     fwrite(&game.answeredWrongCount, sizeof(int), 1, fp);

     // Save each player
     for (int i = 0; i < game.total_players; i++)
     {
          fwrite(&game.players[i].pos, sizeof(int), 1, fp);
          fwrite(&game.players[i].isBot, sizeof(bool), 1, fp);
          fwrite(&game.players[i].usedSkip, sizeof(bool), 1, fp);
          int nameLen = (int)game.players[i].name.size();
          fwrite(&nameLen, sizeof(int), 1, fp);
          fwrite(game.players[i].name.c_str(), 1, nameLen, fp);
     }
     fclose(fp);
     SDL_Log("Game state saved");
}

bool load_game_state()
{
     if (!pref_path) return false;
     string path = string(pref_path) + GameConfig::SAVE_FILE;
     FILE *fp = fopen(path.c_str(), "rb");
     if (!fp) return false;

     char magic[5] = {0};
     int version = 0;
     fread(magic, 1, 4, fp);
     if (string(magic) != "SGAM") { fclose(fp); return false; }
     fread(&version, sizeof(int), 1, fp);

     int state_int;
     fread(&state_int, sizeof(int), 1, fp);
     fread(&game.total_players, sizeof(int), 1, fp);
     fread(&game.current, sizeof(int), 1, fp);
     fread(&game.dice, sizeof(int), 1, fp);
     fread(&game.answeredCorrectCount, sizeof(int), 1, fp);
     fread(&game.answeredWrongCount, sizeof(int), 1, fp);

     for (int i = 0; i < game.total_players; i++)
     {
          fread(&game.players[i].pos, sizeof(int), 1, fp);
          fread(&game.players[i].isBot, sizeof(bool), 1, fp);
          fread(&game.players[i].usedSkip, sizeof(bool), 1, fp);
          int nameLen;
          fread(&nameLen, sizeof(int), 1, fp);
          if (nameLen > 0 && nameLen < 256)
          {
               char nameBuf[256];
               fread(nameBuf, 1, nameLen, fp);
               nameBuf[nameLen] = '\0';
               game.players[i].name = string(nameBuf);
          }
          // Restore colors
          SDL_Color pcolors[] = {colors.p1, colors.p2, colors.p3, colors.p4};
          int offsets[][2] = {{-15, -15}, {15, -15}, {-15, 15}, {15, 15}};
          game.players[i].color = pcolors[i];
          game.players[i].offsetX = offsets[i][0];
          game.players[i].offsetY = offsets[i][1];
     }

     game.state = (State)state_int;
     game.questionState = NO_QUESTION;
     game.moving = false;
     game.rolling = false;
     game.showingAnswerResult = false;
     fclose(fp);

     // Delete save file after loading (one-shot restore)
     remove(path.c_str());
     SDL_Log("Game state restored from save");
     return true;
}

/* P0: IME helper for Android — show/hide keyboard only during NAME_INPUT */
void update_ime_state()
{
#ifdef __ANDROID__
     if (game.state == NAME_INPUT && !ime_active)
     {
          SDL_StartTextInput();
          ime_active = true;
     }
     else if (game.state != NAME_INPUT && ime_active)
     {
          SDL_StopTextInput();
          ime_active = false;
     }
#endif
}

int main(int argc, char **argv)
{
     std::random_device rd;
     g_rng.seed(rd());
     if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) != 0)
     {
          SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_Init Error: %s", SDL_GetError());
          return 1;
     }
     if (TTF_Init() != 0)
     {
          SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "TTF_Init Error: %s", TTF_GetError());
          SDL_Quit();
          return 1;
     }
     if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) != 0)
     {
          SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Mix_OpenAudio Error: %s", Mix_GetError());
          TTF_Quit();
          SDL_Quit();
          return 1;
     }

     // --- Android: Fullscreen + detect screen size ---
#ifdef __ANDROID__
     SDL_DisplayMode displayMode;
     SDL_GetCurrentDisplayMode(0, &displayMode);
     screen_physical_w = displayMode.w;
     screen_physical_h = displayMode.h;
     window = SDL_CreateWindow("Snadder Game", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                               screen_physical_w, screen_physical_h,
                               SDL_WINDOW_FULLSCREEN | SDL_WINDOW_SHOWN);
#else
     window = SDL_CreateWindow("Aditya - Snadder Game Ultimate",
                               SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                               width, height, SDL_WINDOW_SHOWN);
#endif
     if (!window)
     {
          SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_CreateWindow Error: %s", SDL_GetError());
          Mix_CloseAudio();
          TTF_Quit();
          SDL_Quit();
          return 1;
     }
     renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
     if (!renderer)
     {
          SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_CreateRenderer Error: %s", SDL_GetError());
          SDL_DestroyWindow(window);
          Mix_CloseAudio();
          TTF_Quit();
          SDL_Quit();
          return 1;
     }

     // --- Logical rendering: scale the 1024x768 design to any screen ---
     SDL_RenderSetLogicalSize(renderer, width, height);
     scaleX = (float)screen_physical_w / (float)width;
     scaleY = (float)screen_physical_h / (float)height;
     SDL_Log("Screen: %dx%d, Logical: %dx%d, Scale: %.2fx%.2f",
             screen_physical_w, screen_physical_h, width, height, scaleX, scaleY);

     // --- P1: Get preference path for settings persistence ---
     pref_path = SDL_GetPrefPath("SnadderGame", "SnadderGameUltimate");
     if (pref_path)
          SDL_Log("Pref path: %s", pref_path);
     else
          SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Could not get pref path");

     // --- Feature System Init ---
     g_controllerMgr.init();
     g_updateChecker.checkForUpdates();

     // --- P0: Load fonts via SDL_RWops for Android asset support ---
#ifdef __ANDROID__
     // On Android, SDL_RWFromFile uses the APK's assets folder
     SDL_RWops *rw_small = SDL_RWFromFile("font/newspaper.ttf", "rb");
     SDL_RWops *rw_medium = SDL_RWFromFile("font/good_timing_bd.ttf", "rb");
     SDL_RWops *rw_large = SDL_RWFromFile("font/Pixel_Game.ttf", "rb");
     SDL_RWops *rw_gigantic = SDL_RWFromFile("font/KnightWarrior.ttf", "rb");
     small_font = rw_small ? TTF_OpenFontRW(rw_small, 1, 16) : nullptr;
     medium_font = rw_medium ? TTF_OpenFontRW(rw_medium, 1, 22) : nullptr;
     large_font = rw_large ? TTF_OpenFontRW(rw_large, 1, 30) : nullptr;
     gigantic_font = rw_gigantic ? TTF_OpenFontRW(rw_gigantic, 1, 70) : nullptr;
#else
     small_font = TTF_OpenFont("assets/font/newspaper.ttf", 16);
     medium_font = TTF_OpenFont("assets/font/good_timing_bd.ttf", 22);
     large_font = TTF_OpenFont("assets/font/Pixel_Game.ttf", 30);
     gigantic_font = TTF_OpenFont("assets/font/KnightWarrior.ttf", 70);
#endif

     if (!small_font)
          SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to load small_font: %s", TTF_GetError());
     if (!medium_font)
          SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to load medium_font: %s", TTF_GetError());
     if (!large_font)
          SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to load large_font: %s", TTF_GetError());
     if (!gigantic_font)
          SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to load gigantic_font: %s", TTF_GetError());

     // --- P0: Load sounds via SDL_RWops for Android ---
#ifdef __ANDROID__
     #define LOAD_WAV_ANDROID(path) Mix_LoadWAV_RW(SDL_RWFromFile(path, "rb"), 1)
     snd_roll = LOAD_WAV_ANDROID("sound/roll.wav");
     snd_move = LOAD_WAV_ANDROID("sound/move.wav");
     snd_win = LOAD_WAV_ANDROID("sound/win.wav");
     snd_snake = LOAD_WAV_ANDROID("sound/snake.wav");
     snd_correct = LOAD_WAV_ANDROID("sound/correct.wav");
     snd_click = LOAD_WAV_ANDROID("sound/click.wav");
     snd_error = LOAD_WAV_ANDROID("sound/error.wav");
     #undef LOAD_WAV_ANDROID
#else
     snd_roll = Mix_LoadWAV("assets/sound/roll.wav");
     snd_move = Mix_LoadWAV("assets/sound/move.wav");
     snd_win = Mix_LoadWAV("assets/sound/win.wav");
     snd_snake = Mix_LoadWAV("assets/sound/snake.wav");
     snd_correct = Mix_LoadWAV("assets/sound/correct.wav");
     snd_click = Mix_LoadWAV("assets/sound/click.wav");
     snd_error = Mix_LoadWAV("assets/sound/error.wav");
#endif

     if (!snd_roll) SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Failed to load roll.wav: %s", Mix_GetError());
     if (!snd_move) SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Failed to load move.wav: %s", Mix_GetError());
     if (!snd_win) SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Failed to load win.wav: %s", Mix_GetError());
     if (!snd_snake) SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Failed to load snake.wav: %s", Mix_GetError());
     if (!snd_correct) SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Failed to load correct.wav: %s", Mix_GetError());
     if (!snd_click) SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Failed to load click.wav: %s", Mix_GetError());
     if (!snd_error) SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Failed to load error.wav: %s", Mix_GetError());

     // --- P0: Load questions via SDL_RWops on Android ---
#ifdef __ANDROID__
     // On Android, read questions.csv from APK assets via SDL_RWops
     {
          SDL_RWops *csv_rw = SDL_RWFromFile("questions.csv", "rb");
          if (csv_rw)
          {
               Sint64 csv_size = SDL_RWsize(csv_rw);
               if (csv_size > 0)
               {
                    char *csv_buf = new char[csv_size + 1];
                    SDL_RWread(csv_rw, csv_buf, 1, csv_size);
                    csv_buf[csv_size] = '\0';
                    SDL_RWclose(csv_rw);
                    // Write to a temp location so our existing CSV parser can read it
                    string temp_csv = string(SDL_AndroidGetInternalStoragePath()) + "/questions.csv";
                    FILE *fp = fopen(temp_csv.c_str(), "wb");
                    if (fp) { fwrite(csv_buf, 1, csv_size, fp); fclose(fp); }
                    delete[] csv_buf;
                    if (!load_questions_from_csv(temp_csv))
                    {
                         SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Failed to parse questions from APK, using defaults");
                         load_default_questions();
                    }
               }
               else
               {
                    SDL_RWclose(csv_rw);
                    load_default_questions();
               }
          }
          else
          {
               SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Cannot open questions.csv from APK assets");
               load_default_questions();
          }
     }
#else
     if (!load_questions_from_csv("questions.csv"))
     {
          SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Failed to load questions.csv, using defaults");
          load_default_questions();
     }
     else
     {
          SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Loaded %d questions from CSV", (int)questions.size());
     }
#endif
     // --------------------- TOPIC DISTRIBUTION / FALLBACK ---------------------
     // Place this AFTER you loaded 'questions' (either from CSV or defaults).

     int NUM_TOPICS = GameConfig::NUM_TOPICS;
     game.topic_questions.clear();
     game.topic_questions.resize(NUM_TOPICS);

     if (questions.empty())
     {
          // No questions at all — leave topic_questions empty (UI handles this)
     }
     else
     {
           size_t qcount = questions.size();

          // Distribute questions across topics using block-based mapping
          // Each topic gets roughly (qcount / NUM_TOPICS) questions
          size_t block_size = max((size_t)1, qcount / (size_t)NUM_TOPICS);

          if (qcount >= (size_t)NUM_TOPICS * block_size)
          {
               for (size_t i = 0; i < qcount; ++i)
               {
                    int topic = min((int)(i / block_size), NUM_TOPICS - 1);
                    game.topic_questions[topic].push_back((int)i);
               }
          }
          else
          {
               // Round-robin distribution for smaller sets so topics are balanced
               for (size_t i = 0; i < qcount; ++i)
               {
                    int topic = (int)(i % NUM_TOPICS); // spread evenly
                    game.topic_questions[topic].push_back((int)i);
               }
          }

          // Shuffle each topic so practice order isn't identical every run
          for (int t = 0; t < NUM_TOPICS; ++t)
          {
               auto &vec = game.topic_questions[t];
               if (!vec.empty())
                    shuffle(vec.begin(), vec.end(), g_rng);
          }

          // Robust fallback: ensure no topic is empty. Fill from nearest non-empty topic;
          // if everything else fails, put question 0 as fallback.
          for (int t = 0; t < NUM_TOPICS; ++t)
          {
               if (game.topic_questions[t].empty())
               {
                    bool filled = false;
                    for (int dir = 1; dir <= NUM_TOPICS; ++dir)
                    {
                         int left = t - dir;
                         int right = t + dir;
                         if (left >= 0 && !game.topic_questions[left].empty())
                         {
                              game.topic_questions[t].push_back(game.topic_questions[left][0]);
                              filled = true;
                              break;
                         }
                         if (right < NUM_TOPICS && !game.topic_questions[right].empty())
                         {
                              game.topic_questions[t].push_back(game.topic_questions[right][0]);
                              filled = true;
                              break;
                         }
                    }
                    if (!filled)
                    {
                         game.topic_questions[t].push_back(0);
                    }
               }
          }
     }
     // ----------------- end topic distribution / fallback ---------------------

     game.practice_skips_remaining = GameConfig::PRACTICE_MAX_SKIPS;  // Initialize skip counter

     // --- P1: Load saved settings ---
     load_settings();

     bool quit = false;
     SDL_Event e;
     unsigned int last_time = SDL_GetTicks();

     // P0: Don't globally start text input — scope it to NAME_INPUT only
#ifndef __ANDROID__
     SDL_StartTextInput();  // Desktop: always on (keyboard always available)
#endif

     while (!quit)
     {
          unsigned int frame_start = SDL_GetTicks();
          unsigned int curr_time = frame_start;
          float delta_time = (curr_time - last_time) / 1000.0f;
          last_time = curr_time;

          // P0: Skip updates while app is paused (backgrounded)
          if (app_paused)
          {
               SDL_Delay(100);  // Low-power sleep while backgrounded
               SDL_PollEvent(&e);  // Still pump events to catch resume
               if (e.type == SDL_APP_DIDENTERFOREGROUND)
               {
                    app_paused = false;
                    unsigned int pause_duration = SDL_GetTicks() - pause_start_time;
                    total_pause_offset += pause_duration;
                    // P1: Audio focus — resume audio on foreground
                    Mix_Resume(-1);
                    SDL_Log("App resumed, pause offset: %u ms", pause_duration);
               }
               continue;
          }

          game.animation_time += delta_time;

          // P0: Manage soft keyboard visibility on Android
          update_ime_state();
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

               // --- P0: App lifecycle pause/resume ---
               if (e.type == SDL_APP_WILLENTERBACKGROUND || e.type == SDL_APP_DIDENTERBACKGROUND)
               {
                    app_paused = true;
                    pause_start_time = SDL_GetTicks();
                    // P1: Audio focus — pause all audio channels
                    Mix_Pause(-1);
                    SDL_Log("App paused (backgrounded)");
                    // P1: Save in-progress match
                    save_game_state();
                    continue;
               }
               if (e.type == SDL_APP_DIDENTERFOREGROUND)
               {
                    app_paused = false;
                    unsigned int pause_duration = SDL_GetTicks() - pause_start_time;
                    total_pause_offset += pause_duration;
                    Mix_Resume(-1);
                    SDL_Log("App resumed, pause offset: %u ms", pause_duration);
               }

                // --- Feature: Controller Input Processing ---
                if (e.type == SDL_CONTROLLERBUTTONDOWN || e.type == SDL_CONTROLLERBUTTONUP ||
                    e.type == SDL_CONTROLLERAXISMOTION || e.type == SDL_CONTROLLERDEVICEADDED ||
                    e.type == SDL_CONTROLLERDEVICEREMOVED)
                {
                     g_controllerMgr.updateControllerState(e);
                }

                // Controller actions (A = confirm/roll, B = back)
                if (g_controllerMgr.wasButtonJustPressed(ControllerManager::CONFIRM))
                {
                     if (game.state == PLAYING && !game.players[game.current].isBot)
                          dice_roll();
                }
                if (g_controllerMgr.wasButtonJustPressed(ControllerManager::CANCEL))
                {
                     if (game.state != WELCOME)
                          game.state = WELCOME;
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
                         // Only allow Human to use Spacebar
                         if (!game.players[game.current].isBot)
                         {
                              if (e.key.keysym.sym == SDLK_SPACE)
                                   dice_roll();
                         }
                         if (e.key.keysym.sym == SDLK_h)
                              game.show_hints = !game.show_hints;
                    }
                    else if (game.state == PRACTICE_MODE)
                    {
                         // Allow keyboard answers (A..F) in practice mode
                         int choice = -1;
                         if (e.key.keysym.sym == SDLK_a)
                              choice = 0;
                         else if (e.key.keysym.sym == SDLK_b)
                              choice = 1;
                         else if (e.key.keysym.sym == SDLK_c)
                              choice = 2;
                         else if (e.key.keysym.sym == SDLK_d)
                              choice = 3;
                         else if (e.key.keysym.sym == SDLK_e)
                              choice = 4;
                         else if (e.key.keysym.sym == SDLK_f)
                              choice = 5;
                         else if (e.key.keysym.sym == SDLK_q)
                         {
                              // Quit practice and show scoreboard
                              game.practice_question_index = -1;
                              continue;
                         }

                         if (choice >= 0)
                         {
                              // safety: ensure topic/questions available
                              bool noQuestionsAvailable = game.topic_questions.empty()
                                   || game.practice_topic < 0
                                   || game.practice_topic >= (int)game.topic_questions.size();
                              if (noQuestionsAvailable)
                                   continue;
                              if (game.topic_questions[game.practice_topic].empty())
                                   continue;
                              size_t sz = game.topic_questions[game.practice_topic].size();
                              int qidx = game.topic_questions[game.practice_topic][game.practice_question_index % sz];

                              if (game.question_ui_options.empty())
                              {
                                   auto p = prepare_options_for_ui(questions[qidx], g_questionOptionCount);
                                   game.question_ui_options = p.first;
                                   game.question_ui_correct = p.second;
                              }

                              if (choice < (int)game.question_ui_options.size())
                              {
                                   game.practice_total_answered++;
                                   if (choice == game.question_ui_correct)
                                   {
                                        game.practice_score++;
                                        if (game.soundEnabled && snd_correct)
                                             Mix_PlayChannel(-1, snd_correct, 0);
                                   }
                                   else
                                   {
                                        if (game.soundEnabled && snd_error)
                                             Mix_PlayChannel(-1, snd_error, 0);
                                   }
                                   game.practice_question_index++;
                                   game.question_ui_options.clear();
                                   game.question_ui_correct = -1;
                              }
                         }
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
               if (e.type == SDL_MOUSEBUTTONUP)
               {
                    // Stop scrollbar dragging
                    if (practice_scrollbar_dragging)
                         practice_scrollbar_dragging = false;
               }
               if (e.type == SDL_MOUSEMOTION)
                    mouse_motion_handle(e.motion.x, e.motion.y);



               // --- Android Back Button ---
#ifdef __ANDROID__
               if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_AC_BACK)
               {
                    if (game.state == WELCOME)
                         quit = true;  // Exit app from main menu
                    else if (game.state == PLAYING)
                         game.state = WELCOME;  // Back to menu from game
                    else
                         game.state = WELCOME;  // Any other screen -> menu
               }
#endif

               if (e.type == SDL_MOUSEWHEEL && game.state == RULES)
               {
                    if (e.wheel.y > 0)
                         rulesScroll -= rulesScrollStep;
                    if (e.wheel.y < 0)
                         rulesScroll += rulesScrollStep;
               }
               // Practice topic list scrolling with mouse wheel
               if (e.type == SDL_MOUSEWHEEL && game.state == PRACTICE_TOPIC_SELECT)
               {
                    int total_topics = GameConfig::NUM_TOPICS;
                    if (!game.topic_questions.empty())
                         total_topics = (int)game.topic_questions.size();
                    int max_scroll = max(0, total_topics - VISIBLE_TOPICS);
                    if (e.wheel.y > 0)
                         practice_topic_scroll = max(0, practice_topic_scroll - 1);
                    if (e.wheel.y < 0)
                         practice_topic_scroll = min(max_scroll, practice_topic_scroll + 1);
               }
          }
          if (game.requestQuit)
               quit = true;

          if (game.state == PLAYING)
          {
               if (game.preGameTimerActive)
               {
                    unsigned int elapsed = ((SDL_GetTicks() - total_pause_offset) - game.preGameStartTime) / 1000;
                    game.preGameCountdown = max(0, game.preGameDefaultCountdown - (int)elapsed);
                    if (game.preGameCountdown == 0)
                         game.preGameTimerActive = false;
               }

               // Bot Update
               bot_update();

               if (game.questionState == ASKING_QUESTION)
               {
                    unsigned int elapsed = ((SDL_GetTicks() - total_pause_offset) - game.questionStartTime) / 1000;
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
               // --- PROFESSIONAL WINNER SCREEN ---
               SDL_Rect overlay = {0, 0, width, height};
               SDL_SetRenderDrawColor(renderer, 0, 0, 0, 180);
               SDL_RenderFillRect(renderer, &overlay);

               SDL_Rect win_box = {width / 2 - 280, height / 2 - 200, 560, 400};

               // Gold gradient border (trophy style)
               for (int i = 0; i < 6; i++)
               {
                    Uint8 g = (Uint8)(196 - i * 20);
                    SDL_SetRenderDrawColor(renderer, 241, g, 15, 255);
                    SDL_Rect border = {win_box.x - i, win_box.y - i, win_box.w + 2 * i, win_box.h + 2 * i};
                    SDL_RenderDrawRect(renderer, &border);
               }

               // Dark background with slight transparency
               rounded_rect_draw(win_box, {30, 30, 50, 245}, 15);

               // Winner name with crown emoji style
               string playerWinName = game.players[game.current].name.empty()
                    ? ("Player " + to_string(game.current + 1))
                    : game.players[game.current].name;
               center_text_draw("VICTORY!", width / 2, win_box.y + 45, colors.gold, gigantic_font);
               center_text_draw(playerWinName + " wins the game!", width / 2, win_box.y + 95, colors.white, medium_font);

               // Divider line
               SDL_SetRenderDrawColor(renderer, colors.gold.r, colors.gold.g, colors.gold.b, 120);
               SDL_RenderDrawLine(renderer, win_box.x + 40, win_box.y + 120, win_box.x + win_box.w - 40, win_box.y + 120);

               // Player Rankings (sorted by position)
               center_text_draw("FINAL STANDINGS", width / 2, win_box.y + 140, colors.gold, medium_font);

               // Sort players by position (descending)
               struct RankEntry { int idx; int pos; string name; };
               vector<RankEntry> ranks;
               for (int i = 0; i < game.total_players; i++)
               {
                    string n = game.players[i].name.empty() ? ("Player " + to_string(i + 1)) : game.players[i].name;
                    ranks.push_back({i, game.players[i].pos, n});
               }
               sort(ranks.begin(), ranks.end(), [](const RankEntry &a, const RankEntry &b) { return a.pos > b.pos; });

               int rankY = win_box.y + 165;
               string medals[] = {"1st", "2nd", "3rd", "4th"};
               SDL_Color medalColors[] = {colors.gold, {192, 192, 192, 255}, {205, 127, 50, 255}, colors.white};
               for (int i = 0; i < (int)ranks.size(); i++)
               {
                    SDL_Color mc = (i < 4) ? medalColors[i] : colors.white;
                    string rank_text = medals[min(i, 3)] + "  " + ranks[i].name + "  (Cell " + to_string(ranks[i].pos) + ")";
                    center_text_draw(rank_text, width / 2, rankY, mc, small_font);
                    rankY += 24;
               }

               // Game Stats
               int statsY = rankY + 10;
               SDL_SetRenderDrawColor(renderer, colors.gold.r, colors.gold.g, colors.gold.b, 80);
               SDL_RenderDrawLine(renderer, win_box.x + 40, statsY, win_box.x + win_box.w - 40, statsY);
               statsY += 15;

               int totalAnswered = game.answeredCorrectCount + game.answeredWrongCount;
               string statsText = "Questions: " + to_string(totalAnswered) +
                    "  |  Correct: " + to_string(game.answeredCorrectCount) +
                    "  |  Wrong: " + to_string(game.answeredWrongCount);
               center_text_draw(statsText, width / 2, statsY, {180, 180, 200, 255}, small_font);

               // Play Again button
               SDL_Rect replayBtn = {width / 2 - 100, win_box.y + win_box.h - 55, 200, 42};
               int mx, my;
               SDL_GetMouseState(&mx, &my);
               bool hoverReplay = isPointInRect(mx, my, replayBtn);
               button_draw(replayBtn, "PLAY AGAIN", colors.green, colors.white, hoverReplay, medium_font);
               break;
          }
          }

          // --- Feature HUD Overlays (drawn on top of everything) ---
          notification_hud_draw();
          controller_status_draw();

          SDL_RenderPresent(renderer);

          // P1: Adaptive frame timing — don't double-throttle with VSYNC
          unsigned int frame_time = SDL_GetTicks() - frame_start;
          if (frame_time < (unsigned int)GameConfig::FRAME_DELAY_MS)
               SDL_Delay(GameConfig::FRAME_DELAY_MS - frame_time);
     }
     // P1: Save settings on clean exit
     save_settings();

     // --- Feature Cleanup ---
     g_controllerMgr.cleanup();
     g_cloudSave.syncToCloud();

     SDL_StopTextInput();
     if (pref_path) SDL_free(pref_path);
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
