#define _CRT_SECURE_NO_WARNINGS
#include <stdlib.h>
#include <stdio.h>
#include <cstring>
#include <sstream>
#include <fstream>
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
    char shapeType = ' ';
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
        position = glm::vec3(0.0f, 5.0f, 0.0f);
        velocity = glm::vec3(0.0f, 0.0f, 0.0f);
        radius = 1.2f; // 거대 돌멩이
        isGrounded = false;

        acceleration = 0.008f;
        maxSpeed = 0.3f;
        friction = 0.96f;
        jumpForce = 0.45f;
    }
};

// --- 함수 프로토타입 ---
void make_vertexShaders();
void make_fragmentShaders();
GLuint make_shaderProgram();
void setupShapeBuffers(Shape& shape, const std::vector<float>& vertices, const std::vector<float>& colors, const std::vector<float>& normals);
GLvoid drawScene();
GLvoid Reshape(int w, int h);
GLvoid Keyboard(unsigned char key, int x, int y);
void SpecialKeys(int key, int x, int y);
void Mouse(int button, int state, int x, int y);
void Motion(int x, int y);
void TimerFunction(int value);
char* filetobuf(const char* file);
Shape* ShapeSave(std::vector<Shape>& shapeVector, char shapeKey, float r, float g, float b, float sx, float sy, float sz);
void GenerateMap();
void UpdatePhysics();

// --- 전역 변수 ---
GLint g_width = 800, g_height = 800;
GLuint shaderProgramID;
GLuint vertexShader, fragmentShader;

std::vector<Shape> shapes;
std::vector<std::pair<glm::vec3, glm::vec3>> mapBlocks;

// OBJ 맵을 위한 전역 변수
GLuint mapVAO;
size_t mapVertexCount = 0;


// 카메라 제어 변수
glm::vec3 cameraPos = glm::vec3(0.0f, 5.0f, 10.0f);
glm::vec3 cameraTarget = glm::vec3(0.0f, 0.0f, 0.0f);
glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
bool isPerspective = true;

// 3인칭 카메라 변수
float cameraYaw = 90.0f;
float cameraPitch = 20.0f; // 약간 더 낮춰서 박진감 있게
// [수정] 카메라를 구에 훨씬 가깝게 붙임 (15.0f)
float cameraDistance = 15.0f;
bool isDragging = false;
int lastMouseX = 0, lastMouseY = 0;
float mouseSensitivity = 0.3f;

// 조명 (항상 켜짐)
float lightRadius = 50.0f;
float lightAngle = 0.0f;
float lightHeight = 50.0f;

// 플레이어
Player rock;
int playerShapeIndex = -1;
bool keyState[256] = { false };

// 맵 설정 (80x80)
const int MAP_WIDTH = 80;
const int MAP_HEIGHT = 150;
const int MAP_DEPTH = 80;

// 1. OpenGL에 넘겨줄 최종 정점 구조체 (Interleaved Vertex Data)
struct GLVertex {
    float x, y, z;       // 위치 (Position)
    float u, v;          // 텍스처 좌표 (Texture UV)
    float nx, ny, nz;    // 노말 벡터 (Normal)
};
// 2. 모델 데이터 구조체
struct Model {
    GLVertex* vertices;     // 정점 배열 (메모리 할당 됨)
    size_t vertex_count;    // 정점 개수 (glDrawArrays에서 사용)
};

