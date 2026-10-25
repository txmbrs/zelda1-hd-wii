// Wii port modifications, 2026, txmbrs and contributors.
// HUD coordinates use the approved 1280x720 mockup independently of world coordinates.
// This file is included by Graphics_wii.cpp, after the platform texture helpers.
static SDL_Texture* styledHUDTexture = nullptr;

struct StyledHUDPainter
{
    int outputWidth, outputHeight;
    int offsetY=0;
    SDL_Rect Rect(int x, int y, int w, int h) const
    {
        int left = (int)std::lround(x * outputWidth / 1280.0);
        int top = (int)std::lround((y+offsetY) * outputHeight / 720.0);
        int right = (int)std::lround((x+w) * outputWidth / 1280.0);
        int bottom = (int)std::lround((y+h+offsetY) * outputHeight / 720.0);
        return {left, top, right-left, bottom-top};
    }
    void Fill(int x, int y, int w, int h, unsigned color, int alpha=255) const
    {
        SDL_SetRenderDrawColor(renderer, color>>16, (color>>8)&255, color&255, alpha);
        SDL_Rect r = Rect(x,y,w,h); SDL_RenderFillRect(renderer, &r);
    }
    void Copy(int sx, int sy, int sw, int sh, int x, int y, int w, int h) const
    {
        SDL_Rect src = {sx,sy,sw,sh}, dst = Rect(x,y,w,h);
        SDL_RenderCopy(renderer, styledHUDTexture, &src, &dst);
    }
    void Text(const char* text, int x, int y, int size=16) const
    {
        for (; *text; ++text, x+=size)
        {
            int ch = *text>='0' && *text<='9' ? *text-'0' : *text>='A' && *text<='Z' ? *text-'A'+10 : -1;
            if (ch>=0) Copy(ch*8,160,8,8,x,y,size,size);
        }
    }
    void Panel(int x, int y, int w, int h) const
    {
        // Keep the world visible between HUD elements; only the frame is opaque.
        int xy[9][2]={{x+6,y},{x+w-6,y},{x+w,y+6},{x+w,y+h-6},{x+w-6,y+h},{x+6,y+h},{x,y+h-6},{x,y+6},{x+6,y}};
        SDL_Point points[9];
        for (int i=0;i<9;++i) {SDL_Rect r=Rect(xy[i][0],xy[i][1],0,0);points[i]={r.x,r.y};}
        SDL_SetRenderDrawColor(renderer,0x66,0x53,0x3A,255); SDL_RenderDrawLines(renderer,points,9);
        Fill(x+5,y+5,2,2,0xC3A36A);Fill(x+w-6,y+5,2,2,0xC3A36A);
        Fill(x+5,y+h-6,2,2,0xC3A36A);Fill(x+w-6,y+h-6,2,2,0xC3A36A);
    }
    void Outline(int x, int y, int w, int h, unsigned color) const
    {
        SDL_SetRenderDrawColor(renderer,color>>16,(color>>8)&255,color&255,255);
        SDL_Rect rect=Rect(x,y,w,h);SDL_RenderDrawRect(renderer,&rect);
    }
    void Item(int itemId, int x, int y) const
    {
        if (itemId<0 || itemId>=36) return;
        Copy((itemId%8)*48,256+(itemId/8)*48,48,48,x,y,48,48);
    }
    void Cursor(int col, int row, unsigned color) const
    {
        if (col<0 || col>=16 || row<0 || row>=8) return;
        // Bounds are the original 16x8 grid inside PriorityGraphics2.png.
        int x=1072+(19+col*8+4)*176/160;
        int y=32+(18+row*8+4)*106/96;
        Fill(x-4,y-4,9,9,0x332719);Fill(x-2,y-2,5,5,color);
    }
};

