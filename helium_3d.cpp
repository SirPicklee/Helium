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
#include <iomanip>
#include <thread>
#include <chrono>
#include <complex>
#include <random>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
using namespace glm;
using namespace std;

// ================= Constants ================= //
const float a0 = 1.0f;
const double Z_eff = 1.6875; // Helium effective nuclear charge
float electron_r = 0.5f;
const double zmSpeed = 2.0;

// --- quantum numbers (helium ground state: both electrons 1s) ---
int n = 1, l = 0, m_qn = 0;
int N = 100000;
// ================= Sampling ================= //
random_device rd;
mt19937 gen(rd());
uniform_real_distribution<float> dis(0.0f, 1.0f);

double sampleR(mt19937& rng) {
    const int NCDF = 4096;
    const double rMax = 10.0 * n * n * a0 / Z_eff;

    static vector<double> cdf;
    static bool built = false;

    if (!built) {
        cdf.resize(NCDF);
        double dr = rMax / (NCDF - 1);
        double sum = 0.0;
        for (int i = 0; i < NCDF; ++i) {
            double r   = i * dr;
            double rho = 2.0 * Z_eff * r / (n * a0);
            int k = n - l - 1, alpha = 2 * l + 1;
            double L = 1.0, Lm1 = 1.0 + alpha - rho;
            if (k == 1) L = Lm1;
            else if (k > 1) {
                double Lm2 = 1.0;
                for (int j = 2; j <= k; ++j) {
                    L = ((2*j-1+alpha-rho)*Lm1 - (j-1+alpha)*Lm2) / j;
                    Lm2 = Lm1; Lm1 = L;
                }
            }
            double norm = pow(2.0*Z_eff/(n*a0), 3) * tgamma(n-l) / (2.0*n*tgamma(n+l+1));
            double R = sqrt(norm) * exp(-rho/2.0) * pow(rho, l) * L;
            double pdf = r * r * R * R;
            sum += pdf;
            cdf[i] = sum;
        }
        for (double& v : cdf) v /= sum;
        built = true;
    }

    uniform_real_distribution<double> u(0.0, 1.0);
    double val = u(rng);
    int idx = (int)(lower_bound(cdf.begin(), cdf.end(), val) - cdf.begin());
    return idx * (rMax / (NCDF - 1));
}

double sampleTheta(mt19937& rng) {
    const int NCDF = 2048;
    static vector<double> cdf;
    static bool built = false;

    if (!built) {
        cdf.resize(NCDF);
        double sum = 0.0;
        for (int i = 0; i < NCDF; ++i) {
            double theta = i * M_PI / (NCDF - 1);
            double pdf = sin(theta); // l=0, m=0: P_0^0 = 1
            sum += pdf;
            cdf[i] = sum;
        }
        for (double& v : cdf) v /= sum;
        built = true;
    }

    uniform_real_distribution<double> u(0.0, 1.0);
    double val = u(rng);
    int idx = (int)(lower_bound(cdf.begin(), cdf.end(), val) - cdf.begin());
    return idx * (M_PI / (NCDF - 1));
}

float samplePhi() {
    return 2.0f * M_PI * dis(gen);
}
// ================= Color Maps ================= //
vec4 heatmap_fire(float value) {
    value = glm::clamp(value, 0.0f, 1.0f);
    const int n_stops = 6;
    vec4 colors[n_stops] = {
        {0.0f, 0.0f, 0.0f, 1.0f},
        {0.5f, 0.0f, 0.99f, 1.0f},
        {0.8f, 0.0f, 0.0f, 1.0f},
        {1.0f, 0.5f, 0.0f, 1.0f},
        {1.0f, 1.0f, 0.0f, 1.0f},
        {1.0f, 1.0f, 1.0f, 1.0f}
    };
    float scaled = value * (n_stops - 1);
    int i = (int)scaled;
    int j = glm::min(i + 1, n_stops - 1);
    float t = scaled - i;
    return vec4(
        colors[i].r + t * (colors[j].r - colors[i].r),
        colors[i].g + t * (colors[j].g - colors[i].g),
        colors[i].b + t * (colors[j].b - colors[i].b),
        1.0f
    );
}

vec4 heatmap_cool(float value) {
    value = glm::clamp(value, 0.0f, 1.0f);
    const int n_stops = 5;
    vec4 colors[n_stops] = {
        {0.0f, 0.0f, 0.0f, 1.0f},
        {0.0f, 0.0f, 0.8f, 1.0f},
        {0.0f, 0.5f, 1.0f, 1.0f},
        {0.5f, 1.0f, 1.0f, 1.0f},
        {1.0f, 1.0f, 1.0f, 1.0f}
    };
    float scaled = value * (n_stops - 1);
    int i = (int)scaled;
    int j = glm::min(i + 1, n_stops - 1);
    float t = scaled - i;
    return vec4(
        colors[i].r + t * (colors[j].r - colors[i].r),
        colors[i].g + t * (colors[j].g - colors[i].g),
        colors[i].b + t * (colors[j].b - colors[i].b),
        1.0f
    );
}