// 3. OBJ 파일 파싱 함수
Model loadObj(const char* filename) {
    Model model = { nullptr, 0 };

    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "ERROR: 파일을 열 수 없습니다: " << filename << std::endl;
        return model;
    }

    // 파일 읽기 속도를 위한 임시 저장소 (std::vector 사용)
    std::vector<float> temp_positions;
    std::vector<float> temp_uvs;
    std::vector<float> temp_normals;
    std::vector<GLVertex> final_vertices; // 최종 데이터가 모일 곳

    std::string line;
    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string type;
        ss >> type;

        if (type == "v") { // 위치
            float x, y, z;
            ss >> x >> y >> z;
            temp_positions.push_back(x); temp_positions.push_back(y); temp_positions.push_back(z);
        }
        else if (type == "vt") { // 텍스처 좌표
            float u, v, z;
            ss >> u >> v;
            // z값은 보통 있으면 읽고 버림, 2개만 읽어도 됨
            temp_uvs.push_back(u); temp_uvs.push_back(v);
        }
        else if (type == "vn") { // 노말
            float x, y, z;
            ss >> x >> y >> z;
            temp_normals.push_back(x); temp_normals.push_back(y); temp_normals.push_back(z);
        }
        else if (type == "f") { // 면 (Face) -> 여기서 데이터 조립!
            std::string vertexStr;
            int faceVertices = 0;

            // 한 줄에 있는 3개의 점을 읽음 (삼각형 기준)
            while (ss >> vertexStr) {
                unsigned int vIdx = 0, vtIdx = 0, vnIdx = 0;

                // 슬래시(/)를 공백으로 치환하여 파싱 쉽게 만들기
                for (char& c : vertexStr) if (c == '/') c = ' ';

                std::stringstream vss(vertexStr);
                vss >> vIdx;       // 첫 번째 숫자: 정점 인덱스

                // 텍스처/노말 인덱스는 있을 수도 있고 없을 수도 있음
                if (vertexStr.find(" ") != std::string::npos) vss >> vtIdx;
                if (vertexStr.find(" ") != std::string::npos) vss >> vnIdx;

                // --- 데이터 조립 (인덱스로 실제 값 찾기) ---
                GLVertex vertex;

                // 1. Position (필수) - OBJ는 1부터 시작하므로 -1 해줌
                if (vIdx > 0 && (vIdx - 1) * 3 + 2 < temp_positions.size()) {
                    vertex.x = temp_positions[(vIdx - 1) * 3 + 0];
                    vertex.y = temp_positions[(vIdx - 1) * 3 + 1];
                    vertex.z = temp_positions[(vIdx - 1) * 3 + 2];
                }

                // 2. UV (선택)
                if (vtIdx > 0 && !temp_uvs.empty()) {
                    vertex.u = temp_uvs[(vtIdx - 1) * 2 + 0];
                    vertex.v = temp_uvs[(vtIdx - 1) * 2 + 1];
                }
                else { vertex.u = 0; vertex.v = 0; }

                // 3. Normal (선택)
                if (vnIdx > 0 && !temp_normals.empty()) {
                    vertex.nx = temp_normals[(vnIdx - 1) * 3 + 0];
                    vertex.ny = temp_normals[(vnIdx - 1) * 3 + 1];
                    vertex.nz = temp_normals[(vnIdx - 1) * 3 + 2];
                }
                else { vertex.nx = 0; vertex.ny = 0; vertex.nz = 0; }

                final_vertices.push_back(vertex);
                faceVertices++;
            }
        }
    }

    // std::vector -> Raw Array(포인터)로 변환
    model.vertex_count = final_vertices.size();
    if (model.vertex_count > 0) {
        model.vertices = new GLVertex[model.vertex_count];
        // 메모리 복사 (Vector의 데이터를 배열로)
        memcpy(model.vertices, final_vertices.data(), sizeof(GLVertex) * model.vertex_count);

        std::cout << "OBJ 로드 성공: " << filename << std::endl;
        std::cout << " -> 총 정점 수(Vertices): " << model.vertex_count << std::endl;
        std::cout << " -> 총 삼각형 수(Triangles): " << model.vertex_count / 3 << std::endl;
    }
    else {
        std::cerr << "경고: 파일에서 유효한 데이터를 읽지 못했습니다." << std::endl;
    }

    return model;
}

void main(int argc, char** argv)
{
    srand((unsigned int)time(NULL));

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);
    glutInitWindowPosition(100, 100);
    glutInitWindowSize(g_width, g_height);
    glutCreateWindow("ROCK UP - Wide Map & Close Cam");

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        std::cerr << "Glew Init failed." << std::endl;
        return;
    }

    make_vertexShaders();
    make_fragmentShaders();
    shaderProgramID = make_shaderProgram();

    glutDisplayFunc(drawScene);
    glutReshapeFunc(Reshape);

    glutKeyboardFunc([](unsigned char key, int x, int y) {
        keyState[key] = true;
        if (key == 'q') exit(0);
        if (key == ' ') {
            if (rock.isGrounded) {
                float speed = sqrt(rock.velocity.x * rock.velocity.x + rock.velocity.z * rock.velocity.z);
                float bonus = speed * 1.2f;
                rock.velocity.y = rock.jumpForce + bonus;
            }
        }
        // [수정] 'm' 키 기능 제거 (조명 항상 켜짐)
        });
    glutKeyboardUpFunc([](unsigned char key, int x, int y) {
        keyState[key] = false;
        });

    glutSpecialFunc(SpecialKeys);
    glutMouseFunc(Mouse);
    glutMotionFunc(Motion);

    //GenerateMap();
