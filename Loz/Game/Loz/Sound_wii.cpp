// Wii port modifications, 2026, txmbrs and contributors.
/*
   Copyright 2016 Aldo J. Nunez
   Wii audio backend using SDL_mixer. Music stays streamed from OGG files;
   short sound effects are loaded as WAV chunks.
*/

#include "Common.h"
#include "Sound.h"
#include "SoundId.h"
#include "Util.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>

enum
{
    Instances = 5,
    Streams = 2,
    LoPriStreams = Streams - 1,
    Songs = Song_Max,
    Effects = SEffect_Max,
    NoSound = 0xFF,
};

enum SoundFlags
{
    SoundFlag_PlayIfQuietSlot = 1,
};

struct EffectRequest
{
    uint8_t SoundId;
    bool Loop;
};

struct SoundInfo
{
    int16_t Begin;
    int16_t End;
    int8_t Slot;
    int8_t Priority;
    int8_t Flags;
    int8_t Reserved;
    char Filename[20];
};

static SoundInfo songs[Songs];
static SoundInfo effects[Effects];
static EffectRequest effectRequests[Instances];
static Mix_Chunk* effectChunks[Effects] = {};
static Mix_Music* musicStreams[Streams] = {};
static int currentSong[Streams] = { -1, -1 };
static bool songLooping[Streams] = { false, false };
static double savedMainPosition = 0.0;
static bool paused = false;

static const char* const songFiles[Song_Max] =
{
    "TitleTheme.ogg", "Ending.ogg", "OverworldTheme.ogg", "DungeonTheme.ogg",
    "NewItemFanfare.ogg", "TriforceObtained.ogg", "GanonAppears.ogg",
    "GanonsLair.ogg", "GameOver_ZeldaRescued.ogg", "LinkDie.ogg",
    "UseRecorder.ogg", "ZeldaRescuedFanfare.ogg"
};

static const char* const effectFiles[SEffect_Max] =
{
    "ShoreWater.wav", "SwordShoot.wav", "BossHurt.wav", "DoorUnlock.wav",
    "LinkDamage.wav", "BossScream1.wav", "BossScream2.wav", "BossScream3.wav",
    "TextTyping.wav", "GetItem_GetFairy.wav", "SecretFound.wav", "GotAnItem.wav",
    "KillEnemy.wav", "SwordSlash.wav", "Arrow_Boomerang.wav", "UseCandle.wav",
    "Stairs.wav", "BombBlow.wav", "ShieldDeflect.wav", "HitEnemy.wav",
    "UseMagicRod.wav", "GetHeart.wav", "TextTyping.wav", "BombDrop.wav",
    "LowHealthBeep.wav"
};

static Mix_Music* LoadSong(int songId)
{
    char path[96];
    snprintf(path, sizeof(path), "audio/%s", songFiles[songId]);
    return Mix_LoadMUS(path);
}

bool Sound::Init()
{
    if (!Util::LoadList<SoundInfo>("Songs.dat", songs, Songs)
        || !Util::LoadList<SoundInfo>("Effects.dat", effects, Effects))
        return false;

    for (int i = 0; i < Songs; i++)
    {
        songs[i].Begin = FromLE16(songs[i].Begin);
        songs[i].End = FromLE16(songs[i].End);
    }
    for (int i = 0; i < Effects; i++)
    {
        effects[i].Begin = FromLE16(effects[i].Begin);
        effects[i].End = FromLE16(effects[i].End);
    }
    for (int i = 0; i < Instances; i++)
    {
        effectRequests[i].SoundId = NoSound;
        effectRequests[i].Loop = false;
    }

    if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0)
        return false;
    Mix_Init(MIX_INIT_OGG);
    if (Mix_OpenAudio(22050, AUDIO_S16SYS, 2, 1024) < 0)
    {
        Mix_Quit();
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        return false;
    }
    Mix_AllocateChannels(Instances);
    for (int i = 0; i < Effects; i++)
    {
        char path[96];
        snprintf(path, sizeof(path), "audio/%s", effectFiles[i]);
        effectChunks[i] = Mix_LoadWAV(path);
    }

    return true;
}

