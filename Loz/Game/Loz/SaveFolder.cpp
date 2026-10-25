// Wii port modifications, 2026, txmbrs and contributors.
/*
   Copyright 2016 Aldo J. Nunez

   Licensed under the Apache License, Version 2.0.
   See the LICENSE text file for details.
*/

#include "Common.h"
#include "SaveFolder.h"

#if defined(_WIN32)
#include <direct.h>
#define MKDIR(path) _mkdir(path)
#else
#include <sys/stat.h>
#include <sys/types.h>
#define MKDIR(path) mkdir(path, 0777)
#endif

enum OpenMode
{
    Open_Read,
    Open_Write,
};

const char* SaveFileNamePattern = "saves/z1_%d.sav";

static int OpenFile( FILE** file, int slot, OpenMode mode )
{
    char fileName[64] = "";

    MKDIR( "saves" );

#if defined(GEKKO)
    // Try SD card path first on Wii if available
    snprintf( fileName, sizeof( fileName ), "sd:/apps/zelda1hd/saves/z1_%d.sav", slot );
    int err = PortableFOpen( file, fileName, (mode == Open_Write) ? "wb" : "rb" );
    if ( err == 0 )
        return 0;
#endif

    snprintf( fileName, sizeof( fileName ), SaveFileNamePattern, slot );

    if ( mode == Open_Write )
    {
        int err = PortableFOpen( file, fileName, "r+b" );
        if ( err != 0 )
            err = PortableFOpen( file, fileName, "wb" );
        return err;
    }
    else
    {
        return PortableFOpen( file, fileName, "rb" );
    }
}

void SaveFolder::ReadSummaries( ProfileSummarySnapshot& summaries )
{
    for ( int i = 0; i < MaxProfiles; i++ )
    {
        ProfileSummary& summary = summaries.Summaries[i];
        Profile profile;
        FILE* file = nullptr;
        size_t lenRead = 0;

        int err = OpenFile( &file, i, Open_Read );
        if ( err == 0 )
        {
            lenRead = fread( &profile, sizeof profile, 1, file );
            fclose( file );
        }

        if ( lenRead < 1 )
        {
            memset( &summaries.Summaries[i], 0, sizeof(ProfileSummary) );
        }
        else
        {
            summary.Deaths = profile.Deaths;
            summary.HeartContainers = profile.Items[ItemSlot_HeartContainers];
            summary.Quest = profile.Quest;
            summary.NameLength = profile.NameLength;
            memcpy( summary.Name, profile.Name, profile.NameLength );
        }
    }
}

bool SaveFolder::ReadProfile( int index, Profile& profile )
{
    assert( index < MaxProfiles );

    FILE* file = nullptr;
    size_t lenRead = 0;

    int err = OpenFile( &file, index, Open_Read );
    if ( err == 0 )
    {
        lenRead = fread( &profile, sizeof( Profile ), 1, file );
        fclose( file );
    }

    if ( lenRead < 1 )
    {
        memset( &profile, 0, sizeof( Profile ) );
        return false;
    }

    profile.Hearts = FromLE16( profile.Hearts );

    return true;
}

bool SaveFolder::WriteProfile( int index, const Profile& profile )
{
    assert( index < MaxProfiles );

    FILE* file = nullptr;
    size_t lenWritten = 0;

    int err = OpenFile( &file, index, Open_Write );
    if ( err != 0 )
        return false;

    Profile writeCopy = profile;
    writeCopy.Hearts = ToLE16( writeCopy.Hearts );

    lenWritten = fwrite( &writeCopy, sizeof( Profile ), 1, file );
    fclose( file );

    if ( lenWritten != 1 )
        return false;

    return true;
}
