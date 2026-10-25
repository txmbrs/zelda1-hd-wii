// Wii port modifications, 2026, txmbrs and contributors.
/*
   Copyright 2016 Aldo J. Nunez

   Licensed under the Apache License, Version 2.0.
   See the LICENSE text file for details.
*/

#pragma once

#include "HAL.h"

enum TileSheetSlot
{
    Sheet_Background,
    Sheet_PlayerAndItems,
    Sheet_Npcs,
    Sheet_Boss,
    Sheet_Font,
    Sheet_Walls,
    Sheet_Doors,

    Sheet_Max
};

struct SpriteFrame
{
    uint8_t     x;
    uint8_t     y;
    uint8_t     flags;
};

struct SpriteAnim
{
    uint8_t     length;
    uint8_t     width;
    uint8_t     height;
    SpriteFrame frames[1];
};

class Graphics
{
public:
    static bool Init();

    static void LoadTileSheet( int slot, const char* path );
    static void LoadTileSheet( int slot, const char* imagePath, const char* animPath );
    static void UnloadTileSheet( int slot );

    static void Begin();
    static void End();
    static void Present();
    static void Clear( uint8_t r, uint8_t g, uint8_t b );
    static void Clear( int sysColor );

    static void DrawSpriteTile( 
        int slot, 
        int srcX, 
        int srcY, 
        int width, 
        int height, 
        int destX, 
        int destY, 
        int palette,
        int flags
        );

    static void DrawTile( 
        int slot, 
        int srcX, 
        int srcY, 
        int width, 
        int height, 
        int destX, 
        int destY, 
        int palette,
        int flags
        );

#if defined(USE_ALLEGRO)
    static void DrawBitmap(
        ALLEGRO_BITMAP* bitmap,
        int srcX, 
        int srcY,
        int width,
        int height,
        int destX,
        int destY,
        int palette,
        int flags
        );
    static ALLEGRO_COLOR GetSystemColor( int sysColor );
#endif

    static uint32_t GetSystemColorArgb( int sysColor );

    static void LoadSystemPalette( const int* colorsArgb8 );
    static void SetColor( int paletteIndex, int colorIndex, int colorArgb8 );
    static void SetColorIndexed( int paletteIndex, int colorIndex, int sysColor );
    static void SetPaletteIndexed( int paletteIndex, const uint8_t* sysColors );
    static void UpdatePalettes();

    static void EnableGrayscale();
    static void DisableGrayscale();

    static void SetViewParams( float scale, float x, float y );
    static void SetClip( int x, int y, int width, int height );
    static void ResetClip();

#if defined(GEKKO)
    static bool DrawMagicWaveHD( int direction, int destX, int destY );
    static void SetHDLevel( int level );
    static bool MatchBackgroundTile( int tile, const char* pattern, int fineX=0, int fineY=0 );
    static bool IsBackgroundPixelTransparent( int tile, int fineX, int fineY );
    static void SetPlayerBackgroundMask( const uint16_t* rows );
    static bool DrawOverworldHDBackground( int roomId, int quest, int destX, int destY, int width, int height );
    static bool DrawCaveHDBackground( bool noStairs, int destX, int destY, int width, int height );
    static bool DrawCellarHDBackground( bool alt, int destX, int destY, int width, int height );
    static bool DrawPlayerHDSprite( int direction, int frame, int destX, int destY, int tunic = 0, int shield = 0, int pose = 0, int flash = 0 );
    static bool DrawBossHDSprite( int bossType, int frame, int destX, int destY, int pal = 0 );
    static bool DrawHDTitleScreen();
    static bool DrawHDSelectScreen( int frame );
    static bool DrawHDGameOverScreen();
    static bool DrawHDHUD( int baseY );
    static bool DrawStyledHUD( int features );
    static bool DrawStyledInventory( int top, const int* items, const int* passive, int selected );
    static void SetPlayfieldView( bool enabled );
    static void SetHUDOverlay( bool enabled );
#endif

    static const SpriteAnim* GetAnimation( int slot, int animIndex );

private:
    static void SetPalette( int paletteIndex, const int* colorsArgb8 );
    static void SwitchSystemPalette( int* newSystemPalette );
};
