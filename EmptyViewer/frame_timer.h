// frame_timer.h
#pragma once

// (1) 타이머 관련 함수 선언
void init_timer();       // 타이머 초기화
void start_timing();     // 렌더링 직전에 타이머 시작
float stop_timing();     // 렌더링 직후에 호출하면 지난 시간을 초 단위로 반환
void display();

// (2) 프레임 카운터 및 누적 시간 전역 변수 (만약 외부에서 직접 접근해야 할 경우)
extern int   gTotalFrames;
extern float gTotalTimeElapsed;
extern GLuint gTimer;