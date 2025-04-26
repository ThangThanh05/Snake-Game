#include <SDL.h>
#include <SDL_ttf.h>
#include <SDL_image.h>
#include <SDL_mixer.h>
#include <iostream>
#include <vector>
#include <string>
#include <cstdlib>
#include <ctime>
#include <deque>

using namespace std;

// Định nghĩa các hằng số
const int SCREEN_WIDTH = 1010;
const int SCREEN_HEIGHT = 520;
const int BLOCK_SIZE = 35;

// Định nghĩa kiểu dữ liệu enum
enum Direction { STOP = 0, LEFT, RIGHT, UP, DOWN };
enum MenuOption { NONE, MENU_PLAY, MENU_SETTINGS };

// Định nghĩa cấu trúc
struct Point {
    int x, y;
    bool operator==(const Point& other) const {
        return x == other.x && y == other.y;
    }
};

struct Button {
    SDL_Rect rect;
    string label;
};

struct Slider {
    SDL_Rect track;
    SDL_Rect handle;
    int minValue;
    int maxValue;
    int* value;
};

// Khai báo các biến toàn cục
SDL_Window* window = nullptr;
SDL_Renderer* renderer = nullptr;
SDL_Texture* bgMenu = nullptr;
SDL_Texture* bgGame = nullptr;
SDL_Texture* bgGameOver = nullptr;
SDL_Texture* snakeHeadTexture = nullptr;
SDL_Texture* snakeBodyTexture = nullptr;
SDL_Texture* snakeTailTexture = nullptr;
SDL_Texture* snakeTurnTLTexture;
SDL_Texture* snakeTurnTRTexture;
SDL_Texture* snakeTurnBLTexture;
SDL_Texture* snakeTurnBRTexture;
SDL_Texture* foodTexture = nullptr;
TTF_Font* font = nullptr;
TTF_Font* scoreFont = nullptr;
TTF_Font* menuFont = nullptr;
TTF_Font* mainMenuFont = nullptr;
Mix_Chunk* eatSound = nullptr;
Mix_Chunk* hitSound = nullptr;
Mix_Music* menuMusic = nullptr;
Mix_Music* playingMusic = nullptr;
Mix_Chunk* clickSound = nullptr;

// Biến trạng thái trò chơi
bool isRunning = true;
bool gameOver = false;
bool isPaused = false; // Biến mới cho trạng thái tạm dừng
Direction dir = STOP;
deque<Point> snake;
Point fruit;
int score = 0;
int highScore = 0;
int delay = 100;

// Biến âm lượng
int musicVolume = 50;
int effectVolume = 50;

// Hàm khởi tạo trạng thái ban đầu của trò chơi
void InitGame() {
    snake.clear();
    dir = STOP;
    score = 0;
    gameOver = false;
    isPaused = false; // Đặt lại trạng thái tạm dừng
    snake.push_front({ SCREEN_WIDTH / 2 / BLOCK_SIZE * BLOCK_SIZE, SCREEN_HEIGHT / 2 / BLOCK_SIZE * BLOCK_SIZE });
    fruit = { rand() % (SCREEN_WIDTH / BLOCK_SIZE) * BLOCK_SIZE, rand() % (SCREEN_HEIGHT / BLOCK_SIZE) * BLOCK_SIZE };
}

// Hàm hiển thị văn bản
void RenderText(const string& message, int x, int y, SDL_Color color, TTF_Font* FONT) {
    SDL_Surface* surface = TTF_RenderText_Solid(FONT, message.c_str(), color);
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_Rect dst = { x, y, surface->w, surface->h };
    SDL_RenderCopy(renderer, texture, NULL, &dst);
    SDL_FreeSurface(surface);
    SDL_DestroyTexture(texture);
}

