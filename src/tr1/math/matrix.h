#pragma once

#include "global/const.h"
#include "global/types.h"

#include <stdbool.h>
#include <stdint.h>

#define TRIGMULT2(A, B) (((A) * (B)) >> W2V_SHIFT)
#define TRIGMULT3(A, B, C) (TRIGMULT2((TRIGMULT2(A, B)), C))

void Matrix_ResetStack(void);
void Matrix_GenerateW2V(const XYZ_32 *pos, const XYZ_16 *rot);

bool Matrix_Push(void);
bool Matrix_PushUnit(void);
void Matrix_Pop(void);

void Matrix_RotX(int16_t rx);
void Matrix_RotY(int16_t ry);
void Matrix_RotZ(int16_t rz);
void Matrix_RotYXZ(int16_t ry, int16_t rx, int16_t rz);
void Matrix_RotXYZ16(const XYZ_16 *rotation);
void Matrix_TranslateRel(int32_t x, int32_t y, int32_t z);
void Matrix_TranslateAbs(int32_t x, int32_t y, int32_t z);
void Matrix_TranslateSet(int32_t x, int32_t y, int32_t z);

void Matrix_Push_I(void);
void Matrix_Pop_I(void);

void Matrix_RotY_I(int16_t ang);
void Matrix_RotX_I(int16_t ang);
void Matrix_RotZ_I(int16_t ang);
void Matrix_RotYXZ_I(int16_t y, int16_t x, int16_t z);
void Matrix_RotXYZ16_I(const XYZ_16 *rotation_1, const XYZ_16 *rotation_2);

void Matrix_TranslateRel_I(int32_t x, int32_t y, int32_t z);
void Matrix_TranslateRel_ID(
    int32_t x, int32_t y, int32_t z, int32_t x2, int32_t y2, int32_t z2);

void Matrix_InitInterpolate(int32_t frac, int32_t rate);
void Matrix_Interpolate(void);
void Matrix_InterpolateArm(void);

void Matrix_LookAt(
    int32_t xsrc, int32_t ysrc, int32_t zsrc, int32_t xtar, int32_t ytar,
    int32_t ztar, int16_t roll);