// 2. [로드] OBJ 파일 읽어오기
    Model myMap = loadObj("Map_new2.obj");

    if (myMap.vertex_count == 0) {
        return; // 로드 실패 시 종료
    }
    mapVertexCount = myMap.vertex_count;

    // 3. [버퍼 생성] VAO, VBO 설정
    unsigned int VBO;
    glGenVertexArrays(1, &mapVAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(mapVAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    // 구조체 배열을 통째로 GPU 메모리에 전송
    glBufferData(GL_ARRAY_BUFFER, myMap.vertex_count * sizeof(GLVertex), myMap.vertices, GL_STATIC_DRAW);

    // 4. [속성 연결] 셰이더에 데이터가 어떻게 들어있는지 알려줌
    // GLVertex 구조체: { float x,y,z | float u,v | float nx,ny,nz }

    // (1) 위치 (location = 0) : 3개의 float, 시작점 0
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(GLVertex), (void*)0);
    glEnableVertexAttribArray(0);

    // (2) 텍스처 좌표 (location = 1) : 2개의 float, 시작점은 float 3개 뒤(12바이트)
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(GLVertex), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // (3) 노말 (location = 2) : 3개의 float, 시작점은 float 5개 뒤(20바이트)
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(GLVertex), (void*)(5 * sizeof(float)));
    glEnableVertexAttribArray(2);

    // OBJ 모델의 삼각형들을 mapBlocks에 추가 (충돌 감지용)
    for (size_t i = 0; i < myMap.vertex_count; i += 3) {
        // 삼각형의 세 꼭짓점
        glm::vec3 v0(myMap.vertices[i].x, myMap.vertices[i].y, myMap.vertices[i].z);
        glm::vec3 v1(myMap.vertices[i + 1].x, myMap.vertices[i + 1].y, myMap.vertices[i + 1].z);
        glm::vec3 v2(myMap.vertices[i + 2].x, myMap.vertices[i + 2].y, myMap.vertices[i + 2].z);

        // 삼각형의 중심점을 블록의 위치로 사용
        glm::vec3 center = (v0 + v1 + v2) / 3.0f;

        // 삼각형을 감싸는 작은 AABB(축 정렬 경계 상자)의 절반 크기 계산
        float minX = std::min({ v0.x, v1.x, v2.x });
        float maxX = std::max({ v0.x, v1.x, v2.x });
        float minY = std::min({ v0.y, v1.y, v2.y });
        float maxY = std::max({ v0.y, v1.y, v2.y });
        float minZ = std::min({ v0.z, v1.z, v2.z });
        float maxZ = std::max({ v0.z, v1.z, v2.z });

        glm::vec3 size((maxX - minX) / 2.0f, (maxY - minY) / 2.0f, (maxZ - minZ) / 2.0f);

        // 충돌 감지를 위해 mapBlocks에 추가
        mapBlocks.push_back({ center, size });
    }
    delete[] myMap.vertices; // 데이터 복사 후 원본 메모리 해제

    // 플레이어 생성
    Shape* pShape = ShapeSave(shapes, '1', 1.0f, 0.2f, 0.2f, rock.radius, rock.radius, rock.radius);
    playerShapeIndex = shapes.size() - 1;
    rock.position = glm::vec3(0.0f, 5.0f, 0.0f);

    glutTimerFunc(16, TimerFunction, 0);
    glutMainLoop();
}

