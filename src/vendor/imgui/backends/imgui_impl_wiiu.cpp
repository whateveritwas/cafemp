// dear imgui: Platform Backend for the Wii U
// Copyright (C) 2023 GaryOderNichts
// Copyright (C) 2026 Daniel K. O. (dkosmari)

#include <string>

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_wiiu.h"

#include <coreinit/time.h>
#include <gx2/registers.h>
#include <nn/swkbd.h>

static_assert(sizeof(ImWchar) == 2);

// Wii U Data
struct ImGui_ImplWiiU_Data
{
    nn::swkbd::CreateArg CreateArg{};
    nn::swkbd::AppearArg AppearArg{};
    nn::swkbd::ControllerType LastController{};
    std::u16string swkbdInitialText{};
    OSTime Time{};
    GX2ColorBuffer* ColorBuffer{};
    GX2RenderTarget RenderTarget{};

    bool WantedTextInput{};
    bool WasTouched{};
    ImGuiID OldFocused{};
};

// Backend data stored in io.BackendPlatformUserData
static ImGui_ImplWiiU_Data* ImGui_ImplWiiU_GetBackendData()
{
    return ImGui::GetCurrentContext() ? (ImGui_ImplWiiU_Data*)ImGui::GetIO().BackendPlatformUserData : nullptr;
}

bool ImGui_ImplWiiU_Init()
{
    ImGuiIO& io = ImGui::GetIO();
    IM_ASSERT(io.BackendPlatformUserData == NULL && "Already initialized a platform backend!");

    // Setup backend data
    ImGui_ImplWiiU_Data* bd = IM_NEW(ImGui_ImplWiiU_Data)();
    io.BackendPlatformUserData = (void*)bd;
    io.BackendPlatformName = "imgui_impl_wiiu";
    io.BackendFlags |= ImGuiBackendFlags_HasGamepad;

    // Initialize and create software keyboard
    nn::swkbd::CreateArg createArg;

    createArg.workMemory = ImGui::MemAlloc(nn::swkbd::GetWorkMemorySize(0));
    createArg.fsClient = (FSClient*) ImGui::MemAlloc(sizeof(FSClient));
    if (!createArg.workMemory || !createArg.fsClient)
    {
        ImGui::MemFree(createArg.workMemory);
        ImGui::MemFree(createArg.fsClient);
        return false;
    }

    FSAddClient(createArg.fsClient, FS_ERROR_FLAG_NONE);

    if (!nn::swkbd::Create(createArg))
        return false;

    nn::swkbd::AppearArg appearArg;
    bd->CreateArg = createArg;
    bd->AppearArg = appearArg;

    return true;
}

void     ImGui_ImplWiiU_Shutdown()
{
    ImGui_ImplWiiU_Data* bd = ImGui_ImplWiiU_GetBackendData();
    IM_ASSERT(bd != NULL && "No platform backend to shutdown, or already shutdown?");
    ImGuiIO& io = ImGui::GetIO();

    // Destroy software keyboard
    nn::swkbd::Destroy();
    free(bd->CreateArg.workMemory);
    bd->CreateArg.workMemory = NULL;

    if (bd->CreateArg.fsClient)
    {
        FSDelClient(bd->CreateArg.fsClient, FS_ERROR_FLAG_NONE);
        free(bd->CreateArg.fsClient);
        bd->CreateArg.fsClient = NULL;
    }

    io.BackendPlatformName = NULL;
    io.BackendPlatformUserData = NULL;
    IM_DELETE(bd);
}

