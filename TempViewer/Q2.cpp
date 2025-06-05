//=============================================================================
// 파일명: Q2.cpp
//   - Q2: Vertex Array / glDrawElements 방식으로 토끼(bunny) 그리기
//=============================================================================

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
#include <fstream>
#include <float.h>

using namespace glm;

//=============================================================================
// 3) 함수 프로토타입(선언): Q2에 필요한 함수들만 모아두었습니다.
void initScene();
void load_mesh(const std::string& fileName);
void setupVertexArrays();
void init_timer();
void start_timing();
float stop_timing();
void display();
void reshape(int w, int h);
void applyTransforms();
void setupLighting();
void drawBunnyWithArrays();

//=============================================================================
// 4) 전역 상수, 전역 변수

const int WINDOW_WIDTH = 1280;
const int WINDOW_HEIGHT = 1280;

// OBJ 로드용 임시 구조체
struct Vector3 {
    float x, y, z;
};
struct Triangle {
    unsigned int indices[3];
};

// OBJ 파싱 후 원시 저장 공간 (Q1과 동일)
std::vector<Vector3>  gPositions;    // "v" 로 읽은 정점 좌표 (x,y,z)
std::vector<Vector3>  gNormals;      // "vn" 로 읽은 노멀 벡터 (x,y,z)
std::vector<Triangle> gTriangles;    // "f" 로 읽은 삼각형(face) 인덱스

// Q2 전용: GPU에 보낼 연속형 배열들
std::vector<GLfloat>  gVertexArray;   // [ x0, y0, z0,  x1, y1, z1,  ... ] 순서
std::vector<GLfloat>  gNormalArray;   // [ nx0, ny0, nz0,  nx1, ny1, nz1, ... ] 순서
std::vector<GLuint>   gIndexArray;    // 삼각형을 구성하는 정점 인덱스 리스트 [ i0, i1, i2,  i3, i4, i5, ... ]

// 타이머 / FPS 계산용
float   gTotalTimeElapsed = 0;
int     gTotalFrames = 0;
GLuint  gTimer;

//=============================================================================
// 5) initScene: 메시 로드 + 정점·노멀 배열 구성 + 조명 설정 + 타이머 초기화
void initScene() {
    // (1) OBJ 메시 로드 ("bunny.obj" 파일이 실행 파일과 동일 디렉토리에 있어야 합니다)
    load_mesh("bunny.obj");

    // (2) Q2 용: gPositions, gNormals, gTriangles 를 연속형 배열로 변환
    setupVertexArrays();

    // (3) 조명 설정 (고정 파이프라인)
    setupLighting();

    // (4) 타이머 초기화
    init_timer();
}

//=============================================================================
// 6) load_mesh: .obj 파일에서 정점, 노멀, 삼각형 정보 읽어오기
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

        // 공백(" ")을 기준으로 토큰 분리
        std::vector<std::string> tokens;
        char* cstr = _strdup(line);
        {
            char* token = strtok(cstr, " ");
            while (token) {
                tokens.push_back(std::string(token));
                token = strtok(NULL, " ");
            }
        }
        free(cstr);

        if (tokens[0] == "v") {
            // 정점 좌표
            float x = atof(tokens[1].c_str());
            float y = atof(tokens[2].c_str());
            float z = atof(tokens[3].c_str());
            xmin = std::min(x, xmin);  xmax = std::max(x, xmax);
            ymin = std::min(y, ymin);  ymax = std::max(y, ymax);
            zmin = std::min(z, zmin);  zmax = std::max(z, zmax);
            gPositions.push_back({ x, y, z });
        }
        else if (tokens[0] == "vn") {
            // 노멀 벡터
            float x = atof(tokens[1].c_str());
            float y = atof(tokens[2].c_str());
            float z = atof(tokens[3].c_str());
            gNormals.push_back({ x, y, z });
        }
        else if (tokens[0] == "f") {
            // 삼각형 face 정보: "f v1//n1 v2//n2 v3//n3"
            // 여기서는 “v와 n 번호가 항상 같도록” .obj가 만들어졌다는 가정
            auto face_index = [&](const std::string& str) -> unsigned int {
                // "12//12" 같은 형태 → “/”로 토큰 분리해서 앞뒤 정수 값 비교
                int len = (int)str.size();
                char* copy = new char[len + 1];
                strcpy(copy, str.c_str());
                std::vector<std::string> sub;
                char* tok = strtok(copy, "/");
                while (tok) {
                    sub.push_back(std::string(tok));
                    tok = strtok(NULL, "/");
                }
                delete[] copy;
                if (sub.size() >= 2 && atoi(sub[0].c_str()) == atoi(sub.back().c_str())) {
                    return (unsigned int)atoi(sub[0].c_str());
                }
                else {
                    printf("ERROR: Bad face specifier!\n");
                    exit(0);
                }
                };

            unsigned int a = face_index(tokens[1]);
            unsigned int b = face_index(tokens[2]);
            unsigned int c = face_index(tokens[3]);
            gTriangles.push_back({ a - 1, b - 1, c - 1 });
        }
    }

    fin.close();
    printf("Loaded mesh from %s. (%zu vertices, %zu normals, %zu triangles)\n",
        fileName.c_str(), gPositions.size(), gNormals.size(), gTriangles.size());
    printf("Mesh bounding box: [%.4f, %.4f, %.4f] ~ [%.4f, %.4f, %.4f]\n",
        xmin, ymin, zmin, xmax, ymax, zmax);
}

