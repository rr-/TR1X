#include "specific/s_output.h"

#include "3dsystem/3d_gen.h"
#include "3dsystem/phd_math.h"
#include "config.h"
#include "filesystem.h"
#include "global/const.h"
#include "global/types.h"
#include "global/vars.h"
#include "global/vars_platform.h"
#include "log.h"
#include "memory.h"
#include "specific/s_clock.h"
#include "specific/s_display.h"
#include "specific/s_frontend.h"
#include "specific/s_hwr.h"
#include "specific/s_main.h"

#include <assert.h>
#include <math.h>
#include <string.h>

#define VISIBLE(vn1, vn2, vn3)                                                                            \
    ((int32_t)(((vn3->xs - vn2->xs) * (vn1->ys - vn2->ys)) - ((vn1->xs - vn2->xs) * (vn3->ys - vn2->ys))) \
     >= 0)

static bool DecompPCX(const char *pcx, size_t pcx_size, PICTURE *pic);

int32_t GetRenderHeightDownscaled()
{
    return PhdWinHeight * PHD_ONE / GetRenderScale(PHD_ONE);
}

int32_t GetRenderWidthDownscaled()
{
    return PhdWinWidth * PHD_ONE / GetRenderScale(PHD_ONE);
}

int32_t GetRenderHeight()
{
    return PhdWinHeight;
}

int32_t GetRenderWidth()
{
    return PhdWinWidth;
}

int32_t GetRenderScale(int32_t unit)
{
    int32_t baseWidth = 640;
    int32_t baseHeight = 480;
    int32_t scale_x = PhdWinWidth > baseWidth
        ? ((double)PhdWinWidth * unit * T1MConfig.ui.text_scale) / baseWidth
        : unit * T1MConfig.ui.text_scale;
    int32_t scale_y = PhdWinHeight > baseHeight
        ? ((double)PhdWinHeight * unit * T1MConfig.ui.text_scale) / baseHeight
        : unit * T1MConfig.ui.text_scale;
    return MIN(scale_x, scale_y);
}

int32_t GetRenderScaleGLRage(int32_t unit)
{
    // GLRage-style UI scaler
    double result = PhdWinWidth;
    result *= unit;
    result /= 800.0;

    // only scale up, not down
    if (result < unit) {
        result = unit;
    }

    return round(result);
}

void S_InitialisePolyList()
{
    phd_InitPolyList();
}

int32_t S_DumpScreen()
{
    HWR_DumpScreen();
    WinSpinMessageLoop();
    FPSCounter++;
    return ClockSyncTicks(TICKS_PER_FRAME);
}

void S_ClearScreen()
{
    HWR_ClearSurfaceDepth();
}

void S_OutputPolyList()
{
    HWR_OutputPolyList();
}

void S_InitialiseScreen()
{
    if (CurrentLevel != GF.title_level_num) {
        TempVideoRemove();
    }
}

void S_CalculateLight(int32_t x, int32_t y, int32_t z, int16_t room_num)
{
    ROOM_INFO *r = &RoomInfo[room_num];

    if (r->num_lights == 0) {
        LsAdder = r->ambient;
        LsDivider = 0;
    } else {
        int32_t ambient = 0x1FFF - r->ambient;
        int32_t brightest = 0;

        PHD_VECTOR ls = { 0, 0, 0 };
        for (int i = 0; i < r->num_lights; i++) {
            LIGHT_INFO *light = &r->light[i];
            PHD_VECTOR lc;
            lc.x = x - light->x;
            lc.y = y - light->y;
            lc.z = z - light->z;

            int32_t distance =
                (SQUARE(lc.x) + SQUARE(lc.y) + SQUARE(lc.z)) >> 12;
            int32_t falloff = SQUARE(light->falloff) >> 12;
            int32_t shade =
                ambient + (light->intensity * falloff) / (distance + falloff);

            if (shade > brightest) {
                brightest = shade;
                ls = lc;
            }
        }

        LsAdder = (ambient + brightest) / 2;
        if (brightest == ambient) {
            LsDivider = 0;
        } else {
            LsDivider = (1 << (W2V_SHIFT + 12)) / (brightest - LsAdder);
        }
        LsAdder = 0x1FFF - LsAdder;

        PHD_ANGLE angles[2];
        phd_GetVectorAngles(ls.x, ls.y, ls.z, angles);
        phd_RotateLight(angles[1], angles[0]);
    }

    int32_t distance = PhdMatrixPtr->_23 >> W2V_SHIFT;
    LsAdder += phd_CalculateFogShade(distance);
    CLAMPG(LsAdder, 0x1FFF);
}

void S_CalculateStaticLight(int16_t adder)
{
    LsAdder = adder - 16 * 256;
    int32_t distance = PhdMatrixPtr->_23 >> W2V_SHIFT;
    LsAdder += phd_CalculateFogShade(distance);
    CLAMPG(LsAdder, 0x1FFF);
}

void S_SetupBelowWater(bool underwater)
{
    IsWaterEffect = true;
    IsWibbleEffect = !underwater;
    IsShadeEffect = true;
}

