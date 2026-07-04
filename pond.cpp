#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

// --- SCREEN SETTINGS ---
const int SCREEN_WIDTH = 80;
const int SCREEN_HEIGHT = 24;

// --- CAMERA SETTINGS ---
const float FOV = 45.0f;
const float CAM_HEIGHT = 25.0f;
const float CAM_Z_OFFSET = 40.0f;
const int HORIZON_ROW = 3;

const int POND_SIZE = 140;

// --- BUFFERS ---
float buf1[POND_SIZE][POND_SIZE] = {0};
float buf2[POND_SIZE][POND_SIZE] = {0};
float (*curr)[POND_SIZE] = buf1;
float (*prev)[POND_SIZE] = buf2;

char screen[SCREEN_HEIGHT][SCREEN_WIDTH];
float depth[SCREEN_HEIGHT][SCREEN_WIDTH];

struct Raindrop {
    float x, y, z, vy;
    bool active;
};
std::vector<Raindrop> drops(12);

// --- 3D ENGINE ---
void draw_point(float wx, float wy, float wz, char ch) {
    float rx = wx;
    float ry = wy - CAM_HEIGHT;
    float rz = wz + CAM_Z_OFFSET;

    if (rz <= 1.0f) return;

    float px = (rx / rz) * FOV * 2.0f;
    float py = -(ry / rz) * FOV;

    int sx = (int)(px + SCREEN_WIDTH / 2);
    int sy = (int)(py + HORIZON_ROW);

    if (sx < 0 || sx >= SCREEN_WIDTH || sy < 0 || sy >= SCREEN_HEIGHT) return;

    if (rz < depth[sy][sx]) {
        depth[sy][sx] = rz;
        screen[sy][sx] = ch;
    }
}

// --- PHYSICS ---
void splash(int cx, int cz, float radius, float force) {
    int r = (int)ceil(radius);
    for (int i = -r; i <= r; i++) {
        for (int j = -r; j <= r; j++) {
            int px = cx + i;
            int pz = cz + j;
            if (px > 1 && px < POND_SIZE - 1 && pz > 1 && pz < POND_SIZE - 1) {
                float dist = sqrt(i * i + j * j);
                if (dist < radius) {
                    float hump = (cos(dist / radius * 3.14159f) + 1.0f) * 0.5f;
                    curr[px][pz] += force * hump;
                }
            }
        }
    }
}

void update_physics() {
    for (int x = 1; x < POND_SIZE - 1; x++) {
        for (int z = 1; z < POND_SIZE - 1; z++) {
            float neighbors = curr[x - 1][z] + curr[x + 1][z] + curr[x][z - 1] + curr[x][z + 1];

            float val = (neighbors / 2.0f) - prev[x][z];
            val = val * 0.92f;  // Damping
            prev[x][z] = val;
        }
    }
    float (*temp)[POND_SIZE] = curr;
    curr = prev;
    prev = temp;
}
void update_rain() {
    for (auto& d : drops) {
        if (!d.active) {
            if (rand() % 100 < 2) {
                d.y = 80 + (rand() % 40);
                d.x = (float)((rand() % 120) - 60);
                d.z = (float)((rand() % 100) - 20);
                d.vy = 12.0f + (rand() % 5);
                d.active = true;
            }
            continue;
        }

        d.y -= d.vy * 0.15f;

        if (d.y <= 0) {
            int gx = (int)(d.x + POND_SIZE / 2);
            int gz = (int)(d.z + POND_SIZE / 2);

            splash(gx, gz, 2.0f, 4.0f);
            d.active = false;
        }
    }
}

// --- RENDER ---
void render() {
    for (int y = 0; y < SCREEN_HEIGHT; y++) {
        for (int x = 0; x < SCREEN_WIDTH; x++) {
            screen[y][x] = ' ';
            depth[y][x] = 10000.0f;
        }
    }

    // Render pond
    for (int x = 0; x < POND_SIZE; x++) {
        for (int z = 0; z < POND_SIZE; z++) {
            if (z > 130) continue;

            float h = curr[x][z];
            char ch = ' ';

            // Surface background:

            // Close up (z < 60): Checkerboard Pattern
            // (x+z)%2 creates diagonal weave
            if (z < 60) {
                if ((x + z) % 2 == 0) ch = '.';
            }
            // Mid Distance (z < 100): Standard Grid
            else if (z < 100) {
                if (x % 2 == 0 && z % 2 == 0) ch = '.';
            }
            // Far Distance (z < 130): Sparse Grid
            else if (z < 130) {
                if (x % 4 == 0 && z % 4 == 0) ch = '.';
            }
            // Wave density:
            // If wave far away skip drawing some pixels.
            if (z > 80 && (x + z) % 2 != 0) {
                // Skip drawing wave detail here to match floor density
            } else {
                // Standard Wave Logic
                if (h > 0.2f) ch = '-';
                if (h > 0.6f) ch = '~';
                if (h > 1.5f) ch = '^';
                if (h < -0.2f) ch = ',';
            }

            // Draw
            if (ch != ' ') {
                draw_point((float)(x - POND_SIZE / 2), h, (float)(z - POND_SIZE / 2), ch);
            }
        }
    }

    // Render rain
    for (const auto& d : drops) {
        if (!d.active) continue;
        if (d.z > 110) continue;

        float tail = d.vy * 0.35f;
        for (float ty = d.y; ty < d.y + tail; ty += 1.0f) {
            if (ty < 0) continue;
            draw_point(d.x, ty, d.z, '|');
        }
    }
}

int main() {
    srand(time(0));
    for (auto& d : drops) d.active = false;

    std::cout << "\033[2J";

    while (true) {
        update_rain();
        update_physics();
        render();

        std::string out = "\033[H";
        for (int y = 0; y < SCREEN_HEIGHT; ++y) {
            for (int x = 0; x < SCREEN_WIDTH; ++x) {
                out += screen[y][x];
            }
            if (y < SCREEN_HEIGHT - 1) out += "\n";
        }
        std::cout << out << std::flush;

        std::this_thread::sleep_for(std::chrono::milliseconds(33));
    }
    return 0;
}
