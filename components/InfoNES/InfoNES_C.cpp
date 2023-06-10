#include "InfoNES.h"
#include "InfoNES_C.h"

extern WORD nesScreenBuffer[2][NES_DISP_HEIGHT][NES_DISP_WIDTH];
extern WORD nesSoundBuffer[2][1024];

extern bool screenIndex;

void InfoNES_Load_C(){
    InfoNES_Load(nullptr);
}

void InfoNES_Init_C(){
    InfoNES_Init();
}

void InfoNES_Cycle_C(){
    InfoNES_Cycle();
}

WORD * InfoNES_GetScreenBuffer_C(){
    return (WORD *)nesScreenBuffer[!screenIndex];
}

WORD * InfoNES_GetSoundBuffer_C(){
    return (WORD *)nesSoundBuffer[!screenIndex];
}