#include <SFML/Audio.hpp>
#include <SFML/Graphics.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <fstream>
#include <optional>
#include <random>
#include <string>
#include <vector>

namespace {
constexpr int kBoardWidth = 10;
constexpr int kBoardHeight = 20;
constexpr int kCellSize = 30;
constexpr int kSidePanelWidth = 250;
constexpr int kWindowWidth = kBoardWidth * kCellSize + kSidePanelWidth + 110;
constexpr int kWindowHeight = kBoardHeight * kCellSize + 80;
constexpr int kBoardOffsetX = 34;
constexpr int kBoardOffsetY = 40;
constexpr int kPreviewCell = 20;
constexpr int kSidePanelX = kBoardOffsetX + kBoardWidth * kCellSize + 32;
constexpr int kSideContentX = kSidePanelX + 22;
constexpr int kPreviewOffsetX = kSideContentX + 2;
constexpr int kPreviewOffsetY = 322;
constexpr int kAudioSampleRate = 44100;
constexpr float kPi = 3.1415926535f;

using Board = std::array<std::array<int, kBoardWidth>, kBoardHeight>;

struct Piece {
    std::array<sf::Vector2i, 4> blocks{};
    sf::Color color{sf::Color::Cyan};
};

struct Theme {
    sf::Color backgroundTop{15, 16, 35};
    sf::Color backgroundBottom{6, 7, 18};
    sf::Color panel{18, 23, 46, 220};
    sf::Color panelBorder{90, 170, 255, 180};
    sf::Color gridLine{80, 110, 170, 80};
    sf::Color text{240, 245, 255};
    sf::Color accent{80, 210, 255};
};

sf::Vector2i rotateCW(const sf::Vector2i& p) {
    return {p.y, -p.x};
}

std::vector<sf::Vector2i> baseShape(int type) {
    switch (type) {
    case 0: return {{ {-2, 0}, {-1, 0}, {0, 0}, {1, 0} }};
    case 1: return {{ {0, 0}, {1, 0}, {0, 1}, {1, 1} }};
    case 2: return {{ {-1, 0}, {0, 0}, {1, 0}, {0, 1} }};
    case 3: return {{ {-1, 0}, {0, 0}, {0, 1}, {1, 1} }};
    case 4: return {{ {-1, 1}, {0, 1}, {0, 0}, {1, 0} }};
    case 5: return {{ {-1, 0}, {0, 0}, {1, 0}, {-1, 1} }};
    default: return {{ {-1, 0}, {0, 0}, {1, 0}, {1, 1} }};
    }
}

sf::Color pieceColor(int type) {
    static const std::array<sf::Color, 7> colors = {
        sf::Color(0, 255, 255),
        sf::Color(255, 240, 0),
        sf::Color(180, 90, 255),
        sf::Color(0, 255, 120),
        sf::Color(255, 80, 120),
        sf::Color(255, 160, 50),
        sf::Color(90, 140, 255)
    };
    return colors[type % colors.size()];
}

Piece makePiece(int type) {
    Piece piece;
    piece.color = pieceColor(type);
    auto blocks = baseShape(type);
    for (std::size_t i = 0; i < 4; ++i) {
        piece.blocks[i] = blocks[i];
    }
    return piece;
}

bool isInside(int x, int y) {
    return x >= 0 && x < kBoardWidth && y < kBoardHeight;
}

bool canPlace(const Board& board, const Piece& piece, const sf::Vector2i& pos) {
    for (const auto& block : piece.blocks) {
        const int x = pos.x + block.x;
        const int y = pos.y + block.y;
        if (x < 0 || x >= kBoardWidth || y >= kBoardHeight) {
            return false;
        }
        if (y >= 0 && board[y][x] != 0) {
            return false;
        }
    }
    return true;
}

void lockPiece(Board& board, const Piece& piece, const sf::Vector2i& pos) {
    for (const auto& block : piece.blocks) {
        const int x = pos.x + block.x;
        const int y = pos.y + block.y;
        if (y >= 0 && y < kBoardHeight && x >= 0 && x < kBoardWidth) {
            board[y][x] = piece.color.toInteger();
        }
    }
}

int clearLines(Board& board) {
    int cleared = 0;
    for (int y = kBoardHeight - 1; y >= 0; --y) {
        const bool full = std::all_of(board[y].begin(), board[y].end(), [](int v) { return v != 0; });
        if (full) {
            ++cleared;
            for (int row = y; row > 0; --row) {
                board[row] = board[row - 1];
            }
            board[0].fill(0);
            ++y;
        }
    }
    return cleared;
}

Piece randomPiece(std::mt19937& rng) {
    std::uniform_int_distribution<int> dist(0, 6);
    return makePiece(dist(rng));
}

std::filesystem::path assetPath(const std::string& relative) {
    return std::filesystem::path("assets") / relative;
}

sf::Texture makeBackgroundTexture() {
    sf::RenderTexture rt;
    rt.create(1280, 720);
    rt.clear(sf::Color::Black);

    sf::VertexArray gradient(sf::TriangleStrip, 4);
    gradient[0].position = {0.f, 0.f};
    gradient[1].position = {1280.f, 0.f};
    gradient[2].position = {0.f, 720.f};
    gradient[3].position = {1280.f, 720.f};
    gradient[0].color = sf::Color(18, 20, 45);
    gradient[1].color = sf::Color(25, 12, 55);
    gradient[2].color = sf::Color(6, 8, 18);
    gradient[3].color = sf::Color(10, 14, 28);
    rt.draw(gradient);

    sf::CircleShape orb1(120.f);
    orb1.setPosition(80.f, 70.f);
    orb1.setFillColor(sf::Color(60, 150, 255, 35));
    rt.draw(orb1);

    sf::CircleShape orb2(90.f);
    orb2.setPosition(1030.f, 90.f);
    orb2.setFillColor(sf::Color(255, 90, 190, 28));
    rt.draw(orb2);

    sf::CircleShape orb3(180.f);
    orb3.setPosition(880.f, 460.f);
    orb3.setFillColor(sf::Color(0, 220, 180, 20));
    rt.draw(orb3);

    rt.display();
    return rt.getTexture();
}

std::vector<sf::Int16> generateMusic() {
    const std::vector<float> melody = {
        659.25f, 783.99f, 987.77f, 783.99f,
        659.25f, 587.33f, 523.25f, 587.33f,
        659.25f, 659.25f, 783.99f, 987.77f,
        1174.66f, 987.77f, 783.99f, 659.25f
    };

    const float secondsPerNote = 0.35f;
    const float bpmPulse = 0.6f;
    const int samplesPerNote = static_cast<int>(kAudioSampleRate * secondsPerNote);
    std::vector<sf::Int16> samples;
    samples.reserve(samplesPerNote * melody.size() * 2);

    for (std::size_t i = 0; i < melody.size(); ++i) {
        const float freq = melody[i];
        for (int s = 0; s < samplesPerNote; ++s) {
            const float t = static_cast<float>(s) / static_cast<float>(kAudioSampleRate);
            const float env = std::sin(std::min(1.f, t / 0.03f) * kPi * 0.5f);
            const float decay = std::exp(-t * 1.8f);
            const float wave = std::sin(2.f * kPi * freq * t) + 0.35f * std::sin(2.f * kPi * freq * 2.f * t);
            const float pulse = 0.75f + 0.25f * std::sin(2.f * kPi * bpmPulse * (static_cast<float>(i) * secondsPerNote + t));
            const float sample = wave * env * decay * pulse * 0.28f;
            samples.push_back(static_cast<sf::Int16>(std::clamp(sample, -1.f, 1.f) * 32767));
            samples.push_back(static_cast<sf::Int16>(std::clamp(sample, -1.f, 1.f) * 32767));
        }
    }
    return samples;
}

std::optional<sf::Font> loadFont() {
    sf::Font font;
    const auto bundledFont = assetPath("fonts/title.ttf");
    if (std::filesystem::exists(bundledFont) && font.loadFromFile(bundledFont.string())) {
        return font;
    }

    const std::array<std::string, 4> systemFonts = {
        "C:/Windows/Fonts/segoeui.ttf",
        "C:/Windows/Fonts/arial.ttf",
        "C:/Windows/Fonts/calibri.ttf",
        "C:/Windows/Fonts/tahoma.ttf"
    };
    for (const auto& path : systemFonts) {
        if (std::filesystem::exists(path) && font.loadFromFile(path)) {
            return font;
        }
    }
    return std::nullopt;
}

} // namespace