static void ImGui_ImplWiiU_UpdateKeyboardInput(ImGui_ImplWiiU_ControllerInput* input)
{
    ImGuiIO& io = ImGui::GetIO();
    ImGuiInputTextState* state = ImGui::GetInputTextState(ImGui::GetActiveID());
    if (!state)
        return;

    if (input->vpad)
        VPADGetTPCalibratedPoint(VPAD_CHAN_0, &input->vpad->tpNormal, &input->vpad->tpNormal);

    nn::swkbd::ControllerInfo controllerInfo;
    controllerInfo.vpad = input->vpad;
    for (int i = 0; i < 4; i++)
        controllerInfo.kpad[i] = input->kpad[i];

    nn::swkbd::Calc(controllerInfo);

    if (nn::swkbd::IsNeedCalcSubThreadFont())
        nn::swkbd::CalcSubThreadFont();

    if (nn::swkbd::IsNeedCalcSubThreadPredict())
        nn::swkbd::CalcSubThreadPredict();

    if (nn::swkbd::IsDecideOkButton(NULL))
    {
        state->ClearText();

        // Add entered text
        const char16_t* string = nn::swkbd::GetInputFormString();
        for (int i = 0; *string; string++)
            io.AddInputCharacterUTF16(string[i]);

        // close keyboard
        nn::swkbd::DisappearInputForm();
    }

    if (nn::swkbd::IsDecideCancelButton(NULL))
        nn::swkbd::DisappearInputForm();
}

static void ImGui_ImplWiiU_UpdateTouchInput(ImGui_ImplWiiU_ControllerInput* input)
{
    if (!input->vpad)
        return;

    ImGui_ImplWiiU_Data* bd = ImGui_ImplWiiU_GetBackendData();
    ImGuiIO& io = ImGui::GetIO();

    VPADTouchData touch;
    VPADGetTPCalibratedPoint(VPAD_CHAN_0, &touch, &input->vpad->tpNormal);

    if (touch.touched)
    {
        io.AddMouseSourceEvent(ImGuiMouseSource_TouchScreen);
        float scale_x = (io.DisplaySize.x / io.DisplayFramebufferScale.x) / 1280.0f;
        float scale_y = (io.DisplaySize.y / io.DisplayFramebufferScale.y) / 720.0f;
        io.AddMousePosEvent(touch.x * scale_x, touch.y * scale_y);
    }

    if (touch.touched != bd->WasTouched)
    {
        io.AddMouseSourceEvent(ImGuiMouseSource_TouchScreen);
        io.AddMouseButtonEvent(ImGuiMouseButton_Left, touch.touched);
        bd->WasTouched = touch.touched;
        bd->LastController = nn::swkbd::ControllerType::DrcGamepad;
    }
}

#define IM_CLAMP(V, MN, MX)     ((V) < (MN) ? (MN) : (V) > (MX) ? (MX) : (V))