void GenerateMap() {
    // 1. 초대형 바닥
    float floorSize = MAP_WIDTH / 2.0f;
    Shape* s = ShapeSave(shapes, 'c', 0.2f, 0.2f, 0.2f, floorSize, 1.0f, floorSize);
    s->x = 0.0f; s->y = -2.0f; s->z = 0.0f;
    mapBlocks.push_back({ glm::vec3(0, -2.0f, 0), glm::vec3(floorSize, 1.0f, floorSize) });

    // 2. 맵 경계 벽
    float wallHeight = MAP_HEIGHT / 2.0f + 50.0f;
    float wallThickness = 5.0f;
    float wallOffset = floorSize + wallThickness;

    // 4면 벽 생성
    Shape* w1 = ShapeSave(shapes, 'c', 0.5f, 0.5f, 0.5f, wallThickness, wallHeight, floorSize);
    w1->x = wallOffset; w1->y = wallHeight - 10.0f; w1->z = 0.0f;
    mapBlocks.push_back({ glm::vec3(w1->x, w1->y, w1->z), glm::vec3(wallThickness, wallHeight, floorSize) });

    Shape* w2 = ShapeSave(shapes, 'c', 0.5f, 0.5f, 0.5f, wallThickness, wallHeight, floorSize);
    w2->x = -wallOffset; w2->y = wallHeight - 10.0f; w2->z = 0.0f;
    mapBlocks.push_back({ glm::vec3(w2->x, w2->y, w2->z), glm::vec3(wallThickness, wallHeight, floorSize) });

    Shape* w3 = ShapeSave(shapes, 'c', 0.4f, 0.4f, 0.4f, floorSize, wallHeight, wallThickness);
    w3->x = 0.0f; w3->y = wallHeight - 10.0f; w3->z = wallOffset;
    mapBlocks.push_back({ glm::vec3(w3->x, w3->y, w3->z), glm::vec3(floorSize, wallHeight, wallThickness) });

    Shape* w4 = ShapeSave(shapes, 'c', 0.4f, 0.4f, 0.4f, floorSize, wallHeight, wallThickness);
    w4->x = 0.0f; w4->y = wallHeight - 10.0f; w4->z = -wallOffset;
    mapBlocks.push_back({ glm::vec3(w4->x, w4->y, w4->z), glm::vec3(floorSize, wallHeight, wallThickness) });

    // 3. [수정] 맵 전체를 활용하는 발판 배치
    float currentX = 0.0f;
    float currentZ = 0.0f;

    for (int y = 0; y < MAP_HEIGHT; ++y) {
        if (y % 2 != 0) continue; // 층 간격

        // [핵심] 랜덤하게 맵 전체 범위(-35 ~ +35) 내에서 생성
        // 기존의 '연속적인' 배치가 아니라, 점프해서 찾아가야 하는 구조
        float range = (MAP_WIDTH / 2.0f) - 5.0f; // 벽 안쪽 범위

        int blocksPerLayer = (rand() % 2) + 1; // 층당 1~2개
        for (int i = 0; i < blocksPerLayer; ++i) {

            // 완전히 랜덤한 위치 생성 (맵 전체 활용)
            float nextX = ((rand() % 100) / 100.0f * (range * 2)) - range;
            float nextZ = ((rand() % 100) / 100.0f * (range * 2)) - range;

            // 발판 크기도 다양하게
            float sx = 4.0f + (float)(rand() % 30) / 10.0f; // 4.0 ~ 7.0
            float sz = 4.0f + (float)(rand() % 30) / 10.0f;
            float sy = 0.5f;

            float colorVal = (float)y / MAP_HEIGHT;
            Shape* s = ShapeSave(shapes, 'c', colorVal, 0.6f, 1.0f - colorVal, sx, sy, sz);

            s->x = nextX;
            s->y = (float)y * 3.0f;
            s->z = nextZ;

            mapBlocks.push_back({ glm::vec3(nextX, s->y, nextZ), glm::vec3(sx, sy, sz) });
        }
    }
}

void Mouse(int button, int state, int x, int y) {
    if (button == GLUT_LEFT_BUTTON) {
        if (state == GLUT_DOWN) {
            isDragging = true;
            lastMouseX = x;
            lastMouseY = y;
        }
        else if (state == GLUT_UP) {
            isDragging = false;
        }
    }
    glutPostRedisplay();
}