// Hàm vẽ nút bấm
void DrawButton(const Button& button, SDL_Color color, SDL_Color textColor, bool isHovered = false) {
    SDL_Rect drawRect = button.rect;
    if (isHovered) {
        drawRect.x -= 5;
        drawRect.y -= 5;
        drawRect.w += 10;
        drawRect.h += 10;
    }
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_RenderFillRect(renderer, &drawRect);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDrawRect(renderer, &drawRect);
    SDL_Surface* textSurface = TTF_RenderText_Solid(menuFont, button.label.c_str(), textColor);
    SDL_Texture* textTexture = SDL_CreateTextureFromSurface(renderer, textSurface);
    int textWidth = textSurface->w;
    int textHeight = textSurface->h;
    SDL_FreeSurface(textSurface);
    SDL_Rect textRect;
    textRect.x = drawRect.x + (drawRect.w - textWidth) / 2;
    textRect.y = drawRect.y + (drawRect.h - textHeight) / 2;
    textRect.w = textWidth;
    textRect.h = textHeight;
    SDL_RenderCopy(renderer, textTexture, NULL, &textRect);
    SDL_DestroyTexture(textTexture);
}

// Hàm vẽ thanh trượt
void DrawSlider(const Slider& slider, const string& label) {
    RenderText(label + ": " + to_string(*slider.value), slider.track.x - 150, slider.track.y + 15, { 255, 255, 153 }, menuFont);
    SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
    SDL_RenderFillRect(renderer, &slider.track);
    SDL_SetRenderDrawColor(renderer, 70, 130, 180, 255);
    SDL_RenderFillRect(renderer, &slider.handle);
}

// Hàm cập nhật slider
void UpdateSlider(Slider& slider, int mouseX) {
    if (mouseX < slider.track.x) mouseX = slider.track.x;
    if (mouseX > slider.track.x + slider.track.w) mouseX = slider.track.x + slider.track.w;
    slider.handle.x = mouseX - slider.handle.w / 2;
    float ratio = (float)(mouseX - slider.track.x) / slider.track.w;
    *slider.value = slider.minValue + (int)(ratio * (slider.maxValue - slider.minValue));
    if (slider.value == &musicVolume) {
        Mix_VolumeMusic(*slider.value);
    }
    else if (slider.value == &effectVolume) {
        Mix_Volume(-1, *slider.value);
        Mix_VolumeChunk(eatSound, *slider.value);
        Mix_VolumeChunk(hitSound, *slider.value);
        Mix_VolumeChunk(clickSound, *slider.value);
    }
}

// Hàm hiển thị menu cài đặt
MenuOption ShowSettingsMenu() {
    Button backButton = { { SCREEN_WIDTH / 2 - 100, 400, 200, 60 }, "Back" };

    Slider musicSlider = { { SCREEN_WIDTH / 2 - 100, 200, 200, 20 }, { SCREEN_WIDTH / 2 - 10, 190, 20, 40 }, 0, 100, &musicVolume };
    Slider effectSlider = { { SCREEN_WIDTH / 2 - 100, 300, 200, 20 }, { SCREEN_WIDTH / 2 - 10, 290, 20, 40 }, 0, 100, &effectVolume };

    float musicRatio = (float)musicVolume / 100;
    musicSlider.handle.x = musicSlider.track.x + (int)(musicRatio * musicSlider.track.w) - musicSlider.handle.w / 2;
    float effectRatio = (float)effectVolume / 100;
    effectSlider.handle.x = effectSlider.track.x + (int)(effectRatio * effectSlider.track.w) - effectSlider.handle.w / 2;

    bool selecting = true;
    SDL_Event e;
    bool draggingMusic = false;
    bool draggingEffect = false;

    while (selecting) {
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        if (bgMenu) SDL_RenderCopy(renderer, bgMenu, NULL, NULL);
        RenderText("Settings", SCREEN_WIDTH / 2 - 60, 100, { 0, 0, 0, 255 }, font);
        int mouseX, mouseY;
        SDL_GetMouseState(&mouseX, &mouseY);
        SDL_Point mousePoint = { mouseX, mouseY };
        bool hoverBack = SDL_PointInRect(&mousePoint, &backButton.rect);
        DrawButton(backButton, { 200, 60, 60 }, { 255, 255, 255 }, hoverBack);
        DrawSlider(musicSlider, "Music Volume");
        DrawSlider(effectSlider, "Effect Volume");
        SDL_RenderPresent(renderer);

        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) exit(0);
            if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
                if (hoverBack) {
                    Mix_PlayChannel(-1, clickSound, 0);
                    return NONE;
                }
                if (SDL_PointInRect(&mousePoint, &musicSlider.handle)) {
                    draggingMusic = true;
                }
                if (SDL_PointInRect(&mousePoint, &effectSlider.handle)) {
                    draggingEffect = true;
                }
            }
            if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_LEFT) {
                draggingMusic = false;
                draggingEffect = false;
            }
            if (e.type == SDL_MOUSEMOTION && (draggingMusic || draggingEffect)) {
                if (draggingMusic) {
                    UpdateSlider(musicSlider, mouseX);
                }
                if (draggingEffect) {
                    UpdateSlider(effectSlider, mouseX);
                }
            }
        }
        SDL_Delay(16);
    }
    return NONE;
}

