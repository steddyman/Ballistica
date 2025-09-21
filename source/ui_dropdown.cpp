// ui_dropdown.cpp - dropdown widget implementation
#include <citro2d.h>
#include "ui_dropdown.hpp"
#include "hardware.hpp"

UIDropdownEvent ui_dropdown_update(UIDropdown &dd, const InputState &in, bool &touchConsumed) {
    touchConsumed = false;
    if(!in.touchPressed) return UIDropdownEvent::None;
    int x=in.stylusX, y=in.stylusY;
    const auto &items = dd.items? *dd.items : std::vector<std::string>{};
    // Header click toggles
    if (x>=dd.x && x<dd.x+dd.w && y>=dd.y && y<dd.y+dd.h) {
        dd.open = !dd.open;
        touchConsumed = true;
        // clamp
        if (dd.selectedIndex < 0 && !items.empty()) dd.selectedIndex=0;
        if (dd.selectedIndex >= (int)items.size() && !items.empty()) dd.selectedIndex=(int)items.size()-1;
        return UIDropdownEvent::None;
    }
    if (!dd.open) return UIDropdownEvent::None;
    // Click inside list overlay
    bool scrolling = items.size() > (size_t)dd.maxVisible;
    int listY = dd.y + dd.h;
    int overlayH = scrolling ? (dd.maxVisible + 2) * dd.itemHeight : (int)items.size() * dd.itemHeight;
    if (x>=dd.x && x<dd.x+dd.w && y>=listY && y<listY+overlayH) {
        touchConsumed = true;
        if (scrolling) {
            int topArrowY = listY;
            int itemsY0 = topArrowY + dd.itemHeight;
            int itemsY1 = itemsY0 + dd.maxVisible * dd.itemHeight;
            int bottomArrowY = itemsY1;
            if (y >= topArrowY && y < topArrowY + dd.itemHeight) {
                if (dd.scrollOffset>0) {
                    dd.scrollOffset--;
                }
                return UIDropdownEvent::None;
            }
            if (y >= bottomArrowY && y < bottomArrowY + dd.itemHeight) {
                if (dd.scrollOffset + dd.maxVisible < (int)items.size()) {
                    dd.scrollOffset++;
                }
                return UIDropdownEvent::None;
            }
            if (y >= itemsY0 && y < itemsY1) {
                int rel = (y - itemsY0) / dd.itemHeight;
                int idx = dd.scrollOffset + rel;
                if (idx>=0 && idx < (int)items.size()) { dd.selectedIndex=idx; dd.open=false; if(dd.onSelect) dd.onSelect(idx); return UIDropdownEvent::SelectionChanged; }
                dd.open=false; return UIDropdownEvent::None;
            }
            dd.open=false; return UIDropdownEvent::None;
        } else { // non scrolling
            int rel = (y - listY) / dd.itemHeight;
            int idx = rel;
            if (idx>=0 && idx < (int)items.size()) { dd.selectedIndex=idx; dd.open=false; if(dd.onSelect) dd.onSelect(idx); return UIDropdownEvent::SelectionChanged; }
            dd.open=false; return UIDropdownEvent::None;
        }
    }
    // Outside overlay: close swallow
    if (dd.open) { dd.open=false; touchConsumed=true; }
    return UIDropdownEvent::None;
}

void ui_dropdown_render(const UIDropdown &dd) {
    const auto &items = dd.items? *dd.items : std::vector<std::string>{};
    // Header (use dd.h for exact collapsed height)
    C2D_DrawRectSolid(dd.x, dd.y, 0, dd.w, dd.h, dd.headerColor);
    const char *label = items.empty()? "(none)" : (dd.selectedIndex >=0 && dd.selectedIndex < (int)items.size() ? items[dd.selectedIndex].c_str() : "?");
    // Vertically center text within header using dd.h
    int textY = dd.y + dd.h/2 - 4; // 8px font height -> offset 4
    hw_draw_text(dd.x+8, textY, label, 0xFFFFFFFF);
    // Arrow box width scales lightly with header height; min 14px
    int arrowBoxW = dd.h + 3; if (arrowBoxW < 14) arrowBoxW = 14; int arrowX=dd.x+dd.w-arrowBoxW; C2D_DrawRectSolid(arrowX, dd.y, 0, arrowBoxW, dd.h, dd.arrowColor);
    int triH=7; int triW=1+(triH-1)*2; if (triW>11) triW=11; int triCx = arrowX + arrowBoxW/2; int midY = dd.y + dd.h/2; uint32_t triCol = 0xC8C8E6FF;
    if (dd.open) { int apexY=midY-triH/2; for(int row=0; row<triH; ++row){ int span=1+row*2; if(span>triW) span=triW; int x0=triCx-span/2; int y=apexY+row; C2D_DrawRectSolid(x0,y,0,span,1,triCol);} }
    else { int apexY=midY+triH/2; for(int row=0; row<triH; ++row){ int span=1+row*2; if(span>triW) span=triW; int x0=triCx-span/2; int y=apexY-row; C2D_DrawRectSolid(x0,y,0,span,1,triCol);} }
    if(!dd.open) return;
    bool scrolling = items.size() > (size_t)dd.maxVisible;
    int listY = dd.y + dd.h; int itemH = dd.itemHeight - 2; if (itemH < 12) itemH = dd.itemHeight; // tighten but keep readable
    if (scrolling) {
        int h=(dd.maxVisible+2)*itemH; C2D_DrawRectSolid(dd.x, listY, 0, dd.w, h, dd.listBgColor);
        // Top arrow row
        C2D_DrawRectSolid(dd.x+2, listY+2, 0, dd.w-4, itemH-4, dd.itemColor);
        hw_draw_text(dd.x+dd.w/2-12, listY+4, "UP", 0xFFFFFFFF);
        int itemsY0 = listY + itemH;
        for(int vis=0; vis<dd.maxVisible; ++vis){ int fi=dd.scrollOffset+vis; if(fi >= (int)items.size()) break; int iy=itemsY0 + vis*itemH; uint32_t col = (fi==dd.selectedIndex)? dd.itemSelColor : dd.itemColor; C2D_DrawRectSolid(dd.x+2, iy+1, 0, dd.w-4, itemH-2, col); hw_draw_text(dd.x+6, iy+4, items[fi].c_str(), 0xFFFFFFFF);}        
        int bottomY = itemsY0 + dd.maxVisible * itemH; C2D_DrawRectSolid(dd.x+2, bottomY+2, 0, dd.w-4, itemH-4, dd.itemColor); hw_draw_text(dd.x+dd.w/2-16, bottomY+4, "DOWN", 0xFFFFFFFF);
    } else {
        int h=(int)items.size()*itemH; C2D_DrawRectSolid(dd.x, listY, 0, dd.w, h, dd.listBgColor);
        for(size_t i=0;i<items.size();++i){ int iy=listY + (int)i*itemH; uint32_t col=(i==(size_t)dd.selectedIndex)? dd.itemSelColor : dd.itemColor; C2D_DrawRectSolid(dd.x+2, iy+1, 0, dd.w-4, itemH-2, col); hw_draw_text(dd.x+6, iy+4, items[i].c_str(), 0xFFFFFFFF);}    
    }
}
