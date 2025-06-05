// Q1.cpp

// 1) MSVC에서 C 표준 문자열 함수를 쓰면 C4996 오류가 뜨므로, 아래 매크로를 최상단에 추가
#define _CRT_SECURE_NO_WARNINGS

// 2) 기본 헤더들
#include <Windows.h>
#include <iostream>
#include <algorithm>       // std::min, std::max
#include <cstdlib>         // atoi, atof

#include <GL/glew.h>
#include <GL/GL.h>
#include <GL/freeglut.h>
#include <GL/glu.h>        // gluLookAt 등을 위해 (필요 시)

#define GLFW_INCLUDE_GLU
#define GLFW_DLL
#include <GLFW/glfw3.h>

#define GLM_SWIZZLE
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/string_cast.hpp>

#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>
#include <queue>
#include <fstream>
#include <float.h>

using namespace glm;

//====================================================
// 3) 함수 프로토타입(선언): 호출부보다 먼저 나와야 함
void initScene();
void tokenize(char* string, std::vector<std::string>& tokens, const char* delimiter);
int face_index(const char* string);
void load_mesh(const std::string& fileName);
void init_timer();
void start_timing();
float stop_timing();
void display();
void reshape(int w, int h);
void applyTransforms();
void setupLighting();
void drawBunnyImmediate();

//====================================================
// 4) 전역 상수, 전역 변수

const int WINDOW_WIDTH = 1280;
const int WINDOW_HEIGHT = 1280;

struct Vector3 {
    float x, y, z;
};
struct Triangle {
    unsigned int indices[3];
};

std::vector<Vector3>  gPositions;
std::vector<Vector3>  gNormals;
std::vector<Triangle> gTriangles;

float   gTotalTimeElapsed = 0;
int     gTotalFrames = 0;
GLuint  gTimer;

//====================================================
// 5) initScene: 메시 로드 + 뷰포트 + 조명 설정
void initScene() {
    // (1) 메시 로드
    load_mesh("bunny.obj");

    // (2) 조명 설정
    setupLighting();

    // (3) 타이머 초기화
    init_timer();
}

//====================================================
// 6) tokenize, face_index: OBJ 파싱 보조 함수
void tokenize(char* string, std::vector<std::string>& tokens, const char* delimiter) {
    char* token = strtok(string, delimiter);
    while (token != NULL) {
        tokens.push_back(std::string(token));
        token = strtok(NULL, delimiter);
    }
}

int face_index(const char* string) {
    int length = strlen(string);
    char* copy = new char[length + 1];
    memset(copy, 0, length + 1);
    strcpy(copy, string);

    std::vector<std::string> tokens;
    tokenize(copy, tokens, "/");
    delete[] copy;

    if (tokens.front().length() > 0 &&
        tokens.back().length() > 0 &&
        atoi(tokens.front().c_str()) == atoi(tokens.back().c_str()))
    {
        return atoi(tokens.front().c_str());
    }
    else {
        printf("ERROR: Bad face specifier!\n");
        exit(0);
    }
}

