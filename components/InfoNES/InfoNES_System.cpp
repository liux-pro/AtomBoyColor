#include <esp_log.h>
#include <esp_attr.h>
#include "InfoNES_System.h"
#include "InfoNES.h"
#include "string.h"
#include "stdint.h"
#include "stdio.h"

// 1MB psram memory for rom, if we use const for rom, infoNES will crush, still don't know why
EXT_RAM_BSS_ATTR uint8_t rom[1024*1024];

bool screenIndex = true;
EXT_RAM_BSS_ATTR WORD nesScreenBuffer[2][NES_DISP_HEIGHT][NES_DISP_WIDTH];
WORD nesSoundBuffer[2][1024];
uint16_t nesSoundOffset=0;

/* Pad state */
DWORD dwPad1=0;
DWORD dwPad2=0;
DWORD dwSystem=0;


/* Palette data */
/* 引入调色板，nes内部只有64种颜色，每种占2字节，输出帧的时候只会出现这些颜色
 * 这里把他设置成rgb565*/
//https://zhuanlan.zhihu.com/p/34144965?utm_medium=social&utm_source=qq
//https://www.nesdev.org/pal.txt

#if 0
WORD NesPalette[64] = {0x738e, 0x20d1, 0x15, 0x4013, 0x880e, 0xa802, 0xa000, 0x7840,
                       0x4140, 0x200, 0x280, 0x1c2, 0x19cb, 0x0, 0x0, 0x0,
                       0xbdd7, 0x39d, 0x21dd, 0x801e, 0xb817, 0xe00b, 0xd940, 0xca41,
                       0x8b80, 0x480, 0x540, 0x487, 0x411, 0x0, 0x0, 0x0,
                       0xffdf, 0x3ddf, 0x5c9f, 0x445f, 0xf3df, 0xfb96, 0xfb8c, 0xfcc7,
                       0xf5c7, 0x8682, 0x4ec9, 0x5fd3, 0x75b, 0x0, 0x0, 0x0,
                       0xffdf, 0xaf1f, 0xc69f, 0xd65f, 0xfe1f, 0xfe1b, 0xfdd6, 0xfed5,
                       0xff14, 0xe7d4, 0xaf97, 0xb7d9, 0x9fde, 0x0, 0x0, 0x0};
#else
//颠倒大小端，进行spi发送时候免去交换的操作。
WORD NesPalette[64] = {0x8e73,0xd120,0x1500,0x1340,0xe88,0x2a8,0xa0,0x4078,
                       0x4041,0x2,0x8002,0xc201,0xcb19,0x0,0x0,0x0,
                       0xd7bd,0x9d03,0xdd21,0x1e80,0x17b8,0xbe0,0x40d9,0x41ca,
                       0x808b,0x8004,0x4005,0x8704,0x1104,0x0,0x0,0x0,
                       0xdfff,0xdf3d,0x9f5c,0x5f44,0xdff3,0x96fb,0x8cfb,0xc7fc,
                       0xc7f5,0x8286,0xc94e,0xd35f,0x5b07,0x0,0x0,0x0,
                       0xdfff,0x1faf,0x9fc6,0x5fd6,0x1ffe,0x1bfe,0xd6fd,0xd5fe,
                       0x14ff,0xd4e7,0x97af,0xd9b7,0xde9f,0x0,0x0,0x0};
#endif

/* Menu screen */
//相当于退出按钮，只要返回-1,模拟器就退出其内部循环
//这个东西貌似没用阿，只有在启动模拟器时候会调用一次，谁会在启动时候就推出呢。
int InfoNES_Menu() {
    return 0;
}

/* Read ROM image file */
/*
 * NesHeader ROM VROM 这些都已经被Info定义了，并且在InfoNES.h引入近来了
 */
int InfoNES_ReadRom(const char *pszFileName) {
    uint32_t offset = 0;

    /* Read ROM Header */
    memcpy(&NesHeader, rom, sizeof(NesHeader));
    offset += sizeof(NesHeader);

    if (memcmp(NesHeader.byID, "NES\x1a", 4) != 0) {
        /* not .nes file */
        return -1;
    }

    /* Clear SRAM */
    memset(SRAM, 0, SRAM_SIZE);

    /* If trainer presents Read Triner at 0x7000-0x71ff */
    if (NesHeader.byInfo1 & 4) {
        memcpy(&SRAM[0x1000], rom, 512);
        offset += 512;
    }

    ROM = (BYTE *) &rom[offset];
    offset += (NesHeader.byRomSize * 0x4000);

    if (NesHeader.byVRomSize > 0) {
        VROM = (BYTE *) &rom[offset];
        offset += (NesHeader.byVRomSize * 0x2000);
    }

    return 0;
}

/* Release a memory for ROM */
void InfoNES_ReleaseRom() {
    VROM = NULL;
    VROM = NULL;
    //todo
}

/* Transfer the contents of work frame on the screen */
void InfoNES_LoadFrame() {
    screenIndex = !screenIndex;
    nesSoundOffset=0;
    //没用了
}

/* Get a joypad state */
void InfoNES_PadState(DWORD *pdwPad1, DWORD *pdwPad2, DWORD *pdwSystem) {
    *pdwPad1 = dwPad1;
    *pdwPad2 = dwPad2;
    *pdwSystem = dwSystem;
}


/* memcpy */
void *InfoNES_MemoryCopy(void *dest, const void *src, int count) {
    memcpy(dest, src, count);
    return dest;
}

/* memset */
void *InfoNES_MemorySet(void *dest, int c, int count) {
    memset(dest, c, count);
    return dest;
}

/* Print debug message */
void InfoNES_DebugPrint(char *pszMsg) {
//    fprintf(stderr, "%s\n", pszMsg);
}

//这个指的是过去电视的电子枪扫描线，每一帧这个函数都会被调用240次，也就是换行240次。
void InfoNES_Wait() {
    //没用了
}


/* Sound Initialize */
void InfoNES_SoundInit() {
}


/* Sound Open */
int InfoNES_SoundOpen(int samples_per_sync, int sample_rate) {
    return 1;
}


/* Sound Close */
void InfoNES_SoundClose() {
}



/* Sound Output 5 Waves - 2 Pulse, 1 Triangle, 1 Noise, 1 DPCM */
void InfoNES_SoundOutput(int samples, BYTE *wave1, BYTE *wave2, BYTE *wave3, BYTE *wave4, BYTE *wave5) {
//    ESP_LOGI("i2s  ","%d",samples);

    for (int i = 0; i < samples; ++i) {
        int w1 = *wave1++;
        int w2 = *wave2++;
        int w3 = *wave3++;
        int w4 = *wave4++;
        int w5 = *wave5++;
        //            w3 = w2 = w4 = w5 = 0;
        int l = w1 * 6 + w2 * 6 + w3 * 5 + w4 * 3 * 17 + w5 * 2 * 32;
//        int r = w1 * 3 + w2 * 6 + w3 * 5 + w4 * 3 * 17 + w5 * 2 * 32;
        nesSoundBuffer[screenIndex][nesSoundOffset] = l>>3;
        nesSoundOffset++;
    }
}

/* Print system message */
void InfoNES_MessageBox(char *pszMsg, ...) {
    //todo
};

void InfoNES_PreDrawLine(int line){
//    printf("pre %d \n",line);
    InfoNES_SetLineBuffer(nesScreenBuffer[screenIndex][line], NES_DISP_WIDTH);

}
void InfoNES_PostDrawLine(int line){
//    printf("post %d \n",line);

}
int InfoNES_GetSoundBufferSize(){
//    magic number
    return 256;
}
