// Wii port modifications, 2026, txmbrs and contributors.
/*
   Copyright 2016 Aldo J. Nunez
   Wii backend graphics implementation using SDL2 / GX
*/

#include "Common.h"
#include "Graphics.h"
#include "HDGeneratedSheets.h"
#include "HDGeneratedWater.h"
#include "HDGeneratedDungeonWater.h"
#include "PlayerItemsAnim.h"
#include "World.h"
#include "Profile.h"
#include "StatusBar.h"
#include "ItemObj.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <cmath>
#include <string>
#include <unordered_set>
#if defined(GEKKO)
#include <gccore.h>
#endif

extern void WiiDebugLog( const char* format, ... );

static SDL_Window*   window   = nullptr;
static SDL_Renderer* renderer = nullptr;

static SDL_Surface*  tileSheetPixels[Sheet_Max] = { nullptr };
static SDL_Texture*  tileSheets[Sheet_Max][PaletteCount] = {};
static uint32_t      paletteGeneration[PaletteCount] = {};
static uint32_t      textureGeneration[Sheet_Max][PaletteCount] = {};
static bool          hdTileSheet[Sheet_Max][PaletteCount] = {};
static bool          hdTileReferencePalette[Sheet_Max][PaletteCount] = {};
static char          sheetNames[Sheet_Max][40] = {};
static int           hdLevel = 0;
static SDL_Texture*  hdDungeonWaterTextures[sizeof(hdDungeonWaterSpecs)/sizeof(hdDungeonWaterSpecs[0])] = {};
static Util::Table<SpriteAnim> animSpecs[Sheet_Max];

static int      systemPalette[SysPaletteLength];
static int      grayscalePalette[SysPaletteLength];
static int*     activeSystemPalette = systemPalette;
static uint8_t  palettes[PaletteCount][PaletteLength];
static uint32_t directPaletteColors[PaletteCount][PaletteLength] = {};
static bool     directPaletteColorSet[PaletteCount][PaletteLength] = {};

static SDL_Texture* hdLinkTexture = nullptr;
static int curLinkTunic = -1;
static int curLinkShield = -1;
static int curLinkDeath = -1;

static SDL_Texture* hdRoomTextures[2] = {};
static int hdRoomIds[2] = { -1, -1 };
static int hdRoomNext = 0;
static SDL_Texture* hdWaterTextures[2][3][2] = {};

static SDL_Texture* hdCaveTextures[2] = {};
static SDL_Texture* hdTitleTextures[8] = {};

static SDL_Texture* hdCellarTextures[2][3] = {};
static SDL_Texture* hdDeathTextures[2] = {};
static uint16_t playerBackgroundMask[16] = {};
static bool playerBackgroundMaskEnabled = false;
static uint16_t backgroundTransparency[256][16] = {};
static bool backgroundTransparencyValid[256] = {};

static SDL_Texture* hdHudTexture = nullptr;
static SDL_Texture* hdWandTexture = nullptr;
static SDL_Texture* hdHudOverlayTexture = nullptr;
static bool hudOverlay = false;
static SDL_Texture* hdGameOverTexture = nullptr;
static SDL_Texture* hdSelectTextures[6] = {};

static SDL_Texture* hdBossTexture = nullptr;
static int hdBossCurrentType = -1;

static float    viewScaleX  = 2.5f;
static float    viewScaleY  = 2.0f;
static float    viewOffsetX = 0.0f;
static float    viewOffsetY = 0.0f;

static SDL_Rect ScreenRect( int x, int y, int width, int height )
{
    int left = (int)std::lround( viewOffsetX + x * viewScaleX );
    int top = (int)std::lround( viewOffsetY + y * viewScaleY );
    int right = (int)std::lround( viewOffsetX + (x + width) * viewScaleX );
    int bottom = (int)std::lround( viewOffsetY + (y + height) * viewScaleY );
    SDL_Rect result = { left, top, right - left, bottom - top };
    return result;
}

static SDL_Texture* LoadHDTexture( const char* path );

static void ModulateHDPalette(SDL_Texture* texture,int palette,const uint8_t* original=nullptr)
{
    if(!texture || palette<0 || palette>=PaletteCount)return;
    const World::LevelInfoBlock* info=World::GetLevelInfo();
    unsigned reference[3]={},current[3]={};bool changed=false;
    for(int i=1;i<4;++i)
    {
        int before=(original?original[i]:info->Palettes[palette][i])&63,after=palettes[palette][i]&63;
        changed|=before!=after || activeSystemPalette!=systemPalette;
        unsigned a=systemPalette[before],b=activeSystemPalette[after];
        reference[0]+=(a>>16)&255;reference[1]+=(a>>8)&255;reference[2]+=a&255;
        current[0]+=(b>>16)&255;current[1]+=(b>>8)&255;current[2]+=b&255;
    }
    Uint8 channels[3]={255,255,255};
    if(changed)for(int i=0;i<3;++i)channels[i]=(Uint8)std::fmin(255.0,255.0*current[i]/(reference[i]?reference[i]:1));
    SDL_SetTextureColorMod(texture,channels[0],channels[1],channels[2]);
}

bool Graphics::MatchBackgroundTile(int tile,const char* pattern,int fineX,int fineY)
{
    SDL_Surface* surface=tileSheetPixels[Sheet_Background];
    if(!surface)return false;
    int sx=(tile&15)*16+fineX,sy=(tile>>4)*16+fineY;
    if(sx+8>surface->w || sy+8>surface->h)return false;
    for(int bit=0;bit<2;++bit)for(int y=0;y<8;++y)
    {
        unsigned value=0;
        for(int x=0;x<8;++x)
        {
            Uint32 pixel=0;memcpy(&pixel,(Uint8*)surface->pixels+(sy+y)*surface->pitch+(sx+x)*surface->format->BytesPerPixel,surface->format->BytesPerPixel);
            Uint8 r,g,b,a;SDL_GetRGBA(pixel,surface->format,&r,&g,&b,&a);
            value|=(((r>>4)>>bit)&1)<<(7-x);
        }
        unsigned expected=0;sscanf(pattern+(bit*8+y)*2,"%2x",&expected);
        if(value!=expected)return false;
    }
    return true;
}

