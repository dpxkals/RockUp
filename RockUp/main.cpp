#define _CRT_SECURE_NO_WARNINGS
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <vector>
#include <string>
#include <time.h> 
#include <algorithm>
#include <cmath> 

#pragma comment(lib, "glew32.lib")
#pragma comment (lib, "freeglut.lib")

#include <gl/glew.h>
#include <gl/freeglut.h>
#include <gl/freeglut_ext.h>
#include <gl/glm/glm.hpp>
#include <gl/glm/ext.hpp>
#include <gl/glm/gtc/matrix_transform.hpp>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// --- 게임 상태 정의 ---
enum GameState {
    LOBBY,      // 메인 로비 (박스 안)
    FALLING,    // 문 열리고 낙하 중 (터널 보임)
    PLAYING     // 바닥 착지 후 게임 시작 (로비/터널 사라짐)
};

GameState currentState = LOBBY;

// --- 구조체 정의 ---
struct Shape {
    GLuint VAO, VBO, CBO, NBO;
    GLenum primitiveType;
    int vertexCount;
    float color[3];
    std::vector<float> vertices;
    std::vector<float> normals;
    std::vector<float> colors;

    GLfloat x = 0.0f, y = 0.0f, z = 0.0f;
    // 초기 위치 저장용 (리셋 시 사용)
    GLfloat initX = 0.0f, initY = 0.0f, initZ = 0.0f;

    char shapeType = ' ';
    bool isDoor = false;
    int doorDirection = 0;
};

struct Player {
    glm::vec3 position;
    glm::vec3 velocity;
    float radius;
    bool isGrounded;

    float acceleration;
    float maxSpeed;
    float friction;
    float jumpForce;

    Player() {
        Reset();
    }

    void Reset() {
        position = glm::vec3(0.0f, 205.0f, 0.0f); // 로비 시작 위치
        velocity = glm::vec3(0.0f, 0.0f, 0.0f);
        radius = 1.2f;
        isGrounded = false;
        acceleration = 0.008f;
        maxSpeed = 0.3f;
        friction = 0.96f;
        jumpForce = 0.45f;
    }
};

// --- 전역 변수 ---
GLint g_width = 1200, g_height = 1200;
GLuint shaderProgramID;
GLuint vertexShader, fragmentShader;

std::vector<Shape> shapes;         // 플레이어
std::vector<Shape> lobbyShapes;    // 로비 + 터널
std::vector<Shape> mapShapes;      // 게임 맵

std::vector<std::pair<glm::vec3, glm::vec3>> mapBlocks;
std::vector<std::pair<glm::vec3, glm::vec3>> lobbyBlocks;

// 카메라 및 조명
glm::vec3 cameraPos = glm::vec3(0.0f, 5.0f, 10.0f);
glm::vec3 cameraTarget = glm::vec3(0.0f, 0.0f, 0.0f);
glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
bool isPerspective = true;

float cameraYaw = 270.0f;
float cameraPitch = 20.0f;
float cameraDistance = 9.0f;
bool isDragging = false;
int lastMouseX = 0, lastMouseY = 0;
float mouseSensitivity = 0.3f;

float lightRadius = 50.0f;
float lightAngle = 0.0f;
float lightHeight = 50.0f;

Player rock;
int playerShapeIndex = -1;
bool keyState[256] = { false };
bool isDoorOpen = false;

// 맵 설정
const int MAP_WIDTH = 80;
const int MAP_HEIGHT = 150;
const int MAP_DEPTH = 80;

// --- 함수 선언 ---
void make_vertexShaders();
void make_fragmentShaders();
GLuint make_shaderProgram();
void setupShapeBuffers(Shape& shape, const std::vector<float>& vertices, const std::vector<float>& colors, const std::vector<float>& normals);
GLvoid drawScene();
GLvoid Reshape(int w, int h);
GLvoid Keyboard(unsigned char key, int x, int y);
GLvoid KeyboardUp(unsigned char key, int x, int y);
void Mouse(int button, int state, int x, int y);
void Motion(int x, int y);
void TimerFunction(int value);
char* filetobuf(const char* file);
Shape* ShapeSave(std::vector<Shape>& shapeVector, char shapeKey, float r, float g, float b, float sx, float sy, float sz);
void GenerateMap();
void GenerateLobby();
void UpdatePhysics();
void ResetGame(); // 게임 리셋 함수

