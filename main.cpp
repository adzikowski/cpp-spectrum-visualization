#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>
#include <vector>
#include <fftw3.h>
#include <cmath>
#define _WIN32_WINNT 0x0500
#include <windows.h>

float lerp(float a, float b, float f) {
    return a + f * (b - a);
}

struct Rect : public sf::Drawable {
    sf::RectangleShape rect;
    sf::Vector2f rectPos;
    sf::Vector2f blockSize;
    float currentHeight;
    float targetHeight;

    void draw(sf::RenderTarget& target, sf::RenderStates states) const override {
        target.draw(rect, states);
    }
};

int main(int argc, char *argv[]) {
    if (argc < 2 || argc > 3) {
        fprintf(stderr, "Usage: %s <filename>\n", argv[0]);
        return -1;
    }
    HWND hWnd = GetConsoleWindow();
    ShowWindow(hWnd, SW_HIDE);
    constexpr int initialPadding = 10;
    constexpr int padding = 5;
    constexpr float frameRate = 60.f;
    const sf::Vector2f screenRes(1280, 720);
    sf::RenderWindow window(sf::VideoMode(screenRes.x, screenRes.y), "FFT Spectrum");

    sf::Vector2f blockSize(5, 100);

    constexpr float animationDuration = 0.1f;
    float elapsedTime = 0.0f;
    constexpr float updateInterval = 0.05f;

    const int numBlocks = static_cast<int>(screenRes.x / (blockSize.x + padding));
    std::vector<Rect> rects(numBlocks);

    const float missingPadding = (screenRes.x - ((numBlocks * blockSize.x) + ((numBlocks - 1) * padding))) / 2;

    for (int i = 0; i < numBlocks; ++i) {
        rects[i].blockSize = blockSize;
        rects[i].rect.setSize(rects[i].blockSize);
        rects[i].rectPos = sf::Vector2f(missingPadding + i * (blockSize.x + padding), screenRes.y - blockSize.y);
        rects[i].rect.setPosition(rects[i].rectPos);
        rects[i].currentHeight = blockSize.y;
        rects[i].targetHeight = blockSize.y;
    }

    sf::SoundBuffer buffer;
    if (!buffer.loadFromFile(argv[1])) {
        fprintf(stderr, "Failed to load from file\n");
        return -1;
    }

    sf::Sound sound;
    sound.setBuffer(buffer);
    sound.play();

    window.setFramerateLimit(frameRate);

    int sampleCount = buffer.getSampleCount();
    int sampleRate = buffer.getSampleRate();
    int channelCount = buffer.getChannelCount();

    int fftSize = 1024;
    std::vector<double> fftInput(fftSize);
    std::vector<fftw_complex> fftOutput(fftSize);
    fftw_plan fftPlan = fftw_plan_dft_r2c_1d(fftSize, fftInput.data(), fftOutput.data(), FFTW_ESTIMATE);
    while (window.isOpen()) {
        if (sound.getStatus() == sf::SoundSource::Status::Stopped()) break;
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) {
                window.close();
            }
            if ((event.type == sf::Event::KeyPressed) and (event.key.code == sf::Keyboard::Right)) {
                sf::Time newOffset = sound.getPlayingOffset() + sf::seconds(10.f);
                if (newOffset < buffer.getDuration()) {
                    sound.setPlayingOffset(newOffset);
                }
            }

            if ((event.type == sf::Event::KeyPressed) and (event.key.code == sf::Keyboard::Left)) {
                sf::Time newOffset = sound.getPlayingOffset() + sf::seconds(-10.f);
                if (newOffset < buffer.getDuration() and newOffset.asSeconds() > 0) {
                    sound.setPlayingOffset(newOffset);
                }
            }
        }

        const sf::Int16* samples = buffer.getSamples();
        int currentSample = static_cast<int>(sound.getPlayingOffset().asSeconds() * sampleRate);

        if (currentSample + fftSize < sampleCount) {
            for (int i = 0; i < fftSize; ++i) {
                fftInput[i] = samples[(currentSample + i) * channelCount] / 32768.0;
            }

            fftw_execute(fftPlan);

            for (int i = 0; i < numBlocks; ++i) {
                if (i < fftSize / 2) {
                    double magnitude = std::sqrt(fftOutput[i][0] * fftOutput[i][0] + fftOutput[i][1] * fftOutput[i][1]);
                    double logMagnitude = std::log(magnitude + 1);

                   /* double rf = (std::pow(12200, 2) * std::pow(magnitude, 4))/((std::pow(magnitude, 2)+std::pow(20.6, 2))*
                    sqrt((std::pow(magnitude, 2) + std::pow(107.7, 2))*
                        (std::pow(magnitude, 2) + std::pow(737.6, 2)))*
                        (std::pow(magnitude, 2) + std::pow(122000, 2)));*/
    
                    /*double logMagnitude = 20 * std::log(rf);*/
                    rects[i].targetHeight = static_cast<float>(logMagnitude * screenRes.y / 10);
                }
                else {
                    rects[i].targetHeight = 0;
                }
            }
        }

        elapsedTime += 1.f / frameRate;
        if (elapsedTime >= updateInterval) {
            elapsedTime = 0.f;
        }

        float t = std::min(1.f, elapsedTime / animationDuration);
        for (int i = 0; i < numBlocks; ++i) {
            float interpHeight = lerp(rects[i].currentHeight, rects[i].targetHeight, t);
            rects[i].rectPos.y = screenRes.y - interpHeight;
            rects[i].currentHeight = interpHeight;
            rects[i].blockSize.y = interpHeight;
            rects[i].rect.setPosition(rects[i].rectPos);
            rects[i].rect.setSize(rects[i].blockSize);
        }

        window.clear();
        for (const auto& shape : rects) {
            window.draw(shape);
        }
        window.display();

    }

    fftw_destroy_plan(fftPlan);
    fftw_cleanup();

    return 0;
}