bool Graphics::DrawStyledHUD(int features)
{
    if (!renderer) return false;
    if (!styledHUDTexture) styledHUDTexture=LoadHDTexture("hd/ui/hud-style.png");
    if (!styledHUDTexture) return false;
    StyledHUDPainter p; SDL_GetRendererOutputSize(renderer,&p.outputWidth,&p.outputHeight);
    Uint8 r,g,b,a;SDL_GetRenderDrawColor(renderer,&r,&g,&b,&a);
    SDL_BlendMode blend;SDL_GetRenderDrawBlendMode(renderer,&blend);
    SDL_Rect clip;bool clipped=SDL_RenderIsClipEnabled(renderer);SDL_RenderGetClipRect(renderer,&clip);
    SDL_RenderSetClipRect(renderer,nullptr);SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_BLEND);
    if (features & StatusBar::Feature_Counters)
    {
        int total=World::GetItem(ItemSlot_HeartContainers);if(total<1)total=1;if(total>16)total=16;
        int cols=total>8?8:total;if(cols<5)cols=5;
        p.Panel(32,32,26+cols*28,total>8?80:52);
        unsigned health=World::GetProfile().Hearts;
        unsigned full=health>>8,partial=health&255;
        if(partial>=128)++full;
        for(int i=0;i<total;++i)
        {
            int state=(unsigned)i<full?2:((unsigned)i==(health>>8) && partial>0?1:0);
            p.Copy(state*28,112,26,26,46+(i%8)*28,46+(i/8)*28,26,26);
        }
        p.Panel(32,628,292,60);
        p.Fill(133,642,1,33,0x66533A);p.Fill(221,642,1,33,0x66533A);
        int iconX[3]={46,144,232};
        for(int i=0;i<3;++i)p.Copy(192+i*32,48,24,32,iconX[i]-2,642,24,32);
        char count[8];snprintf(count,sizeof(count),"%03u",World::GetItem(ItemSlot_Rupees));p.Text(count,74,650);
        if(World::GetItem(ItemSlot_MagicKey))p.Text("ALL",172,650);
        else{snprintf(count,sizeof(count),"%02u",World::GetItem(ItemSlot_Keys));p.Text(count,172,650);}
        snprintf(count,sizeof(count),"%02u",World::GetItem(ItemSlot_Bombs));p.Text(count,260,650);
    }
    p.Copy(0,0,160,96,1072,32,176,106);
    int room=World::GetRoomId();
    if(World::IsOverworld())
    {
        if(features & StatusBar::Feature_MapCursors)p.Cursor(room&15,(room>>4)&7,0xF5E5C0);
    }
    else
    {
        const World::LevelInfoBlock* info=World::GetLevelInfo();
        p.Fill(1072+19*176/160,32+18*106/96,128*176/160+1,64*106/96+1,0x9D682B);
        if(World::HasCurrentMap())
            for(int c=0;c<12;++c)for(int row=0;row<8;++row)
                if(info->DrawnMap[c+4] & (0x80>>row))
                    p.Fill(1072+(19+(c+2)*8+1)*176/160,32+(18+row*8+1)*106/96,6*176/160,6*106/96,0xF4D79B);
        if(features & StatusBar::Feature_MapCursors)
        {
            if(World::HasCurrentCompass())
            {
                int target=info->TriforceRoomId;
                int col=((target+info->DrawnMapOffset)&15)-4+2;
                unsigned color=!World::GotItem(target) && !(GetFrameCounter()&16)?0xD85B4A:0xF5D582;
                p.Cursor(col,(target>>4)&7,color);
            }
            if(World::IsUWMain(room))p.Cursor(((room+info->DrawnMapOffset)&15)-4+2,(room>>4)&7,0xF5E5C0);
        }
    }
    if(features & StatusBar::Feature_Equipment)
    {
        int xs[2]={1096,1184};
        for(int i=0;i<2;++i)
        {
            int x=xs[i];p.Panel(x,612,64,76);p.Outline(x+20,604,24,24,0x66533A);
            p.Text(i==0?"B":"A",x+25,609,14);
        }
        int sword=World::GetItem(ItemSlot_Sword);
        if(sword)p.Item(ItemValueToItemId(ItemSlot_Sword,sword),1192,632);
        const Profile& profile=World::GetProfile();int slot=profile.SelectedItem;
        if(slot && profile.Items[slot])p.Item(ItemValueToItemId(slot,profile.Items[slot]),1104,632);
    }
    SDL_SetRenderDrawBlendMode(renderer,blend);SDL_SetRenderDrawColor(renderer,r,g,b,a);
    SDL_RenderSetClipRect(renderer,clipped?&clip:nullptr);
    return true;
}