static void ImGui_ImplWiiU_UpdateControllerInput(ImGui_ImplWiiU_ControllerInput* input)
{
    ImGui_ImplWiiU_Data* bd = ImGui_ImplWiiU_GetBackendData();
    ImGuiIO& io = ImGui::GetIO();
    if ((io.ConfigFlags & ImGuiConfigFlags_NavEnableGamepad) == 0)
        return;

    uint32_t vpad_buttons = input->vpad ? input->vpad->hold : 0;
    uint32_t wpad_buttons = 0;
    uint32_t classic_buttons = 0;
    uint32_t pro_buttons = 0;

    float stick_l_x = input->vpad ? input->vpad->leftStick.x : 0.0f;
    float stick_l_y = input->vpad ? input->vpad->leftStick.y : 0.0f;
    float stick_r_x = input->vpad ? input->vpad->rightStick.x : 0.0f;
    float stick_r_y = input->vpad ? input->vpad->rightStick.y : 0.0f;

    for (int i = 0; i < 4; i++)
    {
        KPADStatus* kpad = input->kpad[i];
        if (!kpad)
            continue;

        switch (kpad->extensionType)
        {
        case WPAD_EXT_CORE:
        case WPAD_EXT_NUNCHUK:
        case WPAD_EXT_MPLUS:
        case WPAD_EXT_MPLUS_NUNCHUK:
            wpad_buttons |= kpad->hold;
            break;
        case WPAD_EXT_CLASSIC:
        case WPAD_EXT_MPLUS_CLASSIC:
            classic_buttons |= kpad->classic.hold;
            if (classic_buttons & WPAD_CLASSIC_BUTTON_Y)
                bd->LastController = (nn::swkbd::ControllerType) i;

            stick_l_x += kpad->classic.leftStick.x;
            stick_l_y += kpad->classic.leftStick.y;
            stick_r_x += kpad->classic.rightStick.x;
            stick_r_y += kpad->classic.rightStick.y;
            break;
        case WPAD_EXT_PRO_CONTROLLER:
            pro_buttons |= kpad->pro.hold;
            if (pro_buttons & WPAD_PRO_BUTTON_Y)
                bd->LastController = (nn::swkbd::ControllerType) i;

            stick_l_x += kpad->pro.leftStick.x;
            stick_l_y += kpad->pro.leftStick.y;
            stick_r_x += kpad->pro.rightStick.x;
            stick_r_y += kpad->pro.rightStick.y;
            break;
        }
    }

    if (vpad_buttons & VPAD_BUTTON_Y)
        bd->LastController = nn::swkbd::ControllerType::DrcGamepad;

    io.AddKeyEvent(ImGuiKey_GamepadStart, (vpad_buttons & VPAD_BUTTON_PLUS) || (wpad_buttons & WPAD_BUTTON_PLUS) || (classic_buttons & WPAD_CLASSIC_BUTTON_PLUS) || (pro_buttons & WPAD_PRO_BUTTON_PLUS));
    io.AddKeyEvent(ImGuiKey_GamepadBack, (vpad_buttons & VPAD_BUTTON_MINUS) || (wpad_buttons & WPAD_BUTTON_MINUS) || (classic_buttons & WPAD_CLASSIC_BUTTON_MINUS) || (pro_buttons & WPAD_PRO_BUTTON_MINUS));
    io.AddKeyEvent(ImGuiKey_GamepadFaceLeft, (vpad_buttons & VPAD_BUTTON_X) || (classic_buttons & WPAD_CLASSIC_BUTTON_X) || (pro_buttons & WPAD_PRO_BUTTON_X));
    io.AddKeyEvent(ImGuiKey_GamepadFaceRight, (vpad_buttons & VPAD_BUTTON_B) || (wpad_buttons & WPAD_BUTTON_B) || (classic_buttons & WPAD_CLASSIC_BUTTON_B) || (pro_buttons & WPAD_PRO_BUTTON_B));
    io.AddKeyEvent(ImGuiKey_GamepadFaceUp, (vpad_buttons & VPAD_BUTTON_Y) || (classic_buttons & WPAD_CLASSIC_BUTTON_Y) || (pro_buttons & WPAD_PRO_BUTTON_Y));
    io.AddKeyEvent(ImGuiKey_GamepadFaceDown, (vpad_buttons & VPAD_BUTTON_A) || (wpad_buttons & WPAD_BUTTON_A) || (classic_buttons & WPAD_CLASSIC_BUTTON_A) || (pro_buttons & WPAD_PRO_BUTTON_A));
    io.AddKeyEvent(ImGuiKey_GamepadDpadLeft, (vpad_buttons & VPAD_BUTTON_LEFT) || (wpad_buttons & WPAD_BUTTON_LEFT) || (classic_buttons & WPAD_CLASSIC_BUTTON_LEFT) || (pro_buttons & WPAD_PRO_BUTTON_LEFT));
    io.AddKeyEvent(ImGuiKey_GamepadDpadRight, (vpad_buttons & VPAD_BUTTON_RIGHT) || (wpad_buttons & WPAD_BUTTON_RIGHT) || (classic_buttons & WPAD_CLASSIC_BUTTON_RIGHT) || (pro_buttons & WPAD_PRO_BUTTON_RIGHT));
    io.AddKeyEvent(ImGuiKey_GamepadDpadUp, (vpad_buttons & VPAD_BUTTON_UP) || (wpad_buttons & WPAD_BUTTON_UP) || (classic_buttons & WPAD_CLASSIC_BUTTON_UP) || (pro_buttons & WPAD_PRO_BUTTON_UP));
    io.AddKeyEvent(ImGuiKey_GamepadDpadDown, (vpad_buttons & VPAD_BUTTON_DOWN) || (wpad_buttons & WPAD_BUTTON_DOWN) || (classic_buttons & WPAD_CLASSIC_BUTTON_DOWN) || (pro_buttons & WPAD_PRO_BUTTON_DOWN));
    io.AddKeyEvent(ImGuiKey_GamepadL1, (vpad_buttons & VPAD_BUTTON_L) || (classic_buttons & WPAD_CLASSIC_BUTTON_L) || (pro_buttons & WPAD_PRO_TRIGGER_L));
    io.AddKeyEvent(ImGuiKey_GamepadR1, (vpad_buttons & VPAD_BUTTON_R) || (classic_buttons & WPAD_CLASSIC_BUTTON_R) || (pro_buttons & WPAD_PRO_TRIGGER_R));
    io.AddKeyEvent(ImGuiKey_GamepadL2, (vpad_buttons & VPAD_BUTTON_ZL) || (classic_buttons & WPAD_CLASSIC_BUTTON_ZL) || (pro_buttons & WPAD_PRO_TRIGGER_ZL));
    io.AddKeyEvent(ImGuiKey_GamepadR2, (vpad_buttons & VPAD_BUTTON_ZR) || (classic_buttons & WPAD_CLASSIC_BUTTON_ZR) || (pro_buttons & WPAD_PRO_TRIGGER_ZR));
    io.AddKeyEvent(ImGuiKey_GamepadL3, (vpad_buttons & VPAD_BUTTON_STICK_L) || (pro_buttons & WPAD_PRO_BUTTON_STICK_L));
    io.AddKeyEvent(ImGuiKey_GamepadR3, (vpad_buttons & VPAD_BUTTON_STICK_R) || (pro_buttons & WPAD_PRO_BUTTON_STICK_R));

    stick_l_x = IM_CLAMP(stick_l_x, -1.0f, 1.0f);
    io.AddKeyAnalogEvent(ImGuiKey_GamepadLStickLeft, stick_l_x < -0.1f, (stick_l_x < -0.1f) ? (stick_l_x * -1.0f) : 0.0f);
    io.AddKeyAnalogEvent(ImGuiKey_GamepadLStickRight, stick_l_x > 0.1f, (stick_l_x > 0.1f) ? stick_l_x : 0.0f);

    stick_l_y = IM_CLAMP(stick_l_y, -1.0f, 1.0f);
    io.AddKeyAnalogEvent(ImGuiKey_GamepadLStickUp, stick_l_y > 0.1f, (stick_l_y > 0.1f) ? stick_l_y : 0.0f);
    io.AddKeyAnalogEvent(ImGuiKey_GamepadLStickDown, stick_l_y < -0.1f, (stick_l_y < -0.1f) ? (stick_l_y * -1.0f) : 0.0f);

    stick_r_x = IM_CLAMP(stick_r_x, -1.0f, 1.0f);
    io.AddKeyAnalogEvent(ImGuiKey_GamepadRStickLeft, stick_r_x < -0.1f, (stick_r_x < -0.1f) ? (stick_r_x * -1.0f) : 0.0f);
    io.AddKeyAnalogEvent(ImGuiKey_GamepadRStickRight, stick_r_x > 0.1f, (stick_r_x > 0.1f) ? stick_r_x : 0.0f);

    stick_r_y = IM_CLAMP(stick_r_y, -1.0f, 1.0f);
    io.AddKeyAnalogEvent(ImGuiKey_GamepadRStickUp, stick_r_y > 0.1f, (stick_r_y > 0.1f) ? stick_r_y : 0.0f);
    io.AddKeyAnalogEvent(ImGuiKey_GamepadRStickDown, stick_r_y < -0.1f, (stick_r_y < -0.1f) ? (stick_r_y * -1.0f) : 0.0f);
}

