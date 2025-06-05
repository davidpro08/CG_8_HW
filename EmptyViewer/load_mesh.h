// load_mesh.h
#pragma once
#include <vector>

// obj 로딩 시 사용하는 벡터형 (헤더에 미리 정의 혹은 Vector3.h 포함)
struct Vector3 {
    float x, y, z;
};

struct Triangle {
    unsigned int indices[3];
};

// (1) 메시 로드 함수 선언
void tokenize(char* string, std::vector<std::string>& tokens, const char* delimiter);
int face_index(const char* string);
void load_mesh(std::string fileName);

// (2) load_mesh.cpp 안에서 정의된 전역 데이터들 –
//     다른 CPP(예: main_Phong_Shader.cpp)에서 메시 좌표나 노멀, 삼각형 정보에 접근하려면 extern으로 선언해야 함.
extern std::vector<Vector3> gPositions;
extern std::vector<Vector3> gNormals;
extern std::vector<Triangle> gTriangles;