bool Graphics::IsBackgroundPixelTransparent(int tile,int fineX,int fineY)
{
    SDL_Surface* surface=tileSheetPixels[Sheet_Background];
    if(!surface || tile<0 || tile>=256 || fineX<0 || fineX>=16 || fineY<0 || fineY>=16)return false;
    if(!backgroundTransparencyValid[tile])
    {
        int sx=(tile&15)*16,sy=(tile>>4)*16;
        if(sx+16>surface->w || sy+16>surface->h)return false;
        for(int y=0;y<16;++y)
        {
            uint16_t row=0;
            for(int x=0;x<16;++x)
            {
                Uint32 pixel=0;memcpy(&pixel,(Uint8*)surface->pixels+(sy+y)*surface->pitch+(sx+x)*surface->format->BytesPerPixel,surface->format->BytesPerPixel);
                Uint8 r,g,b,a;SDL_GetRGBA(pixel,surface->format,&r,&g,&b,&a);
                if((r>>4)==0)row|=1u<<x;
            }
            backgroundTransparency[tile][y]=row;
        }
        backgroundTransparencyValid[tile]=true;
    }
    return (backgroundTransparency[tile][fineY]&(1u<<fineX))!=0;
}

void Graphics::SetPlayerBackgroundMask(const uint16_t* rows)
{
    playerBackgroundMaskEnabled=rows!=nullptr;
    if(rows)memcpy(playerBackgroundMask,rows,sizeof(playerBackgroundMask));
}

static void DrawMaskedPlayer(SDL_Texture* texture,const SDL_Rect& source,int x,int y,SDL_RendererFlip flip)
{
    // One geometry submission, rather than changing the GPU clip and drawing
    // the entire sprite again for every one-pixel run of the priority mask.
    static SDL_Vertex vertices[512];static int indices[768];
    int vertexCount=0,indexCount=0,tw=1,th=1;SDL_QueryTexture(texture,nullptr,nullptr,&tw,&th);
    SDL_Color color;SDL_GetTextureColorMod(texture,&color.r,&color.g,&color.b);SDL_GetTextureAlphaMod(texture,&color.a);
    for(int row=0;row<16;)
    {
        int endRow=row+1;
        while(endRow<16 && playerBackgroundMask[endRow]==playerBackgroundMask[row])++endRow;
        for(int col=0;col<16;)
        {
            if(!(playerBackgroundMask[row]&(1u<<col))){++col;continue;}
            int endCol=col+1;while(endCol<16 && (playerBackgroundMask[row]&(1u<<endCol)))++endCol;
            SDL_Rect dst=ScreenRect(x+col,y+row,endCol-col,endRow-row);
            float u0=(source.x+source.w*((flip&SDL_FLIP_HORIZONTAL)?16-col:col)/16.0f)/tw;
            float u1=(source.x+source.w*((flip&SDL_FLIP_HORIZONTAL)?16-endCol:endCol)/16.0f)/tw;
            float v0=(source.y+source.h*((flip&SDL_FLIP_VERTICAL)?16-row:row)/16.0f)/th;
            float v1=(source.y+source.h*((flip&SDL_FLIP_VERTICAL)?16-endRow:endRow)/16.0f)/th;
            int first=vertexCount;
            vertices[vertexCount++]={{(float)dst.x,(float)dst.y},color,{u0,v0}};
            vertices[vertexCount++]={{(float)(dst.x+dst.w),(float)dst.y},color,{u1,v0}};
            vertices[vertexCount++]={{(float)(dst.x+dst.w),(float)(dst.y+dst.h)},color,{u1,v1}};
            vertices[vertexCount++]={{(float)dst.x,(float)(dst.y+dst.h)},color,{u0,v1}};
            for(int index:{0,1,2,0,2,3})indices[indexCount++]=first+index;
            col=endCol;
        }
        row=endRow;
    }
    if(vertexCount)SDL_RenderGeometry(renderer,texture,vertices,vertexCount,indices,indexCount);
}

void Graphics::SetHDLevel( int level ) { hdLevel = level; }

bool Graphics::DrawMagicWaveHD( int direction, int destX, int destY )
{
    if ( !hdWandTexture )
        hdWandTexture = LoadHDTexture("hd/effects/wand.png");
    if ( !hdWandTexture ) return false;
    const SpriteAnim* anim = GetAnimation(Sheet_PlayerAndItems, Anim_PI_Wave_Right + (direction & 3));
    if ( !anim ) return false;
    const SpriteFrame& frame = anim->frames[0];
    int phase = (GetFrameCounter() % 6) / 2;
    SDL_Rect src = { phase * 256 + frame.x * 2, frame.y * 2, anim->width * 2, anim->height * 2 };
    SDL_Rect dst = ScreenRect(destX, destY + 1, anim->width, anim->height);
    SDL_RendererFlip flip = SDL_FLIP_NONE;
    if ( frame.flags & Flip_Horizontal ) flip = (SDL_RendererFlip)(flip | SDL_FLIP_HORIZONTAL);
    if ( frame.flags & Flip_Vertical ) flip = (SDL_RendererFlip)(flip | SDL_FLIP_VERTICAL);
    SDL_RenderCopyEx(renderer, hdWandTexture, &src, &dst, 0, nullptr, flip);
    return true;
}

static void InvalidatePalette( int palette )
{
    if ( palette >= 0 && palette < PaletteCount )
        paletteGeneration[palette]++;
}

static SDL_Texture* LoadHDTexture( const char* path )
{
    if ( renderer == nullptr || path == nullptr )
        return nullptr;

    // A missing optional resource must not cause multiple FAT seeks and a
    // flushed SD log write on every frame. Retry it on the next app launch.
    static std::unordered_set<std::string> missingPaths;
    if ( missingPaths.find(path) != missingPaths.end() )
        return nullptr;

    SDL_Surface* surface = IMG_Load( path );
    if ( surface == nullptr )
    {
        char altPath[256];
        snprintf( altPath, sizeof( altPath ), "sd:/apps/zelda1hd/%s", path );
        surface = IMG_Load( altPath );
        if ( surface == nullptr )
        {
            snprintf( altPath, sizeof( altPath ), "sd:/apps/Zeldas/%s", path );
            surface = IMG_Load( altPath );
        }
        if ( surface == nullptr )
        {
            snprintf( altPath, sizeof( altPath ), "sd:/zelda1hd/%s", path );
            surface = IMG_Load( altPath );
        }
        if ( surface == nullptr )
        {
            snprintf( altPath, sizeof( altPath ), "sd:/Zeldas/%s", path );
            surface = IMG_Load( altPath );
        }
    }

    if ( surface == nullptr )
    {
        missingPaths.insert(path);
        WiiDebugLog( "HD texture failed path=%s error=%s", path, IMG_GetError() );
        return nullptr;
    }

    SDL_SetColorKey(surface, SDL_TRUE, SDL_MapRGB(surface->format,255,0,255));

    SDL_Texture* texture = SDL_CreateTextureFromSurface( renderer, surface );
    SDL_FreeSurface( surface );
    if ( texture != nullptr )
        SDL_SetTextureBlendMode( texture, SDL_BLENDMODE_BLEND );
    return texture;
}