bool ImGui_ImplWiiU_ProcessInput(ImGui_ImplWiiU_ControllerInput* input)
{
    ImGui_ImplWiiU_Data* bd = ImGui_ImplWiiU_GetBackendData();
    IM_ASSERT(bd != NULL && "Did you call ImGui_ImplWiiU_Init()?");
    // ImGuiIO& io = ImGui::GetIO();

    // Update keyboard input
    if (nn::swkbd::GetStateInputForm() != nn::swkbd::State::Hidden)
    {
        ImGui_ImplWiiU_UpdateKeyboardInput(input);
        return true;
    }

    // Update touch screen
    ImGui_ImplWiiU_UpdateTouchInput(input);

    // Update gamepads
    ImGui_ImplWiiU_UpdateControllerInput(input);

    return false;
}

static GX2SurfaceFormat ImGui_ImplWiiU_AdjustGammaForSWKBD()
{
    ImGui_ImplWiiU_Data* bd = ImGui_ImplWiiU_GetBackendData();
    IM_ASSERT(bd != NULL && "Did you call ImGui_ImplWiiU_Init()?");

    GX2ColorBuffer* cb = bd->ColorBuffer;
    IM_ASSERT(cb != NULL && "Did you call ImGui_ImplWiiU_NewFrame()?");

    // NOTE: we only try if the surface is similar enough
    if (cb->surface.format == GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8) {
        GX2SurfaceFormat old_format = cb->surface.format;
        cb->surface.format = GX2_SURFACE_FORMAT_SRGB_R8_G8_B8_A8;
        GX2InitColorBufferRegs(cb);
        GX2SetColorBuffer(cb, bd->RenderTarget);
        return old_format;
    }
    return GX2_SURFACE_FORMAT_INVALID;
}

