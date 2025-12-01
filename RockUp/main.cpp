#define _CRT_SECURE_NO_WARNINGS
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <vector>
#include <string>
#include <time.h> 
#include <algorithm>
#include <cmath> 
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h" // stb_image 라이브러리 필요

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
    GLuint VAO, VBO, CBO, NBO, TBO; // [수정] TBO (Texture Buffer) 추가
    GLenum primitiveType;
    int vertexCount;
    float color[3];
    std::vector<float> vertices;
    std::vector<float> normals;
    std::vector<float> colors;
    std::vector<float> uvs; // [추가] 텍스처 좌표 저장

    GLfloat x = 0.0f, y = 0.0f, z = 0.0f;
    // 초기 위치 저장용 (리셋 시 사용)
    GLfloat initX = 0.0f, initY = 0.0f, initZ = 0.0f;

    char shapeType = ' ';
    bool isDoor = false; // 바닥이 문 역할을 함
    int doorDirection = 0;

    bool isObstacle = false;
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
unsigned int mapSeed = 327;

GLuint rockTextureID; // 텍스처 ID 저장용

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

unsigned int loadTexture(const char* path) {
    unsigned int textureID;
    glGenTextures(1, &textureID);

    int width, height, nrComponents;
    unsigned char* data = stbi_load(path, &width, &height, &nrComponents, 0);
    if (data) {
        GLenum format;
        if (nrComponents == 1) format = GL_RED;
        else if (nrComponents == 3) format = GL_RGB;
        else if (nrComponents == 4) format = GL_RGBA;

        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbi_image_free(data);
        printf("Texture loaded: %s\n", path);
    }
    else {
        printf("Texture failed to load at path: %s\n", path);
        stbi_image_free(data);
    }
    return textureID;
}

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

    // [추가] 텍스처 로드 및 유닛 설정
    rockTextureID = loadTexture("rock.png");
    glUseProgram(shaderProgramID);
    glUniform1i(glGetUniformLocation(shaderProgramID, "texture1"), 0); // 텍스처 유닛 0번

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

    // --- 3. 벽면들 ---
    // 뒤 (Z = -size)
    Shape* back = ShapeSave(lobbyShapes, 'c', 0.4f, 0.4f, 0.4f, size, size, thickness);
    back->x = 0; back->y = lobbyY; back->z = -size;
    back->isObstacle = true; // [추가] 사이드뷰에서 가림

    // 앞 (Z = size)
    Shape* front = ShapeSave(lobbyShapes, 'c', 0.4f, 0.4f, 0.4f, size, size, thickness);
    front->x = 0; front->y = lobbyY; front->z = size;
    front->isObstacle = true; // [추가] 사이드뷰에서 가림

    lobbyBlocks.push_back({ glm::vec3(0, lobbyY, -size), glm::vec3(size, size, thickness) });
    lobbyBlocks.push_back({ glm::vec3(0, lobbyY, size), glm::vec3(size, size, thickness) });

    // 좌/우 벽은 놔둡니다 (사이드뷰의 배경이 되어줌)
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

        // 앞뒤를 막는 벽 (Z축 방향 벽) -> 가림 처리
        Shape* w1 = ShapeSave(lobbyShapes, 'c', g, g, g, shaftR, segmentH / 2, thickness);
        w1->x = 0; w1->y = y - 20.0f; w1->z = shaftR;
        w1->isObstacle = true; // [추가]

        Shape* w2 = ShapeSave(lobbyShapes, 'c', g, g, g, shaftR, segmentH / 2, thickness);
        w2->x = 0; w2->y = y - 20.0f; w2->z = -shaftR;
        w2->isObstacle = true; // [추가]

        // 좌우 벽은 놔둠
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
    bg1->isObstacle = true;
    Shape* bg2 = ShapeSave(mapShapes, 'c', 0.4f, 0.5f, 0.6f, bgT, bgSize, bgSize); bg2->x = -bgDist; bg2->y = 100; bg2->z = 0;
    bg2->isObstacle = true;
    Shape* bg3 = ShapeSave(mapShapes, 'c', 0.4f, 0.5f, 0.6f, bgSize, bgSize, bgT); bg3->x = 0; bg3->y = 100; bg3->z = bgDist;
    bg3->isObstacle = true;
    Shape* bg4 = ShapeSave(mapShapes, 'c', 0.4f, 0.5f, 0.6f, bgSize, bgSize, bgT); bg4->x = 0; bg4->y = 100; bg4->z = -bgDist;
    bg4->isObstacle = true;

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

    // 정상에 황금 목표 지점(Goal) 생성
    float goalY = (MAP_HEIGHT * 3.0f) + 5.0f; // 마지막 층보다 조금 더 위에
    Shape* goal = ShapeSave(mapShapes, 'c', 1.0f, 0.84f, 0.0f, 3.0f, 3.0f, 3.0f); // 황금색 큐브
    goal->x = 0; goal->y = goalY; goal->z = 0;
    goal->isDoor = true; // 편의상 isDoor 플래그를 "목표물" 표시로 재활용합니다

    // 목표물도 충돌체에 등록 (밟거나 닿을 수 있게)
    mapBlocks.push_back({ glm::vec3(0, goalY, 0), glm::vec3(3.0f, 3.0f, 3.0f) });
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

                // 황금 큐브(Goal)에 닿았는지 확인
                // 황금 큐브는 맵의 아주 높은 곳(Y > 440)에 있고 중앙(0,0)에 있음
                if (block.first.y > 440.0f && abs(block.first.x) < 1.0f && abs(block.first.z) < 1.0f) {
                    currentState = CLEAR;

                    // [핵심] 벽과 발판을 모두 제거하여 시야를 뻥 뚫어줌
                    mapShapes.clear();
                    mapBlocks.clear();

                    printf("GAME CLEAR!\n");
                    return; // 함수 즉시 종료
                }

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
            rock.position = glm::vec3(0, 5.0f, 0); rock.velocity = glm::vec3(0, 0, 0);
        }
    }
    else if (currentState == CLEAR) {
        rock.velocity = glm::vec3(0, 0, 0); // 공중 부양 (멈춤)
        //rock.position.y += 0.1f; // 천천히 승천하는 연출

        cameraYaw += 1.0f;
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
    // 1. 그리기 로직 (RenderPass)
    auto RenderPass = [&](glm::mat4 viewMatrix, glm::mat4 projMatrix, bool isMiniMap = false) {

        unsigned int viewLoc = glGetUniformLocation(shaderProgramID, "view");
        unsigned int projLoc = glGetUniformLocation(shaderProgramID, "projection");
        unsigned int modelLoc = glGetUniformLocation(shaderProgramID, "model");
        unsigned int colorLoc = glGetUniformLocation(shaderProgramID, "objectColor");

        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, &viewMatrix[0][0]);
        glUniformMatrix4fv(projLoc, 1, GL_FALSE, &projMatrix[0][0]);

        glm::vec3 lightPos(rock.position.x, rock.position.y + 50.0f, rock.position.z);
        glUniform3f(glGetUniformLocation(shaderProgramID, "lightPos"), lightPos.x, lightPos.y, lightPos.z);
        glUniform3f(glGetUniformLocation(shaderProgramID, "viewPos"), cameraPos.x, cameraPos.y, cameraPos.z);
        glUniform3f(glGetUniformLocation(shaderProgramID, "lightColor"), 1.0f, 1.0f, 1.0f);

        auto drawList = [&](std::vector<Shape>& list, bool isPlayer = false) {
            for (auto& s : list) {
                if (isMiniMap && s.isObstacle) continue;

                glUniform3fv(colorLoc, 1, s.color);

                // [추가] 플레이어(구)일 때만 텍스처 사용
                if (isPlayer && s.shapeType == '1') {
                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D, rockTextureID);
                    glUniform1i(glGetUniformLocation(shaderProgramID, "useTexture"), 1); // 텍스처 ON
                }
                else {
                    glUniform1i(glGetUniformLocation(shaderProgramID, "useTexture"), 0); // 텍스처 OFF
                }

                glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(s.x, s.y, s.z));
                if (isMiniMap && isPlayer) {
                    model = glm::scale(model, glm::vec3(1.0f, 1.0f, 1.0f));
                }

                glUniformMatrix4fv(modelLoc, 1, GL_FALSE, &model[0][0]);
                glBindVertexArray(s.VAO); glDrawArrays(s.primitiveType, 0, s.vertexCount);
            }
            };

        // 플레이어 그리기
        drawList(shapes, true);

        if (currentState == LOBBY || currentState == FALLING) {
            drawList(lobbyShapes, false);
        }
        if (currentState == PLAYING || currentState == CLEAR) {
            drawList(mapShapes, false);
        }
        };

    // -------------------------------------------------------
    // [STEP 1] 메인 화면
    // -------------------------------------------------------
    glViewport(0, 0, g_width, g_height);
    if (currentState == CLEAR) glClearColor(1.0f, 0.84f, 0.0f, 1.0f);
    else glClearColor(0.1f, 0.1f, 0.1f, 1.0f);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(shaderProgramID);
    glEnable(GL_DEPTH_TEST);

    glm::mat4 mainView = glm::lookAt(cameraPos, cameraTarget, cameraUp);
    glm::mat4 mainProj;
    if (isPerspective) mainProj = glm::perspective(glm::radians(60.0f), (float)g_width / g_height, 0.1f, 1000.0f);
    else { float s = 40.0f; float a = (float)g_width / g_height; mainProj = glm::ortho(-s * a, s * a, -s, s, 0.1f, 1000.0f); }

    RenderPass(mainView, mainProj, false);

    // -------------------------------------------------------
    // [STEP 2] 미니맵 (회전 적용)
    // -------------------------------------------------------
    if (currentState == PLAYING || currentState == CLEAR) {

        int mapW = g_width / 5;
        int mapH = g_height / 2.5;
        int mapX = g_width - mapW - 20;
        int mapY = g_height - mapH - 20;

        glEnable(GL_SCISSOR_TEST);
        glScissor(mapX, mapY, mapW, mapH);
        glClearColor(0.9f, 0.9f, 0.9f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glDisable(GL_SCISSOR_TEST);

        glViewport(mapX, mapY, mapW, mapH);
        glDisable(GL_CULL_FACE);

        // [핵심 수정] 카메라 위치를 플레이어의 회전각(cameraYaw)에 맞춰서 계산
        float dist = 800.0f; // 타워 중심에서 떨어진 거리
        float rad = glm::radians(cameraYaw); // 현재 카메라 각도 (라디안)

        // cos, sin을 이용해 원형으로 카메라 위치 결정
        // (cameraYaw는 보통 -90도에서 시작하므로 좌표계에 맞춰서 sin/cos 조정)
        // 메인 카메라 공식과 비슷하게 따라가되, 높이는 중앙(250) 고정
        float camX = cos(rad) * dist;
        float camZ = sin(rad) * dist;

        glm::vec3 miniCamPos(camX, 250.0f, camZ);
        glm::vec3 miniCamTarget(0.0f, 250.0f, 0.0f); // 타워의 허리춤을 바라봄
        glm::mat4 miniView = glm::lookAt(miniCamPos, miniCamTarget, glm::vec3(0, 1, 0));

        // 맵 전체 높이를 커버하는 투영
        // Y축 중심이 250이므로, 위아래로 300씩 잡으면 0~550 커버 가능
        glm::mat4 miniProj = glm::ortho(-70.0f, 70.0f, -300.0f, 300.0f, 0.1f, 2000.0f);

        RenderPass(miniView, miniProj, true);

        glEnable(GL_CULL_FACE);
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
    glGenVertexArrays(1, &s.VAO);
    glGenBuffers(1, &s.VBO); glGenBuffers(1, &s.CBO); glGenBuffers(1, &s.NBO);
    glGenBuffers(1, &s.TBO); // [추가]

    glBindVertexArray(s.VAO);

    glBindBuffer(GL_ARRAY_BUFFER, s.VBO); glBufferData(GL_ARRAY_BUFFER, v.size() * 4, v.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0); glEnableVertexAttribArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, s.NBO); glBufferData(GL_ARRAY_BUFFER, n.size() * 4, n.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, 0); glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, s.CBO); glBufferData(GL_ARRAY_BUFFER, c.size() * 4, c.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 0, 0); glEnableVertexAttribArray(2);

    // [추가] 텍스처 좌표 버퍼 바인딩
    if (!s.uvs.empty()) {
        glBindBuffer(GL_ARRAY_BUFFER, s.TBO);
        glBufferData(GL_ARRAY_BUFFER, s.uvs.size() * sizeof(float), s.uvs.data(), GL_STATIC_DRAW);
        glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, 0, 0); // location 3
        glEnableVertexAttribArray(3);
    }

    glBindVertexArray(0);
}
Shape* ShapeSave(std::vector<Shape>& list, char key, float r, float g, float b, float sx, float sy, float sz) {
    Shape s; s.color[0] = r; s.color[1] = g; s.color[2] = b; s.shapeType = key; s.primitiveType = GL_TRIANGLES;

    if (key == 'c') {
        float x = sx, y = sy, z = sz;
        s.vertices = { -x,-y,z, x,-y,z, x,y,z, -x,-y,z, x,y,z, -x,y,z, -x,-y,-z, -x,y,-z, x,y,-z, -x,-y,-z, x,y,-z, x,-y,-z, -x,-y,-z, -x,-y,z, -x,y,z, -x,-y,-z, -x,y,z, -x,y,-z, x,-y,z, x,-y,-z, x,y,-z, x,-y,z, x,y,-z, x,y,z, -x,y,z, x,y,z, x,y,-z, -x,y,z, x,y,-z, -x,y,-z, -x,-y,-z, x,-y,-z, x,-y,z, -x,-y,-z, x,-y,z, -x,-y,z };
        s.normals = { 0,0,1, 0,0,1, 0,0,1, 0,0,1, 0,0,1, 0,0,1, 0,0,-1, 0,0,-1, 0,0,-1, 0,0,-1, 0,0,-1, 0,0,-1, -1,0,0, -1,0,0, -1,0,0, -1,0,0, -1,0,0, -1,0,0, 1,0,0, 1,0,0, 1,0,0, 1,0,0, 1,0,0, 1,0,0, 0,1,0, 0,1,0, 0,1,0, 0,1,0, 0,1,0, 0,1,0, 0,-1,0, 0,-1,0, 0,-1,0, 0,-1,0, 0,-1,0, 0,-1,0 };
        // 큐브는 텍스처를 안 쓰더라도 버퍼 크기를 맞춰주기 위해 0으로 채움
        for (int i = 0; i < 36; i++) { s.uvs.push_back(0.0f); s.uvs.push_back(0.0f); }
        s.vertexCount = 36;
    }
    else if (key == '1') {
        int sec = 30, st = 30; float rad = sx;
        std::vector<float> tv, tn, tuv; // tuv(텍스처좌표) 추가

        for (int i = 0; i <= st; ++i) {
            float ang = M_PI / 2 - i * M_PI / st, xy = rad * cosf(ang), z = rad * sinf(ang);
            for (int j = 0; j <= sec; ++j) {
                float sa = j * 2 * M_PI / sec, x = xy * cosf(sa), y = xy * sinf(sa);
                tv.push_back(x); tv.push_back(y); tv.push_back(z);
                tn.push_back(x / rad); tn.push_back(y / rad); tn.push_back(z / rad);

                // [추가] UV 좌표 계산
                tuv.push_back((float)j / sec);       // u
                tuv.push_back((float)i / st);        // v
            }
        }
        for (int i = 0; i < st; ++i) {
            int k1 = i * (sec + 1), k2 = k1 + sec + 1;
            for (int j = 0; j < sec; ++j, ++k1, ++k2) {
                if (i != 0) {
                    s.vertices.insert(s.vertices.end(), { tv[k1 * 3],tv[k1 * 3 + 1],tv[k1 * 3 + 2], tv[k2 * 3],tv[k2 * 3 + 1],tv[k2 * 3 + 2], tv[(k1 + 1) * 3],tv[(k1 + 1) * 3 + 1],tv[(k1 + 1) * 3 + 2] });
                    s.normals.insert(s.normals.end(), { tn[k1 * 3],tn[k1 * 3 + 1],tn[k1 * 3 + 2], tn[k2 * 3],tn[k2 * 3 + 1],tn[k2 * 3 + 2], tn[(k1 + 1) * 3],tn[(k1 + 1) * 3 + 1],tn[(k1 + 1) * 3 + 2] });
                    // [추가] UV insert
                    s.uvs.insert(s.uvs.end(), { tuv[k1 * 2],tuv[k1 * 2 + 1], tuv[k2 * 2],tuv[k2 * 2 + 1], tuv[(k1 + 1) * 2],tuv[(k1 + 1) * 2 + 1] });
                }
                if (i != st - 1) {
                    s.vertices.insert(s.vertices.end(), { tv[(k1 + 1) * 3],tv[(k1 + 1) * 3 + 1],tv[(k1 + 1) * 3 + 2], tv[k2 * 3],tv[k2 * 3 + 1],tv[k2 * 3 + 2], tv[(k2 + 1) * 3],tv[(k2 + 1) * 3 + 1],tv[(k2 + 1) * 3 + 2] });
                    s.normals.insert(s.normals.end(), { tn[(k1 + 1) * 3],tn[(k1 + 1) * 3 + 1],tn[(k1 + 1) * 3 + 2], tn[k2 * 3],tn[k2 * 3 + 1],tn[k2 * 3 + 2], tn[(k2 + 1) * 3],tn[(k2 + 1) * 3 + 1],tn[(k2 + 1) * 3 + 2] });
                    // [추가] UV insert
                    s.uvs.insert(s.uvs.end(), { tuv[(k1 + 1) * 2],tuv[(k1 + 1) * 2 + 1], tuv[k2 * 2],tuv[k2 * 2 + 1], tuv[(k2 + 1) * 2],tuv[(k2 + 1) * 2 + 1] });
                }
            }
        }
        s.vertexCount = s.vertices.size() / 3;
    }
    for (int i = 0; i < s.vertexCount; ++i) { s.colors.push_back(r); s.colors.push_back(g); s.colors.push_back(b); }

    // setup 호출
    setupShapeBuffers(s, s.vertices, s.colors, s.normals);
    list.push_back(s); return &list.back();
}