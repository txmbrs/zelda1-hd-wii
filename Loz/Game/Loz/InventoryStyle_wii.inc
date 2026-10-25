// Wii port modifications, 2026, txmbrs and contributors.
bool Graphics::DrawStyledInventory(int top,const int* items,const int* passive,int selected)
{
    if(!renderer)return false;
    if(!styledHUDTexture)styledHUDTexture=LoadHDTexture("hd/ui/hud-style.png");
    if(!styledHUDTexture)return false;
    StyledHUDPainter p;SDL_GetRendererOutputSize(renderer,&p.outputWidth,&p.outputHeight);
    p.offsetY=12+(int)std::lround(top*696.0/240.0);
    Uint8 r,g,b,a;SDL_GetRenderDrawColor(renderer,&r,&g,&b,&a);
    SDL_BlendMode blend;SDL_GetRenderDrawBlendMode(renderer,&blend);SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_BLEND);
    p.Fill(0,0,1280,504,0x10100D,250);p.Panel(32,8,1216,490);
    p.Text("INVENTORY",56,28,24);p.Fill(56,68,1168,1,0x66533A);
    p.Text("EQUIPMENT",56,94,16);
    for(int i=0;i<6;++i){p.Panel(56+i*82,124,68,68);p.Item(passive[i],66+i*82,134);}
    p.Text("ITEM B",56,222,16);p.Panel(56,252,84,84);
    const Profile& profile=World::GetProfile();
    int slot=profile.SelectedItem;
    if(slot && profile.Items[slot])p.Item(ItemValueToItemId(slot,profile.Items[slot]),74,270);
    p.Text("SELECT",660,94,16);
    for(int i=0;i<8;++i)
    {
        int x=660+(i%4)*134,y=124+(i/4)*102;p.Panel(x,y,96,84);
        p.Item(items[i],x+24,y+18);
        if(i==2 && World::GetItem(ItemSlot_Bow))p.Item(Item_Bow,x+40,y+18);
        if(i==selected){p.Outline(x-4,y-4,104,92,0xE6C889);p.Outline(x-2,y-2,100,88,0xC3A36A);}
    }
    if(World::IsOverworld())
    {
        p.Text("TRIFORCE",56,374,16);
        int pieces=World::GetItem(ItemSlot_TriforcePieces);
        for(int i=0;i<8;++i)
        {
            bool have=(pieces&(1<<i))!=0;
            SDL_SetTextureColorMod(styledHUDTexture,have?255:65,have?255:60,have?255:50);
            p.Item(Item_TriforcePiece,56+i*56,404);
        }
        SDL_SetTextureColorMod(styledHUDTexture,255,255,255);
    }
    else
    {
        const World::LevelInfoBlock* info=World::GetLevelInfo();char label[24];snprintf(label,sizeof(label),"LEVEL %u",info->LevelNumber);p.Text(label,56,374,16);
        p.Copy(0,0,160,96,660,344,176,106);
        p.Fill(681,364,141,71,0x9D682B);
        if(World::HasCurrentMap())for(int c=0;c<12;++c)for(int row=0;row<8;++row)
            if(info->DrawnMap[c+4]&(0x80>>row))p.Fill(681+(c+2)*9,364+row*9,7,7,0xF4D79B);
        if(World::HasCurrentMap())p.Item(Item_Map,56,402);
        if(World::HasCurrentCompass())p.Item(Item_Compass,112,402);
        int room=World::GetRoomId(),col=((room+info->DrawnMapOffset)&15)-4+2;
        if(World::IsUWMain(room) && col>=0 && col<16)p.Fill(683+col*9,366+((room>>4)&7)*9,5,5,0xF5E5C0);
    }
    SDL_SetRenderDrawBlendMode(renderer,blend);SDL_SetRenderDrawColor(renderer,r,g,b,a);return true;
}
