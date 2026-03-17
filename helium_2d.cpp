#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <iostream>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <thread>
#include <chrono>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
using namespace glm;
using namespace std;

// --- constants ---
float orbitDistance = 15.0f;
const float Z_eff = 1.6875f;

// --- engine ---
struct Engine {
    GLFWwindow* window;
    int WIDTH = 800, HEIGHT = 600;

    Engine() {
        if (!glfwInit()) { cerr << "failed to init glfw"; exit(EXIT_FAILURE); }
        window = glfwCreateWindow(WIDTH, HEIGHT, "Helium 2D Sim", nullptr, nullptr);
        if (!window) { cerr << "failed to create window"; glfwTerminate(); exit(EXIT_FAILURE); }
        glfwMakeContextCurrent(window);
        int fbWidth, fbHeight;
        glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
        glViewport(0, 0, fbWidth, fbHeight);
    }

    void run() {
        glClear(GL_COLOR_BUFFER_BIT);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        double halfWidth = WIDTH / 2.0f, halfHeight = HEIGHT / 2.0f;
        glOrtho(-halfWidth, halfWidth, -halfHeight, halfHeight, -1.0, 1.0);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
    }
};
Engine engine;

// --- waves ---
struct WavePoint { vec2 localPos; vec2 dir; };
struct Wave {
    float energy;
    float sigma = 4.0f, k = 0.4f, phase = 0.0f, a = 10.0f;
    vector<WavePoint> points;
    vec2 pos, dir;
    vec3 col;

    Wave(float e, vec2 pos, vec2 dir, vec3 col = vec3(0.0f, 1.0f, 1.0f))
        : energy(e), pos(pos), dir(dir), col(col) {
        dir = normalize(dir);
        for (float x = -sigma; x <= sigma; x += 0.1f)
            points.push_back({ pos + x * dir, dir * 200.0f });
    }

    void draw() {
        glColor3f(col.r, col.g, col.b);
        glBegin(GL_LINE_STRIP);
        for (WavePoint& p : points) {
            vec2 perp(-p.dir.y, p.dir.x);
            perp = normalize(perp);
            float y_disp = a * sin(k * length(p.localPos) - phase);
            vec2 drawPos = p.localPos + perp * y_disp;
            glVertex2f(drawPos.x, drawPos.y);
        }
        glEnd();
    }

    bool update(float dt) {
        phase += 10.0f * dt;
        for (WavePoint& p : points) {
            p.localPos += p.dir * dt * 0.3f;
            if (p.localPos.x < -engine.WIDTH / 2.0f || p.localPos.x > engine.WIDTH / 2.0f ||
                p.localPos.y < -engine.HEIGHT / 2.0f || p.localPos.y > engine.HEIGHT / 2.0f)
                return true;
        }
        return false;
    }
};
vector<Wave> waves;

// --- particles ---
struct Particle {
    vec2 pos;
    int charge;
    float angle = 0.0f;
    int n = 1;
    float excitedTimer = 0.0f;
    vec3 color;

    Particle(vec2 pos, int charge, vec3 color = vec3(1.0f))
        : pos(pos), charge(charge), color(color) {}

    void draw(vec2 centre, int segments = 40) {
        if (charge == -1) {
            // orbit ring - color changes with n
            glLineWidth(0.5f);
            glBegin(GL_LINE_LOOP);
            if (n == 1) glColor3f(0.2f, 0.2f, 0.2f);
            else if (n == 2) glColor3f(color.r * 0.6f, color.g * 0.6f, color.b * 0.6f);
            for (int i = 0; i <= segments; i++) {
                float a = 2.0f * M_PI * i / segments;
                glVertex2f(cos(a) * n * orbitDistance * Z_eff + centre.x,
                           sin(a) * n * orbitDistance * Z_eff + centre.y);
            }
            glEnd();

            // electron - brighter when excited
            float brightness = (n == 1) ? 1.0f : 1.8f;
            glColor3f(
                glm::min(color.r * brightness, 1.0f),
                glm::min(color.g * brightness, 1.0f),
                glm::min(color.b * brightness, 1.0f)
            );
            glBegin(GL_TRIANGLE_FAN);
            glVertex2f(pos.x, pos.y);
            for (int i = 0; i <= segments; i++) {
                float a = 2.0f * M_PI * i / segments;
                glVertex2f(cos(a) * 4.0f + pos.x, sin(a) * 4.0f + pos.y);
            }
            glEnd();

        } else if (charge == 1) {
            // nucleus: 2 protons
            float offset = 3.0f;
            for (int p = -1; p <= 1; p += 2) {
                glColor3f(1.0f, 0.2f, 0.2f);
                glBegin(GL_TRIANGLE_FAN);
                glVertex2f(pos.x + p * offset, pos.y);
                for (int i = 0; i <= segments; i++) {
                    float a = 2.0f * M_PI * i / segments;
                    glVertex2f(cos(a) * 5.0f + pos.x + p * offset, sin(a) * 5.0f + pos.y);
                }
                glEnd();
            }
        }
    }