void main(int argc, char** argv)
{
    srand((unsigned int)time(NULL));

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);
    glutInitWindowPosition(100, 100);
    glutInitWindowSize(g_width, g_height);
    glutCreateWindow("ROCK UP - Visual Fall & Reset");

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) return;

    make_vertexShaders();
    make_fragmentShaders();
    shaderProgramID = make_shaderProgram();

    glutDisplayFunc(drawScene);
    glutReshapeFunc(Reshape);
    glutKeyboardFunc(Keyboard);
    glutKeyboardUpFunc(KeyboardUp);
    glutMouseFunc(Mouse);
    glutMotionFunc(Motion);

    GenerateLobby();

    Shape* pShape = ShapeSave(shapes, '1', 1.0f, 0.2f, 0.2f, rock.radius, rock.radius, rock.radius);
    playerShapeIndex = shapes.size() - 1;

    glutTimerFunc(16, TimerFunction, 0);
    glutMainLoop();
}

// --- 리셋 함수 ---
void ResetGame() {
    currentState = LOBBY;
    rock.Reset(); // 플레이어 위치 초기화
    isDoorOpen = false; // 문 닫기

    // 맵 데이터 초기화
    mapShapes.clear();
    mapBlocks.clear();

    // 로비 문 위치 원상복구
    for (auto& s : lobbyShapes) {
        if (s.isDoor) {
            s.x = s.initX;
            s.y = s.initY;
            s.z = s.initZ;
        }
    }

    // 카메라 초기화 (선택 사항)
    cameraYaw = 270.0f;
    cameraPitch = 20.0f;
}

// --- 콜백 함수들 ---
GLvoid Keyboard(unsigned char key, int x, int y) {
    keyState[key] = true;
    if (key == 'q' || key == 'Q') exit(0);

    // [추가] R 키로 리셋
    if (key == 'r' || key == 'R') {
        ResetGame();
    }

    if (key == ' ') {
        if (rock.isGrounded) {
            float speed = sqrt(rock.velocity.x * rock.velocity.x + rock.velocity.z * rock.velocity.z);
            float bonus = speed * 1.2f;
            rock.velocity.y = rock.jumpForce + bonus;
        }
    }
}

GLvoid KeyboardUp(unsigned char key, int x, int y) {
    keyState[key] = false;
}

void Mouse(int button, int state, int x, int y) {
    if (button == GLUT_LEFT_BUTTON) {
        if (state == GLUT_DOWN) {
            isDragging = true;
            lastMouseX = x; lastMouseY = y;
        }
        else if (state == GLUT_UP) {
            isDragging = false;
        }
    }
}

void Motion(int x, int y) {
    if (isDragging) {
        int dx = x - lastMouseX;
        cameraYaw -= dx * mouseSensitivity;
        lastMouseX = x; lastMouseY = y;
        glutPostRedisplay();
    }
}

