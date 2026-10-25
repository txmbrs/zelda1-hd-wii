// Wii port modifications, 2026, txmbrs and contributors.
#include "HDGeneratedConditional.h"
static SDL_Texture* conditionalTextures[HDConditionalPages] = {};

static bool MatchSpritePattern(SDL_Surface* surface,int x,int y,const unsigned char* hash)
{
    if(x<0 || y<0 || x+8>surface->w || y+8>surface->h)return false;
    unsigned char bytes[16]={};
    for(int row=0;row<8;++row)for(int col=0;col<8;++col)
    {
        Uint32 pixel=0;memcpy(&pixel,(Uint8*)surface->pixels+(y+row)*surface->pitch+(x+col)*surface->format->BytesPerPixel,surface->format->BytesPerPixel);
        Uint8 r,g,b,a;SDL_GetRGBA(pixel,surface->format,&r,&g,&b,&a);int value=r>>4;
        bytes[row]|=(value&1)<<(7-col);bytes[row+8]|=((value>>1)&1)<<(7-col);
    }
    return memcmp(bytes,hash,16)==0;
}

static bool DrawConditionalSprite(int slot,int sx,int sy,int width,int height,int dx,int dy,int palette,int flags,SDL_Texture* base)
{
    if(slot!=Sheet_Npcs && slot!=Sheet_Boss && slot!=Sheet_PlayerAndItems)return false;
    if(width%8 || height%8 || width*height>8192 || palette<0 || palette>=PaletteCount)return false;
    static const char* names[]={"owNpcs","uwNpcs","uwBoss1257","uwBoss3468","uwBoss9","playerItem"};
    int sheet=-1;for(int i=0;i<6;++i)if(strcmp(sheetNames[slot],names[i])==0)sheet=i;
    if(sheet<0)return false;
    unsigned key=(palettes[palette][1]<<16)|(palettes[palette][2]<<8)|palettes[palette][3];
    int selected[128];int count=width*height/64;for(int i=0;i<count;++i)selected[i]=-1;
    bool changed=false;int cols=width/8;
    for(const HDConditionalGroup& group:hdConditionalGroups)
    {
        if(group.sheet!=sheet || group.x<sx || group.y<sy || group.x>=sx+width || group.y>=sy+height)continue;
        for(int index=group.first;index<group.first+group.count;++index)
        {
            const HDConditionalRule& rule=hdConditionalRules[index];if(rule.palette!=key)continue;
            bool valid=true;
            for(int c=rule.first;c<rule.first+rule.count && valid;++c)
            {
                const HDPredicate& p=hdPredicates[c];bool value=false;
                switch(p.kind)
                {
                case 0:value=GetFrameCounter()%p.a>=(unsigned)p.b;break;
                case 1:
                    // Conditions may inspect only pixels belonging to this drawn sprite.
                    // An unrelated adjacent cell in the source atlas is not a neighbor.
                    if(group.x+p.a<sx || group.y+p.b<sy || group.x+p.a+8>sx+width || group.y+p.b+8>sy+height){valid=false;continue;}
                    if(p.palette==key && group.x+p.a>=sx && group.y+p.b>=sy
                        && group.x+p.a+8<=sx+width && group.y+p.b+8<=sy+height)
                        value=MatchSpritePattern(tileSheetPixels[slot],group.x+p.a,group.y+p.b,p.hash);
                    break;
                case 2:value=((rule.mirror^flags)&Flip_Horizontal)!=0;break;
                case 3:value=((rule.mirror^flags)&Flip_Vertical)!=0;break;
                case 4:value=hdLevel==p.a;break;
                }
                valid=p.negative?!value:value;
            }
            if(valid)
            {
                int page=rule.patch/1024;
                if(!conditionalTextures[page]){char path[64];snprintf(path,sizeof(path),"hd/conditional/%d.png",page);conditionalTextures[page]=LoadHDTexture(path);}
                if(conditionalTextures[page]){selected[(group.y-sy)/8*cols+(group.x-sx)/8]=rule.patch;changed=true;}
                break;
            }
        }
    }
    if(!changed)return false;
    int scale=hdTileSheet[slot][palette]?2:1;
    SDL_RendererFlip flip=SDL_FLIP_NONE;
    if(flags&Flip_Horizontal)flip=(SDL_RendererFlip)(flip|SDL_FLIP_HORIZONTAL);
    if(flags&Flip_Vertical)flip=(SDL_RendererFlip)(flip|SDL_FLIP_VERTICAL);
    for(int i=0;i<count;++i)
    {
        int x=(i%cols)*8,y=(i/cols)*8,patch=selected[i];SDL_Texture* texture=base;
        SDL_Rect source={(sx+x)*scale,(sy+y)*scale,8*scale,8*scale};
        if(patch>=0){texture=conditionalTextures[patch/1024];int local=patch%1024;source={local%32*16,local/32*16,16,16};}
        SDL_Rect dest=ScreenRect(dx+((flags&Flip_Horizontal)?width-8-x:x),dy+((flags&Flip_Vertical)?height-8-y:y),8,8);
        SDL_RenderCopyEx(renderer,texture,&source,&dest,0,nullptr,flip);
    }
    return true;
}
