#include "benchmark.h"
#include "debug.h"
#include "game/anims.h"
#include "game/gamebuf.h"
#include "game/objects/common.h"
#include "log.h"

static ANIM_FRAME *m_Frames = NULL;

static int32_t M_GetAnimFrameCount(int32_t anim_idx);
static OBJECT *M_GetAnimObject(int32_t anim_idx);
static int32_t M_ParseFrame(
    ANIM_FRAME *frame, const int16_t *data_ptr, int16_t mesh_count);
static void M_ParseMeshRotation(XYZ_16 *rot, const int16_t **data);
static void M_ExtractRotation(
    XYZ_16 *rot, int16_t rot_val_1, int16_t rot_val_2);

static int32_t M_GetAnimFrameCount(const int32_t anim_idx)
{
    const ANIM *const anim = Anim_GetAnim(anim_idx);
#if TR_VERSION == 1
    return (int32_t)ceil(
        ((anim->frame_end - anim->frame_base) / (float)anim->interpolation)
        + 1);
#else
    ASSERT_FAIL();
    return 0;
#endif
}

static OBJECT *M_GetAnimObject(const int32_t anim_idx)
{
    for (int32_t i = 0; i < O_NUMBER_OF; i++) {
        OBJECT *const object = Object_GetObject(i);
        if (object->loaded && object->mesh_count >= 0
            && object->anim_idx == anim_idx) {
            return object;
        }
    }

    return NULL;
}

static int32_t M_ParseFrame(
    ANIM_FRAME *const frame, const int16_t *data_ptr, int16_t mesh_count)
{
#if TR_VERSION > 1
    ASSERT_FAIL();
    return 0;
#else
    const int16_t *const frame_start = data_ptr;

    frame->bounds.min.x = *data_ptr++;
    frame->bounds.max.x = *data_ptr++;
    frame->bounds.min.y = *data_ptr++;
    frame->bounds.max.y = *data_ptr++;
    frame->bounds.min.z = *data_ptr++;
    frame->bounds.max.z = *data_ptr++;
    frame->offset.x = *data_ptr++;
    frame->offset.y = *data_ptr++;
    frame->offset.z = *data_ptr++;
    #if TR_VERSION == 1
    mesh_count = *data_ptr++;
    #endif

    frame->mesh_rots =
        GameBuf_Alloc(sizeof(XYZ_16) * mesh_count, GBUF_ANIM_FRAMES);
    for (int32_t i = 0; i < mesh_count; i++) {
        XYZ_16 *const rot = &frame->mesh_rots[i];
        M_ParseMeshRotation(rot, &data_ptr);
    }

    return data_ptr - frame_start;
#endif
}

static void M_ParseMeshRotation(XYZ_16 *const rot, const int16_t **data)
{
    const int16_t *data_ptr = *data;
#if TR_VERSION == 1
    const int16_t rot_val_1 = *data_ptr++;
    const int16_t rot_val_2 = *data_ptr++;
    M_ExtractRotation(rot, rot_val_2, rot_val_1);
#else
    ASSERT_FAIL();
#endif
    *data = data_ptr;
}

static void M_ExtractRotation(
    XYZ_16 *const rot, const int16_t rot_val_1, const int16_t rot_val_2)
{
    rot->x = (rot_val_1 & 0x3FF0) << 2;
    rot->y = (((rot_val_1 & 0xF) << 6) | ((rot_val_2 & 0xFC00) >> 10)) << 6;
    rot->z = (rot_val_2 & 0x3FF) << 6;
}

int32_t Anim_GetTotalFrameCount(void)
{
#if TR_VERSION > 1
    ASSERT_FAIL();
    return 0;
#else
    const int32_t anim_count = Anim_GetTotalCount();
    int32_t total_frame_count = 0;
    for (int32_t i = 0; i < anim_count; i++) {
        total_frame_count += M_GetAnimFrameCount(i);
    }
    return total_frame_count;
#endif
}

void Anim_InitialiseFrames(const int32_t num_frames)
{
#if TR_VERSION > 1
    ASSERT_FAIL();
#else
    LOG_INFO("%d anim frames", num_frames);
    m_Frames = GameBuf_Alloc(sizeof(ANIM_FRAME) * num_frames, GBUF_ANIM_FRAMES);
#endif
}

void Anim_LoadFrames(const int16_t *data, const int32_t data_length)
{
#if TR_VERSION > 1
    ASSERT_FAIL();
#else
    BENCHMARK *const benchmark = Benchmark_Start();

    const int32_t anim_count = Anim_GetTotalCount();
    OBJECT *cur_obj = NULL;
    int32_t frame_idx = 0;

    for (int32_t i = 0; i < anim_count; i++) {
        OBJECT *const next_obj = M_GetAnimObject(i);
        const bool obj_changed = next_obj != NULL;
        if (obj_changed) {
            cur_obj = next_obj;
        }

        if (cur_obj == NULL) {
            continue;
        }

        ANIM *const anim = Anim_GetAnim(i);
        const int32_t frame_count = M_GetAnimFrameCount(i);
        const int16_t *data_ptr = &data[anim->frame_ofs / sizeof(int16_t)];
        for (int32_t j = 0; j < frame_count; j++) {
            ANIM_FRAME *const frame = &m_Frames[frame_idx++];
            if (j == 0) {
                anim->frame_ptr = frame;
                if (obj_changed) {
                    cur_obj->frame_base = frame;
                }
            }

            data_ptr += M_ParseFrame(frame, data_ptr, cur_obj->mesh_count);
        }
    }

    Benchmark_End(benchmark, NULL);
#endif
}
