// Dinger Derby — Demo Launcher
// Polished menu to browse and launch engine demos.

#include <SFML/Graphics.hpp>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

namespace {

constexpr float pi = 3.1415926535f;
constexpr unsigned winW = 1180;
constexpr unsigned winH = 760;

// Palette — night diamond / ballpark neon
const sf::Color colBgTop(6, 12, 22);
const sf::Color colBgBot(10, 22, 18);
const sf::Color colPanel(14, 22, 34, 230);
const sf::Color colPanelEdge(40, 70, 90);
const sf::Color colAccent(80, 220, 160);
const sf::Color colAccentDim(40, 140, 110);
const sf::Color colGold(255, 210, 90);
const sf::Color colText(236, 244, 250);
const sf::Color colMuted(130, 155, 170);
const sf::Color colRow(18, 28, 42);
const sf::Color colRowHover(28, 48, 62);
const sf::Color colRowSel(24, 72, 78);
const sf::Color colRowSelEdge(100, 240, 200);

struct DemoEntry {
    std::string name;
    std::string executable;
    std::string description;
    std::string category; // Featured | Game | Physics | 3D | Tools
    bool featured = false;
};

std::vector<DemoEntry> makeDemoEntries() {
    return {
        // Featured first
        {"Stadium", "stadium_demo",
         "HR Derby ballpark — field, wall, stands, orbit cameras", "Featured", true},
        {"HR Derby / Bat", "bat_physics_demo",
         "Soft toss, fixed swings, HR count + longest", "Featured", true},
        {"Pitching Simulator", "pitching_simulator_demo",
         "Aim box, spin, Magnus break, catcher/pitcher cams", "Featured", true},
        {"Character Viewer", "character_viewer_demo",
         "CharacterModel3D workshop — orbit, clips, bones", "Featured", true},
        {"Baseball 3D", "baseball_3d_demo",
         "High-poly baseball mesh showcase", "Featured", true},

        {"Physics 3D", "physics3d_demo",
         "Rigid spheres, collisions, world bounds", "Physics", false},
        {"Soft Cube 3D", "soft_cube_3d_demo",
         "Springy deformable mesh body", "Physics", false},
        {"Bat Test", "bat_test",
         "2D bat/body contact sandbox", "Physics", false},
        {"Spin Test", "spin_test",
         "Angular motion and impulses", "Physics", false},
        {"User Ball", "user_ball_test",
         "2D launch-and-collision sandbox", "Physics", false},
        {"Random Spawn", "random_spawn_test",
         "2D physics stress with random balls", "Physics", false},

        {"Rotation 3D", "rotation_3d_demo",
         "Yaw, pitch, roll, and orbit camera", "3D", false},
        {"Raster Cube 3D", "raster_cube_3d_demo",
         "Software-rasterized cube", "3D", false},
        {"Raster Depth 3D", "raster_depth_3d_demo",
         "Depth-buffered raster cubes", "3D", false},
        {"Filled Cube 3D", "filled_cube_3d_demo",
         "Painter-style filled cube", "3D", false},
        {"Wire Cube 3D", "wire_cube_3d_demo",
         "Wireframe cube transform demo", "3D", false},
        {"Depth Sort 3D", "depth_sort_3d_demo",
         "Triangle ordering demo", "3D", false},
        {"Point Cloud 3D", "point_cloud_3d_demo",
         "First 3D projection points", "3D", false},
        {"Transform Stack 3D", "transform_stack_3d_demo",
         "Nested transform hierarchy", "3D", false},
        {"Culling Stress 3D", "culling_stress_3d_demo",
         "Grid, culling, triangle counters", "3D", false},
    };
}

bool loadMenuFont(sf::Font& font) {
    for (const char* path : {
             "/System/Library/Fonts/Supplemental/Arial.ttf",
             "/System/Library/Fonts/Supplemental/Helvetica.ttf",
             "/System/Library/Fonts/Helvetica.ttc",
             "/System/Library/Fonts/SFNS.ttf"}) {
        if (font.openFromFile(path)) {
            return true;
        }
    }
    return false;
}

std::string shellQuote(const std::filesystem::path& path) {
    std::string value = path.string();
    std::string quoted = "'";
    for (char character : value) {
        if (character == '\'') {
            quoted += "'\\''";
        } else {
            quoted += character;
        }
    }
    quoted += "'";
    return quoted;
}

std::filesystem::path executableDirectory(const char* executablePath) {
    std::filesystem::path path(executablePath);
    if (path.is_relative()) {
        path = std::filesystem::current_path() / path;
    }
    return std::filesystem::weakly_canonical(path).parent_path();
}

int numberKeyIndex(sf::Keyboard::Key key) {
    switch (key) {
        case sf::Keyboard::Key::Num1:
        case sf::Keyboard::Key::Numpad1:
            return 0;
        case sf::Keyboard::Key::Num2:
        case sf::Keyboard::Key::Numpad2:
            return 1;
        case sf::Keyboard::Key::Num3:
        case sf::Keyboard::Key::Numpad3:
            return 2;
        case sf::Keyboard::Key::Num4:
        case sf::Keyboard::Key::Numpad4:
            return 3;
        case sf::Keyboard::Key::Num5:
        case sf::Keyboard::Key::Numpad5:
            return 4;
        case sf::Keyboard::Key::Num6:
        case sf::Keyboard::Key::Numpad6:
            return 5;
        case sf::Keyboard::Key::Num7:
        case sf::Keyboard::Key::Numpad7:
            return 6;
        case sf::Keyboard::Key::Num8:
        case sf::Keyboard::Key::Numpad8:
            return 7;
        case sf::Keyboard::Key::Num9:
        case sf::Keyboard::Key::Numpad9:
            return 8;
        default:
            return -1;
    }
}

void drawText(
    sf::RenderWindow& window,
    const sf::Font& font,
    const std::string& value,
    unsigned int size,
    sf::Vector2f position,
    sf::Color color,
    bool boldish = false
) {
    sf::Text text(font, value, size);
    text.setPosition(position);
    text.setFillColor(color);
    if (boldish) {
        text.setStyle(sf::Text::Bold);
    }
    window.draw(text);
}

void drawRoundRect(
    sf::RenderWindow& window,
    sf::FloatRect rect,
    sf::Color fill,
    sf::Color outline,
    float outlineThickness = 1.0f
) {
    sf::RectangleShape shape(rect.size);
    shape.setPosition(rect.position);
    shape.setFillColor(fill);
    shape.setOutlineThickness(outlineThickness);
    shape.setOutlineColor(outline);
    window.draw(shape);
}

// Vertical gradient via stacked strips
void drawVerticalGradient(
    sf::RenderWindow& window,
    float x,
    float y,
    float w,
    float h,
    sf::Color top,
    sf::Color bot,
    int strips = 48
) {
    for (int i = 0; i < strips; i++) {
        float t0 = static_cast<float>(i) / strips;
        float t1 = static_cast<float>(i + 1) / strips;
        auto lerpC = [&](std::uint8_t a, std::uint8_t b, float t) {
            return static_cast<std::uint8_t>(a + (b - a) * t);
        };
        sf::Color c(
            lerpC(top.r, bot.r, t0),
            lerpC(top.g, bot.g, t0),
            lerpC(top.b, bot.b, t0),
            255
        );
        sf::RectangleShape strip(sf::Vector2f(w, h * (t1 - t0) + 1.0f));
        strip.setPosition({x, y + h * t0});
        strip.setFillColor(c);
        window.draw(strip);
    }
}

void drawSoftCircle(sf::RenderWindow& window, sf::Vector2f center, float r, sf::Color c) {
    sf::CircleShape disc(r);
    disc.setOrigin({r, r});
    disc.setPosition(center);
    disc.setFillColor(c);
    window.draw(disc);
}

void drawDiamondMark(sf::RenderWindow& window, sf::Vector2f c, float s, sf::Color col) {
    sf::ConvexShape d;
    d.setPointCount(4);
    d.setPoint(0, {c.x, c.y - s});
    d.setPoint(1, {c.x + s * 0.85f, c.y});
    d.setPoint(2, {c.x, c.y + s});
    d.setPoint(3, {c.x - s * 0.85f, c.y});
    d.setFillColor(col);
    window.draw(d);
}

struct Layout {
    float sidebarW = 280.0f;
    float listX = 304.0f;
    float listW = 540.0f;
    float detailX = 860.0f;
    float detailW = 292.0f;
    float headerH = 108.0f;
    float rowH = 56.0f;
    float listTop = 128.0f;
    float listBottomPad = 28.0f;
};

std::vector<std::string> uniqueCategories(const std::vector<DemoEntry>& demos) {
    std::vector<std::string> cats;
    for (const auto& d : demos) {
        if (std::find(cats.begin(), cats.end(), d.category) == cats.end()) {
            cats.push_back(d.category);
        }
    }
    return cats;
}

std::vector<int> indicesForFilter(
    const std::vector<DemoEntry>& demos,
    const std::string& filter // "All" or category
) {
    std::vector<int> idx;
    for (int i = 0; i < static_cast<int>(demos.size()); i++) {
        if (filter == "All" || demos[i].category == filter) {
            idx.push_back(i);
        }
    }
    return idx;
}

void launchDemo(
    sf::RenderWindow& window,
    const sf::Font& font,
    bool fontLoaded,
    const std::filesystem::path& buildDirectory,
    const DemoEntry& demo
) {
    std::filesystem::path executable = buildDirectory / demo.executable;
    window.setTitle("Dinger Derby | launching " + demo.name + "...");

    drawVerticalGradient(window, 0, 0, static_cast<float>(winW), static_cast<float>(winH), colBgTop, colBgBot);
    drawSoftCircle(window, {winW * 0.5f, winH * 0.45f}, 220.0f, sf::Color(40, 120, 90, 40));
    drawDiamondMark(window, {winW * 0.5f, winH * 0.38f}, 36.0f, sf::Color(80, 220, 160, 90));

    if (fontLoaded) {
        drawText(
            window,
            font,
            "Launching  " + demo.name,
            32,
            {winW * 0.5f - 180.0f, winH * 0.48f},
            colText,
            true
        );
        drawText(
            window,
            font,
            "Close the demo window to return here",
            16,
            {winW * 0.5f - 150.0f, winH * 0.48f + 48.0f},
            colMuted
        );
    }
    window.display();

    std::string command = shellQuote(executable);
    int result = std::system(command.c_str());
    (void)result;
    window.setTitle("Dinger Derby | Demo Launcher");
}

} // namespace

