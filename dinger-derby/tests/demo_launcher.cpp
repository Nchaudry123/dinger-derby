#include <SFML/Graphics.hpp>
#include <cstdlib>
#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

namespace {

struct DemoEntry {
    std::string name;
    std::string executable;
    std::string description;
};

std::vector<DemoEntry> makeDemoEntries() {
    return {
        {"User Ball", "user_ball_test", "2D launch-and-collision sandbox"},
        {"Random Spawn", "random_spawn_test", "2D physics stress with random balls"},
        {"Bat Test", "bat_test", "Bat/body interaction demo"},
        {"Spin Test", "spin_test", "Angular motion and impulses"},
        {"Point Cloud 3D", "point_cloud_3d_demo", "First 3D projection points"},
        {"Wire Cube 3D", "wire_cube_3d_demo", "Wireframe cube transform demo"},
        {"Transform Stack 3D", "transform_stack_3d_demo", "Nested transform demo"},
        {"Filled Cube 3D", "filled_cube_3d_demo", "Painter-style filled cube"},
        {"Depth Sort 3D", "depth_sort_3d_demo", "Triangle ordering demo"},
        {"Raster Cube 3D", "raster_cube_3d_demo", "Software rasterized cube"},
        {"Raster Depth 3D", "raster_depth_3d_demo", "Depth-buffered raster cubes"},
        {"Soft Cube 3D", "soft_cube_3d_demo", "Springy deformable mesh"},
        {"Culling Stress 3D", "culling_stress_3d_demo", "Grid, culling, and triangle counters"},
        {"Physics 3D", "physics3d_demo", "Rigid 3D sphere collisions and bounds"},
        {"Rotation 3D", "rotation_3d_demo", "Yaw, pitch, roll, and orbit camera"}
    };
}

bool loadMenuFont(sf::Font& font) {
    const std::vector<std::filesystem::path> candidates = {
        "/System/Library/Fonts/Supplemental/Arial.ttf",
        "/System/Library/Fonts/Supplemental/Helvetica.ttf",
        "/System/Library/Fonts/Helvetica.ttc",
        "/System/Library/Fonts/SFNS.ttf"
    };

    for (const std::filesystem::path& candidate : candidates) {
        if (font.openFromFile(candidate)) {
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
    sf::Color color
) {
    sf::Text text(font, value, size);
    text.setPosition(position);
    text.setFillColor(color);
    window.draw(text);
}

void launchDemo(
    sf::RenderWindow& window,
    const sf::Font& font,
    bool fontLoaded,
    const std::filesystem::path& buildDirectory,
    const DemoEntry& demo
) {
    std::filesystem::path executable = buildDirectory / demo.executable;

    window.setTitle("Dinger Derby Demo Launcher | running " + demo.name);
    window.clear(sf::Color(7, 10, 16));

    if (fontLoaded) {
        drawText(
            window,
            font,
            "Running " + demo.name + "... close that demo window to return here.",
            24,
            sf::Vector2f(64.0f, 320.0f),
            sf::Color(235, 244, 255)
        );
    }

    window.display();

    std::string command = shellQuote(executable);
    int result = std::system(command.c_str());
    (void)result;

    window.setTitle("Dinger Derby Demo Launcher");
}

}

int main(int argc, char** argv) {
    sf::RenderWindow window(
        sf::VideoMode(sf::Vector2u(1000, 840)),
        "Dinger Derby Demo Launcher"
    );
    window.setFramerateLimit(60);

    std::vector<DemoEntry> demos = makeDemoEntries();
    int selected = 0;
    std::filesystem::path buildDirectory = executableDirectory(argc > 0 ? argv[0] : "./demo_launcher");

    sf::Font font;
    bool fontLoaded = loadMenuFont(font);

    while (window.isOpen()) {
        while (const std::optional event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                window.close();
            }

            if (const auto* key = event->getIf<sf::Event::KeyPressed>()) {
                if (key->code == sf::Keyboard::Key::Escape) {
                    window.close();
                }

                if (key->code == sf::Keyboard::Key::Down) {
                    selected = (selected + 1) % static_cast<int>(demos.size());
                }

                if (key->code == sf::Keyboard::Key::Up) {
                    selected = (selected + static_cast<int>(demos.size()) - 1) %
                        static_cast<int>(demos.size());
                }

                int hotkeyIndex = numberKeyIndex(key->code);
                if (hotkeyIndex >= 0 && hotkeyIndex < static_cast<int>(demos.size())) {
                    selected = hotkeyIndex;
                    launchDemo(window, font, fontLoaded, buildDirectory, demos[selected]);
                }

                if (key->code == sf::Keyboard::Key::Enter) {
                    launchDemo(window, font, fontLoaded, buildDirectory, demos[selected]);
                }
            }
        }

        window.clear(sf::Color(5, 8, 14));

        sf::RectangleShape header(sf::Vector2f(1000.0f, 120.0f));
        header.setPosition(sf::Vector2f(0.0f, 0.0f));
        header.setFillColor(sf::Color(18, 36, 50));
        window.draw(header);

        sf::CircleShape glow(180.0f);
        glow.setPosition(sf::Vector2f(690.0f, -130.0f));
        glow.setFillColor(sf::Color(50, 105, 115, 110));
        window.draw(glow);

        if (fontLoaded) {
            drawText(
                window,
                font,
                "Dinger Derby Engine Demos",
                34,
                sf::Vector2f(42.0f, 28.0f),
                sf::Color(245, 250, 255)
            );
            drawText(
                window,
                font,
                "Up/Down selects   Enter launches   1-9 quick launch   Esc quits",
                17,
                sf::Vector2f(45.0f, 76.0f),
                sf::Color(170, 205, 210)
            );
        }

        float top = 138.0f;
        float rowHeight = 42.0f;

        for (int i = 0; i < static_cast<int>(demos.size()); i++) {
            float y = top + i * rowHeight;
            bool isSelected = i == selected;

            sf::RectangleShape row(sf::Vector2f(920.0f, 34.0f));
            row.setPosition(sf::Vector2f(40.0f, y));
            row.setFillColor(isSelected ? sf::Color(42, 94, 104) : sf::Color(13, 19, 29));
            row.setOutlineThickness(1.0f);
            row.setOutlineColor(isSelected ? sf::Color(120, 220, 220) : sf::Color(25, 36, 48));
            window.draw(row);

            if (!fontLoaded) {
                continue;
            }

            std::ostringstream label;
            if (i < 9) {
                label << (i + 1) << ". ";
            } else {
                label << "   ";
            }
            label << demos[i].name;

            drawText(
                window,
                font,
                label.str(),
                19,
                sf::Vector2f(56.0f, y + 5.0f),
                isSelected ? sf::Color(245, 255, 250) : sf::Color(214, 224, 232)
            );

            drawText(
                window,
                font,
                demos[i].description,
                15,
                sf::Vector2f(365.0f, y + 8.0f),
                isSelected ? sf::Color(190, 236, 232) : sf::Color(125, 145, 158)
            );
        }

        if (fontLoaded) {
            drawText(
                window,
                font,
                "Selected: " + demos[selected].executable,
                15,
                sf::Vector2f(42.0f, 808.0f),
                sf::Color(125, 155, 165)
            );
        }

        window.display();
    }

    return 0;
}
