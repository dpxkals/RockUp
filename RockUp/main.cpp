#define _CRT_SECURE_NO_WARNINGS
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <vector>
#include <string>
#include <time.h> // for srand
#include <algorithm>
#include <cmath> // for sqrt

// 링커 명령어 추가
#pragma comment(lib, "glew32.lib")
#pragma comment (lib, "freeglut.lib")

#include <gl/glew.h>
#include <gl/freeglut.h>
#include <gl/freeglut_ext.h>

#include <gl/glm/glm.hpp>
#include <gl/glm/ext.hpp>
#include <gl/glm/gtc/matrix_transform.hpp>

// 구를 그리기 위한
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

struct Shape {
    GLuint VAO;
    GLuint VBO;
    GLuint CBO; // color buffer object 
    GLuint NBO;
    GLenum primitiveType;
    int vertexCount;
    float color[3];
    float tx, ty;
    std::vector<float> vertices;
    std::vector<float> normals;
    std::vector<float> colors;

    GLfloat x = 0.0f, y = 0.0f, z = 0.0f;
    float size = 0.0f;
    char shapeType = ' ';
};

//--- 기본 사용자 정의 함수
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
//--- 사용자 정의 함수
Shape* ShapeSave(std::vector<Shape>& shapeVector, char shapeKey, float, float, float, float);

//--- 기본 전역 변수
GLint g_width = 800, g_height = 800;
GLuint shaderProgramID;
GLuint vertexShader;
GLuint fragmentShader;

//--- 전역변수
std::vector<Shape> shapes;
// 뷰 변환 관련
glm::vec3 cameraPos = glm::vec3(0.0f, 0.5f, 1.0f); // 카메라 위치
glm::vec3 cameraTarget = glm::vec3(0.0f, 0.0f, 0.0f); // 카메라가 바라보는 점
glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f); // 카메라의 Up 벡터
float yAngle = 0.0f; // 회전 각도(원하면 키보드 등으로 조작)
// 투영 모드 관리
bool isPerspective = true; // true: 원근투영, false: 직교투영

// 카메라 애니메이션 관련
bool cameraAnimation = false; // 카메라 공전 애니메이션 상태
float cameraAngle = 0.0f; // 카메라 공전 각도
// 카메라 이동 및 회전 속도
const float moveSpeed = 0.1f; // 카메라 이동 속도
const float rotateSpeed = 5.0f; // 카메라 회전 속도
float cameraOrbitRadius = 0.0f;
float cameraOrbitY = 0.0f;

float lightRadius = 0.4f;     // 초기 광원 위치 (0, 0, 5)를 기준으로 반지름 설정
float lightAngle = 90.0f;     // 초기 각도 (Z축)
float lightHeight = 1.0f;     // Y축 높이 고정 (객체 중심)

// 조명 키고끄기
bool lightOn = true;

// 색 변경 변수
int setColor = -1;


//--- main 함수
void main(int argc, char** argv)
{
    srand(time(NULL));

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);
    glutInitWindowPosition(100, 100);
    glutInitWindowSize(g_width, g_height);
    glutCreateWindow("RockUp");

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        std::cerr << "Glew Tnit failed." << std::endl;
        return;
    }

    make_vertexShaders();
    make_fragmentShaders();
    shaderProgramID = make_shaderProgram();

    glutDisplayFunc(drawScene);
    glutReshapeFunc(Reshape);
    glutKeyboardFunc(Keyboard);
    glutSpecialFunc(SpecialKeys);
    glutMouseFunc(Mouse);
    glutMotionFunc(Motion);

    // 초기 그리기
    ShapeSave(shapes, 'f', 1.0f, 0.0f, 1.0f, 0.5f);

    ShapeSave(shapes, 'p', 1.0f, 0.0f, 0.0f, 0.2f);

    glutMainLoop();
}