// --- 로비 및 터널 생성 ---
void GenerateLobby() {
    float lobbyY = 200.0f;
    float size = 20.0f;
    float thickness = 1.0f;

    // 1. 로비 본체
    Shape* floor = ShapeSave(lobbyShapes, 'c', 0.3f, 0.3f, 0.3f, size, thickness, size);
    floor->x = 0; floor->y = lobbyY - size; floor->z = 0;
    lobbyBlocks.push_back({ glm::vec3(0, lobbyY - size, 0), glm::vec3(size, thickness, size) });

    Shape* ceil = ShapeSave(lobbyShapes, 'c', 0.3f, 0.3f, 0.3f, size, thickness, size);
    ceil->x = 0; ceil->y = lobbyY + size; ceil->z = 0;
    lobbyBlocks.push_back({ glm::vec3(0, lobbyY + size, 0), glm::vec3(size, thickness, size) });

    Shape* back = ShapeSave(lobbyShapes, 'c', 0.4f, 0.4f, 0.4f, size, size, thickness);
    back->x = 0; back->y = lobbyY; back->z = -size;
    lobbyBlocks.push_back({ glm::vec3(0, lobbyY, -size), glm::vec3(size, size, thickness) });

    Shape* left = ShapeSave(lobbyShapes, 'c', 0.4f, 0.4f, 0.4f, thickness, size, size);
    left->x = -size; left->y = lobbyY; left->z = 0;
    lobbyBlocks.push_back({ glm::vec3(-size, lobbyY, 0), glm::vec3(thickness, size, size) });

    Shape* right = ShapeSave(lobbyShapes, 'c', 0.4f, 0.4f, 0.4f, thickness, size, size);
    right->x = size; right->y = lobbyY; right->z = 0;
    lobbyBlocks.push_back({ glm::vec3(size, lobbyY, 0), glm::vec3(thickness, size, size) });

    // 앞문 (초기 위치 저장 필수)
    Shape* doorL = ShapeSave(lobbyShapes, 'c', 0.6f, 0.3f, 0.1f, size / 2, size, thickness);
    doorL->x = -size / 2; doorL->y = lobbyY; doorL->z = size;
    doorL->initX = doorL->x; doorL->initY = doorL->y; doorL->initZ = doorL->z; // 초기 위치 저장
    doorL->isDoor = true; doorL->doorDirection = -1;

    Shape* doorR = ShapeSave(lobbyShapes, 'c', 0.6f, 0.3f, 0.1f, size / 2, size, thickness);
    doorR->x = size / 2; doorR->y = lobbyY; doorR->z = size;
    doorR->initX = doorR->x; doorR->initY = doorR->y; doorR->initZ = doorR->z; // 초기 위치 저장
    doorR->isDoor = true; doorR->doorDirection = 1;

    // 2. [부활] 낙하 터널 (Falling Shaft) - 시각 효과용
    // 로비 바닥 밑으로 길게 뻗은 줄무늬 벽을 생성하여 떨어지는 느낌을 줌
    float shaftHeight = 200.0f;
    float segmentH = 20.0f;
    float shaftR = size + 5.0f; // 로비보다 약간 넓게

    for (float y = 0; y < shaftHeight; y += segmentH) {
        float g = (int(y / segmentH) % 2 == 0) ? 0.2f : 0.4f; // 어두운 톤 교차

        // 터널 벽은 충돌 판정(lobbyBlocks)에 넣지 않음 -> 자유 낙하 유도
        // 혹은 필요하다면 밖으로 튀지 않게 넣을 수도 있음

        // 앞
        Shape* w1 = ShapeSave(lobbyShapes, 'c', g, g, g, shaftR, segmentH / 2, thickness);
        w1->x = 0; w1->y = y - 20.0f; w1->z = shaftR; // y위치 조정하여 바닥까지 닿게

        // 뒤
        Shape* w2 = ShapeSave(lobbyShapes, 'c', g, g, g, shaftR, segmentH / 2, thickness);
        w2->x = 0; w2->y = y - 20.0f; w2->z = -shaftR;

        // 좌
        Shape* w3 = ShapeSave(lobbyShapes, 'c', g - 0.1f, g - 0.1f, g - 0.1f, thickness, segmentH / 2, shaftR);
        w3->x = -shaftR; w3->y = y - 20.0f; w3->z = 0;

        // 우
        Shape* w4 = ShapeSave(lobbyShapes, 'c', g - 0.1f, g - 0.1f, g - 0.1f, thickness, segmentH / 2, shaftR);
        w4->x = shaftR; w4->y = y - 20.0f; w4->z = 0;

        // [중요] 터널 벽도 lobbyShapes에 포함되어 있으므로, PLAYING 상태가 되면 자동으로 사라짐
    }
}

