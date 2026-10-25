// Wii port modifications, 2026, txmbrs and contributors.
/*
   Copyright 2016 Aldo J. Nunez
   Wii backend implementation
*/

#include "Common.h"
#include "Input.h"

#if defined(GEKKO)
#include <gccore.h>
#include <wiiuse/wpad.h>
#endif

static InputButtons oldInputState( 0 );
static InputButtons inputState( 0 );

InputButtons::InputButtons( uint value )
    :   Buttons( (uint8_t) value )
{
}

bool InputButtons::Has( uint value ) const
{
    return (Buttons & value) != 0;
}

void InputButtons::Mask( uint value )
{
    Buttons &= value;
}

void InputButtons::Clear( uint value )
{
    Buttons = Buttons ^ value;
}

InputButtons Input::GetButtons()
{
    uint buttons = 0;

    buttons = ((oldInputState.Buttons ^ inputState.Buttons) 
        & inputState.Buttons)
        | (inputState.Buttons & 0xF);

    return InputButtons( buttons );
}

bool Input::IsKeyDown( int keyCode )
{
    return false;
}

bool Input::IsKeyPressing( int keyCode )
{
    return false;
}

ButtonState Input::GetKey( int keyCode )
{
    return ButtonState_Lifted;
}

bool Input::IsButtonDown( int buttonCode )
{
    return (inputState.Buttons & buttonCode) != 0;
}

bool Input::IsButtonPressing( int buttonCode )
{
    return GetButton( buttonCode ) == ButtonState_Pressing;
}

ButtonState Input::GetButton( int buttonCode )
{
    int isDown = (inputState.Buttons & buttonCode) ? 1 : 0;
    int wasDown = (oldInputState.Buttons & buttonCode) ? 1 : 0;

    return (ButtonState) ((wasDown << 1) | isDown);
}

Direction Input::GetInputDirection()
{
    if ( IsButtonDown( InputButtons::Left ) )
    {
        return Dir_Left;
    }
    else if ( IsButtonDown( InputButtons::Right ) )
    {
        return Dir_Right;
    }
    else if ( IsButtonDown( InputButtons::Up ) )
    {
        return Dir_Up;
    }
    else if ( IsButtonDown( InputButtons::Down ) )
    {
        return Dir_Down;
    }

    return Dir_None;
}

