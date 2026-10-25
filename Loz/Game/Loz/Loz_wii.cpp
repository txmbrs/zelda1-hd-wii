// Wii port modifications, 2026, txmbrs and contributors.
/*
   Copyright 2016 Aldo J. Nunez
   Wii main entry point and game loop
*/

#include "Common.h"
#include "Graphics.h"
#include "Input.h"
#include "Sound.h"
#include "World.h"

#include <SDL2/SDL.h>

#if defined(GEKKO)
#include <gccore.h>
#include <fat.h>
#include <wiiuse/wpad.h>
#include <unistd.h>
#endif

#include <cstdarg>

static FILE* wiiDebugFile = nullptr;

void WiiDebugLog( const char* format, ... )
{
    if ( wiiDebugFile == nullptr )
        return;

    va_list args;
    va_start( args, format );
    vfprintf( wiiDebugFile, format, args );
    va_end( args );
    fputc( '\n', wiiDebugFile );
    fflush( wiiDebugFile );
}

static uint32_t frameCounter = 0;

uint32_t GetFrameCounter()
{
    return frameCounter;
}

const double FrameTime = 1.0 / 60.0;

// The Wii SDL2main shim provides the platform's real `main` and calls this
// entry point after its libogc/IOS/WPAD setup.
extern "C" int SDL_main( int argc, char* argv[] )
{
#if defined(GEKKO)
    int appDirResult = -2;
    char appDir[256] = "<not found>";

    // SDL2main initializes FAT. Change to the app folder so relative assets
    // resolve beside boot.dol when launched from the Homebrew environment.
    if ( argc > 0 && argv[0] != nullptr )
    {
        char dir[256];
        strncpy( dir, argv[0], sizeof( dir ) );
        dir[sizeof( dir ) - 1] = '\0';
        char* lastSlash = strrchr( dir, '/' );
        if ( lastSlash != nullptr )
        {
            *lastSlash = '\0';
            strncpy( appDir, dir, sizeof( appDir ) );
            appDir[sizeof( appDir ) - 1] = '\0';
            appDirResult = chdir( dir );
        }
    }

    wiiDebugFile = fopen( "zelda-wii-debug.log", "w" );
    WiiDebugLog( "start argc=%d argv0=%s chdir(%s)=%d", argc,
        (argc > 0 && argv[0] != nullptr) ? argv[0] : "<null>",
        appDir, appDirResult );
#endif

    if ( !Graphics::Init() )
    {
        WiiDebugLog( "Graphics::Init failed: %s", SDL_GetError() );
        return 1;
    }
    WiiDebugLog( "Graphics::Init ok" );

    Input::Init();
    if ( !Sound::Init() )
        WiiDebugLog( "Sound::Init failed; audio backend did not open" );

    World::Init();
    WiiDebugLog( "World::Init returned" );

    uint32_t startTicks = SDL_GetTicks();
    double accumulator = 0.0;
    bool running = true;

    while ( running )
    {
        SDL_Event event;
        while ( SDL_PollEvent( &event ) )
        {
            if ( event.type == SDL_QUIT )
            {
                running = false;
            }
        }

#if defined(GEKKO)
        // Check for Wiimote HOME button or console reset to exit cleanly
        uint32_t wpadDown = WPAD_ButtonsHeld( 0 );
        uint16_t padDown  = PAD_ButtonsHeld( 0 );
        if ( (wpadDown & WPAD_BUTTON_HOME) || (padDown & PAD_TRIGGER_Z) )
        {
            running = false;
        }
#endif

        uint32_t currentTicks = SDL_GetTicks();
        double elapsed = (currentTicks - startTicks) / 1000.0;
        startTicks = currentTicks;

        if ( elapsed > 0.25 )
            elapsed = 0.25;

        accumulator += elapsed;

        bool updated = false;
        while ( accumulator >= FrameTime )
        {
            frameCounter++;

            Input::Update();
            World::Update();
            Sound::Update();

            accumulator -= FrameTime;
            updated = true;
        }

        if ( updated )
        {
            World::Draw();
            Graphics::Present(); // Present exactly once after the full frame
            if ( frameCounter < 4 )
                WiiDebugLog( "present frame=%lu", (unsigned long)frameCounter );
        }

        SDL_Delay( 1 );
    }

    World::Uninit();
    Sound::Uninit();

    SDL_Quit();

#if defined(GEKKO)
    exit( 0 );
#endif

    return 0;
}