static void ImGui_ImplWiiU_RestoreGammaForSWKBD(GX2SurfaceFormat old_format)
{
    ImGui_ImplWiiU_Data* bd = ImGui_ImplWiiU_GetBackendData();
    IM_ASSERT(bd != NULL && "Did you call ImGui_ImplWiiU_Init()?");

    GX2ColorBuffer* cb = bd->ColorBuffer;
    IM_ASSERT(cb != NULL && "Did you call ImGui_ImplWiiU_NewFrame()?");

    if (!cb || old_format == GX2_SURFACE_FORMAT_INVALID)
        return;
    cb->surface.format = old_format;
    GX2InitColorBufferRegs(cb);
    GX2SetColorBuffer(cb, bd->RenderTarget);
}

void ImGui_ImplWiiU_DrawKeyboardOverlay(ImGui_ImplWiiU_KeyboardOverlayType type)
{
    ImGui_ImplWiiU_Data* bd = ImGui_ImplWiiU_GetBackendData();
    IM_ASSERT(bd != NULL && "Did you call ImGui_ImplWiiU_Init()?");

    if (nn::swkbd::GetStateInputForm() != nn::swkbd::State::Hidden)
    {
        ImGuiIO& io = ImGui::GetIO();
        GX2SetViewport(0, 0, io.DisplaySize.x, io.DisplaySize.y, 0.0f, 1.0f);
        GX2SetScissor(0, 0, io.DisplaySize.x, io.DisplaySize.y);

        GX2SurfaceFormat old_format = ImGui_ImplWiiU_AdjustGammaForSWKBD();
        if (type == ImGui_KeyboardOverlay_Auto)
        {
            if (bd->LastController == nn::swkbd::ControllerType::DrcGamepad)
                nn::swkbd::DrawDRC();
            else
                nn::swkbd::DrawTV();
        }
        else if (type == ImGui_KeyboardOverlay_DRC)
            nn::swkbd::DrawDRC();
        else if (type == ImGui_KeyboardOverlay_TV)
            nn::swkbd::DrawTV();
        ImGui_ImplWiiU_RestoreGammaForSWKBD(old_format);
    }
}

