#include "game/phase/phase_photo_mode.h"

#include "game/camera.h"
#include "game/game.h"
#include "game/input.h"
#include "game/interpolation.h"
#include "game/music.h"
#include "game/overlay.h"
#include "game/shell.h"
#include "game/sound.h"
#include "game/text.h"
#include "game/viewport.h"
#include "global/vars.h"

#include <libtrx/game/console/common.h>

#include <stdio.h>

#define MIN_PHOTO_FOV 10
#define MAX_PHOTO_FOV 140
#define LABEL_COUNT 10

typedef enum {
    PS_NONE,
    PS_ACTIVE,
    PS_COOLDOWN,
} PHOTO_STATUS;

static int32_t m_OldFOV;
static int32_t m_CurrentFOV;

static PHOTO_STATUS m_Status = PS_NONE;
static bool m_ShowHelp = true;
static TEXTSTRING *m_Labels[LABEL_COUNT];
static BAR_INFO m_SpeedBar = { 0 };

static const char *const m_LabelStrs[LABEL_COUNT] = {
    // clang-format off
    "Photo Mode",
    "QEWASD: Move camera",
    "ARROWS: Rotate camera",
    "STEP L/R: Roll camera",
    "ROLL: Rotate 90 degrees",
    "JUMP/WALK: Adjust FOV",
    "LOOK: Reset camera",
    "TAB: Toggle help",
    "ACTION: Take picture",
    "F1/ESC: Exit",
    // clang-format on
};

static void M_UpdateUI(void);
static void M_Start(void *arg);
static void M_End(void);
static PHASE_CONTROL M_Control(int32_t nframes);
static void M_Draw(void);
static void M_AdjustFOV(void);

static void M_Start(void *arg)
{
    m_Status = PS_NONE;
    g_OldInputDB = g_Input;
    m_OldFOV = Viewport_GetFOV();
    m_CurrentFOV = m_OldFOV;

    Overlay_HideGameInfo();
    Music_Pause();
    Sound_PauseAll();

    const int16_t x = 21;
    int16_t y = 50;
    m_Labels[0] = Text_Create(x, y, m_LabelStrs[0]);
    y += 25;

    for (int32_t i = 1; i < LABEL_COUNT; i++) {
        m_Labels[i] = Text_Create(x, y, m_LabelStrs[i]);
        y += 20;
    }

    m_SpeedBar.type = BT_PROGRESS;
    m_SpeedBar.value = 0;
    m_SpeedBar.max_value = Camera_GetPhotoMaxSpeed();
    m_SpeedBar.show = true;
    m_SpeedBar.blink = false;
    m_SpeedBar.timer = 0;
    m_SpeedBar.color = g_Config.enemy_healthbar_color;
    m_SpeedBar.location = BL_BOTTOM_CENTER;

    M_UpdateUI();
    if (!m_ShowHelp) {
        Console_Log(
            "Entering Photo Mode...\nPress TAB for help\nPress F1 to exit");
    }
}

static void M_UpdateUI(void)
{
    for (int32_t i = 0; i < LABEL_COUNT; i++) {
        Text_Hide(m_Labels[i], !m_ShowHelp);
    }
}

static void M_End(void)
{
    g_Input = g_OldInputDB;
    Viewport_SetFOV(m_OldFOV);

    Music_Unpause();
    Sound_UnpauseAll();

    for (int32_t i = 0; i < LABEL_COUNT; i++) {
        Text_Remove(m_Labels[i]);
        m_Labels[i] = NULL;
    }
}

static PHASE_CONTROL M_Control(int32_t nframes)
{
    if (m_Status == PS_ACTIVE) {
        Shell_MakeScreenshot();
        Sound_Effect(SFX_MENU_CHOOSE, NULL, SPM_ALWAYS);
        m_Status = PS_COOLDOWN;
    } else if (m_Status == PS_COOLDOWN) {
        m_Status = PS_NONE;
    }

    Input_Update();
    Shell_ProcessInput();

    if (g_InputDB.toggle_photo_mode || g_InputDB.menu_back) {
        Phase_Set(PHASE_GAME, NULL);
    } else {
        if (g_InputDB.photo_mode_help) {
            m_ShowHelp = !m_ShowHelp;
            M_UpdateUI();
        }

        if (g_InputDB.action) {
            m_Status = PS_ACTIVE;
        } else {
            M_AdjustFOV();
            Camera_Update();
            m_SpeedBar.value = Camera_GetPhotoCurrentSpeed();
            m_SpeedBar.show = m_SpeedBar.value > 0;
        }
    }

    return (PHASE_CONTROL) { .end = false };
}

static void M_AdjustFOV(void)
{
    if (g_InputDB.look) {
        Viewport_SetFOV(m_OldFOV);
        m_CurrentFOV = m_OldFOV / PHD_DEGREE;
        return;
    }

    if (!(g_Input.jump ^ g_Input.slow)) {
        return;
    }

    if (g_Input.jump) {
        m_CurrentFOV++;
    } else {
        m_CurrentFOV--;
    }
    CLAMP(m_CurrentFOV, MIN_PHOTO_FOV, MAX_PHOTO_FOV);
    Viewport_SetFOV(m_CurrentFOV * PHD_DEGREE);
}

static void M_Draw(void)
{
    Interpolation_Disable();
    Game_DrawScene(false);
    Interpolation_Enable();

    if (m_Status != PS_NONE) {
        return;
    }

    Text_Draw();
    if (m_SpeedBar.show) {
        Overlay_BarDraw(&m_SpeedBar, RSR_BAR);
    }
}

PHASER g_PhotoModePhaser = {
    .start = M_Start,
    .end = M_End,
    .control = M_Control,
    .draw = M_Draw,
    .wait = NULL,
};