static SDL_Texture* GetHDOverworldTexture( int roomId, int quest )
{
    int cacheKey = (quest > 0 ? 0x1000 : 0) | (roomId & 0xFF);
    for ( int i = 0; i < 2; i++ )
    {
        if ( hdRoomIds[i] == cacheKey )
            return hdRoomTextures[i];
    }

    char path[64];
    SDL_Texture* texture = nullptr;
    if ( quest > 0 )
    {
        snprintf( path, sizeof( path ), "hd/ow2/%02X.png", roomId & 0xFF );
        texture = LoadHDTexture( path );
    }
    if ( texture == nullptr )
    {
        snprintf( path, sizeof( path ), "hd/ow/%02X.png", roomId & 0xFF );
        texture = LoadHDTexture( path );
    }

    int slot = hdRoomNext;
    hdRoomNext = (hdRoomNext + 1) % 2;
    for ( int layer = 0; layer < 3; ++layer )
        for ( int page = 0; page < 2; ++page )
        {
            if ( hdWaterTextures[slot][layer][page] )
                SDL_DestroyTexture(hdWaterTextures[slot][layer][page]);
            hdWaterTextures[slot][layer][page] = nullptr;
        }
    if ( hdRoomTextures[slot] != nullptr )
        SDL_DestroyTexture( hdRoomTextures[slot] );
    hdRoomTextures[slot] = texture;
    hdRoomIds[slot] = cacheKey;
    return texture;
}

bool Graphics::DrawOverworldHDBackground( int roomId, int quest, int destX, int destY, int width, int height )
{
    SDL_Texture* texture = GetHDOverworldTexture( roomId, quest );
    if ( texture == nullptr || renderer == nullptr )
        return false;

    SDL_Rect src = { 0, 0, 512, 352 };
    SDL_Rect dst = ScreenRect( destX, destY, width, height );
    ModulateHDPalette(texture,World::GetInnerPalette());
    SDL_RenderCopy( renderer, texture, &src, &dst );
    int key = (quest > 0 ? 0x1000 : 0) | (roomId & 0xFF);
    int slot = hdRoomIds[0] == key ? 0 : 1;
    for ( int layer = 0; layer < 3; ++layer )
    {
        const HDWaterSpec& spec = hdWaterSpecs[roomId & 0x7F][layer];
        if ( spec.frames == 0 ) continue;
        int time = GetFrameCounter() % spec.period;
        int frame = 0;
        for ( int i = 1; i < spec.frames; ++i )
            if ( time >= spec.begins[i] ) frame = i;
        int page = frame / spec.perPage;
        int local = frame % spec.perPage;
        SDL_Texture*& water = hdWaterTextures[slot][layer][page];
        if ( water == nullptr )
        {
            char path[80];
            snprintf(path, sizeof(path), "hd/anim/%02X_%d_%d.png", roomId & 0x7F, layer, page);
            water = LoadHDTexture(path);
        }
        if ( water == nullptr ) continue;
        ModulateHDPalette(water,World::GetInnerPalette());
        SDL_Rect waterSrc = { (local % spec.columns) * spec.w,
            (local / spec.columns) * spec.h, spec.w, spec.h };
        SDL_Rect waterDst = ScreenRect(destX + spec.x / 2, destY + spec.y / 2, spec.w / 2, spec.h / 2);
        SDL_RenderCopy(renderer, water, &waterSrc, &waterDst);
    }
    return true;
}

bool Graphics::DrawCaveHDBackground( bool noStairs, int destX, int destY, int width, int height )
{
    if ( renderer == nullptr )
        return false;

    const int slot = noStairs ? 0 : 1;
    if ( hdCaveTextures[slot] == nullptr )
    {
        hdCaveTextures[slot] = LoadHDTexture(
            noStairs ? "hd/cave/CaveFloor.png" : "hd/cave/CaveFloor2.png" );
    }
    SDL_Texture* texture = hdCaveTextures[slot];
    if ( texture == nullptr )
        return false;

    SDL_Rect src = { 0, 128, 512, 352 };
    SDL_Rect dst = ScreenRect( destX, destY, width, height );
    static uint8_t cavePalette[4]={};
    if(World::GetMode()!=Mode_Death)memcpy(cavePalette,palettes[2],4);
    ModulateHDPalette(texture,2,cavePalette);
    SDL_RenderCopy( renderer, texture, &src, &dst );
    return true;
}

bool Graphics::DrawCellarHDBackground( bool alt, int destX, int destY, int width, int height )
{
    if ( renderer == nullptr )
        return false;

    int slot = alt ? 1 : 0;
    // These palette triples are the original StairwayBackgroundCheckAFade rules.
    int stage = palettes[2][1] == 0x0F && palettes[2][2] == 0 && palettes[2][3] == 0 ? 2
        : palettes[2][1] == 0 && palettes[2][2] == 0 && palettes[2][3] == 0x10 ? 1 : 0;
    if ( hdCellarTextures[slot][stage] == nullptr )
    {
        char path[96];
        if(stage)snprintf(path,sizeof(path),"hd/dungeon/StairwayBackground%dFade%d.png",slot+1,stage);
        else snprintf(path,sizeof(path),"hd/dungeon/StairwayBackground%d.png",slot+1);
        hdCellarTextures[slot][stage] = LoadHDTexture( path );
    }

    SDL_Texture* texture = hdCellarTextures[slot][stage];
    if ( texture == nullptr )
        return false;

    SDL_Rect src = { 0, 128, 512, 352 };
    SDL_Rect dst = ScreenRect( destX, destY, width, height );
    unsigned visible=0;for(int i=1;i<4;++i)visible|=systemPalette[palettes[2][i]&63]&0xFFFFFF;
    SDL_SetTextureColorMod(texture,visible?255:0,visible?255:0,visible?255:0);
    SDL_RenderCopy( renderer, texture, &src, &dst );
    return true;
}