// Hàm hiển thị menu chính
MenuOption ShowMainMenu() {
    Button playButton = { { SCREEN_WIDTH / 2 - 100, 230, 200, 60 }, "Play" };
    Button settingsButton = { { SCREEN_WIDTH / 2 - 100, 330, 200, 60 }, "Settings" };

    SDL_Event e;
    bool selecting = true;

    Mix_VolumeMusic(musicVolume);
    Mix_PlayMusic(menuMusic, -1);

    SDL_Surface* titleSurface = TTF_RenderText_Solid(mainMenuFont, "Snake Game", { 255, 0, 0, 255 });
    SDL_Texture* titleTexture = SDL_CreateTextureFromSurface(renderer, titleSurface);
    SDL_Rect titleRect;
    titleRect.w = titleSurface->w;
    titleRect.h = titleSurface->h;
    titleRect.x = (SCREEN_WIDTH / 2) - 150;
    titleRect.y = 120;
    SDL_FreeSurface(titleSurface);

    while (selecting) {
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        if (bgMenu) SDL_RenderCopy(renderer, bgMenu, NULL, NULL);
        SDL_RenderCopy(renderer, titleTexture, NULL, &titleRect);
        int mouseX, mouseY;
        SDL_GetMouseState(&mouseX, &mouseY);
        SDL_Point mousePoint = { mouseX, mouseY };
        bool hoverPlay = SDL_PointInRect(&mousePoint, &playButton.rect);
        bool hoverSettings = SDL_PointInRect(&mousePoint, &settingsButton.rect);
        DrawButton(playButton, { 70, 130, 180, 255 }, { 255, 255, 255, 255 }, hoverPlay);
        DrawButton(settingsButton, { 70, 130, 180, 255 }, { 255, 255, 255, 255 }, hoverSettings);
        SDL_RenderPresent(renderer);

        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) exit(0);
            if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
                if (hoverPlay) {
                    Mix_PlayChannel(-1, clickSound, 0);
                    return MENU_PLAY;
                }
                if (hoverSettings) {
                    Mix_PlayChannel(-1, clickSound, 0);
                    ShowSettingsMenu();
                }
            }
        }
        SDL_Delay(16);
    }
    SDL_DestroyTexture(titleTexture);
    return NONE;
}