//====================================================
// 7) load_mesh: .obj 파일에서 정점, 노멀, 삼각형 정보 읽어오기
void load_mesh(const std::string& fileName) {
    std::ifstream fin(fileName.c_str());
    if (!fin.is_open()) {
        printf("ERROR: Unable to load mesh from %s!\n", fileName.c_str());
        exit(0);
    }

    float xmin = FLT_MAX, xmax = -FLT_MAX;
    float ymin = FLT_MAX, ymax = -FLT_MAX;
    float zmin = FLT_MAX, zmax = -FLT_MAX;

    while (true) {
        char line[1024] = { 0 };
        fin.getline(line, 1024);
        if (fin.eof()) break;
        if (strlen(line) <= 1) continue;

        std::vector<std::string> tokens;
        tokenize(line, tokens, " ");

        if (tokens[0] == "v") {
            float x = atof(tokens[1].c_str());
            float y = atof(tokens[2].c_str());
            float z = atof(tokens[3].c_str());

            xmin = std::min(x, xmin);  xmax = std::max(x, xmax);
            ymin = std::min(y, ymin);  ymax = std::max(y, ymax);
            zmin = std::min(z, zmin);  zmax = std::max(z, zmax);

            Vector3 position = { x, y, z };
            gPositions.push_back(position);
        }
        else if (tokens[0] == "vn") {
            float x = atof(tokens[1].c_str());
            float y = atof(tokens[2].c_str());
            float z = atof(tokens[3].c_str());
            Vector3 normal = { x, y, z };
            gNormals.push_back(normal);
        }
        else if (tokens[0] == "f") {
            unsigned int a = face_index(tokens[1].c_str());
            unsigned int b = face_index(tokens[2].c_str());
            unsigned int c = face_index(tokens[3].c_str());
            Triangle triangle;
            triangle.indices[0] = a - 1;
            triangle.indices[1] = b - 1;
            triangle.indices[2] = c - 1;
            gTriangles.push_back(triangle);
        }
    }
    fin.close();

    printf("Loaded mesh from %s. (%zu vertices, %zu normals, %zu triangles)\n",
        fileName.c_str(), gPositions.size(), gNormals.size(), gTriangles.size());
    printf("Mesh bounding box is: (%.4f, %.4f, %.4f) to (%.4f, %.4f, %.4f)\n",
        xmin, ymin, zmin, xmax, ymax, zmax);
}

//====================================================
// 8) 타이머 함수들
void init_timer() {
    glGenQueries(1, &gTimer);
}

void start_timing() {
    glBeginQuery(GL_TIME_ELAPSED, gTimer);
}

float stop_timing() {
    glEndQuery(GL_TIME_ELAPSED);

    GLint available = GL_FALSE;
    while (available == GL_FALSE)
        glGetQueryObjectiv(gTimer, GL_QUERY_RESULT_AVAILABLE, &available);

    GLint result;
    glGetQueryObjectiv(gTimer, GL_QUERY_RESULT, &result);

    // result는 나노초 단위라고 가정
    float timeElapsed = result / (1000.0f * 1000.0f * 1000.0f);
    return timeElapsed;
}

//====================================================
// 9) display 콜백 (Immediate Mode 렌더링)
void display() {
    // 1) 버퍼 초기화
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    gluLookAt(
        0.0, 0.0, 0.0,    // eye = (0,0,0)
        0.0, 0.0, -1.0,   // center = (0,0,-1) 방향으로 바라봄
        0.0, 1.0, 0.0     // up = +Y
    );

    // (3) **조명을 이제 여기서 설정** (카메라 변환이 이미 적용된 뒤)
    setupLighting();


    // 4) 이제 모델(버니)을 화면에 맞게 변환한다 (스케일·이동 순서 지켜서).
    glTranslatef(0.1f, -1.0f, -1.5f);
    glScalef(10.0f, 10.0f, 10.0f);

    // 3) 타이머 시작
    start_timing();

    // 4) 즉시 모드로 버니 그림
    drawBunnyImmediate();

    // 5) 타이머 중지 → FPS 계산
    float t = stop_timing();
    gTotalFrames++;
    gTotalTimeElapsed += t;
    float fps = gTotalFrames / gTotalTimeElapsed;

    // 6) 윈도우 타이틀에 FPS 표시
    char title[128];
    snprintf(title, sizeof(title), "OpenGL Bunny (Immediate Mode): %.2f FPS", fps);
    glutSetWindowTitle(title);

    // 7) 버퍼 교체 + 재요청
    glutSwapBuffers();
    glutPostRedisplay();
}

