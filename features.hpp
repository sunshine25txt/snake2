#ifndef FEATURES_HPP
#define FEATURES_HPP

#include <SDL2/SDL.h>
#include <string>
#include <vector>
#include <ctime>
#include <map>
#include <algorithm>

using namespace std;

/* ==================== FEATURE 1: Dark Mode Manager ==================== */
class DarkModeManager {
public:
    enum ThemeMode { LIGHT, DARK, AUTO };

    ThemeMode currentTheme = LIGHT;

    void toggleTheme() {
        currentTheme = (currentTheme == LIGHT) ? DARK : LIGHT;
    }

    void setTheme(ThemeMode mode) {
        currentTheme = mode;
    }

    bool isDarkMode() const {
        return currentTheme == DARK;
    }

    // Get themed colors based on current mode
    SDL_Color getBackgroundPrimary() const {
        if (isDarkMode())
            return {30, 30, 48, 255};   // Dark navy
        return {41, 128, 185, 255};      // Ocean Blue (original)
    }

    SDL_Color getBackgroundSecondary() const {
        if (isDarkMode())
            return {60, 40, 80, 255};    // Dark purple
        return {155, 89, 182, 255};      // Amethyst (original)
    }

    SDL_Color getCardBackground() const {
        if (isDarkMode())
            return {45, 45, 65, 255};    // Dark card
        return {255, 255, 255, 255};     // White (original)
    }

    SDL_Color getTextPrimary() const {
        if (isDarkMode())
            return {230, 230, 240, 255}; // Light gray text
        return {44, 62, 80, 255};        // Dark Blue (original)
    }

    SDL_Color getPanelBackground() const {
        if (isDarkMode())
            return {35, 35, 55, 200};    // Dark panel
        return {44, 62, 80, 150};        // Original dark panel
    }
};

/* ==================== FEATURE 2: Offline Mode Manager ==================== */
class OfflineManager {
private:
    struct GameSnapshot {
        int playerPos[4];
        int currentPlayer;
        int totalPlayers;
        int diceValue;
        bool isAvailable = false;
    };

    GameSnapshot cachedGame;
    bool hasInternet = true;

public:
    void cacheGameState(int playerPos[], int current, int total, int dice) {
        for (int i = 0; i < 4; i++)
            cachedGame.playerPos[i] = playerPos[i];
        cachedGame.currentPlayer = current;
        cachedGame.totalPlayers = total;
        cachedGame.diceValue = dice;
        cachedGame.isAvailable = true;
    }

    bool restoreCachedGame(int playerPos[], int &currentPlayer, int &totalPlayers, int &diceValue) {
        if (cachedGame.isAvailable) {
            for (int i = 0; i < 4; i++)
                playerPos[i] = cachedGame.playerPos[i];
            currentPlayer = cachedGame.currentPlayer;
            totalPlayers = cachedGame.totalPlayers;
            diceValue = cachedGame.diceValue;
            return true;
        }
        return false;
    }

    void clearCache() { cachedGame.isAvailable = false; }

    void setNetworkStatus(bool online) {
        hasInternet = online;
    }

    bool isOnline() const { return hasInternet; }
    bool hasCachedGame() const { return cachedGame.isAvailable; }
};

/* ==================== FEATURE 3: Push Notification System ==================== */
class NotificationManager {
public:
    enum NotificationType { TURN_ALERT, ACHIEVEMENT, MILESTONE, REMINDER };

    struct Notification {
        string title;
        string message;
        NotificationType type;
        unsigned int timestamp;
        bool read = false;
    };

    vector<Notification> notifications;

    void sendNotification(const string &title, const string &msg, NotificationType type) {
        Notification n;
        n.title = title;
        n.message = msg;
        n.type = type;
        n.timestamp = SDL_GetTicks();
        notifications.push_back(n);
    }

    void clearOldNotifications(unsigned int ageMs = 5000) {
        unsigned int now = SDL_GetTicks();
        notifications.erase(
            remove_if(notifications.begin(), notifications.end(),
                [now, ageMs](const Notification &n) { return (now - n.timestamp) > ageMs; }),
            notifications.end()
        );
    }

    // Get only active (recent) notifications for display
    vector<Notification> getActiveNotifications(unsigned int maxAgeMs = 3000) const {
        vector<Notification> active;
        unsigned int now = SDL_GetTicks();
        for (const auto &n : notifications) {
            if ((now - n.timestamp) <= maxAgeMs)
                active.push_back(n);
        }
        return active;
    }

    int getUnreadCount() const {
        int count = 0;
        for (const auto &n : notifications)
            if (!n.read) count++;
        return count;
    }

    void markAllRead() {
        for (auto &n : notifications)
            n.read = true;
    }
};