//--- drawScene: 화면에 그리기
GLvoid drawScene()
{
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(shaderProgramID);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);        // 뒷면을 컬링 (추가)
    glFrontFace(GL_CCW);

    // 변환
    unsigned int modelLoc = glGetUniformLocation(shaderProgramID, "model"); //--- 버텍스 세이더에서 모델링 변환 행렬 변수값을 받아온다.
    unsigned int viewLoc = glGetUniformLocation(shaderProgramID, "view"); //--- 버텍스 세이더에서 뷰잉 변환 행렬 변수값을 받아온다.
    unsigned int projLoc = glGetUniformLocation(shaderProgramID, "projection"); //--- 버텍스 세이더에서 투영 변환 행렬 변수값을 받아온다

    // 1. **광원 위치 계산 및 Uniform 전달** (가장 먼저 수행되어야 함)
    glm::vec3 currentLightPos;
    // Y축을 중심으로 XZ 평면에서 회전 (공전)
    currentLightPos.x = lightRadius * cos(glm::radians(lightAngle));
    currentLightPos.y = lightHeight;
    currentLightPos.z = lightRadius * sin(glm::radians(lightAngle));

    // 조명 Uniform 업데이트
    unsigned int lightPosLocation = glGetUniformLocation(shaderProgramID, "lightPos");
    glUniform3f(lightPosLocation, currentLightPos.x, currentLightPos.y, currentLightPos.z);

    // 모델 행렬 (회전)
    glm::mat4 mTransform = glm::mat4(1.0f);
    mTransform = glm::rotate(mTransform, glm::radians(yAngle), glm::vec3(0.0f, 1.0f, 0.0f));
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, &mTransform[0][0]);

    // 뷰 행렬 (카메라)
    glm::mat4 vTransform = glm::mat4(1.0f);
    vTransform = glm::lookAt(cameraPos, cameraTarget, cameraUp);
    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, &vTransform[0][0]);

    // 프로젝션 행렬 (원근)
    glm::mat4 pTransform;
    if (isPerspective) {
        // 원근투영
        pTransform = glm::perspective(glm::radians(60.0f), (float)g_width / (float)g_height, 0.1f, 200.0f);
    }
    else {
        // 직교투영
        float orthoSize = 1.0f; // 직교투영 크기 조절
        float aspect = (float)g_width / (float)g_height;
        pTransform = glm::ortho(-orthoSize * aspect, orthoSize * aspect, -orthoSize, orthoSize, 0.1f, 200.0f);
    }
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, &pTransform[0][0]);

    for (size_t i = 0; i < shapes.size(); ++i) {
        const auto& shape = shapes[i];

        // 색 넘기기
        GLint vColorLocation = glGetUniformLocation(shaderProgramID, "objectColor");
        glUniform3fv(vColorLocation, 1, shape.color);

        glm::mat4 model = glm::mat4(1.0f);
        glm::mat4 finalModel;

        finalModel = mTransform * model;
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, &finalModel[0][0]);

        glBindVertexArray(shape.VAO);
        glDrawArrays(shape.primitiveType, 0, shape.vertexCount);
    }
    glBindVertexArray(0);
    glutSwapBuffers();
}

//--- Mouse 콜백 함수
void Mouse(int button, int state, int x, int y) {
    float ogl_x = (float)x / g_width * 2.0f - 1.0f;
    float ogl_y = -((float)y / g_height * 2.0f - 1.0f);

    if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) {

    }

    if (button == GLUT_LEFT_BUTTON && state == GLUT_UP) {

    }

    glutPostRedisplay();
}

GLvoid Keyboard(unsigned char key, int x, int y) {
    switch (key) {
    case 'm': {
        lightOn = !lightOn;
        unsigned int lightColorLocation = glGetUniformLocation(shaderProgramID, "lightColor"); //--- lightColor 값 전달: (1.0, 1.0, 1.0) 백색
        if (lightOn) glUniform3f(lightColorLocation, 1.0, 1.0, 1.0);
        else glUniform3f(lightColorLocation, 0.0, 0.0, 0.0);
        break;
    }
    case 'q':
        exit(0);
        break;
    }
    glutPostRedisplay();
}

void SpecialKeys(int key, int x, int y) {
    switch (key) {
    case GLUT_KEY_UP:
        break;
    case GLUT_KEY_DOWN:
        break;
    case GLUT_KEY_LEFT:
        break;
    case GLUT_KEY_RIGHT:
        break;
    }
    glutPostRedisplay();
}

void Motion(int x, int y) {

}