void Motion(int x, int y) {
    if (isDragging) {
        int dx = x - lastMouseX;
        cameraYaw -= dx * mouseSensitivity;
        lastMouseX = x;
        lastMouseY = y;
        glutPostRedisplay();
    }
}

bool CheckCollision(glm::vec3 spherePos, float radius, glm::vec3 boxPos, glm::vec3 boxSize) {
    float minX = boxPos.x - boxSize.x; float maxX = boxPos.x + boxSize.x;
    float minY = boxPos.y - boxSize.y; float maxY = boxPos.y + boxSize.y;
    float minZ = boxPos.z - boxSize.z; float maxZ = boxPos.z + boxSize.z;

    float x = std::max(minX, std::min(spherePos.x, maxX));
    float y = std::max(minY, std::min(spherePos.y, maxY));
    float z = std::max(minZ, std::min(spherePos.z, maxZ));

    float distance = sqrt(pow(x - spherePos.x, 2) + pow(y - spherePos.y, 2) + pow(z - spherePos.z, 2));
    return distance < radius;
}

void UpdatePhysics() {
    glm::vec3 viewDir = rock.position - cameraPos;
    viewDir.y = 0.0f;
    glm::vec3 forwardDir = glm::normalize(viewDir);
    glm::vec3 rightDir = glm::normalize(glm::cross(forwardDir, glm::vec3(0.0f, 1.0f, 0.0f)));

    if (keyState['w']) {
        rock.velocity.x += forwardDir.x * rock.acceleration;
        rock.velocity.z += forwardDir.z * rock.acceleration;
    }
    if (keyState['s']) {
        rock.velocity.x -= forwardDir.x * rock.acceleration;
        rock.velocity.z -= forwardDir.z * rock.acceleration;
    }
    if (keyState['d']) {
        rock.velocity.x += rightDir.x * rock.acceleration;
        rock.velocity.z += rightDir.z * rock.acceleration;
    }
    if (keyState['a']) {
        rock.velocity.x -= rightDir.x * rock.acceleration;
        rock.velocity.z -= rightDir.z * rock.acceleration;
    }

    float currentSpeedSq = rock.velocity.x * rock.velocity.x + rock.velocity.z * rock.velocity.z;
    if (currentSpeedSq > rock.maxSpeed * rock.maxSpeed) {
        float scale = rock.maxSpeed / sqrt(currentSpeedSq);
        rock.velocity.x *= scale;
        rock.velocity.z *= scale;
    }

    rock.velocity.y -= 0.012f;

    glm::vec3 nextPos = rock.position + rock.velocity;
    rock.isGrounded = false;

    for (const auto& blockData : mapBlocks) {
        glm::vec3 blockPos = blockData.first;
        glm::vec3 blockSize = blockData.second;

        if (CheckCollision(nextPos, rock.radius, blockPos, blockSize)) {
            // 충돌 지점(상자 위 가장 가까운 점) 찾기
            float closestX = std::max(blockPos.x - blockSize.x, std::min(nextPos.x, blockPos.x + blockSize.x));
            float closestY = std::max(blockPos.y - blockSize.y, std::min(nextPos.y, blockPos.y + blockSize.y));
            float closestZ = std::max(blockPos.z - blockSize.z, std::min(nextPos.z, blockPos.z + blockSize.z));

            glm::vec3 closestPoint = glm::vec3(closestX, closestY, closestZ);
            glm::vec3 penetrationVec = nextPos - closestPoint;
            float penetrationDepth = glm::length(penetrationVec);
            glm::vec3 collisionNormal = glm::normalize(penetrationVec);

            // 1. 위치 보정: 충돌에서 밀어내기
            float overlap = rock.radius - penetrationDepth;
            nextPos += collisionNormal * overlap;

            // 2. 속도 보정: 충돌한 면을 따라 미끄러지도록
            float separatingVelocity = glm::dot(rock.velocity, collisionNormal);
            rock.velocity -= separatingVelocity * collisionNormal; // 법선 방향 속도 제거

            // 위에서 착지한 경우 isGrounded 설정
            if (collisionNormal.y > 0.707f) { // 법선이 주로 위쪽을 향할 때 (평평한 바닥)
                rock.isGrounded = true;
                rock.velocity.y = 0;
                // nextPos.y = blockPos.y + blockSize.y + rock.radius; // 이 줄은 때때로 문제를 일으킬 수 있어, 위치 보정 로직에 맡김
            }
            else {
                // 벽에 부딪혔을 때 튕겨나가지 않고 미끄러지도록 처리
                // rock.velocity.x *= -0.8f;
                // rock.velocity.z *= -0.8f;
                // nextPos = rock.position;
            }
        }
    }

    if (nextPos.y < -50.0f) { // 추락 시 리셋 높이 조정
        rock.position = glm::vec3(0, 5.0f, 0);
        rock.velocity = glm::vec3(0, 0, 0);
    }
    else {
        rock.position = nextPos;
    }

    if (rock.isGrounded) {
        rock.velocity.x *= rock.friction;
        rock.velocity.z *= rock.friction;
    }
    else {
        rock.velocity.x *= 0.995f;
        rock.velocity.z *= 0.995f;
    }

    if (playerShapeIndex != -1) {
        shapes[playerShapeIndex].x = rock.position.x;
        shapes[playerShapeIndex].y = rock.position.y;
        shapes[playerShapeIndex].z = rock.position.z;
    }

    float camX = cos(glm::radians(cameraYaw)) * cos(glm::radians(cameraPitch));
    float camY = sin(glm::radians(cameraPitch));
    float camZ = sin(glm::radians(cameraYaw)) * cos(glm::radians(cameraPitch));

    glm::vec3 offset = glm::vec3(camX, camY, camZ) * cameraDistance;
    cameraPos = rock.position + offset;
    cameraTarget = rock.position;
}