// --- 게임 맵 생성 ---
void GenerateMap() {
    float floorSize = MAP_WIDTH / 2.0f;
    Shape* s = ShapeSave(mapShapes, 'c', 0.2f, 0.8f, 0.2f, floorSize, 1.0f, floorSize);
    s->x = 0.0f; s->y = -2.0f; s->z = 0.0f;
    mapBlocks.push_back({ glm::vec3(0, -2.0f, 0), glm::vec3(floorSize, 1.0f, floorSize) });

    // 보이지 않는 충돌 벽
    float wallHeight = MAP_HEIGHT + 100.0f;
    float wallT = 10.0f;
    float offset = floorSize + wallT;

    mapBlocks.push_back({ glm::vec3(offset, wallHeight / 2, 0), glm::vec3(wallT, wallHeight, floorSize) });
    mapBlocks.push_back({ glm::vec3(-offset, wallHeight / 2, 0), glm::vec3(wallT, wallHeight, floorSize) });
    mapBlocks.push_back({ glm::vec3(0, wallHeight / 2, offset), glm::vec3(floorSize, wallHeight, wallT) });
    mapBlocks.push_back({ glm::vec3(0, wallHeight / 2, -offset), glm::vec3(floorSize, wallHeight, wallT) });

    // 배경 벽 (원거리)
    float bgDist = 300.0f;
    float bgSize = 400.0f;
    float bgT = 1.0f;
    Shape* bg1 = ShapeSave(mapShapes, 'c', 0.4f, 0.5f, 0.6f, bgT, bgSize, bgSize); bg1->x = bgDist; bg1->y = 100; bg1->z = 0;
    Shape* bg2 = ShapeSave(mapShapes, 'c', 0.4f, 0.5f, 0.6f, bgT, bgSize, bgSize); bg2->x = -bgDist; bg2->y = 100; bg2->z = 0;
    Shape* bg3 = ShapeSave(mapShapes, 'c', 0.4f, 0.5f, 0.6f, bgSize, bgSize, bgT); bg3->x = 0; bg3->y = 100; bg3->z = bgDist;
    Shape* bg4 = ShapeSave(mapShapes, 'c', 0.4f, 0.5f, 0.6f, bgSize, bgSize, bgT); bg4->x = 0; bg4->y = 100; bg4->z = -bgDist;

    // 발판
    float range = (MAP_WIDTH / 2.0f) - 5.0f;
    for (int y = 0; y < MAP_HEIGHT; ++y) {
        if (y % 2 != 0) continue;
        int blocks = (rand() % 2) + 1;
        for (int i = 0; i < blocks; ++i) {
            float nextX = ((rand() % 100) / 100.0f * (range * 2)) - range;
            float nextZ = ((rand() % 100) / 100.0f * (range * 2)) - range;
            float sx = 4.0f + (rand() % 30) / 10.0f;
            float sz = 4.0f + (rand() % 30) / 10.0f;

            float cVal = (float)y / MAP_HEIGHT;
            Shape* p = ShapeSave(mapShapes, 'c', cVal, 0.6f, 1.0f - cVal, sx, 0.5f, sz);
            p->x = nextX; p->y = (float)y * 3.0f; p->z = nextZ;
            mapBlocks.push_back({ glm::vec3(nextX, p->y, nextZ), glm::vec3(sx, 0.5f, sz) });
        }
    }
}

bool CheckCollision(glm::vec3 spherePos, float radius, glm::vec3 boxPos, glm::vec3 boxSize) {
    float x = std::max(boxPos.x - boxSize.x, std::min(spherePos.x, boxPos.x + boxSize.x));
    float y = std::max(boxPos.y - boxSize.y, std::min(spherePos.y, boxPos.y + boxSize.y));
    float z = std::max(boxPos.z - boxSize.z, std::min(spherePos.z, boxPos.z + boxSize.z));
    float distance = sqrt(pow(x - spherePos.x, 2) + pow(y - spherePos.y, 2) + pow(z - spherePos.z, 2));
    return distance < radius;
}

