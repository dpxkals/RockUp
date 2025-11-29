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
    FALLING,    // 바닥 열리고 낙하 중 (터널 보임)
    PLAYING,    // 바닥 착지 후 게임 시작 (로비/터널 사라짐)
    CLEAR       // 게임 클리어
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
    bool isDoor = false; // 바닥이 문 역할을 함
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

Player rock;
int playerShapeIndex = -1;
bool keyState[256] = { false };
bool isDoorOpen = false; // 바닥 열림 상태

// 맵 설정
const int MAP_WIDTH = 80;
const int MAP_HEIGHT = 150;
const int MAP_DEPTH = 80;

// 맵 고정용 시드값
unsigned int mapSeed = 777;

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
void ResetGame();

void main(int argc, char** argv)
{
    // 랜덤 시드
    //srand((unsigned int)time(NULL));
    // 고정 랜덤 시드
    srand(mapSeed);

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);
    glutInitWindowPosition(100, 100);
    glutInitWindowSize(g_width, g_height);
    glutCreateWindow("ROCK UP - Jump to Fall");

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
    rock.Reset();
    isDoorOpen = false; // 바닥 닫기

    srand(mapSeed);

    mapShapes.clear();
    mapBlocks.clear();

    // 로비 바닥(문) 위치 원상복구
    for (auto& s : lobbyShapes) {
        if (s.isDoor) {
            s.x = s.initX;
            s.y = s.initY;
            s.z = s.initZ;
        }
    }

    cameraYaw = 270.0f;
    cameraPitch = 20.0f;
}