void TimerFunction(int value)
{
    glutPostRedisplay(); // 화면 다시 그리기
    glutTimerFunc(16, TimerFunction, 0); // 다음 타이머 설정
}
//--- 사용자 정의 함수
Shape* ShapeSave(std::vector<Shape>& shapeVector, char shapeKey, float r, float g, float b, float sz)
{
    Shape newShape;
    float size = 1.0f;
    float s = sz;
    newShape.color[0] = r; newShape.color[1] = g; newShape.color[2] = b;

    // 바닥
    if (shapeKey == 'f') {
        newShape.primitiveType = GL_TRIANGLES;
        s = 0.5f;
        newShape.vertices = {
            // 아랫면 (y = -s) - CCW
            -s,  0,  s,   s,  0,  s,   s,  0, -s,
            -s,  0,  s,   s,  0, -s,  -s,  0, -s,
        };
        newShape.normals = {
            0.0f, 1.0f, 0.0f,   0.0f, 1.0f, 0.0f,    0.0f, 1.0f, 0.0f,
            0.0f, 1.0f, 0.0f,   0.0f, 1.0f, 0.0f,    0.0f, 1.0f, 0.0f,
        };
        newShape.vertexCount = 6;
    }
    // 정육면체
    else if (shapeKey == 'c') {
        newShape.primitiveType = GL_TRIANGLES;
        newShape.vertices = {
            // 앞면 (z = +s) - CCW
            -s, -s,  s,   s, -s,  s,   s,  s,  s,
            -s, -s,  s,   s,  s,  s,  -s,  s,  s,

            // 뒷면 (z = -s) - CCW (뒤에서 볼 때)
            -s, -s, -s,  -s,  s, -s,   s,  s, -s,
            -s, -s, -s,   s,  s, -s,   s, -s, -s,

            // 왼쪽면 (x = -s) - CCW
            -s, -s, -s,  -s, -s,  s,  -s,  s,  s,
            -s, -s, -s,  -s,  s,  s,  -s,  s, -s,

            // 오른쪽면 (x = +s) - CCW
             s, -s,  s,   s, -s, -s,   s,  s, -s,
             s, -s,  s,   s,  s, -s,   s,  s,  s,

             // 윗면 (y = +s) - CCW
             -s,  s,  s,   s,  s,  s,   s,  s, -s,
             -s,  s,  s,   s,  s, -s,  -s,  s, -s,

             // 아랫면 (y = -s) - CCW
             -s, -s, -s,   s, -s, -s,   s, -s,  s,
             -s, -s, -s,   s, -s,  s,  -s, -s,  s,
        };
        newShape.normals = {
            // 앞면 (z = +s) - CCW
            0.0f,0.0f,1.0f,     0.0f,0.0f,1.0f,     0.0f,0.0f,1.0f,
            0.0f,0.0f,1.0f,     0.0f,0.0f,1.0f,     0.0f,0.0f,1.0f,

            // 뒷면 (z = -s) - CCW (뒤에서 볼 때)
            0.0f,0.0f,-1.0f,    0.0f,0.0f,-1.0f,    0.0f,0.0f,-1.0f,
            0.0f,0.0f,-1.0f,    0.0f,0.0f,-1.0f,    0.0f,0.0f,-1.0f,

            // 왼쪽면 (x = -s) - CCW
            -1.0f,0.0f,0.0f,    -1.0f,0.0f,0.0f,   -1.0f,0.0f,0.0f,
            -1.0f,0.0f,0.0f,    -1.0f,0.0f,0.0f,   -1.0f,0.0f,0.0f,

            // 오른쪽면 (x = +s) - CCW
            1.0f,0.0f,0.0f,     1.0f,0.0f,0.0f,     1.0f,0.0f,0.0f,
            1.0f,0.0f,0.0f,     1.0f,0.0f,0.0f,     1.0f,0.0f,0.0f,

            // 윗면 (y = +s) - CCW
            0.0f,1.0f,0.0f,    0.0f,1.0f,0.0f,     0.0f,1.0f,0.0f,
            0.0f,1.0f,0.0f,    0.0f,1.0f,0.0f,     0.0f,1.0f,0.0f,

            // 아랫면 (y = -s) - CCW
            0.0f,-1.0f,0.0f,   0.0f,-1.0f,0.0f,    0.0f,-1.0f,0.0f,
            0.0f,-1.0f,0.0f,   0.0f,-1.0f,0.0f,    0.0f,-1.0f,0.0f,
        };
        newShape.vertexCount = 36;
        // 정점별 랜덤 색상 할당
        for (int i = 0; i < newShape.vertexCount; ++i) {
            float r = 0.0f;
            float g = 0.7f;
            float b = 0.0f;
            newShape.colors.push_back(r);
            newShape.colors.push_back(g);
            newShape.colors.push_back(b);
        }
    }
    // 사각뿔
    else if (shapeKey == 'p') { // 사각뿔

        const float Ny = 0.447f;
        const float Nxz = 0.894f;

        newShape.primitiveType = GL_TRIANGLES;
        // 밑면 2개 삼각형, 옆면 4개 삼각형
        newShape.vertices = {
            // 밑면 - CCW로 수정
                -s, 0, -s,   s, 0, -s,   s, 0,  s,
                -s, 0, -s,   s, 0,  s,  -s, 0,  s,

                // 앞면 (Z=-s): 노멀 (0.0, Ny, -Nxz)
                -s, 0, -s,   0,  2 * s,  0,   s, 0, -s,

                // 오른면 (X=s): 노멀 (Nxz, Ny, 0.0)
                 s, 0, -s,   0,  2 * s,  0,   s, 0,  s,

                 // 뒷면 (Z=s): 노멀 (0.0, Ny, Nxz)
                  s, 0,  s,   0,  2 * s,  0,  -s, 0,  s,

                  // 왼면 (X=-s): 노멀 (-Nxz, Ny, 0.0)
                  -s, 0,  s,  0,  2 * s,  0,  -s, 0, -s };
        newShape.normals = {
            0.0f,-1.0f,0.0f,    0.0f,-1.0f,0.0f,    0.0f,-1.0f,0.0f,
            0.0f,-1.0f,0.0f,    0.0f,-1.0f,0.0f,    0.0f,-1.0f,0.0f,

            0.0f, Ny,-Nxz,      0.0f, Ny,-Nxz,      0.0f, Ny,-Nxz,
            Nxz, Ny, 0.0f,      Nxz, Ny, 0.0f,      Nxz, Ny, 0.0f,
            0.0f, Ny, Nxz,      0.0f, Ny, Nxz,      0.0f, Ny, Nxz,
            -Nxz, Ny, 0.0f,     -Nxz, Ny, 0.0f,     -Nxz, Ny, 0.0f,
        };
        newShape.vertexCount = 18;
    }
    else if (shapeKey == '1') { // 구 (sphere)
        newShape.primitiveType = GL_TRIANGLES;
        float radius = s;
        int sectors = 20;  // 경도 분할 수
        int stacks = 20;   // 위도 분할 수

        std::vector<float> tempVertices; // 이름 변경: vertices -> tempVertices (혼동 방지)
        std::vector<float> tempNormals;  // [추가] 임시 법선 벡터 저장소
        // std::vector<unsigned int> indices; // (사용되지 않으므로 주석 처리)

        float r = static_cast<float>(rand()) / RAND_MAX;
        float g = static_cast<float>(rand()) / RAND_MAX;
        float b = static_cast<float>(rand()) / RAND_MAX;

        // 1. 구의 정점 및 법선 계산 (임시 저장)
        for (int i = 0; i <= stacks; ++i) {
            float stackAngle = M_PI / 2 - i * M_PI / stacks;
            float xy = radius * cosf(stackAngle);
            float z = radius * sinf(stackAngle);

            for (int j = 0; j <= sectors; ++j) {
                float sectorAngle = j * 2 * M_PI / sectors;
                float x = xy * cosf(sectorAngle);
                float y = xy * sinf(sectorAngle);

                // 위치 저장
                tempVertices.push_back(x);
                tempVertices.push_back(y);
                tempVertices.push_back(z);

                // [추가] 법선 벡터 계산 및 저장
                // 구의 법선 = (x,y,z) / 반지름
                // 정규화된 벡터 (길이가 1)
                tempNormals.push_back(x / radius);
                tempNormals.push_back(y / radius);
                tempNormals.push_back(z / radius);
            }
        }

        // 2. 삼각형 조립 (위치와 법선을 함께 newShape에 넣기)
        for (int i = 0; i < stacks; ++i) {
            int k1 = i * (sectors + 1);
            int k2 = k1 + sectors + 1;

            for (int j = 0; j < sectors; ++j, ++k1, ++k2) {
                // 람다 함수: 코드 중복을 줄이기 위해 데이터를 넣는 도우미
                // 위치와 법선 데이터를 해당 인덱스에서 가져와서 newShape에 넣음
                auto pushVertexData = [&](int index) {
                    // 위치 (x, y, z)
                    newShape.vertices.push_back(tempVertices[index * 3]);
                    newShape.vertices.push_back(tempVertices[index * 3 + 1]);
                    newShape.vertices.push_back(tempVertices[index * 3 + 2]);

                    // [추가] 법선 (nx, ny, nz) -> newShape에 normals 벡터가 있다고 가정
                    newShape.normals.push_back(tempNormals[index * 3]);
                    newShape.normals.push_back(tempNormals[index * 3 + 1]);
                    newShape.normals.push_back(tempNormals[index * 3 + 2]);
                    };

                if (i != 0) {
                    // 첫 번째 삼각형 (k1, k2, k1+1)
                    pushVertexData(k1);
                    pushVertexData(k2);
                    pushVertexData(k1 + 1);
                }

                if (i != (stacks - 1)) {
                    // 두 번째 삼각형 (k1+1, k2, k2+1)
                    pushVertexData(k1 + 1);
                    pushVertexData(k2);
                    pushVertexData(k2 + 1);
                }
            }
        }

        newShape.vertexCount = newShape.vertices.size() / 3;

        // 정점별 랜덤 색상 할당 (기존 코드 유지)
        for (int i = 0; i < newShape.vertexCount; ++i) {
            newShape.colors.push_back(r);
            newShape.colors.push_back(g);
            newShape.colors.push_back(b);
        }
    }

    setupShapeBuffers(newShape, newShape.vertices, newShape.colors, newShape.normals);
    shapeVector.push_back(newShape);
    return &shapeVector.back();
}