// Hàm hiển thị menu chọn độ khó
int ShowMenu() {
    Button easyBtn = { { 420, 220, 200, 40 }, "1.Easy" };
    Button mediumBtn = { { 420, 270, 200, 40 }, "2.Medium" };
    Button hardBtn = { { 420, 320, 200, 40 }, "3.Hard" };
    Button backBtn = { { 420, 380, 200, 40 }, "Back" };  // Nút Back

    bool choosing = true;
    SDL_Event e;

    while (choosing) {
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        if (bgMenu) SDL_RenderCopy(renderer, bgMenu, NULL, NULL);
        RenderText(" Select Difficulty", 380, 150, { 0, 0, 0, 255 }, font);
        int mouseX, mouseY;
        SDL_GetMouseState(&mouseX, &mouseY);
        SDL_Point mousePoint = { mouseX, mouseY };
        bool hoverEasy = SDL_PointInRect(&mousePoint, &easyBtn.rect);
        bool hoverMedium = SDL_PointInRect(&mousePoint, &mediumBtn.rect);
        bool hoverHard = SDL_PointInRect(&mousePoint, &hardBtn.rect);
        bool hoverBack = SDL_PointInRect(&mousePoint, &backBtn.rect);  // Hover cho Back
        DrawButton(easyBtn, { 70, 130, 180 }, { 255, 255, 255 }, hoverEasy);
        DrawButton(mediumBtn, { 70, 130, 180 }, { 255, 255, 255 }, hoverMedium);
        DrawButton(hardBtn, { 70, 130, 180 }, { 255, 255, 255 }, hoverHard);
        DrawButton(backBtn, { 200, 60, 60 }, { 255, 255, 255 }, hoverBack);  // Vẽ nút Back
        SDL_RenderPresent(renderer);

        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) exit(0);
            if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
                if (hoverEasy) {
                    Mix_PlayChannel(-1, clickSound, 0);
                    Mix_HaltMusic();
                    Mix_VolumeMusic(musicVolume);
                    Mix_PlayMusic(playingMusic, -1);
                    return 150;
                }
                if (hoverMedium) {
                    Mix_PlayChannel(-1, clickSound, 0);
                    Mix_HaltMusic();
                    Mix_VolumeMusic(musicVolume);
                    Mix_PlayMusic(playingMusic, -1);
                    return 100;
                }
                if (hoverHard) {
                    Mix_PlayChannel(-1, clickSound, 0);
                    Mix_HaltMusic();
                    Mix_VolumeMusic(musicVolume);
                    Mix_PlayMusic(playingMusic, -1);
                    return 50;
                }
                if (hoverBack) {
                    Mix_PlayChannel(-1, clickSound, 0);
                    return -1;  // Trả về -1 để biết là người chơi chọn quay lại
                }
            }
            if (e.type == SDL_KEYDOWN) {
                if (e.key.keysym.sym == SDLK_1) {
                    Mix_HaltMusic();
                    Mix_VolumeMusic(musicVolume);
                    Mix_PlayMusic(playingMusic, -1);
                    return 150;
                }
                if (e.key.keysym.sym == SDLK_2) {
                    Mix_HaltMusic();
                    Mix_VolumeMusic(musicVolume);
                    Mix_PlayMusic(playingMusic, -1);
                    return 100;
                }
                if (e.key.keysym.sym == SDLK_3) {
                    Mix_HaltMusic();
                    Mix_VolumeMusic(musicVolume);
                    Mix_PlayMusic(playingMusic, -1);
                    return 50;
                }
            }
        }
    }
    return 100;
}

// Hàm hiển thị màn hình Game Over
void ShowGameOver() {
    bool waiting = true;
    SDL_Event e;

    while (waiting) {
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        if (bgGameOver) SDL_RenderCopy(renderer, bgGameOver, NULL, NULL);
        RenderText("Game Over!", 315, 150, { 255, 0, 0 }, font);
        RenderText("Score: " + to_string(score), 340, 200, { 255, 255, 0, 255 }, font);
        RenderText("High Score: " + to_string(highScore), 300, 250, { 255, 255, 0, 255 }, font);
        RenderText("Press R to Play Again", 250, 300, { 255, 255, 255 }, font);
        RenderText("Press ESC to Exit", 290, 350, { 255, 255, 255 }, font);
        SDL_RenderPresent(renderer);

        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) exit(0);
            if (e.type == SDL_KEYDOWN) {
                if (e.key.keysym.sym == SDLK_ESCAPE) exit(0);
                if (e.key.keysym.sym == SDLK_r) {
                    Mix_VolumeMusic(musicVolume);
                    Mix_PlayMusic(menuMusic, -1);
                    isPaused = false; // Đặt lại trạng thái tạm dừng
                    delay = ShowMainMenu();
                    if (delay == MENU_PLAY) {
                        delay = ShowMenu();
                        InitGame();
                    }
                    waiting = false;
                }
            }
        }
    }
}