// --- 콜백 함수들 ---
GLvoid Keyboard(unsigned char key, int x, int y) {
    keyState[key] = true;
    if (key == 'q' || key == 'Q') exit(0);
    if (key == 'r' || key == 'R') ResetGame();

    if (key == ' ') {
        if (rock.isGrounded) {
            float speed = sqrt(rock.velocity.x * rock.velocity.x + rock.velocity.z * rock.velocity.z);
            float bonus = speed * 1.2f;
            rock.velocity.y = rock.jumpForce + bonus;

            // [수정] 로비에서 점프하면 바닥 열림 트리거 작동
            if (currentState == LOBBY) {
                isDoorOpen = true;
            }
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

// --- 로비 생성 (수정됨: 바닥 갈라짐) ---
void GenerateLobby() {
    float lobbyY = 200.0f;
    float size = 20.0f;
    float thickness = 1.0f;

    // 1. 바닥을 좌우 두 개로 분할 (문 역할)
    // 왼쪽 바닥
    Shape* floorL = ShapeSave(lobbyShapes, 'c', 0.3f, 0.3f, 0.3f, size / 2, thickness, size);
    floorL->x = -size / 2; floorL->y = lobbyY - size; floorL->z = 0;
    floorL->initX = floorL->x; floorL->initY = floorL->y; floorL->initZ = floorL->z;
    floorL->isDoor = true;
    floorL->doorDirection = -1;  // 왼쪽으로 이동
    lobbyBlocks.push_back({ glm::vec3(-size / 2, lobbyY - size, 0), glm::vec3(size / 2, thickness, size) });

    // 오른쪽 바닥
    Shape* floorR = ShapeSave(lobbyShapes, 'c', 0.3f, 0.3f, 0.3f, size / 2, thickness, size);
    floorR->x = size / 2; floorR->y = lobbyY - size; floorR->z = 0;
    floorR->initX = floorR->x; floorR->initY = floorR->y; floorR->initZ = floorR->z;
    floorR->isDoor = true;
    floorR->doorDirection = 1;   // 오른쪽으로 이동
    lobbyBlocks.push_back({ glm::vec3(size / 2, lobbyY - size, 0), glm::vec3(size / 2, thickness, size) });

    // 2. 천장
    Shape* ceil = ShapeSave(lobbyShapes, 'c', 0.3f, 0.3f, 0.3f, size, thickness, size);
    ceil->x = 0; ceil->y = lobbyY + size; ceil->z = 0;
    lobbyBlocks.push_back({ glm::vec3(0, lobbyY + size, 0), glm::vec3(size, thickness, size) });

    // 3. 4면 벽 (앞문 제거하고 벽으로 막음)
    Shape* back = ShapeSave(lobbyShapes, 'c', 0.4f, 0.4f, 0.4f, size, size, thickness);
    back->x = 0; back->y = lobbyY; back->z = -size;
    lobbyBlocks.push_back({ glm::vec3(0, lobbyY, -size), glm::vec3(size, size, thickness) });

    Shape* front = ShapeSave(lobbyShapes, 'c', 0.4f, 0.4f, 0.4f, size, size, thickness);
    front->x = 0; front->y = lobbyY; front->z = size;
    lobbyBlocks.push_back({ glm::vec3(0, lobbyY, size), glm::vec3(size, size, thickness) });

    Shape* left = ShapeSave(lobbyShapes, 'c', 0.4f, 0.4f, 0.4f, thickness, size, size);
    left->x = -size; left->y = lobbyY; left->z = 0;
    lobbyBlocks.push_back({ glm::vec3(-size, lobbyY, 0), glm::vec3(thickness, size, size) });

    Shape* right = ShapeSave(lobbyShapes, 'c', 0.4f, 0.4f, 0.4f, thickness, size, size);
    right->x = size; right->y = lobbyY; right->z = 0;
    lobbyBlocks.push_back({ glm::vec3(size, lobbyY, 0), glm::vec3(thickness, size, size) });

    // 4. 낙하 터널 (시각 효과)
    float shaftHeight = 230.0f;
    float segmentH = 20.0f;
    float shaftR = size + 5.0f;

    for (float y = 0; y < shaftHeight; y += segmentH) {
        float g = (int(y / segmentH) % 2 == 0) ? 0.2f : 0.4f;
        Shape* w1 = ShapeSave(lobbyShapes, 'c', g, g, g, shaftR, segmentH / 2, thickness);
        w1->x = 0; w1->y = y - 20.0f; w1->z = shaftR;
        Shape* w2 = ShapeSave(lobbyShapes, 'c', g, g, g, shaftR, segmentH / 2, thickness);
        w2->x = 0; w2->y = y - 20.0f; w2->z = -shaftR;
        Shape* w3 = ShapeSave(lobbyShapes, 'c', g - 0.1f, g - 0.1f, g - 0.1f, thickness, segmentH / 2, shaftR);
        w3->x = -shaftR; w3->y = y - 20.0f; w3->z = 0;
        Shape* w4 = ShapeSave(lobbyShapes, 'c', g - 0.1f, g - 0.1f, g - 0.1f, thickness, segmentH / 2, shaftR);
        w4->x = shaftR; w4->y = y - 20.0f; w4->z = 0;
    }
}

// --- 게임 맵 생성 ---
void GenerateMap() {
    float floorSize = MAP_WIDTH / 2.0f;
    Shape* s = ShapeSave(mapShapes, 'c', 0.2f, 0.8f, 0.2f, floorSize, 1.0f, floorSize);
    s->x = 0.0f; s->y = -2.0f; s->z = 0.0f;
    mapBlocks.push_back({ glm::vec3(0, -2.0f, 0), glm::vec3(floorSize, 1.0f, floorSize) });

    float wallHeight = MAP_HEIGHT + 100.0f;
    float wallT = 10.0f;
    float offset = floorSize + wallT;

    mapBlocks.push_back({ glm::vec3(offset, wallHeight / 2, 0), glm::vec3(wallT, wallHeight, floorSize) });
    mapBlocks.push_back({ glm::vec3(-offset, wallHeight / 2, 0), glm::vec3(wallT, wallHeight, floorSize) });
    mapBlocks.push_back({ glm::vec3(0, wallHeight / 2, offset), glm::vec3(floorSize, wallHeight, wallT) });
    mapBlocks.push_back({ glm::vec3(0, wallHeight / 2, -offset), glm::vec3(floorSize, wallHeight, wallT) });

    float bgDist = 300.0f;
    float bgSize = 400.0f;
    float bgT = 1.0f;
    Shape* bg1 = ShapeSave(mapShapes, 'c', 0.4f, 0.5f, 0.6f, bgT, bgSize, bgSize); bg1->x = bgDist; bg1->y = 100; bg1->z = 0;
    Shape* bg2 = ShapeSave(mapShapes, 'c', 0.4f, 0.5f, 0.6f, bgT, bgSize, bgSize); bg2->x = -bgDist; bg2->y = 100; bg2->z = 0;
    Shape* bg3 = ShapeSave(mapShapes, 'c', 0.4f, 0.5f, 0.6f, bgSize, bgSize, bgT); bg3->x = 0; bg3->y = 100; bg3->z = bgDist;
    Shape* bg4 = ShapeSave(mapShapes, 'c', 0.4f, 0.5f, 0.6f, bgSize, bgSize, bgT); bg4->x = 0; bg4->y = 100; bg4->z = -bgDist;

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

    rock.velocity.y -= 0.012f;
    glm::vec3 nextPos = rock.position + rock.velocity;
    rock.isGrounded = false;

    if (currentState == LOBBY) {
        // [수정] 바닥 열림 애니메이션
        if (isDoorOpen) {
            for (auto& s : lobbyShapes) {
                if (s.isDoor) {
                    s.x += s.doorDirection * 0.3f; // 바닥이 양옆으로 이동
                }
            }
        }

        if (nextPos.y > 175.0f) {
            for (const auto& block : lobbyBlocks) {
                // [수정] 바닥이 열리면 바닥(y < 190 근처) 충돌 무시 -> 추락
                if (isDoorOpen && block.first.y < 190.0f) continue;

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
            currentState = FALLING; // 떨어짐
        }
    }
    else if (currentState == FALLING) {
        if (nextPos.y < 5.0f) {
            currentState = PLAYING; // 게임 시작
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

        // 정상 도달 체크
        if (rock.position.y > 450.0f) {
            currentState = CLEAR;
            printf("게임 클리어! 축하합니다!\n");
        }

        if (nextPos.y < -15.0f) {
            rock.position = glm::vec3(0, 5.0f, 0); rock.velocity = glm::vec3(0, 0, 0);
        }
    }
    else if (currentState == CLEAR) {
        rock.velocity = glm::vec3(0, 0, 0); // 공중 부양 (멈춤)
        rock.position.y += 0.1f; // 천천히 승천하는 연출
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
    if (currentState == CLEAR) {
        glClearColor(1.0f, 0.84f, 0.0f, 1.0f); // 황금색 배경 (승리!)
    }
    else {
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f); // 기본 어두운 배경
    }
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

    drawList(shapes);

    if (currentState == LOBBY || currentState == FALLING) {
        drawList(lobbyShapes);
    }

    if (currentState == PLAYING) {
        drawList(mapShapes);
    }

    glBindVertexArray(0);
    glutSwapBuffers();
}

void TimerFunction(int value) {
    UpdatePhysics();
    glutPostRedisplay();
    glutTimerFunc(16, TimerFunction, 0);
}

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