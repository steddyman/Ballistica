// game.cpp - incremental gameplay port (modernizing legacy main.cpp)
#include <vector>
#include <citro2d.h>
#include <algorithm>
#include <cstdint>
#include <cmath>
#include "hardware.hpp"
#include "IMAGE.h"
#include "IMAGE_t3x.h"
#include "sprite_indexes/image_indices.h"
#include "TITLE.h"
#include "BREAK.h"
#include "INSTRUCT.h"
#include "DESIGNER.h"
#include "HIGH.h"
#include "highscores.hpp"
#include "game.hpp"
#include "levels.hpp"
#include "brick.hpp"
#include <vector>

// Basic game state migrated from legacy structures (incremental port)
namespace game {
    struct Ball { float x, y; float vx, vy; bool active; C2D_Image img; };
    struct Laser { float x,y; bool active; };
    struct Bat  { float x, y; float width, height; C2D_Image img; };
    enum class Mode { Title, Playing, Editor };

    struct State {
        Bat bat{};
        std::vector<Ball> balls;
    std::vector<Laser> lasers;
        C2D_Image imgBall{};
        C2D_Image imgBatNormal{};
        Mode mode = Mode::Title;
        int lives = 5;
        unsigned long score = 0;
        uint8_t bonusBits = 0; // collected B1..B5 letters
        bool editorLaunched = false;
    // Timers/effects
    int reverseTimer = 0; // frames remaining reverse controls
    int lightsOffTimer = 0; // frames until lights restore
    int murderTimer = 0; // murderball active
    int laserCharges = 0; // available shots
    int fireCooldown = 0; // frames until next laser
    // Moving brick offsets per index (size = 13*11) small oscillation
    std::vector<float> moveOffset; // current offset x in pixels
    std::vector<float> moveDir;    // direction (-1/+1)
    };

    static State G;

    void init_assets() {
        G.imgBall = hw_image(IMAGE_ball_sprite_idx);
        G.imgBatNormal = hw_image(IMAGE_bat_normal_idx);
        G.bat = { 160.f - 32.f, 200.f, 64.f, 8.f, G.imgBatNormal };
        G.balls.push_back({160.f - 4.f, 180.f, 0.0f, -1.5f, true, G.imgBall});
    }

    void init() {
    hw_log("game_init\n");
        init_assets();
        levels_load();
    hw_log("assets loaded\n");
    // Initialize moving brick buffers
    int total = levels_grid_width()*levels_grid_height();
    G.moveOffset.assign(total,0.f);
    G.moveDir.assign(total,1.f);
    highscores::init();
    }

    // Title screen configuration (button rectangles)
    static const struct { int x,y,w,h; Mode next; } kTitleButtons[] = {
        {16,28,22,12, Mode::Playing}, // New Game
        {16,39,22,12, Mode::Editor}   // Edit Levels
    };

    // Sequence of sheets to show while idling (fallback to BREAK if others missing)
    struct SeqEntry { HwSheet sheet; int index; };
    static const SeqEntry kSequence[] = {
        {HwSheet::Title, TITLE_idx},
        {HwSheet::High, HIGH_idx},
        {HwSheet::Instruct, INSTRUCT_idx},
        {HwSheet::Break, BREAK_idx}
    };
    static int seqPos = 0;
    static int seqTimer = 0; // frames
    static const int kSeqDelayFrames = 180; // ~3s at 60fps

    static void spawn_extra_ball(float x,float y, float vx,float vy) {
        if(G.balls.size()>=8) return;
        G.balls.push_back({x,y,vx,vy,true,G.imgBall});
    }