void S_SetupAboveWater(bool underwater)
{
    IsWaterEffect = false;
    IsWibbleEffect = underwater;
    IsShadeEffect = underwater;
}

void S_AnimateTextures(int32_t ticks)
{
    WibbleOffset = (WibbleOffset + ticks) % WIBBLE_SIZE;

    static int32_t tick_comp = 0;
    tick_comp += ticks;

    while (tick_comp > TICKS_PER_FRAME * 5) {
        int16_t *ptr = AnimTextureRanges;
        int16_t i = *ptr++;
        while (i > 0) {
            int16_t j = *ptr++;
            PHD_TEXTURE temp = PhdTextureInfo[*ptr];
            while (j > 0) {
                PhdTextureInfo[ptr[0]] = PhdTextureInfo[ptr[1]];
                j--;
                ptr++;
            }
            PhdTextureInfo[*ptr] = temp;
            i--;
            ptr++;
        }
        tick_comp -= TICKS_PER_FRAME * 5;
    }
}

static bool DecompPCX(const char *pcx, size_t pcx_size, PICTURE *pic)
{
    PCX_HEADER *header = (PCX_HEADER *)pcx;
    pic->width = header->x_max - header->x_min + 1;
    pic->height = header->y_max - header->y_min + 1;

    if (header->manufacturer != 10 || header->version < 5 || header->bpp != 8
        || header->rle != 1 || header->planes != 1 || !pic->width
        || !pic->height) {
        return false;
    }

    pic->data = Memory_Alloc(pic->width * pic->height * sizeof(RGB888));

    RGB888 pal[256];
    {
        const uint8_t *src = (uint8_t *)pcx + pcx_size - sizeof(RGB888) * 256;
        for (int i = 0; i < 256; i++) {
            pal[i].r = ((*src++) >> 2) & 0x3F;
            pal[i].g = ((*src++) >> 2) & 0x3F;
            pal[i].b = ((*src++) >> 2) & 0x3F;
        }
    }

    {
        const int32_t stride = pic->width + pic->width % 2;
        const uint8_t *src = (uint8_t *)pcx + sizeof(PCX_HEADER);
        RGB888 *dst = pic->data;
        int32_t x = 0;
        int32_t y = 0;

        while (y < pic->height) {
            if ((*src & 0xC0) == 0xC0) {
                uint8_t n = (*src++) & 0x3F;
                uint8_t c = *src++;
                if (n > 0) {
                    if (x < pic->width) {
                        CLAMPG(n, pic->width - x);
                        for (int i = 0; i < n; i++) {
                            *dst++ = pal[c];
                        }
                    }
                    x += n;
                }
            } else {
                *dst++ = pal[*src++];
                x++;
            }
            if (x >= stride) {
                x = 0;
                y++;
            }
        }
    }

    return true;
}

void S_DisplayPicture(const char *file_stem)
{
    char file_name[128];
    strcpy(file_name, file_stem);
    strcat(file_name, ".pcx");

    char *file_data = NULL;
    size_t file_size = 0;
    File_Load(file_name, &file_data, &file_size);

    PICTURE pic = { 0 };
    if (!DecompPCX(file_data, file_size, &pic)) {
        LOG_ERROR("failed to decompress PCX %s", file_name);
    }

    Memory_Free(file_data);

    HWR_DownloadPicture(&pic);

    if (pic.data) {
        Memory_Free(pic.data);
    }
}

void S_DrawLightningSegment(
    int32_t x1, int32_t y1, int32_t z1, int32_t x2, int32_t y2, int32_t z2,
    int32_t width)
{
    if (z1 >= phd_GetNearZ() && z1 <= phd_GetFarZ() && z2 >= phd_GetNearZ()
        && z2 <= phd_GetFarZ()) {
        x1 = PhdWinCenterX + x1 / (z1 / PhdPersp);
        y1 = PhdWinCenterY + y1 / (z1 / PhdPersp);
        x2 = PhdWinCenterX + x2 / (z2 / PhdPersp);
        y2 = PhdWinCenterY + y2 / (z2 / PhdPersp);
        int32_t thickness1 = (width << W2V_SHIFT) / (z1 / PhdPersp);
        int32_t thickness2 = (width << W2V_SHIFT) / (z2 / PhdPersp);
        HWR_DrawLightningSegment(
            x1, y1, z1, thickness1, x2, y2, z2, thickness2);
    }
}