    void update(vec2 c, vec2 otherElectronPos) {
        if (excitedTimer > 0.0f) excitedTimer -= 0.001f;
        
        float r = n * orbitDistance * Z_eff;

        // electron-electron repulsion
        vec2 toOther = otherElectronPos - pos;
        float dist = length(toOther);
        float repulsion = 0.0f;
        if (dist > 0.1f) repulsion = 0.3f / dist;

        angle += 0.003f + repulsion * 0.5f;
        pos = vec2(cos(angle) * r + c.x, sin(angle) * r + c.y);

        // emit photon when dropping back down
        if (excitedTimer <= 0.0f && n > 1) {
            n--;
            excitedTimer += 0.003f;
            float waveDirX = ((float)rand() / RAND_MAX) * 2.0f - 1.0f;
            float waveDirY = ((float)rand() / RAND_MAX) * 2.0f - 1.0f;
            float energyDiff = -13.6f * Z_eff * Z_eff / ((n + 1) * (n + 1))
                             - (-13.6f * Z_eff * Z_eff / (n * n));
            waves.emplace_back(energyDiff, pos, vec2(waveDirX, waveDirY), vec3(1.0f, 1.0f, 0.0f));
        }
        if (n > 2) n = 2;
    }
};

// --- atom ---
struct Atom {
    vec2 pos;
    vec2 v = vec2(0.0f);
    vector<Particle> particles;

    Atom(vec2 p) : pos(p) {
        particles.emplace_back(pos, 1);
        Particle e1(vec2(pos.x - orbitDistance, pos.y), -1, vec3(0.0f, 1.0f, 1.0f));
        e1.angle = 0.0f;
        particles.push_back(e1);
        Particle e2(vec2(pos.x + orbitDistance, pos.y), -1, vec3(1.0f, 0.5f, 0.0f));
        e2.angle = (float)M_PI; // opposite side - Pauli
        particles.push_back(e2);
    }
};
vector<Atom> atoms;

// --- mouse callback ---
static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    if (button != GLFW_MOUSE_BUTTON_LEFT || action != GLFW_PRESS) return;
    double mx, my;
    glfwGetCursorPos(window, &mx, &my);
    Engine* eng = static_cast<Engine*>(glfwGetWindowUserPointer(window));
    float worldX = (float)mx - eng->WIDTH / 2.0f;
    float worldY = eng->HEIGHT / 2.0f - (float)my;
    vec2 spawnPos(worldX, worldY);

    float energy = -13.6f * Z_eff * Z_eff / (2 * 2) - (-13.6f * Z_eff * Z_eff);
    for (int i = 0; i < 50; i++) {
        float angle = ((float)rand() / RAND_MAX) * 2.0f * M_PI;
        // incoming photons are cyan
        waves.push_back(Wave(energy, spawnPos, vec2(cos(angle), sin(angle)), vec3(0.0f, 1.0f, 1.0f)));
    }
}