    static void apply_brick_effect(BrickType bt, float cx, float cy, Ball &ball) {
        switch(bt) {
            case BrickType::YB: G.score += 10; break;
            case BrickType::GB: G.score += 20; break;
            case BrickType::CB: G.score += 30; break;
            case BrickType::TB: G.score += 40; break;
            case BrickType::PB: G.score += 50; break;
            case BrickType::RB: G.score += 100; break;
            case BrickType::LB: if(G.lives < 99) G.lives++; break;
            case BrickType::SB: ball.vx *= 0.9f; ball.vy *= 0.9f; break;
            case BrickType::FB: ball.vx *= 1.1f; ball.vy *= 1.1f; break;
            case BrickType::AB: spawn_extra_ball(cx,cy,-ball.vx,-std::fabs(ball.vy)); break;
            case BrickType::T5: G.score += 60; break; // per-hit score (placeholder)
            case BrickType::BO: G.score += 120; break; // base bomb score before chain
            case BrickType::RW: {
                int cur = levels_current(); if(levels_count()>0) { cur = (cur - 1 + levels_count()) % levels_count(); levels_set_current(cur); }
            } break;
            case BrickType::FO: {
                int cur = levels_current(); if(levels_count()>0) { cur = (cur + 1) % levels_count(); levels_set_current(cur); }
            } break;
            case BrickType::RE: G.reverseTimer = 600; break; // ~10s
            case BrickType::IS: ball.vx *= 0.8f; ball.vy *= 0.8f; break;
            case BrickType::IF: ball.vx *= 1.25f; ball.vy *= 1.25f; break;
            case BrickType::LA: G.laserCharges += 3; break;
            case BrickType::MB: G.murderTimer = 600; break;
            case BrickType::OF: G.lightsOffTimer = 600; break;
            case BrickType::ON: G.lightsOffTimer = 0; break;
            case BrickType::BA: G.score += 1000; break;
            case BrickType::B1: G.bonusBits |= 0x01; break;
            case BrickType::B2: G.bonusBits |= 0x02; break;
            case BrickType::B3: G.bonusBits |= 0x04; break;
            case BrickType::B4: G.bonusBits |= 0x08; break;
            case BrickType::B5: G.bonusBits |= 0x10; break;
            default: break; // others TBD
        }
        if(G.bonusBits == 0x1F) { G.score += 5000; G.bonusBits = 0; hw_log("BONUS COMPLETE\n"); }
    }

    static bool is_moving_type(int raw) { return raw==(int)BrickType::SS || raw==(int)BrickType::SF; }

    static void handle_ball_bricks(Ball &ball) {
        const int bw=8,bh=8; int cols = levels_grid_width(); int rows=levels_grid_height();
        int ls=levels_left(), ts=levels_top(), cw=levels_brick_width(), ch=levels_brick_height();
        // Broad phase: compute grid span overlapped by ball AABB (minus movement inside cell)
        int minCol = std::max(0, (int)((ball.x - ls)/cw));
        int maxCol = std::min(cols-1, (int)((ball.x + bw - ls)/cw));
        int minRow = std::max(0, (int)((ball.y - ts)/ch));
        int maxRow = std::min(rows-1, (int)((ball.y + bh - ts)/ch));
        for(int r=minRow; r<=maxRow; ++r) {
            for(int c=minCol; c<=maxCol; ++c) {
                int raw = levels_brick_at(c,r);
                if(raw<=0) continue;
                BrickType bt = (BrickType)raw;
                // Brick AABB (apply horizontal offset for moving types)
                int idx = r*cols + c;
                float off = 0.f;
                if(is_moving_type(raw) && idx < (int)G.moveOffset.size()) off = G.moveOffset[idx];
                float bx = ls + c*cw + off;
                float by = ts + r*ch;
                float br = bx + cw;
                float bb = by + ch;
                float ballL = ball.x, ballR = ball.x + bw, ballT = ball.y, ballB = ball.y + bh;
                if(ballR <= bx || ballL >= br || ballB <= by || ballT >= bb) continue; // no overlap
                // Collision: decide reflection axis
                float penX = std::min(ballR - bx, br - ballL);
                float penY = std::min(ballB - by, bb - ballT);
                bool destroyed = true;
                if(bt == BrickType::T5) {
                    destroyed = levels_damage_brick(c,r);
                } else if(bt == BrickType::BO) {
                    std::vector<DestroyedBrick> destroyedList;
                    levels_explode_bomb(c,r,&destroyedList);
                    for(auto &db : destroyedList) apply_brick_effect((BrickType)db.type, ls + db.col * cw + cw/2, ts + db.row * ch + ch/2, ball);
                } else if(bt == BrickType::ID) {
                    destroyed = false; // no removal
                } else {
                    levels_remove_brick(c,r);
                }
                apply_brick_effect(bt, ball.x+4, ball.y+4, ball);
                if(G.murderTimer<=0) {
                    if(penX < penY) {
                        // Reflect X
                        if(ball.x < bx) ball.x -= penX; else ball.x += penX;
                        ball.vx = -ball.vx;
                    } else {
                        // Reflect Y
                        if(ball.y < by) ball.y -= penY; else ball.y += penY;
                        ball.vy = -ball.vy;
                    }
                }
                if(destroyed && levels_remaining_breakable()==0 && levels_count()>0) {
                    int next = (levels_current()+1) % levels_count();
                    levels_set_current(next);
                    hw_log("LEVEL COMPLETE\n");
                }
                return; // handle only first brick per frame per ball
            }
        }
    }