int main() {
    sf::RenderWindow window(sf::VideoMode(kWindowWidth, kWindowHeight), "Neon Tetris", sf::Style::Close);
    window.setVerticalSyncEnabled(true);

    Theme theme;
    sf::Texture backgroundTexture = makeBackgroundTexture();
    sf::Sprite background(backgroundTexture);
    background.setScale(
        static_cast<float>(kWindowWidth) / backgroundTexture.getSize().x,
        static_cast<float>(kWindowHeight) / backgroundTexture.getSize().y);

    auto fontOpt = loadFont();
    sf::Font fallbackFont;
    sf::Font* font = nullptr;
    if (fontOpt) {
        font = &*fontOpt;
    } else if (fallbackFont.loadFromFile("C:/Windows/Fonts/arial.ttf")) {
        font = &fallbackFont;
    }

    sf::Music backgroundMusic;
    sf::SoundBuffer generatedMusicBuffer;
    sf::Sound generatedMusic;
    const auto customMusicPath = assetPath("bg.mp3");
    if (std::filesystem::exists(customMusicPath) && backgroundMusic.openFromFile(customMusicPath.string())) {
        backgroundMusic.setLoop(true);
        backgroundMusic.setVolume(35.f);
        backgroundMusic.play();
    } else {
        const auto musicSamples = generateMusic();
        generatedMusicBuffer.loadFromSamples(musicSamples.data(), musicSamples.size(), 2, kAudioSampleRate);
        generatedMusic.setBuffer(generatedMusicBuffer);
        generatedMusic.setLoop(true);
        generatedMusic.setVolume(28.f);
        generatedMusic.play();
    }

    std::mt19937 rng{std::random_device{}()};
    Board board{};
    for (auto& row : board) row.fill(0);

    std::deque<Piece> queue;
    for (int i = 0; i < 4; ++i) queue.push_back(randomPiece(rng));
    Piece current = queue.front();
    queue.pop_front();
    queue.push_back(randomPiece(rng));
    Piece holdPiece = makePiece(1);
    bool hasHold = false;
    bool holdUsed = false;
    sf::Vector2i currentPos{kBoardWidth / 2, 1};

    int score = 0;
    int lines = 0;
    int level = 1;
    int highScore = 0;
    bool gameOver = false;
    bool paused = false;
    float fallTimer = 0.f;

    std::ifstream in("highscore.txt");
    if (in) in >> highScore;

    auto saveHighScore = [&]() {
        std::ofstream out("highscore.txt", std::ios::trunc);
        if (out) out << highScore;
    };

    auto spawnNext = [&]() {
        current = queue.front();
        queue.pop_front();
        queue.push_back(randomPiece(rng));
        currentPos = {kBoardWidth / 2, 1};
        holdUsed = false;
        if (!canPlace(board, current, currentPos)) {
            gameOver = true;
        }
    };

    auto tryMove = [&](int dx, int dy) {
        sf::Vector2i next = currentPos + sf::Vector2i(dx, dy);
        if (canPlace(board, current, next)) {
            currentPos = next;
            return true;
        }
        return false;
    };

    auto rotatePiece = [&]() {
        Piece rotated = current;
        for (auto& b : rotated.blocks) b = rotateCW(b);
        const std::array<sf::Vector2i, 5> kicks = {{{0,0},{-1,0},{1,0},{-2,0},{2,0}}};
        for (const auto& kick : kicks) {
            if (canPlace(board, rotated, currentPos + kick)) {
                current = rotated;
                currentPos += kick;
                return;
            }
        }
    };

    auto hardDrop = [&]() {
        while (tryMove(0, 1)) {}
        lockPiece(board, current, currentPos);
        const int cleared = clearLines(board);
        if (cleared > 0) {
            static const int scores[] = {0, 100, 300, 500, 800};
            score += scores[cleared] * level;
            lines += cleared;
            level = 1 + lines / 10;
        }
        if (score > highScore) {
            highScore = score;
            saveHighScore();
        }
        spawnNext();
    };

    sf::Clock clock;
    while (window.isOpen()) {
        const float dt = clock.restart().asSeconds();

        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) {
                window.close();
            } else if (event.type == sf::Event::KeyPressed) {
                if (event.key.code == sf::Keyboard::Escape) {
                    window.close();
                } else if (event.key.code == sf::Keyboard::P) {
                    paused = !paused;
                } else if (!gameOver && !paused) {
                    if (event.key.code == sf::Keyboard::Left) tryMove(-1, 0);
                    if (event.key.code == sf::Keyboard::Right) tryMove(1, 0);
                    if (event.key.code == sf::Keyboard::Down) {
                        if (!tryMove(0, 1)) hardDrop();
                    }
                    if (event.key.code == sf::Keyboard::Up || event.key.code == sf::Keyboard::X) rotatePiece();
                    if (event.key.code == sf::Keyboard::Space) hardDrop();
                    if (event.key.code == sf::Keyboard::C && !holdUsed) {
                        if (!hasHold) {
                            holdPiece = current;
                            hasHold = true;
                            spawnNext();
                        } else {
                            std::swap(current, holdPiece);
                            currentPos = {kBoardWidth / 2, 1};
                            if (!canPlace(board, current, currentPos)) gameOver = true;
                        }
                        holdUsed = true;
                    }
                }
                if (gameOver && event.key.code == sf::Keyboard::R) {
                    board = Board{};
                    for (auto& row : board) row.fill(0);
                    score = 0;
                    lines = 0;
                    level = 1;
                    gameOver = false;
                    paused = false;
                    hasHold = false;
                    holdUsed = false;
                    queue.clear();
                    for (int i = 0; i < 4; ++i) queue.push_back(randomPiece(rng));
                    spawnNext();
                }
            }
        }

        if (!gameOver && !paused) {
            fallTimer += dt;
            const float fallDelay = std::max(0.1f, 0.7f - (level - 1) * 0.05f);
            if (fallTimer >= fallDelay) {
                fallTimer = 0.f;
                if (!tryMove(0, 1)) {
                    lockPiece(board, current, currentPos);
                    const int cleared = clearLines(board);
                    if (cleared > 0) {
                        static const int scores[] = {0, 100, 300, 500, 800};
                        score += scores[cleared] * level;
                        lines += cleared;
                        level = 1 + lines / 10;
                    }
                    if (score > highScore) {
                        highScore = score;
                        saveHighScore();
                    }
                    spawnNext();
                }
            }
        }

        window.clear();
        window.draw(background);

        sf::RectangleShape boardPanel({static_cast<float>(kBoardWidth * kCellSize + 20), static_cast<float>(kBoardHeight * kCellSize + 20)});
        boardPanel.setPosition(kBoardOffsetX - 10.f, kBoardOffsetY - 10.f);
        boardPanel.setFillColor(theme.panel);
        boardPanel.setOutlineThickness(2.f);
        boardPanel.setOutlineColor(theme.panelBorder);
        window.draw(boardPanel);

        for (int y = 0; y < kBoardHeight; ++y) {
            for (int x = 0; x < kBoardWidth; ++x) {
                sf::RectangleShape cell({static_cast<float>(kCellSize - 2), static_cast<float>(kCellSize - 2)});
                cell.setPosition(kBoardOffsetX + x * kCellSize + 1.f, kBoardOffsetY + y * kCellSize + 1.f);
                if (board[y][x] != 0) {
                    cell.setFillColor(sf::Color(board[y][x]));
                } else {
                    cell.setFillColor(sf::Color(20, 28, 54, 120));
                }
                window.draw(cell);
            }
        }

        for (int x = 0; x <= kBoardWidth; ++x) {
            sf::Vertex line[] = {
                sf::Vertex(sf::Vector2f(kBoardOffsetX + x * kCellSize, kBoardOffsetY), theme.gridLine),
                sf::Vertex(sf::Vector2f(kBoardOffsetX + x * kCellSize, kBoardOffsetY + kBoardHeight * kCellSize), theme.gridLine)
            };
            window.draw(line, 2, sf::Lines);
        }
        for (int y = 0; y <= kBoardHeight; ++y) {
            sf::Vertex line[] = {
                sf::Vertex(sf::Vector2f(kBoardOffsetX, kBoardOffsetY + y * kCellSize), theme.gridLine),
                sf::Vertex(sf::Vector2f(kBoardOffsetX + kBoardWidth * kCellSize, kBoardOffsetY + y * kCellSize), theme.gridLine)
            };
            window.draw(line, 2, sf::Lines);
        }

        for (const auto& block : current.blocks) {
            const int x = currentPos.x + block.x;
            const int y = currentPos.y + block.y;
            if (isInside(x, y) && y >= 0) {
                sf::RectangleShape cell({static_cast<float>(kCellSize - 2), static_cast<float>(kCellSize - 2)});
                cell.setPosition(kBoardOffsetX + x * kCellSize + 1.f, kBoardOffsetY + y * kCellSize + 1.f);
                cell.setFillColor(current.color);
                window.draw(cell);
            }
        }

        sf::RectangleShape sidePanel({static_cast<float>(kSidePanelWidth), static_cast<float>(kWindowHeight - 80)});
        sidePanel.setPosition(static_cast<float>(kSidePanelX), 40.f);
        sidePanel.setFillColor(theme.panel);
        sidePanel.setOutlineThickness(2.f);
        sidePanel.setOutlineColor(theme.panelBorder);
        window.draw(sidePanel);

        sf::RectangleShape titleGlow({190.f, 3.f});
        titleGlow.setPosition(static_cast<float>(kSideContentX), 104.f);
        titleGlow.setFillColor(sf::Color(80, 210, 255, 170));
        window.draw(titleGlow);

        sf::RectangleShape statsCard({206.f, 122.f});
        statsCard.setPosition(static_cast<float>(kSideContentX), 128.f);
        statsCard.setFillColor(sf::Color(8, 13, 28, 150));
        statsCard.setOutlineThickness(1.2f);
        statsCard.setOutlineColor(sf::Color(100, 160, 255, 90));
        window.draw(statsCard);

        sf::RectangleShape controlsCard({206.f, 58.f});
        controlsCard.setPosition(static_cast<float>(kSideContentX), 552.f);
        controlsCard.setFillColor(sf::Color(8, 13, 28, 135));
        controlsCard.setOutlineThickness(1.2f);
        controlsCard.setOutlineColor(sf::Color(100, 160, 255, 75));
        window.draw(controlsCard);

        if (font) {
            auto drawText = [&](const std::string& str, float size, float x, float y, sf::Color color) {
                sf::Text text(str, *font, static_cast<unsigned>(size));
                text.setPosition(x, y);
                text.setFillColor(color);
                window.draw(text);
            };

            const float sx = static_cast<float>(kSideContentX);
            drawText("TETRIS", 30.f, sx + 44.f, 68.f, theme.accent);
            drawText("SCORE", 14.f, sx + 14.f, 144.f, sf::Color(155, 178, 220));
            drawText(std::to_string(score), 26.f, sx + 14.f, 162.f, sf::Color::White);
            drawText("LEVEL", 14.f, sx + 124.f, 144.f, sf::Color(155, 178, 220));
            drawText(std::to_string(level), 26.f, sx + 124.f, 162.f, sf::Color::White);
            drawText("BEST", 14.f, sx + 14.f, 205.f, sf::Color(155, 178, 220));
            drawText(std::to_string(highScore), 24.f, sx + 14.f, 222.f, sf::Color::White);
            drawText("NEXT BLOCK", 16.f, sx, 286.f, theme.text);
            drawText("HOLD BLOCK", 16.f, sx, 426.f, theme.text);
            drawText("Move: Arrow     Rotate: Up/X", 13.f, sx + 12.f, 564.f, sf::Color(210, 220, 255));
            drawText("Drop: Space     Hold: C", 13.f, sx + 12.f, 582.f, sf::Color(210, 220, 255));

            auto drawPreview = [&](const Piece& piece, float ox, float oy) {
                for (const auto& block : piece.blocks) {
                    sf::RectangleShape shadow({static_cast<float>(kPreviewCell), static_cast<float>(kPreviewCell)});
                    shadow.setPosition(ox + (block.x + 2) * kPreviewCell + 2.f, oy + (block.y + 2) * kPreviewCell + 2.f);
                    shadow.setFillColor(sf::Color(piece.color.r, piece.color.g, piece.color.b, 55));
                    window.draw(shadow);

                    sf::RectangleShape cell({static_cast<float>(kPreviewCell - 2), static_cast<float>(kPreviewCell - 2)});
                    cell.setPosition(ox + (block.x + 2) * kPreviewCell, oy + (block.y + 2) * kPreviewCell);
                    cell.setFillColor(piece.color);
                    window.draw(cell);
                }
            };

            sf::RectangleShape previewBox({206.f, 96.f});
            previewBox.setPosition(sx, kPreviewOffsetY - 16.f);
            previewBox.setFillColor(sf::Color(10, 15, 30, 180));
            previewBox.setOutlineThickness(1.5f);
            previewBox.setOutlineColor(sf::Color(100, 160, 255, 120));
            window.draw(previewBox);
            drawPreview(queue.front(), sx + 54.f, static_cast<float>(kPreviewOffsetY));

            sf::RectangleShape holdBox({206.f, 96.f});
            holdBox.setPosition(sx, kPreviewOffsetY + 124.f);
            holdBox.setFillColor(sf::Color(10, 15, 30, 180));
            holdBox.setOutlineThickness(1.5f);
            holdBox.setOutlineColor(sf::Color(100, 160, 255, 120));
            window.draw(holdBox);
            if (hasHold) drawPreview(holdPiece, sx + 54.f, static_cast<float>(kPreviewOffsetY + 140));

            if (paused) {
                sf::RectangleShape overlay({static_cast<float>(kBoardWidth * kCellSize), 78.f});
                overlay.setPosition(static_cast<float>(kBoardOffsetX), 286.f);
                overlay.setFillColor(sf::Color(4, 6, 14, 170));
                window.draw(overlay);
                drawText("PAUSE", 34.f, 132.f, 302.f, sf::Color::Yellow);
            }
            if (gameOver) {
                sf::RectangleShape overlay({static_cast<float>(kBoardWidth * kCellSize), 104.f});
                overlay.setPosition(static_cast<float>(kBoardOffsetX), 274.f);
                overlay.setFillColor(sf::Color(4, 6, 14, 185));
                window.draw(overlay);
                drawText("GAME OVER", 34.f, 104.f, 292.f, sf::Color(255, 100, 120));
                drawText("Press R Restart", 20.f, 112.f, 336.f, sf::Color::White);
            }
        }

        window.display();
    }

    if (score > highScore) {
        highScore = score;
        saveHighScore();
    }
    return 0;
}