GLvoid drawScene()
{
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

    glm::vec3 currentLightPos;
    currentLightPos.x = rock.position.x + lightRadius * cos(glm::radians(lightAngle));
    currentLightPos.y = rock.position.y + lightHeight;
    currentLightPos.z = rock.position.z + lightRadius * sin(glm::radians(lightAngle));

    glUniform3f(glGetUniformLocation(shaderProgramID, "lightPos"), currentLightPos.x, currentLightPos.y, currentLightPos.z);
    glUniform3f(glGetUniformLocation(shaderProgramID, "viewPos"), cameraPos.x, cameraPos.y, cameraPos.z);

    // [수정] 조명 색상 항상 밝은 흰색으로 고정
    glUniform3f(glGetUniformLocation(shaderProgramID, "lightColor"), 1.0f, 1.0f, 1.0f);

    glm::mat4 mTransform = glm::mat4(1.0f);

    glm::mat4 vTransform = glm::lookAt(cameraPos, cameraTarget, cameraUp);
    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, &vTransform[0][0]);

    glm::mat4 pTransform;
    if (isPerspective)
        pTransform = glm::perspective(glm::radians(60.0f), (float)g_width / (float)g_height, 0.1f, 300.0f);
    else {
        float orthoSize = 40.0f;
        float aspect = (float)g_width / (float)g_height;
        pTransform = glm::ortho(-orthoSize * aspect, orthoSize * aspect, -orthoSize, orthoSize, 0.1f, 300.0f);
    }
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, &pTransform[0][0]);

    // 맵 그리기 (OBJ)
    if (mapVertexCount > 0) {
        glUniform3f(glGetUniformLocation(shaderProgramID, "objectColor"), 0.5f, 0.5f, 0.5f); // 맵 색상
        glm::mat4 model = glm::mat4(1.0f);
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, &model[0][0]);
        glBindVertexArray(mapVAO);
        glDrawArrays(GL_TRIANGLES, 0, mapVertexCount);
    }

    for (size_t i = 0; i < shapes.size(); ++i) {
        const auto& shape = shapes[i];

        glUniform3fv(glGetUniformLocation(shaderProgramID, "objectColor"), 1, shape.color);

        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(shape.x, shape.y, shape.z));
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, &model[0][0]);

        glBindVertexArray(shape.VAO);
        glDrawArrays(shape.primitiveType, 0, shape.vertexCount);
    }
    glBindVertexArray(0);
    glutSwapBuffers();
}

void TimerFunction(int value)
{
    UpdatePhysics();
    lightAngle += 0.5f;
    if (lightAngle >= 360.0f) lightAngle -= 360.0f;
    glutPostRedisplay();
    glutTimerFunc(16, TimerFunction, 0);
}