//--- 기본 OPENGL함수
void setupShapeBuffers(Shape& shape, const std::vector<float>& vertices, const std::vector<float>& colors, const std::vector<float>& normals) {
    glGenVertexArrays(1, &shape.VAO);
    glGenBuffers(1, &shape.VBO);
    glGenBuffers(1, &shape.CBO);
    glGenBuffers(1, &shape.NBO);

    glBindVertexArray(shape.VAO);

    // vertex
    glBindBuffer(GL_ARRAY_BUFFER, shape.VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);
    glEnableVertexAttribArray(0);

    // 2. 법선 (Normal Vector) - [수정] 별도의 버퍼(NBO) 사용
    glBindBuffer(GL_ARRAY_BUFFER, shape.NBO);
    glBufferData(GL_ARRAY_BUFFER, normals.size() * sizeof(float), normals.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);
    glEnableVertexAttribArray(1);

    // color
    glBindBuffer(GL_ARRAY_BUFFER, shape.CBO);
    glBufferData(GL_ARRAY_BUFFER, colors.size() * sizeof(float), colors.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);
    glEnableVertexAttribArray(2);

    glUseProgram(shaderProgramID);
    unsigned int lightPosLocation = glGetUniformLocation(shaderProgramID, "lightPos"); //--- lightPos 값 전달: (0.0, 0.0, 5.0);
    glUniform3f(lightPosLocation, 0.0, 1.0, 0.0);
    unsigned int lightColorLocation = glGetUniformLocation(shaderProgramID, "lightColor"); //--- lightColor 값 전달: (1.0, 1.0, 1.0) 백색
    glUniform3f(lightColorLocation, 1.0, 1.0, 1.0);
    unsigned int viewPosLocation = glGetUniformLocation(shaderProgramID, "viewPos"); //--- viewPos 값 전달: 카메라 위치
    glUniform3f(viewPosLocation, cameraPos.x, cameraPos.y, cameraPos.z);


    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

GLvoid Reshape(int w, int h)
{
    g_width = w;
    g_height = h;
    glViewport(0, 0, w, h);
}

char* filetobuf(const char* file)
{
    FILE* fptr;
    long length;
    char* buf;
    fptr = fopen(file, "rb");
    if (!fptr) return NULL;
    fseek(fptr, 0, SEEK_END);
    length = ftell(fptr);
    buf = (char*)malloc(length + 1);
    fseek(fptr, 0, SEEK_SET);
    fread(buf, length, 1, fptr);
    fclose(fptr);
    buf[length] = 0;
    return buf;
}

void make_vertexShaders()
{
    GLchar* vertexSource;
    vertexSource = filetobuf("vertex.glsl");
    vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexSource, NULL);
    glCompileShader(vertexShader);
    GLint result;
    GLchar errorLog[512];
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &result);
    if (!result) {
        glGetShaderInfoLog(vertexShader, 512, NULL, errorLog);
        std::cerr << "ERROR: vertex shader compile failed\n" << errorLog << std::endl;
    }
}

void make_fragmentShaders()
{
    GLchar* fragmentSource;
    fragmentSource = filetobuf("fragment.glsl");
    fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentSource, NULL);
    glCompileShader(fragmentShader);
    GLint result;
    GLchar errorLog[512];
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &result);
    if (!result) {
        glGetShaderInfoLog(fragmentShader, 512, NULL, errorLog);
        std::cerr << "ERROR: fragment shader compile failed\n" << errorLog << std::endl;
    }
}

GLuint make_shaderProgram()
{
    GLuint shaderID = glCreateProgram();
    glAttachShader(shaderID, vertexShader);
    glAttachShader(shaderID, fragmentShader);
    glLinkProgram(shaderID);
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    GLint result;
    GLchar errorLog[512];
    glGetProgramiv(shaderID, GL_LINK_STATUS, &result);
    if (!result) {
        glGetProgramInfoLog(shaderID, 512, NULL, errorLog);
        std::cerr << "ERROR: shader program link failed\n" << errorLog << std::endl;
    }
    return shaderID;
}