//====================================================
// 10) reshape 콜백 (윈도우 리사이즈 시 호출)
void reshape(int w, int h) {
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    // -0.1, -1000이지만 openGL에서는 z가 -이므로 고려해서 하기
    glFrustum(-0.1, 0.1, -0.1, 0.1, 0.1, 1000.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

//====================================================
// 11) 카메라 + 모델 변환
void applyTransforms() {
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // 1) 카메라(뷰 변환) : 원점(0,0,0)에서 –Z축 방향(0,0,–1) 바라보기
    gluLookAt(0.0, 0.0, 0.0,
        0.0, 0.0, -1.0,
        0.0, 1.0, 0.0);

    // 2) 모델링 변환 : “translate 먼저 → scale 나중”
    glTranslatef(0.1f, -1.0f, -1.5f);
    glScalef(10.0f, 10.0f, 10.0f);
}

//====================================================
// 12) 조명 설정 (고정 파이프라인)
void setupLighting() {

    // 전역 ambient
    GLfloat globalAmbient[] = { 0.2f, 0.2f, 0.2f, 1.0f };
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, globalAmbient);

    // Light0 활성화
    glEnable(GL_NORMALIZE);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);

    // Light0 속성: ambient=0, diffuse=1, specular=0
    GLfloat lightAmbient[] = { 0.0f, 0.0f, 0.0f, 1.0f };
    GLfloat lightDiffuse[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    GLfloat lightSpecular[] = { 0.0f, 0.0f, 0.0f, 1.0f };
    // 방향성 조명: (-1,-1,-1), w=0
    // 이거 왜 1,1,1로 해야 결과가 나옴?
    GLfloat lightDir[] = { 1.0f, 1.0f, 1.0f, 0.0f };

    glLightfv(GL_LIGHT0, GL_AMBIENT, lightAmbient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, lightDiffuse);
    glLightfv(GL_LIGHT0, GL_SPECULAR, lightSpecular);
    glLightfv(GL_LIGHT0, GL_POSITION, lightDir);

    // 버니 재질(Material) 설정: ka=kd=(1,1,1), ks=(0,0,0), shininess=0
    GLfloat matAmbient[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    GLfloat matDiffuse[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    GLfloat matSpecular[] = { 0.0f, 0.0f, 0.0f, 1.0f };
    GLfloat matShininess = 0.0f;

    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, matAmbient);
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, matDiffuse);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, matSpecular);
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, matShininess);
}

//====================================================
// 13) 즉시 모드로 버니 그리기
void drawBunnyImmediate() {
    glBegin(GL_TRIANGLES);
    for (size_t i = 0; i < gTriangles.size(); ++i) {
        unsigned int k0 = gTriangles[i].indices[0];
        unsigned int k1 = gTriangles[i].indices[1];
        unsigned int k2 = gTriangles[i].indices[2];

        // 첫 번째 정점
        glNormal3f(
            gNormals[k0].x,
            gNormals[k0].y,
            gNormals[k0].z
        );
        glVertex3f(
            gPositions[k0].x,
            gPositions[k0].y,
            gPositions[k0].z
        );

        // 두 번째 정점
        glNormal3f(
            gNormals[k1].x,
            gNormals[k1].y,
            gNormals[k1].z
        );
        glVertex3f(
            gPositions[k1].x,
            gPositions[k1].y,
            gPositions[k1].z
        );

        // 세 번째 정점
        glNormal3f(
            gNormals[k2].x,
            gNormals[k2].y,
            gNormals[k2].z
        );
        glVertex3f(
            gPositions[k2].x,
            gPositions[k2].y,
            gPositions[k2].z
        );
    }
    glEnd();
}

//====================================================
// 14) main(): GLUT + GLEW 초기화, 콜백 등록, 메인 루프
int main(int argc, char** argv) {
    // GLUT 초기화
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE | GLUT_DEPTH);
    glutInitWindowSize(WINDOW_WIDTH, WINDOW_HEIGHT);
    glutCreateWindow("OpenGL Bunny");

    // GLEW 초기화
    GLenum err = glewInit();
    if (GLEW_OK != err) {
        fprintf(stderr, "GLEW Error: %s\n", glewGetErrorString(err));
        return -1;
    }

    // 깊이버퍼 활성화
    glEnable(GL_DEPTH_TEST);
    // Back-face Culling 비활성화 (hw8 요구사항)
    glDisable(GL_CULL_FACE);

    // ▶ 여기서 한 번만 초기 투영 매트릭스를 세팅해 주자
    reshape(WINDOW_WIDTH, WINDOW_HEIGHT);

    // 씬 초기화 (메시, 조명, 타이머)
    initScene();

    // 콜백 등록
    glutDisplayFunc(display);
    glutReshapeFunc(reshape);

    // GLUT 메인 루프
    glutMainLoop();
    return 0;
}