void S_PrintShadow(int16_t size, int16_t *bptr, ITEM_INFO *item)
{
    int i;

    ShadowInfo.vertex_count = T1MConfig.enable_round_shadow ? 32 : 8;

    int32_t x0 = bptr[FRAME_BOUND_MIN_X];
    int32_t x1 = bptr[FRAME_BOUND_MAX_X];
    int32_t z0 = bptr[FRAME_BOUND_MIN_Z];
    int32_t z1 = bptr[FRAME_BOUND_MAX_Z];

    int32_t x_mid = (x0 + x1) / 2;
    int32_t z_mid = (z0 + z1) / 2;

    int32_t x_add = (x1 - x0) * size / 1024;
    int32_t z_add = (z1 - z0) * size / 1024;

    for (i = 0; i < ShadowInfo.vertex_count; i++) {
        int32_t angle = (PHD_180 + i * PHD_360) / ShadowInfo.vertex_count;
        ShadowInfo.vertex[i].x = x_mid + (x_add * 2) * phd_sin(angle) / PHD_90;
        ShadowInfo.vertex[i].z = z_mid + (z_add * 2) * phd_cos(angle) / PHD_90;
        ShadowInfo.vertex[i].y = 0;
    }

    phd_PushMatrix();
    phd_TranslateAbs(item->pos.x, item->floor, item->pos.z);
    phd_RotY(item->pos.y_rot);

    if (calc_object_vertices(&ShadowInfo.poly_count)) {
        int16_t clip_and = 1;
        int16_t clip_positive = 1;
        int16_t clip_or = 0;
        for (i = 0; i < ShadowInfo.vertex_count; i++) {
            clip_and &= PhdVBuf[i].clip;
            clip_positive &= PhdVBuf[i].clip >= 0;
            clip_or |= PhdVBuf[i].clip;
        }
        PHD_VBUF *vn1 = &PhdVBuf[0];
        PHD_VBUF *vn2 = &PhdVBuf[T1MConfig.enable_round_shadow ? 4 : 1];
        PHD_VBUF *vn3 = &PhdVBuf[T1MConfig.enable_round_shadow ? 8 : 2];

        int visible = VISIBLE(vn1, vn2, vn3);

        if (!clip_and && clip_positive && visible) {
            HWR_PrintShadow(
                &PhdVBuf[0], clip_or ? 1 : 0, ShadowInfo.vertex_count);
        }
    }

    phd_PopMatrix();
}

int S_GetObjectBounds(int16_t *bptr)
{
    if (PhdMatrixPtr->_23 >= phd_GetFarZ()) {
        return 0;
    }

    int32_t x_min = bptr[0];
    int32_t x_max = bptr[1];
    int32_t y_min = bptr[2];
    int32_t y_max = bptr[3];
    int32_t z_min = bptr[4];
    int32_t z_max = bptr[5];

    PHD_VECTOR vtx[8];
    vtx[0].x = x_min;
    vtx[0].y = y_min;
    vtx[0].z = z_min;
    vtx[1].x = x_max;
    vtx[1].y = y_min;
    vtx[1].z = z_min;
    vtx[2].x = x_max;
    vtx[2].y = y_max;
    vtx[2].z = z_min;
    vtx[3].x = x_min;
    vtx[3].y = y_max;
    vtx[3].z = z_min;
    vtx[4].x = x_min;
    vtx[4].y = y_min;
    vtx[4].z = z_max;
    vtx[5].x = x_max;
    vtx[5].y = y_min;
    vtx[5].z = z_max;
    vtx[6].x = x_max;
    vtx[6].y = y_max;
    vtx[6].z = z_max;
    vtx[7].x = x_min;
    vtx[7].y = y_max;
    vtx[7].z = z_max;

    int num_z = 0;
    x_min = 0x3FFFFFFF;
    y_min = 0x3FFFFFFF;
    x_max = -0x3FFFFFFF;
    y_max = -0x3FFFFFFF;

    for (int i = 0; i < 8; i++) {
        int32_t zv = PhdMatrixPtr->_20 * vtx[i].x + PhdMatrixPtr->_21 * vtx[i].y
            + PhdMatrixPtr->_22 * vtx[i].z + PhdMatrixPtr->_23;

        if (zv > phd_GetNearZ() && zv < phd_GetFarZ()) {
            ++num_z;
            int32_t zp = zv / PhdPersp;
            int32_t xv =
                (PhdMatrixPtr->_00 * vtx[i].x + PhdMatrixPtr->_01 * vtx[i].y
                 + PhdMatrixPtr->_02 * vtx[i].z + PhdMatrixPtr->_03)
                / zp;
            int32_t yv =
                (PhdMatrixPtr->_10 * vtx[i].x + PhdMatrixPtr->_11 * vtx[i].y
                 + PhdMatrixPtr->_12 * vtx[i].z + PhdMatrixPtr->_13)
                / zp;

            if (x_min > xv) {
                x_min = xv;
            } else if (x_max < xv) {
                x_max = xv;
            }

            if (y_min > yv) {
                y_min = yv;
            } else if (y_max < yv) {
                y_max = yv;
            }
        }
    }

    x_min += PhdWinCenterX;
    x_max += PhdWinCenterX;
    y_min += PhdWinCenterY;
    y_max += PhdWinCenterY;

    if (!num_z || x_min > PhdRight || y_min > PhdBottom || x_max < PhdLeft
        || y_max < PhdTop) {
        return 0; // out of screen
    }

    if (num_z < 8 || x_min < 0 || y_min < 0 || x_max > PhdWinMaxX
        || y_max > PhdWinMaxY) {
        return -1; // clipped
    }

    return 1; // fully on screen
}