void UpdatePhysics() {
    glm::vec3 viewDir = rock.position - cameraPos;
    viewDir.y = 0.0f;
    glm::vec3 fwd = glm::normalize(viewDir);
    glm::vec3 right = glm::normalize(glm::cross(fwd, glm::vec3(0, 1, 0)));

    if (keyState['w']) { rock.velocity.x += fwd.x * rock.acceleration; rock.velocity.z += fwd.z * rock.acceleration; }
    if (keyState['s']) { rock.velocity.x -= fwd.x * rock.acceleration; rock.velocity.z -= fwd.z * rock.acceleration; }
    if (keyState['d']) { rock.velocity.x += right.x * rock.acceleration; rock.velocity.z += right.z * rock.acceleration; }
    if (keyState['a']) { rock.velocity.x -= right.x * rock.acceleration; rock.velocity.z -= right.z * rock.acceleration; }

    float speedSq = rock.velocity.x * rock.velocity.x + rock.velocity.z * rock.velocity.z;
    if (speedSq > rock.maxSpeed * rock.maxSpeed) {
        float scale = rock.maxSpeed / sqrt(speedSq);
        rock.velocity.x *= scale; rock.velocity.z *= scale;
    }

    rock.velocity.y -= 0.012f; // 중력
    glm::vec3 nextPos = rock.position + rock.velocity;
    rock.isGrounded = false;

    if (currentState == LOBBY) {
        if (rock.position.z > 10.0f && !isDoorOpen) {
            isDoorOpen = true;
        }

        if (isDoorOpen) {
            for (auto& s : lobbyShapes) {
                if (s.isDoor) {
                    s.x += s.doorDirection * 0.15f;
                }
            }
        }

        if (nextPos.y > 175.0f) { // 로비 바닥권
            for (const auto& block : lobbyBlocks) {
                if (isDoorOpen && block.first.z > 15.0f && block.first.y > 180.0f) continue;

                if (CheckCollision(nextPos, rock.radius, block.first, block.second)) {
                    if (rock.position.y > block.first.y + block.second.y && rock.velocity.y < 0) {
                        rock.isGrounded = true; rock.velocity.y = 0;
                        nextPos.y = block.first.y + block.second.y + rock.radius;
                    }
                    else {
                        rock.velocity.x *= -0.5f; rock.velocity.z *= -0.5f; nextPos = rock.position;
                    }
                }
            }
        }
        else {
            currentState = FALLING; // 떨어지기 시작하면 FALLING 상태
        }
    }
    else if (currentState == FALLING) {
        // 낙하 중에는 터널 벽(시각적)이 보이지만, 충돌 체크는 하지 않음 (또는 필요시 추가)
        if (nextPos.y < 5.0f) {
            currentState = PLAYING; // 바닥 도착 -> PLAYING 상태 (로비/터널 사라짐)
            GenerateMap();
            rock.velocity.y *= 0.5f;
        }
    }
    else if (currentState == PLAYING) {
        for (const auto& block : mapBlocks) {
            if (CheckCollision(nextPos, rock.radius, block.first, block.second)) {
                if (rock.position.y > block.first.y + block.second.y && rock.velocity.y < 0) {
                    rock.isGrounded = true; rock.velocity.y = 0;
                    nextPos.y = block.first.y + block.second.y + rock.radius;
                }
                else {
                    rock.velocity.x *= -0.8f; rock.velocity.z *= -0.8f; nextPos = rock.position;
                }
            }
        }
        if (nextPos.y < -15.0f) {
            // 게임 중 추락 시 시작점으로 이동 (완전 초기화는 아님)
            rock.position = glm::vec3(0, 5.0f, 0); rock.velocity = glm::vec3(0, 0, 0);
        }
    }

    rock.position = nextPos;

    if (rock.isGrounded) { rock.velocity.x *= rock.friction; rock.velocity.z *= rock.friction; }
    else { rock.velocity.x *= 0.995f; rock.velocity.z *= 0.995f; }

    if (playerShapeIndex != -1) {
        shapes[playerShapeIndex].x = rock.position.x;
        shapes[playerShapeIndex].y = rock.position.y;
        shapes[playerShapeIndex].z = rock.position.z;
    }

    float cx = cos(glm::radians(cameraYaw)) * cos(glm::radians(cameraPitch));
    float cy = sin(glm::radians(cameraPitch));
    float cz = sin(glm::radians(cameraYaw)) * cos(glm::radians(cameraPitch));
    cameraPos = rock.position + glm::vec3(cx, cy, cz) * cameraDistance;
    cameraTarget = rock.position;
}