void SpecialKeys(int key, int x, int y) { glutPostRedisplay(); }
GLvoid Reshape(int w, int h) {
    g_width = w; g_height = h;
    glViewport(0, 0, w, h);
}

char* filetobuf(const char* file) {
    FILE* fptr; long length; char* buf;
    fptr = fopen(file, "rb");
    if (!fptr) return NULL;
    fseek(fptr, 0, SEEK_END); length = ftell(fptr);
    buf = (char*)malloc(length + 1);
    fseek(fptr, 0, SEEK_SET); fread(buf, length, 1, fptr);
    fclose(fptr); buf[length] = 0; return buf;
}
void make_vertexShaders() {
    GLchar* vertexSource = filetobuf("vertex.glsl");
    vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexSource, NULL);
    glCompileShader(vertexShader);
}
void make_fragmentShaders() {
    GLchar* fragmentSource = filetobuf("fragment.glsl");
    fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentSource, NULL);
    glCompileShader(fragmentShader);
}
GLuint make_shaderProgram() {
    GLuint shaderID = glCreateProgram();
    glAttachShader(shaderID, vertexShader);
    glAttachShader(shaderID, fragmentShader);
    glLinkProgram(shaderID);
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    return shaderID;
}

void setupShapeBuffers(Shape& shape, const std::vector<float>& vertices, const std::vector<float>& colors, const std::vector<float>& normals) {
    glGenVertexArrays(1, &shape.VAO);
    glGenBuffers(1, &shape.VBO); glGenBuffers(1, &shape.CBO); glGenBuffers(1, &shape.NBO);
    glBindVertexArray(shape.VAO);

    glBindBuffer(GL_ARRAY_BUFFER, shape.VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*)0); glEnableVertexAttribArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, shape.NBO);
    glBufferData(GL_ARRAY_BUFFER, normals.size() * sizeof(float), normals.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, (void*)0); glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, shape.CBO);
    glBufferData(GL_ARRAY_BUFFER, colors.size() * sizeof(float), colors.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 0, (void*)0); glEnableVertexAttribArray(2);

    glBindBuffer(GL_ARRAY_BUFFER, 0); glBindVertexArray(0);
}