//=============================================================================
// 7) setupVertexArrays: gPositions, gNormals, gTriangles 를 연속형 배열로 변환
void setupVertexArrays() {
    // gVertexArray: 각 정점의 x,y,z 순서대로
    gVertexArray.resize(gPositions.size() * 3);
    for (size_t i = 0; i < gPositions.size(); ++i) {
        gVertexArray[i * 3 + 0] = gPositions[i].x;
        gVertexArray[i * 3 + 1] = gPositions[i].y;
        gVertexArray[i * 3 + 2] = gPositions[i].z;
    }

    // gNormalArray: 각 정점 노멀의 x,y,z 순서대로
    gNormalArray.resize(gNormals.size() * 3);
    for (size_t i = 0; i < gNormals.size(); ++i) {
        gNormalArray[i * 3 + 0] = gNormals[i].x;
        gNormalArray[i * 3 + 1] = gNormals[i].y;
        gNormalArray[i * 3 + 2] = gNormals[i].z;
    }

    // gIndexArray: 삼각형 인덱스 (GLuint)
    gIndexArray.resize(gTriangles.size() * 3);
    for (size_t i = 0; i < gTriangles.size(); ++i) {
        gIndexArray[i * 3 + 0] = gTriangles[i].indices[0];
        gIndexArray[i * 3 + 1] = gTriangles[i].indices[1];
        gIndexArray[i * 3 + 2] = gTriangles[i].indices[2];
    }
}

//=============================================================================
// 8) 타이머 및 GPU 타임 쿼리 함수들
void init_timer() {
    glGenQueries(1, &gTimer);
}

void start_timing() {
    glBeginQuery(GL_TIME_ELAPSED, gTimer);
}

float stop_timing() {
    glEndQuery(GL_TIME_ELAPSED);
    GLint available = GL_FALSE;
    while (available == GL_FALSE) {
        glGetQueryObjectiv(gTimer, GL_QUERY_RESULT_AVAILABLE, &available);
    }
    GLint result = 0;
    glGetQueryObjectiv(gTimer, GL_QUERY_RESULT, &result);
    // result는 나노초 단위
    return (float)result / (1000.0f * 1000.0f * 1000.0f);
}

//=============================================================================
// 9) display 콜백 (Q2: glDrawElements 방식 렌더링)
void display() {
    // 1) 화면/깊이버퍼 초기화
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // 2) 모델뷰 행렬 초기화 & 카메라(뷰) 설정
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    gluLookAt(
        0.0, 0.0, 0.0,    // eye=(0,0,0)
        0.0, 0.0, -1.0,   // center=(0,0,-1)을 바라봄
        0.0, 1.0, 0.0     // up=(0,1,0)
    );

    // 3) 조명 설정 (카메라 변환 후)
    setupLighting();

    // 4) 모델링 변환: “translate 먼저 → scale 나중” (Q1 조건과 동일)
    glTranslatef(0.1f, -1.0f, -1.5f);
    glScalef(10.0f, 10.0f, 10.0f);

    // 5) 타이머 시작
    start_timing();

    // 6) Vertex Array + glDrawElements 방식으로 버니 그리기
    drawBunnyWithArrays();

    // 7) 타이머 정지 → FPS 계산
    float t_elapsed = stop_timing();
    gTotalFrames++;
    gTotalTimeElapsed += t_elapsed;
    float fps = gTotalFrames / gTotalTimeElapsed;

    // 8) 윈도우 타이틀에 FPS 표시
    char title[128];
    snprintf(title, sizeof(title), "OpenGL Bunny (Vertex Array / glDrawElements): %.2f FPS", fps);
    glutSetWindowTitle(title);

    // 9) 버퍼 스왑 + 다시 그리기 요청
    glutSwapBuffers();
    glutPostRedisplay();
}

