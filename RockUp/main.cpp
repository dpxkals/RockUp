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
void setupShapeBuffers(Shape& shape, const std::vector<float>& vertices, const std::vector<float>& colors);
GLvoid drawScene();
GLvoid Reshape(int w, int h);
GLvoid Keyboard(unsigned char key, int x, int y);
void SpecialKeys(int key, int x, int y);
void Mouse(int button, int state, int x, int y);
void Motion(int x, int y);
void TimerFunction(int value);
char* filetobuf(const char* file);
//--- 사용자 정의 함수

//--- 기본 전역 변수
GLint g_width = 800, g_height = 800;
GLuint shaderProgramID;
GLuint vertexShader;
GLuint fragmentShader;

//--- 전역변수
std::vector<Shape> shapes;
// 뷰 변환 관련
glm::vec3 cameraPos = glm::vec3(0.0f, 0.15f, 0.7f); // 카메라 위치
glm::vec3 cameraTarget = glm::vec3(0.0f, 0.0f, 0.0f); // 카메라가 바라보는 점
glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f); // 카메라의 Up 벡터
float yAngle = 0.0f; // 회전 각도(원하면 키보드 등으로 조작)
// 투영 모드 관리
bool isPerspective = true; // true: 원근투영, false: 직교투영


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
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    // 변환
    unsigned int modelLoc = glGetUniformLocation(shaderProgramID, "model");
    unsigned int viewLoc = glGetUniformLocation(shaderProgramID, "view");
    unsigned int projLoc = glGetUniformLocation(shaderProgramID, "projection");

    // 1. 모델 행렬 (Y축 회전) - "글로벌" 회전 (모든 객체에 적용)
    glm::mat4 mTransform = glm::mat4(1.0f);
    mTransform = glm::rotate(mTransform, glm::radians(yAngle), glm::vec3(0.0f, 1.0f, 0.0f));

    // 2. 뷰 행렬 (카메라)
    glm::mat4 vTransform = glm::mat4(1.0f);
    vTransform = glm::lookAt(cameraPos, cameraTarget, cameraUp);
    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, &vTransform[0][0]);

    // 3. 프로젝션 행렬 (원근)
    glm::mat4 pTransform;
    pTransform = glm::perspective(glm::radians(60.0f), (float)g_width / (float)g_height, 0.1f, 200.0f);
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, &pTransform[0][0]);

    for (size_t i = 0; i < shapes.size(); ++i) {
        const auto& shape = shapes[i];
        glm::mat4 model = glm::mat4(1.0f); // 개별 객체 변환 (Identity로 시작)
        glm::mat4 finalModel;

        // 여기다가 변환 추가 했었음

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