// Hàm di chuyển con rắn
void MoveSnake() {
    if (gameOver) return;

    Point head = snake.front();
    Point nextHead = head;

    if (dir == LEFT) nextHead.x -= BLOCK_SIZE;
    else if (dir == RIGHT) nextHead.x += BLOCK_SIZE;
    else if (dir == UP) nextHead.y -= BLOCK_SIZE;
    else if (dir == DOWN) nextHead.y += BLOCK_SIZE;

    if (nextHead.x < 0 || nextHead.y < 0 || nextHead.x >= SCREEN_WIDTH || nextHead.y >= SCREEN_HEIGHT) {
        gameOver = true;
        Mix_PlayChannel(-1, hitSound, 0);
        Mix_HaltMusic();
        return;
    }

    for (const auto& p : snake) {
        if (p == nextHead) {
            gameOver = true;
            Mix_PlayChannel(-1, hitSound, 0);
            Mix_HaltMusic();
            return;
        }
    }

    snake.push_front(nextHead);

    if (nextHead.x == fruit.x && nextHead.y == fruit.y) {
        score += 10;
        if (score > highScore) highScore = score;
        fruit = { rand() % (SCREEN_WIDTH / BLOCK_SIZE) * BLOCK_SIZE, rand() % (SCREEN_HEIGHT / BLOCK_SIZE) * BLOCK_SIZE };
        Mix_PlayChannel(-1, eatSound, 0);
    }
    else {
        snake.pop_back();
    }
}

// Hàm vẽ trò chơi
void DrawGame() {
    SDL_RenderClear(renderer);
    if (bgGame) SDL_RenderCopy(renderer, bgGame, NULL, NULL);
    for (size_t i = 0; i < snake.size(); i++) {
        SDL_Rect rect = { snake[i].x, snake[i].y, BLOCK_SIZE, BLOCK_SIZE };
        if (i == 0) {
            int angle = 0;
            switch (dir) {
            case UP: angle = 270; break;
            case DOWN: angle = 90; break;
            case LEFT: angle = 180; break;
            case RIGHT: angle = 0; break;
            }
            SDL_RenderCopyEx(renderer, snakeHeadTexture, NULL, &rect, angle, NULL, SDL_FLIP_NONE);
        }
        else if (i == snake.size() - 1) {
            if (snake.size() > 1) {
                int angle = 0;
                Point tail = snake.back();
                Point secondLast = snake[snake.size() - 2];
                if (tail.x < secondLast.x) angle = 0;
                else if (tail.x > secondLast.x) angle = 180;
                else if (tail.y < secondLast.y) angle = 90;
                else if (tail.y > secondLast.y) angle = 270;
                SDL_RenderCopyEx(renderer, snakeTailTexture, NULL, &rect, angle, NULL, SDL_FLIP_NONE);
            }
            else {
                SDL_RenderCopy(renderer, snakeTailTexture, NULL, &rect);
            }
        }
        else {
            Point current = snake[i];
            Point prev = snake[i - 1];
            Point next = snake[i + 1];

            SDL_Texture* textureToUse = snakeBodyTexture;
            int angle = 0;

            int dx1 = current.x - prev.x;
            int dy1 = current.y - prev.y;
            int dx2 = next.x - current.x;
            int dy2 = next.y - current.y;

            // Xử lý rẽ
            if ((dx1 < 0 && dy2 > 0) || (dy1 < 0 && dx2 > 0)) {
                // Phải → lên hoặc xuống → trái
                textureToUse = snakeTurnTLTexture;
            }
            else if ((dx1 < 0 && dy2 < 0) || (dy1 > 0 && dx2 > 0)) {
                // Phải → xuống hoặc lên → trái
                textureToUse = snakeTurnBLTexture;
            }
            else if ((dx1 > 0 && dy2 > 0) || (dy1 < 0 && dx2 < 0)) {
                // Trái → lên hoặc xuống → phải
                textureToUse = snakeTurnTRTexture;
            }
            else if ((dx1 > 0 && dy2 < 0) || (dy1 > 0 && dx2 < 0)) {
                // Trái → xuống hoặc lên → phải
                textureToUse = snakeTurnBRTexture;
            }
            else {
                // Di chuyển thẳng
                textureToUse = snakeBodyTexture;
                if (dx1 != 0) angle = 0;       // ngang
                else if (dy1 != 0) angle = 90; // dọc
            }

            SDL_RenderCopyEx(renderer, textureToUse, NULL, &rect, angle, NULL, SDL_FLIP_NONE);
        }

    }
    SDL_Rect foodRect = { fruit.x, fruit.y, BLOCK_SIZE, BLOCK_SIZE };
    SDL_RenderCopy(renderer, foodTexture, nullptr, &foodRect);
    RenderText("Score: " + to_string(score), 10, 10, { 255, 255, 255 }, scoreFont);
    RenderText("High Score: " + to_string(highScore), 10, 40, { 255, 255, 255 }, scoreFont);
    if (isPaused) {
        RenderText("Paused", SCREEN_WIDTH / 2 - 80, SCREEN_HEIGHT / 2 - 50, { 255, 255, 255 }, font);
        RenderText("Press SPACE to Continue", SCREEN_WIDTH / 2 - 170, SCREEN_HEIGHT / 2, { 255, 255, 255 }, scoreFont);
    }
    SDL_RenderPresent(renderer);
}