    static void fire_laser() {
        if(G.laserCharges<=0 || G.fireCooldown>0) return;
        G.laserCharges--; G.fireCooldown = 10; // simple cooldown
        G.lasers.push_back({G.bat.x + G.bat.width/2 - 1, G.bat.y - 4, true});
    }

    static void update_lasers() {
        if(G.fireCooldown>0) --G.fireCooldown;
        const int cw=levels_brick_width(), ch=levels_brick_height();
        int ls=levels_left(), ts=levels_top();
        for(auto &L : G.lasers) if(L.active) {
            L.y -= 4.0f;
            if(L.y < 0) { L.active=false; continue; }
            // collision with brick (grid based at laser x,y)
            int col = (int)((L.x - ls)/cw); int row = (int)((L.y - ts)/ch);
            if(col>=0 && col<levels_grid_width() && row>=0 && row<levels_grid_height()) {
                int raw = levels_brick_at(col,row);
                if(raw>0) {
                    BrickType bt = (BrickType)raw;
                        if(bt==BrickType::T5) { (void)levels_damage_brick(col,row); }
                    else if(bt==BrickType::BO) { std::vector<DestroyedBrick> list; levels_explode_bomb(col,row,&list); for(auto &db:list) apply_brick_effect((BrickType)db.type,0,0, G.balls[0]); }
                    else if(bt==BrickType::ID) { /* indestructible: no action */ }
                    else levels_remove_brick(col,row);
                    apply_brick_effect(bt,0,0,G.balls[0]);
                    L.active=false;
                }
            }
        }
    }