// ================= Particles ================= //
struct Particle {
    vec3 pos;
    vec4 color;
    Particle(vec3 p, vec4 c) : pos(p), color(c) {}
};
vector<Particle> particles_e1;
vector<Particle> particles_e2;

vec3 sphericalToCartesian(float r, float theta, float phi) {
    return vec3(
        r * sin(theta) * cos(phi),
        r * cos(theta),
        r * sin(theta) * sin(phi)
    );
}

void generateParticles(int count) {
    particles_e1.clear();
    particles_e2.clear();

    for (int i = 0; i < count; ++i) {
        // electron 1 - fire colormap
        float r1 = (float)sampleR(gen);
        float t1 = (float)sampleTheta(gen);
        float p1 = samplePhi();
        vec3 pos1 = sphericalToCartesian(r1, t1, p1);
        double peak = pow(Z_eff / a0, 3) / M_PI;
        double psi2 = pow(Z_eff / a0, 3) / M_PI * exp(-2.0 * Z_eff * r1 / a0);
        float val1 = pow((float)glm::min(psi2 / peak, 1.0), 0.4f);
        particles_e1.emplace_back(pos1, heatmap_fire(val1));

        // electron 2 - cool colormap
        float r2 = (float)sampleR(gen);
        float t2 = (float)sampleTheta(gen);
        float p2 = samplePhi();
        vec3 pos2 = sphericalToCartesian(r2, t2, p2);
        double psi2b = pow(Z_eff / a0, 3) / M_PI * exp(-2.0 * Z_eff * r2 / a0);
        float val2 = pow((float)glm::min(psi2b / peak, 1.0), 0.4f);
        pos2 += vec3(0.5f, 0.0f, 0.0f);
        particles_e2.emplace_back(pos2, heatmap_cool(val2));
    }

    cout << "Generated " << count << " particles per electron. Z_eff=" << Z_eff << "\n";
}
// ================= Camera ================= //
struct Camera {
    vec3  target    = vec3(0.0f);
    float radius    = 15.0f;
    float azimuth   = 0.0f;
    float elevation = (float)(M_PI / 2.0);
    float orbitSpeed = 0.01f;
    double zoomSpeed  = zmSpeed;
    bool  dragging  = false;
    double lastX = 0.0, lastY = 0.0;

    vec3 position() const {
        float clampedEl = glm::clamp(elevation, 0.01f, (float)M_PI - 0.01f);
        return vec3(
            radius * sin(clampedEl) * cos(azimuth),
            radius * cos(clampedEl),
            radius * sin(clampedEl) * sin(azimuth)
        );
    }
    void processMouseMove(double x, double y) {
        float dx = (float)(x - lastX);
        float dy = (float)(y - lastY);
        if (dragging) {
            azimuth   += dx * orbitSpeed;
            elevation -= dy * orbitSpeed;
            elevation  = glm::clamp(elevation, 0.01f, (float)M_PI - 0.01f);
        }
        lastX = x; lastY = y;
    }
    void processMouseButton(int button, int action, int mods, GLFWwindow* win) {
        if (button == GLFW_MOUSE_BUTTON_LEFT || button == GLFW_MOUSE_BUTTON_MIDDLE) {
            dragging = (action == GLFW_PRESS);
            if (dragging) glfwGetCursorPos(win, &lastX, &lastY);
        }
    }
    void processScroll(double xoffset, double yoffset) {
        radius -= (float)(yoffset * zoomSpeed);
        if (radius < 1.0f) radius = 1.0f;
    }
};
Camera camera;

// ================= Engine ================= //
struct Engine {
    GLFWwindow* window;
    int WIDTH = 800, HEIGHT = 600;
    GLuint sphereVAO, sphereVBO;
    int    sphereVertexCount;
    GLuint shaderProgram;
    GLint  modelLoc, viewLoc, projLoc, colorLoc;