GLvoid drawScene() {
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(shaderProgramID);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    unsigned int modelLoc = glGetUniformLocation(shaderProgramID, "model");
    unsigned int viewLoc = glGetUniformLocation(shaderProgramID, "view");
    unsigned int projLoc = glGetUniformLocation(shaderProgramID, "projection");

    glm::vec3 lightPos(rock.position.x, rock.position.y + 50.0f, rock.position.z);
    glUniform3f(glGetUniformLocation(shaderProgramID, "lightPos"), lightPos.x, lightPos.y, lightPos.z);
    glUniform3f(glGetUniformLocation(shaderProgramID, "viewPos"), cameraPos.x, cameraPos.y, cameraPos.z);
    glUniform3f(glGetUniformLocation(shaderProgramID, "lightColor"), 1.0f, 1.0f, 1.0f);

    glm::mat4 view = glm::lookAt(cameraPos, cameraTarget, cameraUp);
    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, &view[0][0]);

    glm::mat4 proj;
    if (isPerspective) proj = glm::perspective(glm::radians(60.0f), (float)g_width / g_height, 0.1f, 1000.0f);
    else { float s = 40.0f; float a = (float)g_width / g_height; proj = glm::ortho(-s * a, s * a, -s, s, 0.1f, 1000.0f); }
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, &proj[0][0]);

    auto drawList = [&](std::vector<Shape>& list) {
        for (auto& s : list) {
            glUniform3fv(glGetUniformLocation(shaderProgramID, "objectColor"), 1, s.color);
            glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(s.x, s.y, s.z));
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, &model[0][0]);
            glBindVertexArray(s.VAO); glDrawArrays(s.primitiveType, 0, s.vertexCount);
        }
        };

    drawList(shapes); // 플레이어 항상 그림

    // [조건부 렌더링]
    if (currentState == LOBBY || currentState == FALLING) {
        drawList(lobbyShapes); // 로비 & 터널 그림
    }

    if (currentState == PLAYING) {
        drawList(mapShapes);   // 게임 맵 그림
    }

    glBindVertexArray(0);
    glutSwapBuffers();
}

void TimerFunction(int value) {
    UpdatePhysics();
    glutPostRedisplay();
    glutTimerFunc(16, TimerFunction, 0);
}