    void update(const InputState& in) {
        if(G.mode == Mode::Title) {
            // If touch, check hit against buttons
            if(in.touching) {
                for(auto &b : kTitleButtons) {
                    if(in.stylusX >= b.x && in.stylusX < b.x + b.w &&
                       in.stylusY >= b.y && in.stylusY < b.y + b.h) {
                        G.mode = b.next;
                        hw_log(b.next==Mode::Playing?"start\n":"editor\n");
                        return;
                    }
                }
            }
            // Cycle sequence if user idle
            if(++seqTimer > kSeqDelayFrames) { seqTimer = 0; seqPos = (seqPos + 1) % (int)(sizeof(kSequence)/sizeof(kSequence[0])); }
            return;
        }
    if(G.mode == Mode::Editor) { return; }
        // Stylus controls bat X
        if(in.touching) {
            float sx = static_cast<float>(in.stylusX);
            if(G.reverseTimer>0) sx = 320.0f - sx;
            float targetX = sx - G.bat.width * 0.5f;
            if(targetX < 0) {
                targetX = 0;
            }
            if(targetX > 320 - G.bat.width) {
                targetX = 320 - G.bat.width;
            }
            G.bat.x = targetX;
        }
        if(in.fireHeld) fire_laser();
        update_lasers();
        if(G.reverseTimer>0) --G.reverseTimer;
        if(G.lightsOffTimer>0) --G.lightsOffTimer;
        if(G.murderTimer>0) --G.murderTimer;
        // Update moving bricks offsets with neighbor blocking
        int total = levels_grid_width()*levels_grid_height();
        for(int i=0;i<total;i++) {
            int c = i % levels_grid_width(); int r = i / levels_grid_width();
            int raw = levels_brick_at(c, r);
            if(raw == (int)BrickType::SS || raw == (int)BrickType::SF) {
                float speed = (raw == (int)BrickType::SS) ? 0.25f : 0.5f;
                bool leftFree  = (c>0) && (levels_brick_at(c-1,r)==0);
                bool rightFree = (c<levels_grid_width()-1) && (levels_brick_at(c+1,r)==0);
                float baseLimit = (raw == (int)BrickType::SS)?4.f:6.f;
                float negLimit = leftFree ? -baseLimit : 0.f;
                float posLimit = rightFree ? baseLimit : 0.f;
                G.moveOffset[i] += G.moveDir[i]*speed;
                if(G.moveOffset[i] > posLimit) { G.moveOffset[i]=posLimit; G.moveDir[i] = (negLimit<0?-1.f:0.f); }
                else if(G.moveOffset[i] < negLimit) { G.moveOffset[i]=negLimit; G.moveDir[i] = (posLimit>0?1.f:0.f); }
                if(posLimit==0.f && negLimit==0.f) { G.moveOffset[i]=0.f; G.moveDir[i]=0.f; }
            } else {
                G.moveOffset[i] = 0.f; if(G.moveDir[i]==0.f) G.moveDir[i]=1.f;
            }
        }
        // Update ball(s)
        for(auto &b : G.balls) if(b.active) {
            b.x += b.vx; b.y += b.vy;
            // simple wall bounce
            if(b.x < 0) { b.x = 0; b.vx = -b.vx; }
            if(b.x > 320 - 8) { b.x = 320 - 8; b.vx = -b.vx; }
            if(b.y < 0) { b.y = 0; b.vy = -b.vy; }
            // bottom: lose life
            if(b.y > 240) {
                G.lives--; if(G.lives<=0) {
                    hw_log("GAME OVER\n");
                    int levelReached = levels_current()+1;
                    int pos = highscores::submit(G.score, levelReached);
                    if(pos >= 0) {
                        #ifdef PLATFORM_3DS
                        {
                            SwkbdState swkbd; char name[highscores::MAX_NAME+1]=""; swkbdInit(&swkbd, SWKBD_TYPE_QWERTY, 1, highscores::MAX_NAME);
                            swkbdSetHintText(&swkbd, "Name");
                            if(swkbdInputText(&swkbd, name, sizeof(name))==SWKBD_BUTTON_RIGHT) highscores::set_name(pos, name); else highscores::set_name(pos, "PLAYER");
                            highscores::save();
                        }
                        #endif
                        seqPos = 1; seqTimer = 0; // jump to High screen
                    }
                    G.mode = Mode::Title; G.balls.clear(); G.balls.push_back({160.f - 4.f, 180.f, 0.0f, -1.5f, true, G.imgBall}); G.lives = 5; G.score = 0; G.bonusBits=0; G.reverseTimer=G.lightsOffTimer=G.murderTimer=0; G.laserCharges=0; G.fireCooldown=0;
                    return; }
                b.x = G.bat.x + G.bat.width/2 - 4; b.y = G.bat.y - 8; b.vx = 0; b.vy = -1.5f; continue;
            }
            // bat collision
            if(b.y + 8 >= G.bat.y && b.y <= G.bat.y + G.bat.height && b.x + 8 >= G.bat.x && b.x <= G.bat.x + G.bat.width && b.vy > 0) {
                b.y = G.bat.y - 8; b.vy = -b.vy; // reflect
                float rel = (b.x + 4 - (G.bat.x + G.bat.width/2)) / (G.bat.width/2);
                b.vx = rel * 2.0f; // vary angle
            }
            handle_ball_bricks(b);
        }
        // Fire (D-Pad Up) could spawn extra balls later
        if(in.fireHeld && G.balls.size() < 3) {
            // simple rate limit not implemented yet
        }
    }