static std::u16string to_utf16(const char* input, int size)
{
    std::u16string result;
    if (size <= 0)
        return result;
    // TODO: if we had ImTextCountUtf16BytesFcalculate how many UTF-16 elements we need, this is overestimating it.
    result.resize(size);
    ImTextStrFromUtf8(reinterpret_cast<ImWchar*>(result.data()),
                      result.size() + 1,
                      input,
                      input + size);
    return result;
}

void ImGui_ImplWiiU_NewFrame(GX2ColorBuffer* cb, GX2RenderTarget target)
{
    ImGui_ImplWiiU_Data* bd = ImGui_ImplWiiU_GetBackendData();
    IM_ASSERT(bd != NULL && "Did you call ImGui_ImplWiiU_Init()?");
    ImGuiIO& io = ImGui::GetIO();

    IM_ASSERT(cb != NULL && "Color buffer cannot be null!");
    bd->ColorBuffer = cb;
    bd->RenderTarget = target;

    // Update io.DeltaTime
    const OSTime now = OSGetSystemTime();
    io.DeltaTime = bd->Time > 0 ? static_cast<float>(now - bd->Time) / static_cast<float>(OSTimerClockSpeed) : 1.0f / 60.0f;
    bd->Time = now;

    // Update io.DisplaySize
    io.DisplaySize.x = bd->ColorBuffer->surface.width;
    io.DisplaySize.y = bd->ColorBuffer->surface.height;

    // Show swkbd if either:
    //   - io.WantTextInput changed to true
    //   - io.WantTextInput is true and focused widget changed
    // Hide swkbd if:
    //   - io.WantTextInput changed to false
    const bool changed_want_text = io.WantTextInput && !bd->WantedTextInput;
    const ImGuiID focused = ImGui::GetFocusID();
    const bool changed_focus = focused != bd->OldFocused;
    bool starting_text_input = false;
    bool stopping_text_input = false;
    if (changed_want_text)
    {
        if (io.WantTextInput)
            starting_text_input = true;
        else
            stopping_text_input = true;
    }
    if (changed_focus)
    {
        if (focused == 0)
            stopping_text_input = true;
        if (io.WantTextInput)
            starting_text_input = true;
    }

    if (starting_text_input)
    {
        ImGuiInputTextState* state = ImGui::GetInputTextState(focused);
        if (state)
        {
            if (!(state->Flags & ImGuiInputTextFlags_AlwaysOverwrite))
            {
                bd->swkbdInitialText = to_utf16(state->TextA.Data, state->TextA.Size);
                bd->AppearArg.inputFormArg.initialText = bd->swkbdInitialText.data();
            }

            bd->AppearArg.inputFormArg.maxTextLength = state->BufCapacity;
            bd->AppearArg.inputFormArg.higlightInitialText = !!(state->Flags & ImGuiInputTextFlags_AutoSelectAll);

            if (state->Flags & ImGuiInputTextFlags_Password)
                bd->AppearArg.inputFormArg.passwordMode = nn::swkbd::PasswordMode::Fade;
            else
                bd->AppearArg.inputFormArg.passwordMode = nn::swkbd::PasswordMode::Clear;

            // Open the keyboard for the controller which requested the text input
            bd->AppearArg.keyboardArg.configArg.controllerType = bd->LastController;

            if (nn::swkbd::GetStateInputForm() == nn::swkbd::State::Hidden)
                nn::swkbd::AppearInputForm(bd->AppearArg);
        }
    }
    if (stopping_text_input)
    {
        nn::swkbd::DisappearInputForm();
    }

    bd->WantedTextInput = io.WantTextInput;
    bd->OldFocused = focused;
}