bool Graphics::DrawPlayerHDSprite( int direction, int frame, int destX, int destY, int tunic, int shield, int pose, int flash )
{
    if ( renderer == nullptr )
        return false;

    if(World::GetMode()==Mode_Death && palettes[PlayerPalette][1]==0x10
        && palettes[PlayerPalette][2]==0x30 && palettes[PlayerPalette][3]==0)
    {
        int index=shield?1:0;
        if(!hdDeathTextures[index])hdDeathTextures[index]=LoadHDTexture(index?"hd/link/death-1.png":"hd/link/death-0.png");
        if(!hdDeathTextures[index])return false;
        SDL_Rect src={0,0,32,32},dst=ScreenRect(destX,destY,16,16);
        SDL_RenderCopy(renderer,hdDeathTextures[index],&src,&dst);
        return true;
    }

    int death = (pose == 4) ? 1 : 0;
    if ( hdLinkTexture == nullptr || curLinkTunic != tunic || curLinkShield != shield || curLinkDeath != death )
    {
        if ( hdLinkTexture != nullptr )
        {
            SDL_DestroyTexture( hdLinkTexture );
            hdLinkTexture = nullptr;
        }

        const char* path = nullptr;
        if ( death )
        {
            path = "hd/link/LinkGreyDeath.png";
        }
        else if ( tunic == 1 )
        {
            path = shield ? "hd/link/LinkCustomRunningBlueCrossShield.png" : "hd/link/LinkCustomRunningBlueSmallShield.png";
        }
        else if ( tunic == 2 )
        {
            path = shield ? "hd/link/LinkCustomRunningRedCrossShield.png" : "hd/link/LinkCustomRunningRedSmallShield.png";
        }
        else
        {
            path = shield ? "hd/link/LinkCustomRunningGreenCrossShield.png" : "hd/link/LinkCustomRunningGreenSmallShield.png";
        }

        hdLinkTexture = LoadHDTexture( path );
        if ( hdLinkTexture == nullptr )
            hdLinkTexture = LoadHDTexture( "hd/link-green-small.png" );

        curLinkTunic = tunic;
        curLinkShield = shield;
        curLinkDeath = death;
    }

    if ( hdLinkTexture == nullptr )
        return false;

    int col = 0;
    int row = 0;
    SDL_RendererFlip flip = SDL_FLIP_NONE;

    if ( pose == 2 ) // Attack / Thrust
    {
        switch ( direction & 3 )
        {
        case 0: col = 4; row = 5; break;                                // Right
        case 1: col = 4; row = 5; flip = SDL_FLIP_HORIZONTAL; break;   // Left
        case 2: col = 5; row = 5; break;                                // Down
        case 3: col = 6; row = 5; break;                                // Up
        }
    }
    else if ( pose == 3 ) // Lift Item
    {
        col = 3;
        row = 6;
    }
    else if ( pose == 1 ) // Idle
    {
        if ( frame < 0 ) frame = 0;
        col = frame % 8;
        switch ( direction & 3 )
        {
        case 0: row = 12; break;                               // Right
        case 1: row = 12; flip = SDL_FLIP_HORIZONTAL; break;  // Left
        case 2: row = 9; break;                                // Down / Face
        case 3: row = 7; break;                                // Up / Back
        }
    }
    else // Walk (pose == 0 or default)
    {
        if ( frame < 0 ) frame = 0;
        col = frame % 6;
        switch ( direction & 3 )
        {
        case 0: row = 0; break;                                // Right
        case 1: row = 0; flip = SDL_FLIP_HORIZONTAL; break;   // Left
        case 2: row = 3; break;                                // Down / Face
        case 3: row = 1; break;                                // Up / Back
        }
    }

    SDL_Rect src = { col * 32, row * 32, 32, 32 };
    SDL_Rect dst = ScreenRect( destX, destY, 16, 16 );

    if ( flash > 0 )
    {
        static const uint8_t flashColors[4][3] = {
            { 255, 120, 120 },
            { 120, 255, 120 },
            { 120, 120, 255 },
            { 255, 255, 150 }
        };
        int fidx = flash & 3;
        SDL_SetTextureColorMod( hdLinkTexture, flashColors[fidx][0], flashColors[fidx][1], flashColors[fidx][2] );
    }
    else
    {
        SDL_SetTextureColorMod( hdLinkTexture, 255, 255, 255 );
    }

    if ( pose == 3 )
    {
        // hires.txt: lift-left hashes map to (96,192)/(96,208).
        // ItemObtain1/2 map the right tile to (112,192)/(112,208),
        // with the NES sprite's horizontal flip. The atlas is not a
        // contiguous 32px sprite: its two halves both face left.
        SDL_Rect leftSrc = { 96, 192, 16, 32 };
        SDL_Rect rightSrc = { frame == 1 ? 96 : 112, 192, 16, 32 };
        SDL_Rect leftDst = dst;
        leftDst.w /= 2;
        SDL_Rect rightDst = leftDst;
        rightDst.x += leftDst.w;
        SDL_RenderCopyEx( renderer, hdLinkTexture, &leftSrc, &leftDst, 0, nullptr, SDL_FLIP_NONE );
        SDL_RenderCopyEx( renderer, hdLinkTexture, &rightSrc, &rightDst, 0, nullptr, SDL_FLIP_HORIZONTAL );
        return true;
    }
    if(playerBackgroundMaskEnabled)DrawMaskedPlayer(hdLinkTexture,src,destX,destY,flip);
    else SDL_RenderCopyEx( renderer, hdLinkTexture, &src, &dst, 0, nullptr, flip );
    return true;
}