    void render() {
        if(G.mode == Mode::Title) {
            // Draw current sequence image (skip if not loaded, fallback attempts)
            const SeqEntry& cur = kSequence[seqPos];
            C2D_Image img = hw_image_from(cur.sheet, cur.index);
            if(!img.tex) { // simple fallback chain
                for(auto &alt : kSequence) { img = hw_image_from(alt.sheet, alt.index); if(img.tex) break; }
            }
            if(img.tex) hw_draw_sprite(img, 0, 0);
            if(kSequence[seqPos].sheet == HwSheet::High) {
                const highscores::Entry* tab = highscores::table();
                for(int i=0;i<highscores::NUM_SCORES;i++) {
                    char line[48];
                    snprintf(line,sizeof line, "%d %6lu L%02d %-15s", i+1, (unsigned long)tab[i].score, tab[i].level, tab[i].name);
                    hw_draw_text(8, 40 + i*10, line, 0xFFFFFFFF);
                }
            }
            return;
        }
        if(G.mode == Mode::Editor) {
            C2D_Image img = hw_image_from(HwSheet::Designer, DESIGNER_idx);
            if(img.tex) hw_draw_sprite(img, 0, 0); else {
                img = hw_image_from(HwSheet::Instruct, INSTRUCT_idx);
                if(img.tex) hw_draw_sprite(img, 0, 0);
            }
            return;
        }
    // Render static bricks first (will also draw moving, we'll override position for moving types by drawing again)
    levels_render();
    // Draw moving bricks with offset (overwrite at new position). Simple: iterate grid.
    int cols = levels_grid_width(); int rows = levels_grid_height(); int ls=levels_left(); int ts=levels_top(); int cw=levels_brick_width(); int ch=levels_brick_height();
    for(int r=0;r<rows;++r) for(int c=0;c<cols;++c) {
        int raw = levels_brick_at(c,r); if(raw == (int)BrickType::SS || raw == (int)BrickType::SF) {
            int idx = r*cols + c; float off = G.moveOffset[idx]; int atlas = levels_atlas_index(raw); if(atlas<0) continue; float x = ls + c*cw + off; float y = ts + r*ch; hw_draw_sprite(hw_image(atlas), x, y);
        }
    }
    // HUD overlay
    char hud[128];
    char bonus[8]; int bi=0; if(G.bonusBits & 0x01) bonus[bi++]='B'; if(G.bonusBits & 0x02) bonus[bi++]='O'; if(G.bonusBits & 0x04) bonus[bi++]='N'; if(G.bonusBits & 0x08) bonus[bi++]='U'; if(G.bonusBits & 0x10) bonus[bi++]='S'; bonus[bi]='\0';
    snprintf(hud,sizeof hud,"L%02d SCO:%lu LIVES:%d LAS:%d %s%s%s", levels_current()+1, G.score, G.lives, G.laserCharges,
             (G.reverseTimer>0?"REV ":""), (G.murderTimer>0?"MB ":""), bonus);
    hw_draw_text(4,4,hud,0xFFFFFFFF);
    if(G.lightsOffTimer>0) {
        // dark overlay (draw first so HUD stays bright) - simple approach: semi-transparent rect
        C2D_DrawRectSolid(0,0,0,320,240, C2D_Color32(0,0,0,140));
    }
    // TODO: overlay HUD (score/lives/bonus) using tiny font logger or future UI layer
    // Draw bat
        hw_draw_sprite(G.bat.img, G.bat.x, G.bat.y);
        // Draw balls
        for(auto &b : G.balls) if(b.active) hw_draw_sprite(b.img, b.x, b.y);
        // Draw lasers
        for(auto &L : G.lasers) if(L.active) C2D_DrawRectSolid(L.x, L.y, 0, 2, 6, C2D_Color32(255,255,100,255));
    }
}

// Public facade
void game_init() { game::init(); }
void game_update(const InputState& in) { game::update(in); }
void game_render() { game::render(); }