// --- main ---
int main() {
    srand(time(0));

    int num_atoms = 10;
    float radius = 150.0f;
    for (int i = 0; i < num_atoms; i++) {
        float angle = 2.0f * M_PI * i / num_atoms;
        atoms.emplace_back(vec2(cos(angle) * radius, sin(angle) * radius));
    }

    glfwSetWindowUserPointer(engine.window, &engine);
    glfwSetMouseButtonCallback(engine.window, mouseButtonCallback);

    // initial cyan photons coming from the right
    float energy = -13.6f * Z_eff * Z_eff / (2 * 2) - (-13.6f * Z_eff * Z_eff);
    // right to left
    for (int i = 0; i < 8; i++)
        waves.push_back(Wave(energy, vec2(300.0f, i * 40 - 160), vec2(-1.0f, 0.0f), vec3(0.0f, 1.0f, 1.0f)));
    // left to right
    for (int i = 0; i < 8; i++)
        waves.push_back(Wave(energy, vec2(-300.0f, i * 40 - 160), vec2(1.0f, 0.0f), vec3(0.0f, 1.0f, 1.0f)));
    // top to bottom
    for (int i = 0; i < 8; i++)
        waves.push_back(Wave(energy, vec2(i * 40 - 160, 250.0f), vec2(0.0f, -1.0f), vec3(0.0f, 1.0f, 1.0f)));
    // bottom to top
    for (int i = 0; i < 8; i++)
        waves.push_back(Wave(energy, vec2(i * 40 - 160, -250.0f), vec2(0.0f, 1.0f), vec3(0.0f, 1.0f, 1.0f)));
    while (!glfwWindowShouldClose(engine.window)) {
        engine.run();

        for (Atom& a : atoms) {
            // atom-atom repulsion
            for (Atom& a2 : atoms) {
                if (&a2 == &a) continue;
                float dist = length(a.pos - a2.pos);
                if (dist < 0.01f) continue;
                vec2 dir = normalize(a.pos - a2.pos);
                a.v += dir / dist * 57.5f;
            }

            // boundary
            const float bs = 0.01f, bt = 180.0f;
            if (a.pos.x + engine.WIDTH / 2.0f < bt)  a.v.x += (bt - (a.pos.x + engine.WIDTH / 2.0f)) * bs;
            if (engine.WIDTH / 2.0f - a.pos.x < bt)  a.v.x -= (bt - (engine.WIDTH / 2.0f - a.pos.x)) * bs;
            if (engine.HEIGHT / 2.0f - a.pos.y < bt) a.v.y -= (bt - (engine.HEIGHT / 2.0f - a.pos.y)) * bs;
            if (a.pos.y + engine.HEIGHT / 2.0f < bt) a.v.y += (bt - (a.pos.y + engine.HEIGHT / 2.0f)) * bs;
            a.v *= 0.99f;
            a.pos += a.v * 0.11f;

            vec2 e1pos = a.particles.size() > 1 ? a.particles[1].pos : a.pos;
            vec2 e2pos = a.particles.size() > 2 ? a.particles[2].pos : a.pos;

            for (int i = 0; i < (int)a.particles.size(); i++) {
                Particle& p = a.particles[i];
                p.draw(a.pos);
                if (p.charge == 1) { p.pos = a.pos; continue; }
                if (p.excitedTimer > 0.0f) p.excitedTimer -= 0.001f;
                vec2 otherPos = (i == 1) ? e2pos : e1pos;
                p.update(a.pos, otherPos);

                // photon absorption
                for (Wave& wave : waves) {
                    if (wave.energy == 0.0f) continue;
                    for (WavePoint& wp : wave.points) {
                        float dist = length(p.pos - wp.localPos);
                        float energyForUp = -13.6f * Z_eff * Z_eff / ((p.n + 1) * (p.n + 1))
                                          - (-13.6f * Z_eff * Z_eff / (p.n * p.n));
                        if (dist < 20.0f && wave.energy == energyForUp && wave.col == vec3(0.0f, 1.0f, 1.0f)) {
                            wave.col = vec3(1.0f, 1.0f, 0.0f);
                            wave.a = 15.0f;
                            wave.energy = 0.0f;
                            p.n += 1;
                            if (p.n > 2) p.n = 2;
                            p.excitedTimer += 0.003f;
                            break;
                        }
                    }
                }
            }
        }

        // draw waves
        for (auto it = waves.begin(); it != waves.end(); ) {
            if (it->energy == 0.0f) { ++it; continue; }
            it->draw();
            if (it->update(0.01f)) it = waves.erase(it);
            else ++it;
        }

        this_thread::sleep_for(chrono::milliseconds(8));
        glfwSwapBuffers(engine.window);
        glfwPollEvents();
    }
}