bool Graphics::DrawBossHDSprite( int bossType, int frame, int destX, int destY, int pal )
{
    (void) pal;
    if ( renderer == nullptr )
        return false;

    if ( hdBossCurrentType != bossType )
    {
        if ( hdBossTexture != nullptr )
        {
            SDL_DestroyTexture( hdBossTexture );
            hdBossTexture = nullptr;
        }
        const char* path = nullptr;
        if ( bossType == 0 ) path = "hd/boss/AquamentusBlue.png";
        else if ( bossType == 1 ) path = "hd/boss/AquamentusRed.png";
        else if ( bossType == 2 ) path = "hd/boss/Ganon.png";

        if ( path != nullptr )
            hdBossTexture = LoadHDTexture( path );
        hdBossCurrentType = bossType;
    }

    if ( hdBossTexture == nullptr )
        return false;

    if ( bossType == 0 || bossType == 1 )
    {
        int col = (frame % 2);
        SDL_Rect src = { col * 48, 0, 48, 64 };
        SDL_Rect dst = ScreenRect( destX, destY, 24, 32 );
        SDL_RenderCopy( renderer, hdBossTexture, &src, &dst );
        return true;
    }
    else if ( bossType == 2 )
    {
        int col = (frame % 4);
        SDL_Rect src = { col * 64, 0, 64, 64 };
        SDL_Rect dst = ScreenRect( destX, destY, 32, 32 );
        SDL_RenderCopy( renderer, hdBossTexture, &src, &dst );
        return true;
    }
    return false;
}

bool Graphics::DrawHDTitleScreen()
{
    if ( renderer == nullptr )
        return false;

    const uint32_t frameTime = GetFrameCounter() % 60;
    static const uint8_t frameEnds[8] = { 7, 15, 22, 30, 37, 45, 52, 60 };
    int frame = 0;
    while ( frame < 7 && frameTime >= frameEnds[frame] )
        frame++;

    if ( hdTitleTextures[frame] == nullptr )
    {
        char path[32];
        snprintf( path, sizeof( path ), "hd/title/title%d.png", frame + 1 );
        hdTitleTextures[frame] = LoadHDTexture( path );
    }
    SDL_Texture* texture = hdTitleTextures[frame];
    if ( texture == nullptr )
        return false;

    SDL_Rect src = { 0, 0, 512, 480 };
    SDL_Rect dst = ScreenRect( 0, 0, 256, 240 );
    SDL_RenderCopy( renderer, texture, &src, &dst );
    return true;
}

bool Graphics::DrawHDSelectScreen( int frame )
{
    if ( renderer == nullptr )
        return false;

    int slot = (frame % 6);
    if ( slot < 0 ) slot = 0;

    if ( hdSelectTextures[slot] == nullptr )
    {
        char path[64];
        snprintf( path, sizeof( path ), "hd/ui/selectscreen%d.png", slot + 1 );
        hdSelectTextures[slot] = LoadHDTexture( path );
    }

    SDL_Texture* texture = hdSelectTextures[slot];
    if ( texture == nullptr )
        return false;

    SDL_Rect src = { 0, 0, 512, 480 };
    SDL_Rect dst = ScreenRect( 0, 0, 256, 240 );
    SDL_RenderCopy( renderer, texture, &src, &dst );
    return true;
}

bool Graphics::DrawHDGameOverScreen()
{
    if ( renderer == nullptr )
        return false;

    if ( hdGameOverTexture == nullptr )
        hdGameOverTexture = LoadHDTexture( "hd/ui/GameOver.png" );
    if ( hdGameOverTexture == nullptr )
        return false;

    SDL_Rect src = { 0, 0, 512, 480 };
    SDL_Rect dst = ScreenRect( 0, 0, 256, 240 );
    SDL_RenderCopy( renderer, hdGameOverTexture, &src, &dst );
    return true;
}

bool Graphics::DrawHDHUD( int baseY )
{
    if ( renderer == nullptr )
        return false;

    SDL_Texture*& texture = hudOverlay ? hdHudOverlayTexture : hdHudTexture;
    if ( texture == nullptr )
        texture = LoadHDTexture( hudOverlay ? "hd/ui/hud-overlay.png" : "hd/ui/hud.png" );
    if ( texture == nullptr )
        return false;

    SDL_Rect src = { 0, 0, 512, 128 };
    if ( hudOverlay )
    {
        auto part = [texture]( int sx, int sy, int w, int h, int dx, int dy )
        {
            SDL_Rect source = { sx * 2, sy * 2, w * 2, h * 2 };
            SDL_Rect dest = ScreenRect( dx, dy, w, h );
            SDL_RenderCopy( renderer, texture, &source, &dest );
        };
        part( 184, 24, 64, 8, 8, 8 );       // LIFE, top left
        part( 88, 24, 8, 16, 8, 216 );      // rupee, bottom left
        part( 88, 40, 8, 8, 8, 200 );       // key
        part( 88, 48, 8, 8, 8, 184 );       // bomb
        part( 120, 24, 24, 32, 192, 196 );  // B frame, bottom right
        part( 144, 24, 24, 32, 220, 196 );  // A frame
        return true;
    }
    SDL_Rect dst = ScreenRect( 0, baseY, 256, 64 );
    SDL_RenderCopy( renderer, texture, &src, &dst );
    return true;
}

void Graphics::SetHUDOverlay( bool enabled )
{
    hudOverlay = enabled;
}

void Graphics::SetPlayfieldView( bool enabled )
{
    if ( renderer == nullptr )
        return;
    int width = 640, height = 480;
    SDL_GetRendererOutputSize( renderer, &width, &height );
    SDL_RenderSetClipRect( renderer, nullptr );
    viewScaleX = width / 256.0f;
    float safeY = height / 60.0f;
    viewScaleY = enabled ? height / 176.0f : (height - 2 * safeY) / 240.0f;
    viewOffsetX = 0;
    viewOffsetY = enabled ? -64.0f * viewScaleY : safeY;
    // NES playfield y=64..240 maps to the entire output; menu/HUD use
    // their original y=0..240 canvas. Game coordinates never change.
    // OGC_QueueGeometry in SDL Wii multiplies BOTH axes by scale_x.
    // Keep SDL's scale at 1:1 and transform every rect/clip ourselves.
}

static uint32_t GetPaletteColor( int palette, int color )
{
    if ( directPaletteColorSet[palette][color] )
        return directPaletteColors[palette][color];
    if ( color == 0 )
        return 0;
    return (uint32_t) activeSystemPalette[palettes[palette][color] & 0x3F];
}