int main(int argc, char* argv[]) {
    srand((unsigned)time(0));
    SDL_Init(SDL_INIT_VIDEO);
    IMG_Init(IMG_INIT_JPG | IMG_INIT_PNG);
    Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048);
    TTF_Init();

    window = SDL_CreateWindow("Snake Game", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCREEN_WIDTH, SCREEN_HEIGHT, 0);
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    eatSound = Mix_LoadWAV("assets/eat.mp3");
    hitSound = Mix_LoadWAV("assets/game_over.mp3");
    menuMusic = Mix_LoadMUS("assets/music.mp3");
    playingMusic = Mix_LoadMUS("assets/music_playing.mp3");
    clickSound = Mix_LoadWAV("assets/click.mp3");

    Mix_VolumeMusic(musicVolume);
    Mix_VolumeChunk(eatSound, effectVolume);
    Mix_VolumeChunk(hitSound, effectVolume);
    Mix_VolumeChunk(clickSound, effectVolume);

    bgMenu = IMG_LoadTexture(renderer, "assets/snakeBackground.jpg");
    bgGame = IMG_LoadTexture(renderer, "assets/playingGame.jpg");
    bgGameOver = IMG_LoadTexture(renderer, "assets/bgGameOver.jpg");
    snakeHeadTexture = IMG_LoadTexture(renderer, "assets/headSnake.png");
    snakeBodyTexture = IMG_LoadTexture(renderer, "assets/bodySnake.png");
    snakeTailTexture = IMG_LoadTexture(renderer, "assets/tailSnake.png");
    snakeTurnTLTexture = IMG_LoadTexture(renderer, "assets/btTopLeft.png");
    snakeTurnTRTexture = IMG_LoadTexture(renderer, "assets/btTopRight.png");
    snakeTurnBLTexture = IMG_LoadTexture(renderer, "assets/btBottomLeft.png");
    snakeTurnBRTexture = IMG_LoadTexture(renderer, "assets/btBottomRight.png");

    foodTexture = IMG_LoadTexture(renderer, "assets/food.png");

    font = TTF_OpenFont("assets/font.ttf", 36);
    scoreFont = TTF_OpenFont("assets/font.ttf", 28);
    menuFont = TTF_OpenFont("assets/font.ttf", 24);
    mainMenuFont = TTF_OpenFont("assets/RAVIE.TTF", 40);

    MenuOption choice = NONE;

    // Lặp cho tới khi người chơi chọn bắt đầu game
    while (choice != MENU_PLAY) {
        choice = ShowMainMenu();
        if (choice == MENU_PLAY) {
            int speed = ShowMenu();
            if (speed == -1) {
                // Người chơi chọn nút "Back" -> quay lại menu chính
                choice = NONE;
            }
            else {
                delay = speed;
                InitGame();
                break; // Thoát khỏi vòng lặp để vào game
            }
        }
        else if (choice == MENU_SETTINGS) {
            ShowSettingsMenu(); // Nếu bạn có menu cài đặt
        }
        else if (choice == NONE) {
            isRunning = false; // Thoát nếu không chọn gì
        }
    }

    Uint32 lastTick = SDL_GetTicks();

    while (isRunning) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) isRunning = false;
            if (e.type == SDL_KEYDOWN) {
                if (e.key.keysym.sym == SDLK_SPACE && !gameOver) {
                    isPaused = !isPaused; // Chuyển đổi trạng thái tạm dừng
                    Mix_PlayChannel(-1, clickSound, 0); // Phát âm thanh click khi tạm dừng/tiếp tục
                }
                if (!isPaused && !gameOver) { // Chỉ xử lý điều khiển di chuyển khi không tạm dừng và không game over
                    if ((e.key.keysym.sym == SDLK_UP || e.key.keysym.sym == SDLK_w) && dir != DOWN) dir = UP;
                    else if ((e.key.keysym.sym == SDLK_DOWN || e.key.keysym.sym == SDLK_s) && dir != UP) dir = DOWN;
                    else if ((e.key.keysym.sym == SDLK_LEFT || e.key.keysym.sym == SDLK_a) && dir != RIGHT) dir = LEFT;
                    else if ((e.key.keysym.sym == SDLK_RIGHT || e.key.keysym.sym == SDLK_d) && dir != LEFT) dir = RIGHT;
                }
            }
        }

        Uint32 now = SDL_GetTicks();
        if (now - lastTick > (Uint32)delay) {
            lastTick = now;
            if (!gameOver) {
                if (dir != STOP && !isPaused) { // Chỉ di chuyển rắn khi không tạm dừng
                    MoveSnake();
                }
                DrawGame(); // Vẽ khung hình game (bao gồm thông báo tạm dừng nếu có)
            }
            else {
                ShowGameOver();
            }
        }
    }

    Mix_FreeChunk(eatSound);
    Mix_FreeChunk(hitSound);
    Mix_FreeMusic(menuMusic);
    Mix_FreeMusic(playingMusic);
    Mix_FreeChunk(clickSound);
    Mix_CloseAudio();

    TTF_CloseFont(font);
    TTF_CloseFont(scoreFont);
    TTF_CloseFont(menuFont);
    TTF_CloseFont(mainMenuFont);

    if (bgMenu) SDL_DestroyTexture(bgMenu);
    if (bgGame) SDL_DestroyTexture(bgGame);
    if (bgGameOver) SDL_DestroyTexture(bgGameOver);

    SDL_DestroyTexture(snakeHeadTexture);
    SDL_DestroyTexture(snakeBodyTexture);
    SDL_DestroyTexture(snakeTailTexture);
    SDL_DestroyTexture(snakeTurnTLTexture);
    SDL_DestroyTexture(snakeTurnTRTexture);
    SDL_DestroyTexture(snakeTurnBLTexture);
    SDL_DestroyTexture(snakeTurnBRTexture);
    SDL_DestroyTexture(foodTexture);

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    TTF_Quit();
    IMG_Quit();
    SDL_Quit();

    return 0;
}