#if defined(GEKKO)
static void PollWiiPads()
{
    WPAD_ScanPads();
    PAD_ScanPads();

    uint32_t wpadDown = WPAD_ButtonsHeld( 0 );
    uint16_t padDown  = PAD_ButtonsHeld( 0 );

    uint8_t buttons = 0;

    // 1. Wiimote (held horizontally)
    if ( wpadDown & WPAD_BUTTON_RIGHT ) buttons |= InputButtons::Up;
    if ( wpadDown & WPAD_BUTTON_LEFT )  buttons |= InputButtons::Down;
    if ( wpadDown & WPAD_BUTTON_UP )    buttons |= InputButtons::Left;
    if ( wpadDown & WPAD_BUTTON_DOWN )  buttons |= InputButtons::Right;
    if ( wpadDown & WPAD_BUTTON_2 )     buttons |= InputButtons::A;
    if ( wpadDown & WPAD_BUTTON_1 )     buttons |= InputButtons::B;
    if ( wpadDown & WPAD_BUTTON_A )     buttons |= InputButtons::A;
    if ( wpadDown & WPAD_BUTTON_B )     buttons |= InputButtons::B;
    if ( wpadDown & WPAD_BUTTON_PLUS )  buttons |= InputButtons::Start;
    if ( wpadDown & WPAD_BUTTON_MINUS ) buttons |= InputButtons::Select;

    // 2. Wiimote + Nunchuk / Classic Controller extension
    WPADData* wd = WPAD_Data( 0 );
    if ( wd != nullptr )
    {
        if ( wd->exp.type == WPAD_EXP_NUNCHUK )
        {
            // Nunchuk analog stick
            float mag = wd->exp.nunchuk.js.mag;
            float ang = wd->exp.nunchuk.js.ang;
            if ( mag > 0.4f )
            {
                if ( ang > 45.0f && ang < 135.0f )   buttons |= InputButtons::Right;
                else if ( ang > 135.0f && ang < 225.0f ) buttons |= InputButtons::Down;
                else if ( ang > 225.0f && ang < 315.0f ) buttons |= InputButtons::Left;
                else buttons |= InputButtons::Up;
            }

            if ( wd->exp.nunchuk.btns & WPAD_NUNCHUK_BUTTON_Z ) buttons |= InputButtons::B;
            if ( wd->exp.nunchuk.btns & WPAD_NUNCHUK_BUTTON_C ) buttons |= InputButtons::Start;
            if ( wpadDown & WPAD_BUTTON_A )                     buttons |= InputButtons::A;
            if ( wpadDown & WPAD_BUTTON_B )                     buttons |= InputButtons::B;
        }
        else if ( wd->exp.type == WPAD_EXP_CLASSIC )
        {
            uint32_t cDown = wd->exp.classic.btns;
            if ( cDown & WPAD_CLASSIC_BUTTON_UP )    buttons |= InputButtons::Up;
            if ( cDown & WPAD_CLASSIC_BUTTON_DOWN )  buttons |= InputButtons::Down;
            if ( cDown & WPAD_CLASSIC_BUTTON_LEFT )  buttons |= InputButtons::Left;
            if ( cDown & WPAD_CLASSIC_BUTTON_RIGHT ) buttons |= InputButtons::Right;
            if ( cDown & WPAD_CLASSIC_BUTTON_A )     buttons |= InputButtons::A;
            if ( cDown & WPAD_CLASSIC_BUTTON_B )     buttons |= InputButtons::B;
            if ( cDown & WPAD_CLASSIC_BUTTON_PLUS )  buttons |= InputButtons::Start;
            if ( cDown & WPAD_CLASSIC_BUTTON_MINUS ) buttons |= InputButtons::Select;
        }
    }

    // 3. GameCube Controller
    if ( padDown & PAD_BUTTON_UP )    buttons |= InputButtons::Up;
    if ( padDown & PAD_BUTTON_DOWN )  buttons |= InputButtons::Down;
    if ( padDown & PAD_BUTTON_LEFT )  buttons |= InputButtons::Left;
    if ( padDown & PAD_BUTTON_RIGHT ) buttons |= InputButtons::Right;
    if ( padDown & PAD_BUTTON_A )     buttons |= InputButtons::A;
    if ( padDown & PAD_BUTTON_B )     buttons |= InputButtons::B;
    if ( padDown & PAD_BUTTON_START ) buttons |= InputButtons::Start;
    if ( padDown & PAD_TRIGGER_Z )    buttons |= InputButtons::Select;

    // GameCube main analog stick
    int stickX = PAD_StickX( 0 );
    int stickY = PAD_StickY( 0 );
    if ( stickX > 30 )  buttons |= InputButtons::Right;
    if ( stickX < -30 ) buttons |= InputButtons::Left;
    if ( stickY > 30 )  buttons |= InputButtons::Up;
    if ( stickY < -30 ) buttons |= InputButtons::Down;

    inputState.Buttons = buttons;
}
#endif

void Input::Update()
{
    oldInputState = inputState;
    inputState = 0;

#if defined(GEKKO)
    PollWiiPads();
#endif
}

void Input::Init()
{
#if defined(GEKKO)
    PAD_Init();
    WPAD_Init();
    WPAD_SetDataFormat( 0, WPAD_FMT_BTNS_ACC_IR );
    WPAD_SetVRes( 0, 512, 480 );
#endif
    oldInputState = 0;
    inputState = 0;
}

#if defined(GEKKO)
bool Input::GetMenuPointer( int& x, int& y )
{
    WPADData* data = WPAD_Data( 0 );
    if ( data == nullptr || !data->ir.valid )
        return false;
    x = (int)(data->ir.x / 2);
    // Menu drawing has an 8px TV safe inset in the 480px IR canvas.
    y = (int)((data->ir.y - 8.0f) * 240.0f / 464.0f);
    return x >= 0 && x < 256 && y >= 0 && y < 240;
}
#endif