static SDL_Texture* BuildPaletteTexture( int slot, int palette )
{
    SDL_Surface* source = tileSheetPixels[slot];
    if ( source == nullptr || renderer == nullptr )
        return nullptr;

    hdTileSheet[slot][palette] = false;
    hdTileReferencePalette[slot][palette] = false;
    if ( activeSystemPalette == systemPalette && sheetNames[slot][0] != '\0' )
    {
        unsigned key = ((unsigned)palettes[palette][1] << 16)
            | ((unsigned)palettes[palette][2] << 8) | palettes[palette][3];
        const World::LevelInfoBlock* info=World::GetLevelInfo();
        unsigned reference=(info->Palettes[palette][1]<<16)|(info->Palettes[palette][2]<<8)|info->Palettes[palette][3];
        for(int palettePass=0;palettePass<2;++palettePass)
        for ( int pass = 0; pass < 2; ++pass )
        for ( const HDSheetKey& entry : hdSheetKeys )
        {
            if ( entry.level != (pass == 0 ? hdLevel : -1) ) continue;
            if ( entry.palette != (palettePass?reference:key) || strcmp(entry.name, sheetNames[slot]) != 0 )
                continue;
            char path[128];
            snprintf( path, sizeof(path), "hd/tiles/%s/%06X.png", entry.folder, palettePass?reference:key );
            SDL_Texture* hd = LoadHDTexture( path );
            if ( hd != nullptr )
            {
                hdTileSheet[slot][palette] = true;
                hdTileReferencePalette[slot][palette] = palettePass!=0;
                return hd;
            }
            break;
        }
    }

    SDL_Surface* rgba = SDL_ConvertSurfaceFormat( source, SDL_PIXELFORMAT_RGBA32, 0 );
    if ( rgba == nullptr )
        return nullptr;

    SDL_LockSurface( rgba );
    for ( int y = 0; y < rgba->h; y++ )
    {
        uint32_t* row = (uint32_t*)((uint8_t*)rgba->pixels + y * rgba->pitch);
        for ( int x = 0; x < rgba->w; x++ )
        {
            uint8_t indexR, indexG, indexB, indexA;
            SDL_GetRGBA( row[x], rgba->format, &indexR, &indexG, &indexB, &indexA );
            int color = indexR >> 4; // source atlas stores NES indexes as 0,16,32,48
            if ( color >= PaletteLength ) color = PaletteLength - 1;

            uint32_t argb = GetPaletteColor( palette, color );
            row[x] = SDL_MapRGBA( rgba->format,
                (argb >> 16) & 0xFF, (argb >> 8) & 0xFF,
                argb & 0xFF, (argb >> 24) & 0xFF );
        }
    }
    SDL_UnlockSurface( rgba );

    SDL_Texture* texture = SDL_CreateTextureFromSurface( renderer, rgba );
    SDL_FreeSurface( rgba );
    if ( texture != nullptr )
        SDL_SetTextureBlendMode( texture, SDL_BLENDMODE_BLEND );
    return texture;
}

static SDL_Texture* GetPaletteTexture( int slot, int palette )
{
    if ( palette < 0 || palette >= PaletteCount )
        palette = 0;
    if ( tileSheets[slot][palette] == nullptr ||
         textureGeneration[slot][palette] != paletteGeneration[palette] )
    {
        if ( tileSheets[slot][palette] != nullptr )
            SDL_DestroyTexture( tileSheets[slot][palette] );
        tileSheets[slot][palette] = BuildPaletteTexture( slot, palette );
        textureGeneration[slot][palette] = paletteGeneration[palette];
    }
    return tileSheets[slot][palette];
}


#include "HUDStyle_wii.inc"
#include "InventoryStyle_wii.inc"
#include "HDConditional_wii.inc"

SDL_Renderer* Graphics_GetRenderer()
{
    return renderer;
}

bool Graphics::Init()
{
    // The main loop uses SDL_GetTicks() and SDL_Delay(), so initialize the
    // timer subsystem along with video before entering it.
    if ( SDL_InitSubSystem( SDL_INIT_VIDEO | SDL_INIT_TIMER ) < 0 )
    {
        SDL_Log( "SDL video initialization failed: %s", SDL_GetError() );
        return false;
    }

    // Standard Wii 480p / 480i resolution
    const int ScreenWidth = 640;
    const int ScreenHeight = 480;

    window = SDL_CreateWindow(
        "Zelda 1 HD Wii",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        ScreenWidth,
        ScreenHeight,
        SDL_WINDOW_SHOWN | SDL_WINDOW_FULLSCREEN );

    if ( window == nullptr )
    {
        WiiDebugLog( "SDL_CreateWindow failed: %s", SDL_GetError() );
        SDL_Log( "SDL window creation failed: %s", SDL_GetError() );
        return false;
    }

    renderer = SDL_CreateRenderer(
        window,
        -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC );

    // Some SDL2 backends (including console ports) do not support every
    // renderer flag. Retry with the backend defaults before failing startup.
    if ( renderer == nullptr )
    {
        WiiDebugLog( "SDL_CreateRenderer failed: %s", SDL_GetError() );
        SDL_Log( "Accelerated renderer unavailable, retrying defaults: %s", SDL_GetError() );
        renderer = SDL_CreateRenderer( window, -1, 0 );
    }

    if ( renderer == nullptr )
    {
        SDL_Log( "SDL renderer creation failed: %s", SDL_GetError() );
        return false;
    }

    // Use nearest neighbor for pixel art clarity
    SDL_SetHint( SDL_HINT_RENDER_SCALE_QUALITY, "0" );

    // Keep NES coordinates for maps, actors and hitboxes, then stretch the
    // completed 512x480 canvas to the actual Wii output dimensions.
    int outputWidth = ScreenWidth, outputHeight = ScreenHeight;
    SDL_GetRendererOutputSize( renderer, &outputWidth, &outputHeight );
    SDL_RenderSetScale( renderer, 1.0f, 1.0f );
    SDL_RenderSetViewport( renderer, nullptr );
    SetPlayfieldView( false );
#if defined(GEKKO)
    // SDL's OGC display mode stores its actual GXRModeObj in driverdata.
    // Expand the VI scanout rather than the framebuffer to fill TV width.
    SDL_DisplayMode displayMode;
    const char* videoDriver = SDL_GetCurrentVideoDriver();
    if ( videoDriver != nullptr && strcmp(videoDriver, "ogc-video") == 0
        && SDL_GetCurrentDisplayMode( SDL_GetWindowDisplayIndex(window), &displayMode ) == 0
        && displayMode.driverdata != nullptr )
    {
        static GXRModeObj wideMode;
        wideMode = *(GXRModeObj*)displayMode.driverdata;
        wideMode.viWidth = 720;
        wideMode.viXOrigin = 0;
        VIDEO_Configure( &wideMode );
        VIDEO_Flush();
        WiiDebugLog( "VI scanout=%d origin=%d", wideMode.viWidth, wideMode.viXOrigin );
    }
#endif
    SDL_ShowCursor( SDL_DISABLE );
    WiiDebugLog( "video requested=640x480 actual=%dx%d", outputWidth, outputHeight );

    int imageInit = IMG_Init( IMG_INIT_PNG );
    WiiDebugLog( "SDL renderer created imageInit=%d imageError=%s",
        imageInit, IMG_GetError() );

    return true;
}