Shape* ShapeSave(std::vector<Shape>& shapeVector, char shapeKey, float r, float g, float b, float sx, float sy, float sz)
{
    Shape newShape;
    newShape.color[0] = r; newShape.color[1] = g; newShape.color[2] = b;
    newShape.shapeType = shapeKey;

    if (shapeKey == 'c') {
        newShape.primitiveType = GL_TRIANGLES;
        float x = sx, y = sy, z = sz;
        newShape.vertices = {
            -x,-y,z, x,-y,z, x,y,z, -x,-y,z, x,y,z, -x,y,z,
            -x,-y,-z, -x,y,-z, x,y,-z, -x,-y,-z, x,y,-z, x,-y,-z,
            -x,-y,-z, -x,-y,z, -x,y,z, -x,-y,-z, -x,y,z, -x,y,-z,
             x,-y,z, x,-y,-z, x,y,-z, x,-y,z, x,y,-z, x,y,z,
            -x,y,z, x,y,z, x,y,-z, -x,y,z, x,y,-z, -x,y,-z,
            -x,-y,-z, x,-y,-z, x,-y,z, -x,-y,-z, x,-y,z, -x,-y,z
        };
        newShape.normals = {
            0,0,1, 0,0,1, 0,0,1, 0,0,1, 0,0,1, 0,0,1,
            0,0,-1, 0,0,-1, 0,0,-1, 0,0,-1, 0,0,-1, 0,0,-1,
            -1,0,0, -1,0,0, -1,0,0, -1,0,0, -1,0,0, -1,0,0,
             1,0,0, 1,0,0, 1,0,0, 1,0,0, 1,0,0, 1,0,0,
             0,1,0, 0,1,0, 0,1,0, 0,1,0, 0,1,0, 0,1,0,
             0,-1,0, 0,-1,0, 0,-1,0, 0,-1,0, 0,-1,0, 0,-1,0
        };
        newShape.vertexCount = 36;
    }
    else if (shapeKey == '1') {
        newShape.primitiveType = GL_TRIANGLES;
        float radius = sx;
        int sectors = 30; int stacks = 30;
        std::vector<float> tempV, tempN;

        for (int i = 0; i <= stacks; ++i) {
            float stackAngle = M_PI / 2 - i * M_PI / stacks;
            float xy = radius * cosf(stackAngle);
            float z = radius * sinf(stackAngle);
            for (int j = 0; j <= sectors; ++j) {
                float sectorAngle = j * 2 * M_PI / sectors;
                float x = xy * cosf(sectorAngle);
                float y = xy * sinf(sectorAngle);
                tempV.push_back(x); tempV.push_back(y); tempV.push_back(z);
                tempN.push_back(x / radius); tempN.push_back(y / radius); tempN.push_back(z / radius);
            }
        }
        for (int i = 0; i < stacks; ++i) {
            int k1 = i * (sectors + 1); int k2 = k1 + sectors + 1;
            for (int j = 0; j < sectors; ++j, ++k1, ++k2) {
                if (i != 0) {
                    newShape.vertices.push_back(tempV[k1 * 3]); newShape.vertices.push_back(tempV[k1 * 3 + 1]); newShape.vertices.push_back(tempV[k1 * 3 + 2]);
                    newShape.normals.push_back(tempN[k1 * 3]); newShape.normals.push_back(tempN[k1 * 3 + 1]); newShape.normals.push_back(tempN[k1 * 3 + 2]);
                    newShape.vertices.push_back(tempV[k2 * 3]); newShape.vertices.push_back(tempV[k2 * 3 + 1]); newShape.vertices.push_back(tempV[k2 * 3 + 2]);
                    newShape.normals.push_back(tempN[k2 * 3]); newShape.normals.push_back(tempN[k2 * 3 + 1]); newShape.normals.push_back(tempN[k2 * 3 + 2]);
                    newShape.vertices.push_back(tempV[(k1 + 1) * 3]); newShape.vertices.push_back(tempV[(k1 + 1) * 3 + 1]); newShape.vertices.push_back(tempV[(k1 + 1) * 3 + 2]);
                    newShape.normals.push_back(tempN[(k1 + 1) * 3]); newShape.normals.push_back(tempN[(k1 + 1) * 3 + 1]); newShape.normals.push_back(tempN[(k1 + 1) * 3 + 2]);
                }
                if (i != (stacks - 1)) {
                    newShape.vertices.push_back(tempV[(k1 + 1) * 3]); newShape.vertices.push_back(tempV[(k1 + 1) * 3 + 1]); newShape.vertices.push_back(tempV[(k1 + 1) * 3 + 2]);
                    newShape.normals.push_back(tempN[(k1 + 1) * 3]); newShape.normals.push_back(tempN[(k1 + 1) * 3 + 1]); newShape.normals.push_back(tempN[(k1 + 1) * 3 + 2]);
                    newShape.vertices.push_back(tempV[k2 * 3]); newShape.vertices.push_back(tempV[k2 * 3 + 1]); newShape.vertices.push_back(tempV[k2 * 3 + 2]);
                    newShape.normals.push_back(tempN[k2 * 3]); newShape.normals.push_back(tempN[k2 * 3 + 1]); newShape.normals.push_back(tempN[k2 * 3 + 2]);
                    newShape.vertices.push_back(tempV[(k2 + 1) * 3]); newShape.vertices.push_back(tempV[(k2 + 1) * 3 + 1]); newShape.vertices.push_back(tempV[(k2 + 1) * 3 + 2]);
                    newShape.normals.push_back(tempN[(k2 + 1) * 3]); newShape.normals.push_back(tempN[(k2 + 1) * 3 + 1]); newShape.normals.push_back(tempN[(k2 + 1) * 3 + 2]);
                }
            }
        }
        newShape.vertexCount = newShape.vertices.size() / 3;
    }

    for (int i = 0; i < newShape.vertexCount; ++i) {
        newShape.colors.push_back(r); newShape.colors.push_back(g); newShape.colors.push_back(b);
    }
    setupShapeBuffers(newShape, newShape.vertices, newShape.colors, newShape.normals);
    shapeVector.push_back(newShape);
    return &shapeVector.back();
}