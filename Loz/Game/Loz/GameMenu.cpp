// Wii port modifications, 2026, txmbrs and contributors.
/*
   Copyright 2016 Aldo J. Nunez

   Licensed under the Apache License, Version 2.0.
   See the LICENSE text file for details.
*/

#include "Common.h"
#include "GameMenu.h"
#include "Graphics.h"
#include "Input.h"
#include "ItemObj.h"
#include "Profile.h"
#include "SaveFolder.h"
#include "Sound.h"
#include "SoundId.h"
#include "World.h"


const static uint8_t SelectStr[] = 
{ 0x62, 0x24, 0x1C, 0x24, 0x0E, 0x24, 0x15, 0x24, 0x0E, 0x24, 0x0C, 0x24, 0x1D, 0x24, 0x62 };

const static uint8_t NameStr[] = 
{ 0x24, 0x17, 0x0A, 0x16, 0x0E, 0x24 };

const static uint8_t LifeStr[] = 
{ 0x24, 0x15, 0x12, 0x0F, 0x0E, 0x24 };

const static uint8_t RegisterStr[] = 
{ 
    0x1B, 0x0E, 0x10, 0x12, 0x1C, 0x1D, 0x0E, 0x1B, 0x24, 0x22, 0x18, 0x1E, 0x1B, 0x24, 0x17, 0x0A, 
    0x16, 0x0E
};

const static uint8_t EliminateStr[] = 
{ 0x0E, 0x15, 0x12, 0x16, 0x12, 0x17, 0x0A, 0x1D, 0x12, 0x18, 0x17, 0x24, 0x16, 0x18, 0x0D, 0x0E };


GameMenu::GameMenu( const std::shared_ptr<ProfileSummarySnapshot>& summaries )
    :   summaries( summaries ),
        selectedIndex( -1 )
{
    static const uint8_t palettes[PaletteCount][4] = 
    {
        { 0x0F, 0x30, 0x00, 0x12 },
        { 0x0F, 0x16, 0x27, 0x36 },
        { 0x0F, 0x0C, 0x1C, 0x2C },
        { 0x0F, 0x12, 0x1C, 0x2C },
        { 0x00, 0x29, 0x27, 0x07 },
        { 0x00, 0x29, 0x27, 0x07 },
        { 0x00, 0x29, 0x27, 0x07 },
        { 0x00, 0x15, 0x27, 0x30 }
    };

    for ( int i = 0; i < PaletteCount; i++ )
    {
        Graphics::SetPaletteIndexed( i, palettes[i] );
    }

    // So that characters are fully opaque.
    Graphics::SetColor( 0, 0, 0xFF000000 );
    Graphics::UpdatePalettes();

    SelectNext();
}

void GameMenu::SelectNext()
{
    do
    {
        selectedIndex++;
        if ( selectedIndex >= 5 )
            selectedIndex = 0;
    } while ( selectedIndex < 3 && !summaries->Summaries[selectedIndex].IsActive() );
}

void GameMenu::SelectPrevious()
{
    do
    {
        selectedIndex--;
        if ( selectedIndex < 0 )
            selectedIndex = 4;
    } while ( selectedIndex < 3 && !summaries->Summaries[selectedIndex].IsActive() );
}

void StartWorld( int fileIndex )
{
    Profile profile;
    SaveFolder::ReadProfile( fileIndex, profile );
    World::Start( fileIndex, profile );
}