void Graphics::LoadTileSheet( int slot, const char* path )
{
    if ( slot < 0 || slot >= Sheet_Max )
        return;

    UnloadTileSheet( slot );
    if(slot==Sheet_Background)memset(backgroundTransparencyValid,0,sizeof(backgroundTransparencyValid));

    if ( path != nullptr && renderer != nullptr )
    {
        const char* name = strrchr(path, '/');
        name = name ? name + 1 : path;
        snprintf(sheetNames[slot], sizeof(sheetNames[slot]), "%s", name);
        char* extension = strrchr(sheetNames[slot], '.');
        if ( extension ) *extension = '\0';
        tileSheetPixels[slot] = IMG_Load( path );
        if ( tileSheetPixels[slot] == nullptr )
        {
            char altPath[256] = "";
            snprintf( altPath, sizeof( altPath ), "sd:/apps/zelda1hd/%s", path );
            tileSheetPixels[slot] = IMG_Load( altPath );
            if ( tileSheetPixels[slot] == nullptr )
            {
                snprintf( altPath, sizeof( altPath ), "sd:/zelda1hd/%s", path );
                tileSheetPixels[slot] = IMG_Load( altPath );
            }
        }

        WiiDebugLog( "texture slot=%d path=%s result=%s error=%s", slot, path,
            tileSheetPixels[slot] ? "ok" : "FAILED", tileSheetPixels[slot] ? "" : IMG_GetError() );
    }
}

void Graphics::LoadTileSheet( int slot, const char* imagePath, const char* animPath )
{
    LoadTileSheet( slot, imagePath );
    if ( animPath != nullptr )
    {
        Util::LoadResource( animPath, &animSpecs[slot] );
    }
}

void Graphics::UnloadTileSheet( int slot )
{
    if ( slot >= 0 && slot < Sheet_Max )
    {
        sheetNames[slot][0] = '\0';
        for ( int palette = 0; palette < PaletteCount; palette++ )
        {
            hdTileSheet[slot][palette] = false;
            if ( tileSheets[slot][palette] != nullptr )
            {
                SDL_DestroyTexture( tileSheets[slot][palette] );
                tileSheets[slot][palette] = nullptr;
            }
            textureGeneration[slot][palette] = 0;
        }
        if ( tileSheetPixels[slot] != nullptr )
        {
            SDL_FreeSurface( tileSheetPixels[slot] );
            tileSheetPixels[slot] = nullptr;
        }
    }
}

void Graphics::Begin()
{
}

void Graphics::End()
{
    // Match the desktop backend: End closes a drawing batch, it does not
    // present the frame. Several menu/world draw paths call End themselves.
}

void Graphics::Present()
{
    if ( renderer != nullptr )
    {
        SDL_RenderPresent( renderer );
    }
}

void Graphics::Clear( uint8_t r, uint8_t g, uint8_t b )
{
    if ( renderer != nullptr )
    {
        SDL_SetRenderDrawColor( renderer, r, g, b, 255 );
        // SDL_RenderClear ignores SDL_RenderSetClipRect on SDL renderers.
        // The world clears only the playfield after StatusBar::Draw has drawn
        // the HUD, so clearing the whole target here erased the HUD every
        // frame. A filled rect obeys the active clip and still clears the
        // full target when no clip is set.
        SDL_RenderFillRect( renderer, nullptr );
    }
}

void Graphics::Clear( int sysColor )
{
    uint32_t argb = GetSystemColorArgb( sysColor );
    uint8_t r = (argb >> 16) & 0xFF;
    uint8_t g = (argb >>  8) & 0xFF;
    uint8_t b = (argb >>  0) & 0xFF;
    Clear( r, g, b );
}

void Graphics::DrawSpriteTile( 
    int slot, 
    int srcX, 
    int srcY, 
    int width, 
    int height, 
    int destX, 
    int destY, 
    int palette,
    int flags )
{
    DrawTile(
        slot,
        srcX,
        srcY,
        width,
        height,
        destX,
        destY + 1,
        palette,
        flags );
}