int main(int argc, char** argv) {
    sf::ContextSettings settings;
    settings.antiAliasingLevel = 8;

    // Titles must be plain ASCII — SFML/Cocoa can fail setTitle with nil
    // if conversion from non-ASCII string fails.
    sf::RenderWindow window(
        sf::VideoMode(sf::Vector2u(winW, winH)),
        "Dinger Derby | Demo Launcher",
        sf::Style::Default,
        sf::State::Windowed,
        settings
    );
    window.setFramerateLimit(60);
    window.setVerticalSyncEnabled(true);

    std::vector<DemoEntry> demos = makeDemoEntries();
    std::vector<std::string> categories = uniqueCategories(demos);
    categories.insert(categories.begin(), "All");

    int filterIndex = 0; // "All"
    int selectedLocal = 0; // index into filtered list
    float scrollY = 0.0f;
    float animTime = 0.0f;

    std::filesystem::path buildDirectory =
        executableDirectory(argc > 0 ? argv[0] : "./demo_launcher");

    sf::Font font;
    bool fontLoaded = loadMenuFont(font);
    Layout L;

    auto filtered = [&]() {
        return indicesForFilter(demos, categories[filterIndex]);
    };

    auto clampSelection = [&]() {
        auto idx = filtered();
        if (idx.empty()) {
            selectedLocal = 0;
            return;
        }
        selectedLocal = std::clamp(selectedLocal, 0, static_cast<int>(idx.size()) - 1);
    };

    auto selectedDemoIndex = [&]() -> int {
        auto idx = filtered();
        if (idx.empty()) {
            return 0;
        }
        return idx[std::clamp(selectedLocal, 0, static_cast<int>(idx.size()) - 1)];
    };

    auto ensureVisible = [&]() {
        auto idx = filtered();
        float viewH = static_cast<float>(winH) - L.listTop - L.listBottomPad - 8.0f;
        float selTop = selectedLocal * L.rowH;
        float selBot = selTop + L.rowH;
        if (selTop < scrollY) {
            scrollY = selTop;
        }
        if (selBot > scrollY + viewH) {
            scrollY = selBot - viewH;
        }
        float contentH = static_cast<float>(idx.size()) * L.rowH;
        float maxScroll = std::max(0.0f, contentH - viewH);
        scrollY = std::clamp(scrollY, 0.0f, maxScroll);
    };

    sf::Clock clock;
    while (window.isOpen()) {
        float dt = std::min(clock.restart().asSeconds(), 0.05f);
        animTime += dt;

        auto idxList = filtered();
        clampSelection();

        while (const std::optional event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                window.close();
            }

            if (const auto* key = event->getIf<sf::Event::KeyPressed>()) {
                if (key->code == sf::Keyboard::Key::Escape) {
                    window.close();
                }
                if (key->code == sf::Keyboard::Key::Down) {
                    if (!idxList.empty()) {
                        selectedLocal = (selectedLocal + 1) % static_cast<int>(idxList.size());
                        ensureVisible();
                    }
                }
                if (key->code == sf::Keyboard::Key::Up) {
                    if (!idxList.empty()) {
                        selectedLocal =
                            (selectedLocal + static_cast<int>(idxList.size()) - 1) %
                            static_cast<int>(idxList.size());
                        ensureVisible();
                    }
                }
                if (key->code == sf::Keyboard::Key::Tab || key->code == sf::Keyboard::Key::Right) {
                    filterIndex = (filterIndex + 1) % static_cast<int>(categories.size());
                    selectedLocal = 0;
                    scrollY = 0.0f;
                }
                if (key->code == sf::Keyboard::Key::Left) {
                    filterIndex =
                        (filterIndex + static_cast<int>(categories.size()) - 1) %
                        static_cast<int>(categories.size());
                    selectedLocal = 0;
                    scrollY = 0.0f;
                }
                int hotkeyIndex = numberKeyIndex(key->code);
                // Hotkeys 1-9 map into the filtered list for featured-style quick launch
                if (hotkeyIndex >= 0) {
                    auto fi = filtered();
                    if (hotkeyIndex < static_cast<int>(fi.size())) {
                        selectedLocal = hotkeyIndex;
                        launchDemo(
                            window, font, fontLoaded, buildDirectory, demos[fi[hotkeyIndex]]
                        );
                    }
                }
                if (key->code == sf::Keyboard::Key::Enter ||
                    key->code == sf::Keyboard::Key::Space) {
                    if (!idxList.empty()) {
                        launchDemo(
                            window,
                            font,
                            fontLoaded,
                            buildDirectory,
                            demos[selectedDemoIndex()]
                        );
                    }
                }
            }

            if (const auto* wh = event->getIf<sf::Event::MouseWheelScrolled>()) {
                scrollY -= wh->delta * L.rowH * 0.85f;
                auto fi = filtered();
                float viewH = static_cast<float>(winH) - L.listTop - L.listBottomPad - 8.0f;
                float contentH = static_cast<float>(fi.size()) * L.rowH;
                float maxScroll = std::max(0.0f, contentH - viewH);
                scrollY = std::clamp(scrollY, 0.0f, maxScroll);
            }

            if (const auto* m = event->getIf<sf::Event::MouseButtonPressed>()) {
                if (m->button == sf::Mouse::Button::Left) {
                    float mx = static_cast<float>(m->position.x);
                    float my = static_cast<float>(m->position.y);

                    // Category pills in sidebar
                    float catY = 168.0f;
                    for (int c = 0; c < static_cast<int>(categories.size()); c++) {
                        sf::FloatRect r({28.0f, catY}, {L.sidebarW - 56.0f, 36.0f});
                        if (r.contains({mx, my})) {
                            filterIndex = c;
                            selectedLocal = 0;
                            scrollY = 0.0f;
                        }
                        catY += 44.0f;
                    }

                    // Demo rows
                    auto fi = filtered();
                    float viewTop = L.listTop;
                    float viewBot = static_cast<float>(winH) - L.listBottomPad;
                    if (mx >= L.listX && mx <= L.listX + L.listW && my >= viewTop && my <= viewBot) {
                        float localY = my - viewTop + scrollY;
                        int row = static_cast<int>(localY / L.rowH);
                        if (row >= 0 && row < static_cast<int>(fi.size())) {
                            if (row == selectedLocal) {
                                launchDemo(
                                    window, font, fontLoaded, buildDirectory, demos[fi[row]]
                                );
                            } else {
                                selectedLocal = row;
                            }
                        }
                    }

                    // Launch button in detail panel
                    sf::FloatRect launchBtn(
                        {L.detailX + 18.0f, static_cast<float>(winH) - 92.0f},
                        {L.detailW - 36.0f, 44.0f}
                    );
                    if (launchBtn.contains({mx, my}) && !fi.empty()) {
                        launchDemo(
                            window,
                            font,
                            fontLoaded,
                            buildDirectory,
                            demos[selectedDemoIndex()]
                        );
                    }
                }
            }
        }

        // Hover highlight for rows
        sf::Vector2i mouse = sf::Mouse::getPosition(window);
        int hoverLocal = -1;
        {
            float mx = static_cast<float>(mouse.x);
            float my = static_cast<float>(mouse.y);
            float viewTop = L.listTop;
            float viewBot = static_cast<float>(winH) - L.listBottomPad;
            if (mx >= L.listX && mx <= L.listX + L.listW && my >= viewTop && my <= viewBot) {
                float localY = my - viewTop + scrollY;
                int row = static_cast<int>(localY / L.rowH);
                if (row >= 0 && row < static_cast<int>(idxList.size())) {
                    hoverLocal = row;
                }
            }
        }

        // ── Draw ────────────────────────────────────────────────────────
        drawVerticalGradient(
            window, 0, 0, static_cast<float>(winW), static_cast<float>(winH), colBgTop, colBgBot
        );

        // Ambient orbs
        float pulse = 0.5f + 0.5f * std::sin(animTime * 1.2f);
        drawSoftCircle(
            window,
            {winW - 80.0f, 40.0f},
            200.0f,
            sf::Color(30, 90, 80, static_cast<std::uint8_t>(28 + pulse * 18))
        );
        drawSoftCircle(
            window,
            {60.0f, winH - 40.0f},
            160.0f,
            sf::Color(40, 60, 110, 30)
        );
        drawSoftCircle(
            window,
            {winW * 0.55f, winH * 0.7f},
            280.0f,
            sf::Color(20, 50, 40, 22)
        );

        // Header bar
        drawRoundRect(
            window,
            {{0, 0}, {static_cast<float>(winW), L.headerH}},
            sf::Color(10, 18, 28, 240),
            sf::Color(30, 50, 60, 0),
            0.0f
        );
        // Accent line under header
        sf::RectangleShape accentLine({static_cast<float>(winW), 2.0f});
        accentLine.setPosition({0, L.headerH - 2.0f});
        accentLine.setFillColor(sf::Color(colAccent.r, colAccent.g, colAccent.b, 160));
        window.draw(accentLine);

        // Logo diamond
        drawDiamondMark(window, {52.0f, 54.0f}, 18.0f, colAccent);
        drawDiamondMark(window, {52.0f, 54.0f}, 8.0f, colGold);

        if (fontLoaded) {
            drawText(window, font, "DINGER DERBY", 30, {82.0f, 28.0f}, colText, true);
            drawText(
                window,
                font,
                "Engine demos  |  Home Run Derby foundation",
                15,
                {84.0f, 66.0f},
                colMuted
            );
            drawText(
                window,
                font,
                "Up/Down select   Enter launch   Tab category   Esc quit",
                13,
                {winW - 420.0f, 42.0f},
                sf::Color(100, 130, 145)
            );
        }

        // Sidebar panel
        drawRoundRect(
            window,
            {{16.0f, L.headerH + 16.0f},
             {L.sidebarW - 8.0f, static_cast<float>(winH) - L.headerH - 32.0f}},
            colPanel,
            colPanelEdge,
            1.0f
        );

        if (fontLoaded) {
            drawText(window, font, "CATEGORIES", 12, {36.0f, L.headerH + 32.0f}, colMuted, true);
        }

        float catY = 168.0f;
        for (int c = 0; c < static_cast<int>(categories.size()); c++) {
            bool on = c == filterIndex;
            sf::FloatRect r({28.0f, catY}, {L.sidebarW - 56.0f, 36.0f});
            drawRoundRect(
                window,
                r,
                on ? colRowSel : sf::Color(20, 30, 44),
                on ? colRowSelEdge : sf::Color(30, 44, 58),
                on ? 1.5f : 1.0f
            );
            if (on) {
                sf::RectangleShape bar({4.0f, 36.0f});
                bar.setPosition({28.0f, catY});
                bar.setFillColor(colAccent);
                window.draw(bar);
            }
            if (fontLoaded) {
                int count = static_cast<int>(
                    indicesForFilter(
                        demos, categories[c] == "All" ? "All" : categories[c]
                    )
                        .size()
                );
                // For "All" indicesForFilter with "All" works
                if (categories[c] == "All") {
                    count = static_cast<int>(demos.size());
                } else {
                    count = 0;
                    for (const auto& d : demos) {
                        if (d.category == categories[c]) {
                            count++;
                        }
                    }
                }
                drawText(
                    window,
                    font,
                    categories[c],
                    15,
                    {44.0f, catY + 8.0f},
                    on ? colText : colMuted,
                    on
                );
                std::ostringstream n;
                n << count;
                drawText(
                    window,
                    font,
                    n.str(),
                    13,
                    {L.sidebarW - 52.0f, catY + 9.0f},
                    on ? colAccent : sf::Color(80, 100, 115)
                );
            }
            catY += 44.0f;
        }

        // Sidebar footer tip
        if (fontLoaded) {
            drawText(
                window,
                font,
                "Featured demos power\nthe HR Derby path.",
                13,
                {36.0f, static_cast<float>(winH) - 78.0f},
                sf::Color(90, 120, 130)
            );
        }

        // List panel
        drawRoundRect(
            window,
            {{L.listX - 12.0f, L.headerH + 16.0f},
             {L.listW + 24.0f, static_cast<float>(winH) - L.headerH - 32.0f}},
            colPanel,
            colPanelEdge,
            1.0f
        );

        if (fontLoaded) {
            std::ostringstream title;
            title << categories[filterIndex] << "  |  " << idxList.size()
                  << (idxList.size() == 1 ? " demo" : " demos");
            drawText(window, font, title.str(), 13, {L.listX, L.headerH + 28.0f}, colMuted, true);
        }

        // Clip rows by only drawing those in view
        float viewTop = L.listTop;
        float viewBot = static_cast<float>(winH) - L.listBottomPad;
        for (int li = 0; li < static_cast<int>(idxList.size()); li++) {
            float y = viewTop + li * L.rowH - scrollY;
            if (y + L.rowH < viewTop || y > viewBot) {
                continue;
            }

            int di = idxList[li];
            bool sel = li == selectedLocal;
            bool hover = li == hoverLocal;

            sf::Color fill = colRow;
            sf::Color edge(32, 48, 64);
            if (sel) {
                fill = colRowSel;
                edge = colRowSelEdge;
            } else if (hover) {
                fill = colRowHover;
                edge = sf::Color(50, 80, 100);
            }

            drawRoundRect(
                window,
                {{L.listX, y + 4.0f}, {L.listW, L.rowH - 8.0f}},
                fill,
                edge,
                sel ? 1.6f : 1.0f
            );

            if (sel) {
                sf::RectangleShape bar({4.0f, L.rowH - 8.0f});
                bar.setPosition({L.listX, y + 4.0f});
                bar.setFillColor(colAccent);
                window.draw(bar);
            }

            if (demos[di].featured) {
                drawDiamondMark(
                    window,
                    {L.listX + 22.0f, y + L.rowH * 0.5f},
                    7.0f,
                    sel ? colGold : sf::Color(colGold.r, colGold.g, colGold.b, 160)
                );
            } else if (li < 9) {
                if (fontLoaded) {
                    drawText(
                        window,
                        font,
                        std::to_string(li + 1),
                        13,
                        {L.listX + 14.0f, y + 16.0f},
                        sel ? colAccent : sf::Color(70, 95, 110)
                    );
                }
            }

            if (fontLoaded) {
                drawText(
                    window,
                    font,
                    demos[di].name,
                    17,
                    {L.listX + 40.0f, y + 8.0f},
                    sel ? colText : sf::Color(210, 222, 232),
                    sel
                );
                drawText(
                    window,
                    font,
                    demos[di].description,
                    13,
                    {L.listX + 40.0f, y + 30.0f},
                    sel ? sf::Color(170, 210, 200) : sf::Color(100, 120, 135)
                );
            }
        }

        // Scroll hint
        {
            float viewH = viewBot - viewTop;
            float contentH = static_cast<float>(idxList.size()) * L.rowH;
            if (contentH > viewH + 1.0f) {
                float trackH = viewH - 16.0f;
                float thumbH = std::max(28.0f, trackH * (viewH / contentH));
                float maxScroll = contentH - viewH;
                float t = maxScroll > 1e-3f ? scrollY / maxScroll : 0.0f;
                float thumbY = viewTop + 8.0f + t * (trackH - thumbH);
                sf::RectangleShape track({4.0f, trackH});
                track.setPosition({L.listX + L.listW + 6.0f, viewTop + 8.0f});
                track.setFillColor(sf::Color(30, 44, 58));
                window.draw(track);
                sf::RectangleShape thumb({4.0f, thumbH});
                thumb.setPosition({L.listX + L.listW + 6.0f, thumbY});
                thumb.setFillColor(colAccentDim);
                window.draw(thumb);
            }
        }

        // Detail panel
        drawRoundRect(
            window,
            {{L.detailX, L.headerH + 16.0f},
             {L.detailW, static_cast<float>(winH) - L.headerH - 32.0f}},
            colPanel,
            colPanelEdge,
            1.0f
        );

        if (!idxList.empty() && fontLoaded) {
            const DemoEntry& d = demos[selectedDemoIndex()];

            // Hero card
            drawRoundRect(
                window,
                {{L.detailX + 16.0f, L.headerH + 32.0f}, {L.detailW - 32.0f, 120.0f}},
                sf::Color(20, 40, 48),
                d.featured ? colGold : colAccentDim,
                1.5f
            );
            drawSoftCircle(
                window,
                {L.detailX + L.detailW * 0.5f, L.headerH + 88.0f},
                36.0f,
                sf::Color(40, 100, 80, 50)
            );
            drawDiamondMark(
                window,
                {L.detailX + L.detailW * 0.5f, L.headerH + 88.0f},
                16.0f,
                d.featured ? colGold : colAccent
            );

            drawText(window, font, "SELECTED", 11, {L.detailX + 20.0f, L.headerH + 168.0f}, colMuted, true);
            drawText(
                window,
                font,
                d.name,
                22,
                {L.detailX + 20.0f, L.headerH + 188.0f},
                colText,
                true
            );

            // Word-wrap description roughly
            drawText(
                window,
                font,
                d.description,
                14,
                {L.detailX + 20.0f, L.headerH + 228.0f},
                sf::Color(150, 175, 185)
            );

            drawText(window, font, "CATEGORY", 11, {L.detailX + 20.0f, L.headerH + 300.0f}, colMuted, true);
            drawText(window, font, d.category, 15, {L.detailX + 20.0f, L.headerH + 320.0f}, colAccent);

            drawText(window, font, "EXECUTABLE", 11, {L.detailX + 20.0f, L.headerH + 360.0f}, colMuted, true);
            drawText(
                window,
                font,
                d.executable,
                13,
                {L.detailX + 20.0f, L.headerH + 380.0f},
                sf::Color(160, 180, 140)
            );

            if (d.featured) {
                drawRoundRect(
                    window,
                    {{L.detailX + 20.0f, L.headerH + 420.0f}, {100.0f, 24.0f}},
                    sf::Color(60, 50, 20),
                    colGold,
                    1.0f
                );
                drawText(
                    window,
                    font,
                    "* FEATURED",
                    12,
                    {L.detailX + 32.0f, L.headerH + 424.0f},
                    colGold,
                    true
                );
            }

            // Launch button
            float pulseA = 0.75f + 0.25f * std::sin(animTime * 3.0f);
            sf::Color btnFill(
                static_cast<std::uint8_t>(30 + 20 * pulseA),
                static_cast<std::uint8_t>(90 + 40 * pulseA),
                static_cast<std::uint8_t>(70 + 20 * pulseA)
            );
            drawRoundRect(
                window,
                {{L.detailX + 18.0f, static_cast<float>(winH) - 92.0f},
                 {L.detailW - 36.0f, 44.0f}},
                btnFill,
                colAccent,
                2.0f
            );
            drawText(
                window,
                font,
                "   >  LAUNCH",
                18,
                {L.detailX + 70.0f, static_cast<float>(winH) - 80.0f},
                colText,
                true
            );
        }

        window.display();
    }

    return 0;
}
