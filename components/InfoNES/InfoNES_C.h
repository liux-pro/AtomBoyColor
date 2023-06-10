#include "InfoNES_Types.h"

#ifdef __cplusplus
extern "C" {
#endif

void InfoNES_Load_C();

void InfoNES_Init_C();

void InfoNES_Cycle_C();

WORD * InfoNES_GetScreenBuffer_C();

WORD * InfoNES_GetSoundBuffer_C();
#ifdef __cplusplus
}
#endif