void Graphics::DrawTile( 
    int slot, 
    int srcX, 
    int srcY, 
    int width, 
    int height, 
    int destX, 
    int destY, 
    int palette,
    int flags )
{
    if ( slot < 0 || slot >= Sheet_Max || tileSheetPixels[slot] == nullptr || renderer == nullptr )
        return;

    if ( slot == Sheet_Background && hdLevel > 0 && strcmp(sheetNames[slot], "underworldTiles") == 0
        && srcX == 96 && srcY == 0 && width == 16 && height == 16
        && palette >= 0 && palette < PaletteCount && activeSystemPalette == systemPalette )
    {
        unsigned key = ((unsigned)palettes[palette][1] << 16) | ((unsigned)palettes[palette][2] << 8) | palettes[palette][3];
        for ( unsigned i = 0; i < sizeof(hdDungeonWaterSpecs)/sizeof(hdDungeonWaterSpecs[0]); ++i )
        {
            const HDDungeonWater& spec = hdDungeonWaterSpecs[i];
            if ( spec.palette != key || spec.level != hdLevel ) continue;
            if ( !hdDungeonWaterTextures[i] ) hdDungeonWaterTextures[i] = LoadHDTexture(spec.path);
            if ( !hdDungeonWaterTextures[i] ) break;
            int phase = 0, time = GetFrameCounter() % spec.period;
            for ( int f = 1; f < 4; ++f ) if ( time >= spec.begins[f] ) phase = f;
            SDL_Rect source = {phase * 32, 0, 32, 32};
            SDL_Rect dest = ScreenRect(destX, destY, width, height);
            SDL_RenderCopy(renderer, hdDungeonWaterTextures[i], &source, &dest);
            int above=(flags>>8)-1;bool edge=false;
            if(above>=0)for(int e=0;e<7 && !edge;++e)
                if(spec.edges[e][0])edge=MatchBackgroundTile(above,spec.edges[e],0,8)||MatchBackgroundTile(above,spec.edges[e],8,8);
            if(edge)
            {
                source={phase*32,32,32,16};dest=ScreenRect(destX,destY,width,8);
                SDL_RenderCopy(renderer,hdDungeonWaterTextures[i],&source,&dest);
            }
            return;
        }
    }

    SDL_Texture* texture = GetPaletteTexture( slot, palette );
    if ( texture == nullptr )
        return;

    if(palette>=0 && palette<PaletteCount && hdTileSheet[slot][palette])
    {
        if(hdTileReferencePalette[slot][palette])ModulateHDPalette(texture,palette);
        else SDL_SetTextureColorMod(texture,255,255,255);
    }
    if(DrawConditionalSprite(slot,srcX,srcY,width,height,destX,destY,palette,flags,texture))return;

    SDL_Rect srcRect = { srcX, srcY, width, height };
    if ( palette >= 0 && palette < PaletteCount && hdTileSheet[slot][palette] )
    {
        srcRect.x *= 2;
        srcRect.y *= 2;
        srcRect.w *= 2;
        srcRect.h *= 2;
    }
    SDL_Rect dstRect = ScreenRect( destX, destY, width, height );

    SDL_RendererFlip flip = SDL_FLIP_NONE;
    if ( flags & Flip_Horizontal )
        flip = (SDL_RendererFlip)(flip | SDL_FLIP_HORIZONTAL);
    if ( flags & Flip_Vertical )
        flip = (SDL_RendererFlip)(flip | SDL_FLIP_VERTICAL);

    SDL_RenderCopyEx( renderer, texture, &srcRect, &dstRect, 0, nullptr, flip );
}

void Graphics::LoadSystemPalette( const int* colorsArgb8 )
{
    for ( int i = 0; i < SysPaletteLength; i++ )
    {
        systemPalette[i] = FromLE32( (uint32_t) colorsArgb8[i] );
        grayscalePalette[i] = systemPalette[i & 0x30];
    }
    for ( int i = 0; i < PaletteCount; i++ ) InvalidatePalette( i );
}

uint32_t Graphics::GetSystemColorArgb( int sysColor )
{
    if ( sysColor >= 0 && sysColor < SysPaletteLength )
        return (uint32_t) activeSystemPalette[sysColor];
    return 0xFF000000;
}

void Graphics::SetColor( int paletteIndex, int colorIndex, int colorArgb8 )
{
    if ( paletteIndex < 0 || paletteIndex >= PaletteCount ||
         colorIndex < 0 || colorIndex >= PaletteLength )
        return;
    directPaletteColors[paletteIndex][colorIndex] = (uint32_t) colorArgb8;
    directPaletteColorSet[paletteIndex][colorIndex] = true;
    InvalidatePalette( paletteIndex );
}

void Graphics::SetColorIndexed( int paletteIndex, int colorIndex, int sysColor )
{
    if ( paletteIndex >= 0 && paletteIndex < PaletteCount && colorIndex >= 0 && colorIndex < PaletteLength )
    {
        palettes[paletteIndex][colorIndex] = (uint8_t) sysColor;
        directPaletteColorSet[paletteIndex][colorIndex] = false;
        InvalidatePalette( paletteIndex );
    }
}

void Graphics::SetPaletteIndexed( int paletteIndex, const uint8_t* sysColors )
{
    if ( paletteIndex >= 0 && paletteIndex < PaletteCount )
    {
        memcpy( palettes[paletteIndex], sysColors, PaletteLength );
        memset( directPaletteColorSet[paletteIndex], 0, sizeof directPaletteColorSet[paletteIndex] );
        InvalidatePalette( paletteIndex );
    }
}

void Graphics::UpdatePalettes()
{
    // Paletted SDL textures are rebuilt lazily on their next draw.
}

void Graphics::SwitchSystemPalette( int* newSystemPalette )
{
    if ( activeSystemPalette == newSystemPalette )
        return;

    activeSystemPalette = newSystemPalette;
    for ( int i = 0; i < PaletteCount; i++ ) InvalidatePalette( i );
}

void Graphics::EnableGrayscale()
{
    SwitchSystemPalette( grayscalePalette );
}

void Graphics::DisableGrayscale()
{
    SwitchSystemPalette( systemPalette );
}

void Graphics::SetViewParams( float scale, float x, float y )
{
    viewScaleX = scale;
    viewScaleY = scale;
    viewOffsetX = x;
    viewOffsetY = y;
}

void Graphics::SetClip( int x, int y, int width, int height )
{
    if ( renderer == nullptr )
        return;

    SDL_Rect r = ScreenRect( x, y, width, height );
    int outputWidth, outputHeight;
    SDL_GetRendererOutputSize( renderer, &outputWidth, &outputHeight );
    SDL_Rect bounds = { 0, 0, outputWidth, outputHeight };
    SDL_Rect clipped = { 0, 0, 0, 0 };
    SDL_IntersectRect( &r, &bounds, &clipped );
    SDL_RenderSetClipRect( renderer, &clipped );
}

void Graphics::ResetClip()
{
    if ( renderer == nullptr )
        return;

    SDL_RenderSetClipRect( renderer, nullptr );
}

const SpriteAnim* Graphics::GetAnimation( int slot, int animIndex )
{
    if ( slot >= 0 && slot < Sheet_Max )
        return animSpecs[slot].GetItem( animIndex );
    return nullptr;
}