/* ==================== FEATURE 4: Cloud Save Manager ==================== */
class CloudSaveManager {
private:
    string lastSyncTime;
    bool isSyncing = false;

public:
    struct CloudData {
        int highScore = 0;
        int gamesPlayed = 0;
        int totalWins = 0;
        string lastPlayerName;
        unsigned int lastPlayDate = 0;
    };

    CloudData cloudData;

    void syncToCloud() {
        isSyncing = true;
        // Simulate cloud sync
        lastSyncTime = getCurrentTimestamp();
        isSyncing = false;
    }

    void loadFromCloud() {
        // Load cloud data
        SDL_Log("Loading data from cloud");
    }

    void updateStats(const string &playerName, bool isWin) {
        cloudData.gamesPlayed++;
        if (isWin) cloudData.totalWins++;
        cloudData.lastPlayerName = playerName;
        cloudData.lastPlayDate = SDL_GetTicks();
    }

    bool isSyncInProgress() const { return isSyncing; }

    string getLastSyncTime() const { return lastSyncTime; }

    int getGamesPlayed() const { return cloudData.gamesPlayed; }
    int getTotalWins() const { return cloudData.totalWins; }

private:
    string getCurrentTimestamp() {
        time_t now = time(0);
        struct tm *timeinfo = localtime(&now);
        char buffer[80];
        strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);
        return string(buffer);
    }
};

/* ==================== FEATURE 5: Achievement System ==================== */
class AchievementManager {
public:
    enum AchievementID {
        FIRST_WIN,
        SNAKE_MASTER,
        LADDER_CLIMBER,
        SPEEDRUN,
        PERFECT_GAME,
        SOCIAL_BUTTERFLY,
        CONSISTENCY,
        LEGEND
    };

    struct Achievement {
        AchievementID id;
        string name;
        string description;
        bool unlocked = false;
        unsigned int unlockedTime = 0;
        int progress = 0;  // 0-100%
    };

    map<AchievementID, Achievement> achievements;

    // Track counters for progress
    int snakesSurvived = 0;
    int laddersClimbed = 0;
    int consecutiveCorrect = 0;

    AchievementManager() {
        initializeAchievements();
    }

    void initializeAchievements() {
        achievements[FIRST_WIN] = {FIRST_WIN, "First Victory", "Win your first game", false, 0, 0};
        achievements[SNAKE_MASTER] = {SNAKE_MASTER, "Snake Master", "Survive 5 snakes in one game", false, 0, 0};
        achievements[LADDER_CLIMBER] = {LADDER_CLIMBER, "Ladder Climber", "Climb 10 ladders total", false, 0, 0};
        achievements[SPEEDRUN] = {SPEEDRUN, "Speed Runner", "Win in under 5 minutes", false, 0, 0};
        achievements[PERFECT_GAME] = {PERFECT_GAME, "Perfect Game", "Answer 10 questions correctly in a row", false, 0, 0};
        achievements[SOCIAL_BUTTERFLY] = {SOCIAL_BUTTERFLY, "Social Butterfly", "Play with 4 players", false, 0, 0};
        achievements[CONSISTENCY] = {CONSISTENCY, "Consistent", "Play 10 games", false, 0, 0};
        achievements[LEGEND] = {LEGEND, "LEGEND", "Unlock all other achievements", false, 0, 0};
    }

    // Returns true if newly unlocked (for notification trigger)
    bool checkAndUnlock(AchievementID id) {
        if (!achievements[id].unlocked) {
            achievements[id].unlocked = true;
            achievements[id].unlockedTime = SDL_GetTicks();
            achievements[id].progress = 100;
            SDL_Log("Achievement Unlocked: %s", achievements[id].name.c_str());

            // Check if LEGEND should unlock (all others unlocked)
            if (id != LEGEND) {
                bool allUnlocked = true;
                for (const auto &a : achievements) {
                    if (a.first != LEGEND && !a.second.unlocked) {
                        allUnlocked = false;
                        break;
                    }
                }
                if (allUnlocked) checkAndUnlock(LEGEND);
            }
            return true;
        }
        return false;
    }

    void updateProgress(AchievementID id, int progress) {
        if (progress > achievements[id].progress)
            achievements[id].progress = min(100, progress);
    }

    void resetGameCounters() {
        snakesSurvived = 0;
        laddersClimbed = 0;
        consecutiveCorrect = 0;
    }

    int getUnlockedCount() const {
        int count = 0;
        for (const auto &a : achievements)
            if (a.second.unlocked) count++;
        return count;
    }

    int getTotalCount() const {
        return (int)achievements.size();
    }

    vector<Achievement> getRecentUnlocks(int count = 3) const {
        vector<Achievement> recent;
        for (const auto &a : achievements) {
            if (a.second.unlocked)
                recent.push_back(a.second);
        }
        sort(recent.begin(), recent.end(),
            [](const Achievement &a, const Achievement &b) {
                return a.unlockedTime > b.unlockedTime;
            });
        if (recent.size() > (size_t)count)
            recent.resize(count);
        return recent;
    }