// --- 유틸리티 함수들 ---
GLvoid Reshape(int w, int h) { g_width = w; g_height = h; glViewport(0, 0, w, h); }
char* filetobuf(const char* file) {
    FILE* f = fopen(file, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long len = ftell(f); char* buf = (char*)malloc(len + 1);
    fseek(f, 0, SEEK_SET); fread(buf, len, 1, f); fclose(f); buf[len] = 0; return buf;
}
void make_vertexShaders() {
    GLchar* src = filetobuf("vertex.glsl"); vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &src, NULL); glCompileShader(vertexShader);
}
void make_fragmentShaders() {
    GLchar* src = filetobuf("fragment.glsl"); fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &src, NULL); glCompileShader(fragmentShader);
}
GLuint make_shaderProgram() {
    GLuint id = glCreateProgram(); glAttachShader(id, vertexShader); glAttachShader(id, fragmentShader);
    glLinkProgram(id); glDeleteShader(vertexShader); glDeleteShader(fragmentShader); return id;
}
void setupShapeBuffers(Shape& s, const std::vector<float>& v, const std::vector<float>& c, const std::vector<float>& n) {
    glGenVertexArrays(1, &s.VAO); glGenBuffers(1, &s.VBO); glGenBuffers(1, &s.CBO); glGenBuffers(1, &s.NBO);
    glBindVertexArray(s.VAO);
    glBindBuffer(GL_ARRAY_BUFFER, s.VBO); glBufferData(GL_ARRAY_BUFFER, v.size() * 4, v.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0); glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, s.NBO); glBufferData(GL_ARRAY_BUFFER, n.size() * 4, n.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, 0); glEnableVertexAttribArray(1);
    glBindBuffer(GL_ARRAY_BUFFER, s.CBO); glBufferData(GL_ARRAY_BUFFER, c.size() * 4, c.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 0, 0); glEnableVertexAttribArray(2);
    glBindVertexArray(0);
}
Shape* ShapeSave(std::vector<Shape>& list, char key, float r, float g, float b, float sx, float sy, float sz) {
    Shape s; s.color[0] = r; s.color[1] = g; s.color[2] = b; s.shapeType = key; s.primitiveType = GL_TRIANGLES;
    if (key == 'c') {
        float x = sx, y = sy, z = sz;
        s.vertices = { -x,-y,z, x,-y,z, x,y,z, -x,-y,z, x,y,z, -x,y,z, -x,-y,-z, -x,y,-z, x,y,-z, -x,-y,-z, x,y,-z, x,-y,-z, -x,-y,-z, -x,-y,z, -x,y,z, -x,-y,-z, -x,y,z, -x,y,-z, x,-y,z, x,-y,-z, x,y,-z, x,-y,z, x,y,-z, x,y,z, -x,y,z, x,y,z, x,y,-z, -x,y,z, x,y,-z, -x,y,-z, -x,-y,-z, x,-y,-z, x,-y,z, -x,-y,-z, x,-y,z, -x,-y,z };
        s.normals = { 0,0,1, 0,0,1, 0,0,1, 0,0,1, 0,0,1, 0,0,1, 0,0,-1, 0,0,-1, 0,0,-1, 0,0,-1, 0,0,-1, 0,0,-1, -1,0,0, -1,0,0, -1,0,0, -1,0,0, -1,0,0, -1,0,0, 1,0,0, 1,0,0, 1,0,0, 1,0,0, 1,0,0, 1,0,0, 0,1,0, 0,1,0, 0,1,0, 0,1,0, 0,1,0, 0,1,0, 0,-1,0, 0,-1,0, 0,-1,0, 0,-1,0, 0,-1,0, 0,-1,0 };
        s.vertexCount = 36;
    }
    else if (key == '1') {
        int sec = 30, st = 30; float rad = sx; std::vector<float> tv, tn;
        for (int i = 0; i <= st; ++i) {
            float ang = M_PI / 2 - i * M_PI / st, xy = rad * cosf(ang), z = rad * sinf(ang);
            for (int j = 0; j <= sec; ++j) { float sa = j * 2 * M_PI / sec, x = xy * cosf(sa), y = xy * sinf(sa); tv.push_back(x); tv.push_back(y); tv.push_back(z); tn.push_back(x / rad); tn.push_back(y / rad); tn.push_back(z / rad); }
        }
        for (int i = 0; i < st; ++i) {
            int k1 = i * (sec + 1), k2 = k1 + sec + 1; for (int j = 0; j < sec; ++j, ++k1, ++k2) {
                if (i != 0) { s.vertices.insert(s.vertices.end(), { tv[k1 * 3],tv[k1 * 3 + 1],tv[k1 * 3 + 2], tv[k2 * 3],tv[k2 * 3 + 1],tv[k2 * 3 + 2], tv[(k1 + 1) * 3],tv[(k1 + 1) * 3 + 1],tv[(k1 + 1) * 3 + 2] }); s.normals.insert(s.normals.end(), { tn[k1 * 3],tn[k1 * 3 + 1],tn[k1 * 3 + 2], tn[k2 * 3],tn[k2 * 3 + 1],tn[k2 * 3 + 2], tn[(k1 + 1) * 3],tn[(k1 + 1) * 3 + 1],tn[(k1 + 1) * 3 + 2] }); }
                if (i != st - 1) { s.vertices.insert(s.vertices.end(), { tv[(k1 + 1) * 3],tv[(k1 + 1) * 3 + 1],tv[(k1 + 1) * 3 + 2], tv[k2 * 3],tv[k2 * 3 + 1],tv[k2 * 3 + 2], tv[(k2 + 1) * 3],tv[(k2 + 1) * 3 + 1],tv[(k2 + 1) * 3 + 2] }); s.normals.insert(s.normals.end(), { tn[(k1 + 1) * 3],tn[(k1 + 1) * 3 + 1],tn[(k1 + 1) * 3 + 2], tn[k2 * 3],tn[k2 * 3 + 1],tn[k2 * 3 + 2], tn[(k2 + 1) * 3],tn[(k2 + 1) * 3 + 1],tn[(k2 + 1) * 3 + 2] }); }
            }
        }
        s.vertexCount = s.vertices.size() / 3;
    }
    for (int i = 0; i < s.vertexCount; ++i) { s.colors.push_back(r); s.colors.push_back(g); s.colors.push_back(b); }
    setupShapeBuffers(s, s.vertices, s.colors, s.normals); list.push_back(s); return &list.back();
}