//=============================================================================
// 10) reshape 콜백 (윈도우 리사이즈 시 호출)
void reshape(int w, int h) {
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    // 원근 투영: near=0.1, far=1000
    glFrustum(-0.1, 0.1, -0.1, 0.1, 0.1, 1000.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

//=============================================================================
// 11) setupLighting: 고정 파이프라인 조명 설정
void setupLighting() {
    // (1) 전역 ambient 설정
    GLfloat globalAmbient[] = { 0.2f, 0.2f, 0.2f, 1.0f };
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, globalAmbient);

    // (2) Light0 활성화
    glEnable(GL_NORMALIZE);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);

    // (3) Light0 속성: ambient=0, diffuse=1, specular=0
    GLfloat lightAmbient[] = { 0.0f, 0.0f, 0.0f, 1.0f };
    GLfloat lightDiffuse[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    GLfloat lightSpecular[] = { 0.0f, 0.0f, 0.0f, 1.0f };
    // 방향성 조명: (+1, +1, +1), w=0 → 실질적으로 (-1,-1,-1) 방향으로 향하는 광원
    GLfloat lightDir[] = { 1.0f,  1.0f,  1.0f, 0.0f };

    glLightfv(GL_LIGHT0, GL_AMBIENT, lightAmbient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, lightDiffuse);
    glLightfv(GL_LIGHT0, GL_SPECULAR, lightSpecular);
    glLightfv(GL_LIGHT0, GL_POSITION, lightDir);

    // (4) 토끼 재질 설정: ka=kd=(1,1,1), ks=(0,0,0), shininess=0
    GLfloat matAmbient[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    GLfloat matDiffuse[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    GLfloat matSpecular[] = { 0.0f, 0.0f, 0.0f, 1.0f };
    GLfloat matShininess = 0.0f;

    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, matAmbient);
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, matDiffuse);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, matSpecular);
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, matShininess);
}

//=============================================================================
// 12) drawBunnyWithArrays: glVertexPointer / glNormalPointer + glDrawElements 호출
void drawBunnyWithArrays() {
    // (1) 정점 배열 활성화
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(
        3,            // 좌표 3개 (x,y,z)
        GL_FLOAT,     // 타입: 부동 소수점
        0,            // stride = 0 (연속 저장)
        gVertexArray.data()
    );

    // (2) 노멀 배열 활성화
    glEnableClientState(GL_NORMAL_ARRAY);
    glNormalPointer(
        GL_FLOAT,     // 타입: 부동 소수점
        0,            // stride = 0 (연속 저장)
        gNormalArray.data()
    );

    // (3) glDrawElements 로 삼각형 그리기
    glDrawElements(
        GL_TRIANGLES,
        (GLsizei)gIndexArray.size(),    // 총 인덱스 개수
        GL_UNSIGNED_INT,
        gIndexArray.data()
    );

    // (4) 배열 비활성화
    glDisableClientState(GL_NORMAL_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);
}

//=============================================================================
// 13) main(): GLUT + GLEW 초기화, 콜백 등록, 메인 루프
int main(int argc, char** argv) {
    // GLUT 초기화
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE | GLUT_DEPTH);
    glutInitWindowSize(WINDOW_WIDTH, WINDOW_HEIGHT);
    glutCreateWindow("OpenGL Bunny (Q2: Vertex Array / glDrawElements)");

    // GLEW 초기화
    GLenum err = glewInit();
    if (GLEW_OK != err) {
        fprintf(stderr, "GLEW Error: %s\n", glewGetErrorString(err));
        return -1;
    }

    // 깊이버퍼 활성화
    glEnable(GL_DEPTH_TEST);
    // Back-face Culling 비활성화 (과제 요구사항)
    glDisable(GL_CULL_FACE);

    // ▶ 초기 투영 매트릭스 설정 (reshape 호출)
    reshape(WINDOW_WIDTH, WINDOW_HEIGHT);

    // 씬 초기화 (메시, 연속형 배열 구성, 조명, 타이머)
    initScene();

    // 콜백 등록
    glutDisplayFunc(display);
    glutReshapeFunc(reshape);

    // GLUT 메인 루프
    glutMainLoop();
    return 0;
}