    vector<Achievement> getAllAchievements() const {
        vector<Achievement> all;
        for (const auto &a : achievements)
            all.push_back(a.second);
        return all;
    }
};

/* ==================== FEATURE 6: Leaderboard Manager ==================== */
class LeaderboardManager {
public:
    struct PlayerRank {
        string name;
        int score = 0;
        int wins = 0;
        int gamesPlayed = 0;
        float winRate = 0.0f;
        unsigned int lastPlayedTime = 0;
    };

    vector<PlayerRank> globalLeaderboard;

    void addPlayerScore(const string &name, int score, bool isWin) {
        for (auto &player : globalLeaderboard) {
            if (player.name == name) {
                player.score += score;
                player.gamesPlayed++;
                if (isWin) player.wins++;
                player.winRate = (player.gamesPlayed > 0) ? (float)player.wins / player.gamesPlayed : 0.0f;
                player.lastPlayedTime = SDL_GetTicks();
                sortLeaderboard();
                return;
            }
        }
        // New player
        PlayerRank newPlayer;
        newPlayer.name = name;
        newPlayer.score = score;
        newPlayer.gamesPlayed = 1;
        newPlayer.wins = isWin ? 1 : 0;
        newPlayer.winRate = isWin ? 1.0f : 0.0f;
        newPlayer.lastPlayedTime = SDL_GetTicks();
        globalLeaderboard.push_back(newPlayer);
        sortLeaderboard();
    }

    int getPlayerRank(const string &name) const {
        for (int i = 0; i < (int)globalLeaderboard.size(); i++) {
            if (globalLeaderboard[i].name == name)
                return i + 1;
        }
        return -1;
    }

    vector<PlayerRank> getTopPlayers(int count = 10) const {
        vector<PlayerRank> top = globalLeaderboard;
        if (top.size() > (size_t)count)
            top.resize(count);
        return top;
    }

    bool isEmpty() const { return globalLeaderboard.empty(); }

private:
    void sortLeaderboard() {
        sort(globalLeaderboard.begin(), globalLeaderboard.end(),
            [](const PlayerRank &a, const PlayerRank &b) {
                if (a.score != b.score) return a.score > b.score;
                return a.wins > b.wins;
            });
    }
};

/* ==================== FEATURE 7: Auto Update Checker ==================== */
class UpdateChecker {
public:
    struct VersionInfo {
        string currentVersion = "1.2";
        string latestVersion = "1.2";
        string downloadUrl;
        string changelog;
        bool updateAvailable = false;
    };

    VersionInfo versionInfo;
    bool isCheckingForUpdates = false;
    bool hasChecked = false;

    void checkForUpdates() {
        isCheckingForUpdates = true;
        // Simulate checking remote server
        SDL_Log("Checking for updates...");
        // Simulated check (would normally hit a server)
        versionInfo.updateAvailable = false;
        isCheckingForUpdates = false;
        hasChecked = true;
    }

    bool shouldUpdate() const {
        return versionInfo.updateAvailable;
    }

    string getVersionString() const {
        return "v" + versionInfo.currentVersion;
    }

    string getStatusString() const {
        if (!hasChecked) return "Not checked";
        if (isCheckingForUpdates) return "Checking...";
        if (versionInfo.updateAvailable) return "Update available!";
        return "Up to date";
    }
};

/* ==================== FEATURE 8: Controller Support Manager ==================== */
class ControllerManager {
public:
    enum ButtonAction { NONE, CONFIRM, CANCEL, ROLL_DICE, MENU, NAVIGATE_UP, NAVIGATE_DOWN, NAVIGATE_LEFT, NAVIGATE_RIGHT };

    struct ControllerInput {
        float leftStickX = 0.0f;
        float leftStickY = 0.0f;
        float rightStickX = 0.0f;
        float rightStickY = 0.0f;
        bool buttonA = false;
        bool buttonB = false;
        bool buttonX = false;
        bool buttonY = false;
        bool buttonStart = false;
        bool buttonSelect = false;
        bool triggerL = false;
        bool triggerR = false;
        bool dpadUp = false;
        bool dpadDown = false;
        bool dpadLeft = false;
        bool dpadRight = false;
    };

    ControllerInput currentInput;
    ControllerInput previousInput;
    int connectedControllers = 0;
    SDL_GameController *activeController = nullptr;

    void init() {
        // Open first available controller
        for (int i = 0; i < SDL_NumJoysticks(); i++) {
            if (SDL_IsGameController(i)) {
                activeController = SDL_GameControllerOpen(i);
                if (activeController) {
                    connectedControllers++;
                    SDL_Log("Controller connected: %s", SDL_GameControllerName(activeController));
                    break;
                }
            }
        }
    }