    const char* vertexShaderSource = R"glsl(
        #version 330 core
        layout(location=0) in vec3 aPos;
        uniform mat4 model; uniform mat4 view; uniform mat4 projection;
        out float lightIntensity;
        void main() {
            gl_Position = projection * view * model * vec4(aPos, 1.0);
            vec3 normal   = normalize(aPos);
            vec3 lightDir = normalize(vec3(1.0, 1.0, 1.0));
            lightIntensity = max(dot(normal, lightDir), 0.5);
        }
    )glsl";

    const char* fragmentShaderSource = R"glsl(
        #version 330 core
        in  float lightIntensity;
        out vec4  FragColor;
        uniform vec4 objectColor;
        void main() {
            FragColor = vec4(objectColor.rgb, objectColor.a);
        }
    )glsl";

    Engine() {
        if (!glfwInit()) exit(-1);
        window = glfwCreateWindow(WIDTH, HEIGHT, "Helium 3D Orbital", NULL, NULL);
        glfwMakeContextCurrent(window);
        glewInit();
        glEnable(GL_DEPTH_TEST);

        vector<float> vertices;
        float r = 0.05f;
        int stacks = 8, sectors = 8;
        for (int i = 0; i <= stacks; ++i) {
            float t1 = (float)i / stacks * (float)M_PI;
            float t2 = (float)(i+1) / stacks * (float)M_PI;
            for (int j = 0; j < sectors; ++j) {
                float p1 = (float)j / sectors * 2.0f * (float)M_PI;
                float p2 = (float)(j+1) / sectors * 2.0f * (float)M_PI;
                auto gp = [&](float t, float p) {
                    return vec3(r*sin(t)*cos(p), r*cos(t), r*sin(t)*sin(p));
                };
                vec3 v1=gp(t1,p1), v2=gp(t1,p2), v3=gp(t2,p1), v4=gp(t2,p2);
                vertices.insert(vertices.end(), {v1.x,v1.y,v1.z, v2.x,v2.y,v2.z, v3.x,v3.y,v3.z});
                vertices.insert(vertices.end(), {v2.x,v2.y,v2.z, v4.x,v4.y,v4.z, v3.x,v3.y,v3.z});
            }
        }
        sphereVertexCount = (int)(vertices.size() / 3);
        CreateVBOVAO(sphereVAO, sphereVBO, vertices);

        GLuint vs = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vs, 1, &vertexShaderSource, NULL);
        glCompileShader(vs);
        GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fs, 1, &fragmentShaderSource, NULL);
        glCompileShader(fs);
        shaderProgram = glCreateProgram();
        glAttachShader(shaderProgram, vs);
        glAttachShader(shaderProgram, fs);
        glLinkProgram(shaderProgram);

        modelLoc = glGetUniformLocation(shaderProgram, "model");
        viewLoc  = glGetUniformLocation(shaderProgram, "view");
        projLoc  = glGetUniformLocation(shaderProgram, "projection");
        colorLoc = glGetUniformLocation(shaderProgram, "objectColor");
    }

    void CreateVBOVAO(GLuint& VAO, GLuint& VBO, const vector<float>& verts) {
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, verts.size()*sizeof(float), verts.data(), GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3*sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
    }

    void drawParticles(vector<Particle>& pts) {
        glUseProgram(shaderProgram);
        mat4 proj = perspective(radians(45.0f), (float)WIDTH/(float)HEIGHT, 0.1f, 2000.0f);
        mat4 view = lookAt(camera.position(), camera.target, vec3(0,1,0));
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, value_ptr(view));
        glUniformMatrix4fv(projLoc, 1, GL_FALSE, value_ptr(proj));
        glBindVertexArray(sphereVAO);
        for (auto& p : pts) {
            mat4 model = translate(mat4(1.0f), p.pos);
            model = scale(model, vec3(electron_r));
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, value_ptr(model));
            glUniform4f(colorLoc, p.color.r, p.color.g, p.color.b, p.color.a);
            glDrawArrays(GL_TRIANGLES, 0, sphereVertexCount);
        }
    }

    void setupCallbacks() {
        glfwSetWindowUserPointer(window, &camera);
        glfwSetMouseButtonCallback(window, [](GLFWwindow* win, int btn, int act, int mods) {
            ((Camera*)glfwGetWindowUserPointer(win))->processMouseButton(btn, act, mods, win);
        });
        glfwSetCursorPosCallback(window, [](GLFWwindow* win, double x, double y) {
            ((Camera*)glfwGetWindowUserPointer(win))->processMouseMove(x, y);
        });
        glfwSetScrollCallback(window, [](GLFWwindow* win, double xo, double yo) {
            ((Camera*)glfwGetWindowUserPointer(win))->processScroll(xo, yo);
        });
        glfwSetKeyCallback(window, [](GLFWwindow* win, int key, int sc, int action, int mods) {
            if (!(action == GLFW_PRESS || action == GLFW_REPEAT)) return;
            if (key == GLFW_KEY_T) { N += 50000; generateParticles(N); }
            else if (key == GLFW_KEY_G) { N = (N - 50000 < 10000) ? 10000 : N - 50000; generateParticles(N); }
            else if (key == GLFW_KEY_R) { generateParticles(N); }
            cout << "Particles per electron: " << N << "\n";
        });
    }
};
Engine engine;
// ================= Main ================= //
int main() {
    engine.setupCallbacks();

    cout << "=== Helium 3D Orbital Visualizer ===\n";
    cout << "Z_eff = " << Z_eff << "\n";
    cout << "Both electrons in 1s orbital (n=1, l=0, m=0)\n";
    cout << "Orange/red = Electron 1 | Cyan/blue = Electron 2\n";
    cout << "Controls: Drag=rotate, Scroll=zoom, T/G=more/fewer particles, R=resample\n\n";

    generateParticles(N);

    while (!glfwWindowShouldClose(engine.window)) {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        engine.drawParticles(particles_e1);
        engine.drawParticles(particles_e2);

        glfwSwapBuffers(engine.window);
        glfwPollEvents();
    }

    glfwDestroyWindow(engine.window);
    glfwTerminate();
    return 0;
}