void GameMenu::Update()
{
#if defined(GEKKO)
    int pointerX, pointerY;
    static int lastPointerX = -1000, lastPointerY = -1000;
    bool hasPointer = Input::GetMenuPointer( pointerX, pointerY );
    bool pointerMoved = hasPointer && (abs(pointerX - lastPointerX) > 3 || abs(pointerY - lastPointerY) > 3);
    if ( !hasPointer )
        lastPointerX = lastPointerY = -1000;
    if ( pointerMoved )
    {
        lastPointerX = pointerX;
        lastPointerY = pointerY;
    }
    if ( pointerMoved && pointerX >= 0x18 && pointerX < 0xE8 )
    {
        int row = -1;
        if ( pointerY >= 0x58 && pointerY < 0xA0 )
            row = (pointerY - 0x58) / 24;
        else if ( pointerY >= 0xA8 && pointerY < 0xB8 )
            row = 3;
        else if ( pointerY >= 0xB8 && pointerY < 0xC8 )
            row = 4;
        if ( row >= 0 && (row >= 3 || summaries->Summaries[row].IsActive()) )
            selectedIndex = row;
    }
#endif
    if ( Input::IsButtonPressing( InputButtons::Down )
        || Input::IsButtonPressing( InputButtons::Right )
        || Input::IsButtonPressing( InputButtons::Select ) )
    {
        SelectNext();
        Sound::PlayEffect( SEffect_cursor );
    }
    else if ( Input::IsButtonPressing( InputButtons::Up )
        || Input::IsButtonPressing( InputButtons::Left ) )
    {
        SelectPrevious();
        Sound::PlayEffect( SEffect_cursor );
    }
    else if ( Input::IsButtonPressing( InputButtons::Start )
        || Input::IsButtonPressing( InputButtons::A ) )
    {
        if ( selectedIndex < 3 )
            StartWorld( selectedIndex );
        else if ( selectedIndex == 3 )
            World::RegisterFile( summaries );
        else if ( selectedIndex == 4 )
            World::EliminateFile( summaries );
    }
}

void GameMenu::Draw()
{
    Graphics::Begin();

#if defined(GEKKO)
    static int selectFrameCounter = 0;
    selectFrameCounter++;
    int frame = (selectFrameCounter / 10) % 6;
    if ( !Graphics::DrawHDSelectScreen( frame ) )
        Graphics::Clear( 0, 0, 0 );
#else
    Graphics::Clear( 0, 0, 0 );
#endif

    DrawBox( 0x18, 0x40, 0xD0, 0x90 );

    DrawString( SelectStr, sizeof SelectStr, 0x40, 0x28, 0 );
    DrawString( NameStr, sizeof NameStr, 0x50, 0x40, 0 );
    DrawString( LifeStr, sizeof LifeStr, 0x98, 0x40, 0 );
    DrawString( RegisterStr, sizeof RegisterStr, 0x30, 0xA8, 0 );
    DrawString( EliminateStr, sizeof EliminateStr, 0x30, 0xB8, 0 );

    int y = 0x58;
    for ( int i = 0; i < 3; i++ )
    {
        ProfileSummary& summary = summaries->Summaries[i];
        if ( summary.IsActive() )
        {
            uint8_t numBuf[3] = "";
            NumberToStringR( summary.Deaths, NumberSign_None, numBuf, sizeof numBuf );
            DrawString( numBuf, sizeof numBuf, 0x48, y + 8, 0 );
            DrawString( summary.Name, summary.NameLength, 0x48, y, 0 );
            uint totalHearts = summary.HeartContainers;
            uint heartsValue = Profile::GetMaxHeartsValue( totalHearts );
            DrawHearts( heartsValue, totalHearts, 0x90, y + 8 );
            DrawFileIcon( 0x30, y, summary.Quest );
        }
        DrawChar( Char_Minus, 0x88, y, 0 );
        y += 24;
    }

    if ( selectedIndex < 3 )
        y = 0x58 + selectedIndex * 24 + 5;
    else
        y = 0xA8 + (selectedIndex - 3) * 16;
    DrawChar( Char_FullHeart, 0x28, y, 7 );

#if defined(GEKKO)
    int pointerX, pointerY;
    if ( Input::GetMenuPointer( pointerX, pointerY ) )
        DrawChar( Char_FullHeart, pointerX - 4, pointerY - 4, 7 );
#endif

    Graphics::End();
}