    void updateControllerState(const SDL_Event &e) {
        previousInput = currentInput;

        if (e.type == SDL_CONTROLLERBUTTONDOWN) {
            handleButtonPress(e.cbutton.button, true);
        } else if (e.type == SDL_CONTROLLERBUTTONUP) {
            handleButtonPress(e.cbutton.button, false);
        } else if (e.type == SDL_CONTROLLERAXISMOTION) {
            handleAxisMotion(e.caxis.axis, e.caxis.value);
        } else if (e.type == SDL_CONTROLLERDEVICEADDED) {
            if (!activeController) {
                activeController = SDL_GameControllerOpen(e.cdevice.which);
            }
            connectedControllers++;
            SDL_Log("Controller connected! Total: %d", connectedControllers);
        } else if (e.type == SDL_CONTROLLERDEVICEREMOVED) {
            connectedControllers = max(0, connectedControllers - 1);
            if (activeController && e.cdevice.which == SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(activeController))) {
                SDL_GameControllerClose(activeController);
                activeController = nullptr;
            }
            SDL_Log("Controller disconnected! Total: %d", connectedControllers);
        }
    }

    // Returns true only on the frame the button was first pressed (edge detection)
    bool wasButtonJustPressed(ButtonAction action) const {
        switch (action) {
            case CONFIRM: return currentInput.buttonA && !previousInput.buttonA;
            case CANCEL: return currentInput.buttonB && !previousInput.buttonB;
            case ROLL_DICE: return currentInput.buttonX && !previousInput.buttonX;
            case MENU: return currentInput.buttonStart && !previousInput.buttonStart;
            default: return false;
        }
    }

    ButtonAction getNavigationAction() const {
        const float THRESHOLD = 0.5f;
        if (currentInput.dpadUp || currentInput.leftStickY > THRESHOLD) return NAVIGATE_UP;
        if (currentInput.dpadDown || currentInput.leftStickY < -THRESHOLD) return NAVIGATE_DOWN;
        if (currentInput.dpadLeft || currentInput.leftStickX < -THRESHOLD) return NAVIGATE_LEFT;
        if (currentInput.dpadRight || currentInput.leftStickX > THRESHOLD) return NAVIGATE_RIGHT;
        return NONE;
    }

    bool isControllerConnected() const {
        return connectedControllers > 0;
    }

    int getConnectedControllerCount() const {
        return connectedControllers;
    }

    void cleanup() {
        if (activeController) {
            SDL_GameControllerClose(activeController);
            activeController = nullptr;
        }
    }

private:
    void handleButtonPress(int button, bool pressed) {
        switch(button) {
            case SDL_CONTROLLER_BUTTON_A:
                currentInput.buttonA = pressed;
                break;
            case SDL_CONTROLLER_BUTTON_B:
                currentInput.buttonB = pressed;
                break;
            case SDL_CONTROLLER_BUTTON_X:
                currentInput.buttonX = pressed;
                break;
            case SDL_CONTROLLER_BUTTON_Y:
                currentInput.buttonY = pressed;
                break;
            case SDL_CONTROLLER_BUTTON_START:
                currentInput.buttonStart = pressed;
                break;
            case SDL_CONTROLLER_BUTTON_BACK:
                currentInput.buttonSelect = pressed;
                break;
            case SDL_CONTROLLER_BUTTON_DPAD_UP:
                currentInput.dpadUp = pressed;
                break;
            case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
                currentInput.dpadDown = pressed;
                break;
            case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
                currentInput.dpadLeft = pressed;
                break;
            case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
                currentInput.dpadRight = pressed;
                break;
        }
    }

    void handleAxisMotion(int axis, int value) {
        float normalized = (float)value / 32768.0f;
        if (abs(value) < 5000) normalized = 0.0f;  // Deadzone

        switch(axis) {
            case SDL_CONTROLLER_AXIS_LEFTX:
                currentInput.leftStickX = normalized;
                break;
            case SDL_CONTROLLER_AXIS_LEFTY:
                currentInput.leftStickY = -normalized;
                break;
            case SDL_CONTROLLER_AXIS_RIGHTX:
                currentInput.rightStickX = normalized;
                break;
            case SDL_CONTROLLER_AXIS_RIGHTY:
                currentInput.rightStickY = -normalized;
                break;
            case SDL_CONTROLLER_AXIS_TRIGGERLEFT:
                currentInput.triggerL = (value > 10000);
                break;
            case SDL_CONTROLLER_AXIS_TRIGGERRIGHT:
                currentInput.triggerR = (value > 10000);
                break;
        }
    }
};

#endif // FEATURES_HPP