void Sound::Uninit()
{
    StopAll();
    for (int i = 0; i < Effects; i++)
    {
        if (effectChunks[i] != nullptr)
        {
            Mix_FreeChunk(effectChunks[i]);
            effectChunks[i] = nullptr;
        }
    }
    Mix_CloseAudio();
    Mix_Quit();
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

void Sound::Update()
{
    // Only a completed event resumes the main song. A global finished hook
    // also fires for ordinary songs and can restart a different active track.
    if (!paused && musicStreams[EventSongStream] != nullptr && !Mix_PlayingMusic())
    {
        if (musicStreams[EventSongStream] != nullptr)
        {
            Mix_FreeMusic(musicStreams[EventSongStream]);
            musicStreams[EventSongStream] = nullptr;
            currentSong[EventSongStream] = -1;
        }
        if (!paused && musicStreams[MainSongStream] != nullptr)
        {
            Mix_PlayMusic(musicStreams[MainSongStream], songLooping[MainSongStream] ? -1 : 0);
            if (savedMainPosition > 0.0)
                Mix_SetMusicPosition(savedMainPosition);
        }
    }

    for (int i = 0; i < Instances; i++)
    {
        int id = effectRequests[i].SoundId;
        effectRequests[i].SoundId = NoSound;
        if (id == NoSound || effectChunks[id] == nullptr)
            continue;
        if ((effects[id].Flags & SoundFlag_PlayIfQuietSlot) == 0 || !Mix_Playing(i))
            Mix_PlayChannel(i, effectChunks[id], effectRequests[i].Loop ? -1 : 0);
    }
}

void Sound::PlaySong(int songId, int streamId, bool loop)
{
    if (streamId < 0 || streamId >= LoPriStreams || songId < 0 || songId >= Songs)
        return;

    if (currentSong[streamId] == songId && musicStreams[streamId] != nullptr
        && (Mix_PlayingMusic() || musicStreams[EventSongStream] != nullptr))
        return;

    if (streamId == MainSongStream && musicStreams[EventSongStream] != nullptr
        && Mix_PlayingMusic())
    {
        savedMainPosition = 0.0;
        Mix_Music* replacement = LoadSong(songId);
        if (replacement == nullptr)
            return;
        if (musicStreams[MainSongStream] != nullptr)
            Mix_FreeMusic(musicStreams[MainSongStream]);
        musicStreams[MainSongStream] = replacement;
        currentSong[MainSongStream] = songId;
        songLooping[MainSongStream] = loop;
        return;
    }

    Mix_Music* replacement = LoadSong(songId);
    if (replacement == nullptr)
        return;
    if (musicStreams[streamId] != nullptr)
    {
        Mix_HaltMusic();
        Mix_FreeMusic(musicStreams[streamId]);
    }
    musicStreams[streamId] = replacement;
    currentSong[streamId] = songId;
    songLooping[streamId] = loop;

    if (streamId == MainSongStream && musicStreams[EventSongStream] != nullptr
        && Mix_PlayingMusic())
        return;
    Mix_PlayMusic(replacement, loop ? -1 : 0);
}

void Sound::PushSong(int songId)
{
    if (songId < 0 || songId >= Songs)
        return;

    Mix_Music* event = LoadSong(songId);
    if (event == nullptr)
        return;

    if (musicStreams[EventSongStream] == nullptr
        && musicStreams[MainSongStream] != nullptr && Mix_PlayingMusic())
    {
        double position = Mix_GetMusicPosition(musicStreams[MainSongStream]);
        savedMainPosition = position > 0.0 ? position : 0.0;
    }

    Mix_HaltMusic();
    if (musicStreams[EventSongStream] != nullptr)
    {
        Mix_HaltMusic();
        Mix_FreeMusic(musicStreams[EventSongStream]);
    }
    musicStreams[EventSongStream] = event;
    currentSong[EventSongStream] = songId;
    Mix_PlayMusic(event, 0);
}

void Sound::StopSongs()
{
    Mix_HaltMusic();
    for (int i = 0; i < Streams; i++)
    {
        if (musicStreams[i] != nullptr)
        {
            Mix_FreeMusic(musicStreams[i]);
            musicStreams[i] = nullptr;
        }
        currentSong[i] = -1;
        songLooping[i] = false;
    }
    savedMainPosition = 0.0;
}

void Sound::PlayEffect(int id, bool loop, int instance)
{
    if (id < 0 || id >= Effects || instance < -1 || instance >= Instances)
        return;
    int index = instance == -1 ? effects[id].Slot - 1 : instance;
    if (index < 0 || index >= Instances)
        return;
    int previous = effectRequests[index].SoundId;
    if (previous == NoSound || effects[id].Priority < effects[previous].Priority)
    {
        effectRequests[index].SoundId = (uint8_t)id;
        effectRequests[index].Loop = loop;
    }
}

void Sound::StopEffect(int instance)
{
    if (instance >= 0 && instance < Instances)
    {
        Mix_HaltChannel(instance);
        effectRequests[instance].SoundId = NoSound;
        effectRequests[instance].Loop = false;
    }
}

void Sound::StopEffects()
{
    for (int i = 0; i < Instances; i++)
        StopEffect(i);
}

void Sound::StopAll()
{
    StopSongs();
    StopEffects();
}

void Sound::Pause()
{
    paused = true;
    Mix_PauseMusic();
    Mix_Pause(-1);
}

void Sound::Unpause()
{
    paused = false;
    Mix_ResumeMusic();
    Mix_Resume(-1);
}
