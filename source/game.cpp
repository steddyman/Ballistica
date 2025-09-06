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
#include "SUPPORT.HPP" // legacy constants BATWIDTH, BATHEIGHT, BALLWIDTH, BALLHEIGHT
#include <vector>

// Basic game state migrated from legacy structures (incremental port)
namespace game {
    struct Ball { float x, y; float vx, vy; float px, py; bool active; C2D_Image img; };
    struct Laser { float x,y; bool active; };
    struct Bat  { float x, y; float width, height; C2D_Image img; };
    enum class Mode { Title, Playing, Editor };

    struct MovingBrickData { float pos; float dir; float minX; float maxX; };
    struct Particle { float x,y,vx,vy; int life; uint32_t color; };
    struct BombEvent { int c; int r; int frames; }; // scheduled bomb trigger

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
    // Moving brick dynamic traversal across contiguous empty span
    std::vector<MovingBrickData> moving; // per-cell data (pos<0 => unused)
    // Particles (bomb / generic)
    std::vector<Particle> particles;
    std::vector<BombEvent> bombEvents; // pending delayed explosions
    };

    static State G;

    void init_assets() {
        G.imgBall = hw_image(IMAGE_ball_sprite_idx);
        G.imgBatNormal = hw_image(IMAGE_bat_normal_idx);
    float bw = (G.imgBatNormal.subtex) ? G.imgBatNormal.subtex->width : 64.f;
    float bh = (G.imgBatNormal.subtex) ? G.imgBatNormal.subtex->height : 8.f;
    G.bat = { 160.f - bw/2.f, 200.f, bw, bh, G.imgBatNormal };
    G.balls.push_back({160.f - 4.f, 180.f, 0.0f, -1.5f, 160.f - 4.f, 180.f, true, G.imgBall});
    }

    void init() {
    hw_log("game_init\n");
        init_assets();
        levels_load();
    hw_log("assets loaded\n");
    // Initialize moving brick buffers
    int total = levels_grid_width()*levels_grid_height();
    G.moving.assign(total, { -1.f, 1.f, 0.f, 0.f });
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
        {HwSheet::Instruct, INSTRUCT_idx}
    };
    static int seqPos = 0;
    static int seqTimer = 0; // frames
    static const int kSeqDelayFrames = 300; // ~5s at 60fps

    static void spawn_extra_ball(float x,float y, float vx,float vy) {
        if(G.balls.size()>=8) return;
        G.balls.push_back(Ball{ x,y,vx,vy,x,y,true,G.imgBall });
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

    static bool bomb_event_scheduled(int c,int r) {
        for(const auto &e : G.bombEvents) {
            if(e.c==c && e.r==r) return true;
        }
        return false;
    }
    static void schedule_neighbor_bombs(int c,int r,int delay) {
        for(int dy=-1; dy<=1; ++dy) for(int dx=-1; dx<=1; ++dx) if(dx||dy) {
            int nc=c+dx, nr=r+dy; int raw = levels_brick_at(nc,nr); if(raw == (int)BrickType::BO) {
                if(!bomb_event_scheduled(nc,nr)) G.bombEvents.push_back({nc,nr,delay});
            }
        }
    }
    static void process_bomb_events() {
        if(G.bombEvents.empty()) return;
        const int ls=levels_left(), ts=levels_top(), cw=levels_brick_width(), ch=levels_brick_height();
        for(auto &e : G.bombEvents) if(e.frames>0) --e.frames;
        std::vector<size_t> toExplode; toExplode.reserve(G.bombEvents.size());
        for(size_t i=0;i<G.bombEvents.size();++i) if(G.bombEvents[i].frames<=0) toExplode.push_back(i);
        for(size_t idx : toExplode) {
            if(idx >= G.bombEvents.size()) continue;
            auto ev = G.bombEvents[idx];
            int raw = levels_brick_at(ev.c, ev.r); if(raw != (int)BrickType::BO) continue;
            levels_remove_brick(ev.c, ev.r);
            apply_brick_effect(BrickType::BO, ls + ev.c*cw + cw/2, ts + ev.r*ch + ch/2, G.balls[0]);
            for(int k=0;k<8;k++) { float angle=(float)k/8.f*6.28318f; float sp=0.6f+0.4f*(k%4); Particle p{ (float)(ls + ev.c*cw + cw/2), (float)(ts + ev.r*ch + ch/2), std::cos(angle)*sp, std::sin(angle)*sp, 32, C2D_Color32(255,200,50,255) }; G.particles.push_back(p); }
            schedule_neighbor_bombs(ev.c, ev.r, 15); // 15 frame delay
        }
        G.bombEvents.erase(std::remove_if(G.bombEvents.begin(), G.bombEvents.end(), [](const BombEvent& e){return e.frames<=0;}), G.bombEvents.end());
    }

    static void update_moving_bricks(); // fwd

    static void handle_ball_bricks(Ball &ball) {
        const int bw=8,bh=8; int cols = levels_grid_width(); int rows=levels_grid_height();
        int ls=levels_left(), ts=levels_top(), cw=levels_brick_width(), ch=levels_brick_height();
        // Broad phase: compute grid span overlapped by ball AABB (minus movement inside cell)
        int minCol = std::max(0, (int)((ball.x - ls)/cw));
        int maxCol = std::min(cols-1, (int)((ball.x + bw - ls)/cw));
        int minRow = std::max(0, (int)((ball.y - ts)/ch));
        int maxRow = std::min(rows-1, (int)((ball.y + bh - ts)/ch));
        bool hit = false; // prevent double-hit in same frame
        for(int r=minRow; r<=maxRow; ++r) {
            for(int c=minCol; c<=maxCol; ++c) {
                int raw = levels_brick_at(c,r);
                if(raw<=0) continue;
                if(is_moving_type(raw)) continue; // handle moving bricks separately with dynamic position
                BrickType bt = (BrickType)raw;
                // Brick AABB (use dynamic horizontal position if moving)
                float bx = ls + c*cw;
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
                    // Immediate bomb: remove and schedule neighbors
                    levels_remove_brick(c,r);
                    apply_brick_effect(BrickType::BO, ls + c*cw + cw/2, ts + r*ch + ch/2, ball);
                    for(int k=0;k<8;k++){float angle=(float)k/8.f*6.28318f;float sp=0.6f+0.4f*(k%4);Particle p{(float)(ls + c*cw + cw/2),(float)(ts + r*ch + ch/2),std::cos(angle)*sp,std::sin(angle)*sp,32,C2D_Color32(255,200,50,255)};G.particles.push_back(p);} 
                    schedule_neighbor_bombs(c,r,15);
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
        hit = true;
        goto after_static;
            }
        }
    after_static:;
    if(hit) return;
        // Second pass: moving bricks dynamic collision (first hit only)
        for(int r=0;r<rows;++r) {
            for(int c=0;c<cols;++c) {
                int raw = levels_brick_at(c,r);
                if(!is_moving_type(raw)) continue;
                int idx = r*cols + c;
        if(idx >= (int)G.moving.size() || G.moving[idx].pos < 0.f) continue;
        float bx = G.moving[idx].pos;
                float by = ts + r*ch;
                float br = bx + cw;
                float bb = by + ch;
                float ballL = ball.x, ballR = ball.x + bw, ballT = ball.y, ballB = ball.y + bh;
                if(ballR <= bx || ballL >= br || ballB <= by || ballT >= bb) continue;
                BrickType bt = (BrickType)raw;
                float penX = std::min(ballR - bx, br - ballL);
                float penY = std::min(ballB - by, bb - ballT);
                bool destroyed = true;
                if(bt == BrickType::T5) {
                    destroyed = levels_damage_brick(c,r);
                } else if(bt == BrickType::BO) {
                    levels_remove_brick(c,r);
                    apply_brick_effect(BrickType::BO, ls + c*cw + cw/2, ts + r*ch + ch/2, ball);
                    for(int k=0;k<8;k++){float angle=(float)k/8.f*6.28318f;float sp=0.6f+0.4f*(k%4);Particle p{(float)(ls + c*cw + cw/2),(float)(ts + r*ch + ch/2),std::cos(angle)*sp,std::sin(angle)*sp,32,C2D_Color32(255,200,50,255)};G.particles.push_back(p);} 
                    schedule_neighbor_bombs(c,r,15);
                } else if(bt == BrickType::ID) {
                    destroyed = false;
                } else {
                    levels_remove_brick(c,r);
                }
                apply_brick_effect(bt, ball.x+4, ball.y+4, ball);
                if(G.murderTimer<=0) {
                    if(penX < penY) {
                        if(ball.x < bx) ball.x -= penX; else ball.x += penX;
                        ball.vx = -ball.vx;
                    } else {
                        if(ball.y < by) ball.y -= penY; else ball.y += penY;
                        ball.vy = -ball.vy;
                    }
                }
                if(destroyed && levels_remaining_breakable()==0 && levels_count()>0) {
                    int next = (levels_current()+1) % levels_count();
                    levels_set_current(next); hw_log("LEVEL COMPLETE\n");
                }
                return; // only first moving brick hit
            }
        }
    }

    static void update_moving_bricks() {
        int cols = levels_grid_width(); int rows = levels_grid_height();
        int ls = levels_left(); int cw = levels_brick_width();
        for(int r=0;r<rows;++r) {
            for(int c=0;c<cols;++c) {
                int raw = levels_brick_at(c,r);
                if(!is_moving_type(raw)) continue;
                int idx = r*cols + c;
                auto &mb = G.moving[idx];
                int left=c; while(left-1>=0 && levels_brick_at(left-1,r)==0) --left;
                int right=c; while(right+1<cols && levels_brick_at(right+1,r)==0) ++right;
                float newMin = ls + left * cw;
                float newMax = ls + right * cw;
                bool hadNoSpan = (mb.minX == mb.maxX);
                mb.minX = newMin; mb.maxX = newMax;
                if(mb.pos < 0.f) mb.pos = ls + c*cw;
                // If we previously had no span (blocked) and now have space, kick movement on
                if(hadNoSpan && newMin != newMax && mb.dir == 0.f) mb.dir = 1.f;
                // Clamp position into new bounds before advancing
                if(mb.pos < mb.minX) mb.pos = mb.minX;
                if(mb.pos > mb.maxX) mb.pos = mb.maxX;
                float speed = (raw==(int)BrickType::SS)?0.5f:1.0f;
                if(mb.dir != 0.f) {
                    mb.pos += mb.dir*speed;
                    if(mb.pos < mb.minX) { mb.pos = mb.minX; mb.dir = 1.f; }
                    else if(mb.pos > mb.maxX) { mb.pos = mb.maxX; mb.dir = -1.f; }
                }
                if(mb.minX==mb.maxX) { mb.pos = mb.minX; mb.dir = 0.f; }
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
            // Press START on any rotating title/high/instruction screen to begin immediately
            if(in.startPressed) {
                G.mode = Mode::Playing;
                hw_log("start (START key)\n");
                return;
            }
            // Cycle sequence if user idle
            if(++seqTimer > kSeqDelayFrames) { seqTimer = 0; seqPos = (seqPos + 1) % (int)(sizeof(kSequence)/sizeof(kSequence[0])); }
            return;
        }
    if(G.mode == Mode::Editor) {
        // SELECT returns to title screen
        if(in.selectPressed) {
            G.mode = Mode::Title;
            hw_log("editor->title (SELECT)\n");
            return;
        }
        return;
    }
        #if defined(DEBUG) && DEBUG
        // Debug level switching (L previous, R next)
        if(in.levelPrevPressed || in.levelNextPressed) {
            int cur = levels_current();
            int total = levels_count();
            if(in.levelPrevPressed && cur > 0) cur--;
            if(in.levelNextPressed && cur < total-1) cur++;
            if(cur != levels_current()) {
                levels_set_current(cur);
                levels_reset_level(cur);
                // Reset game state (fresh start)
                G.balls.clear();
                G.balls.push_back(Ball{160.f - 4.f, 180.f, 0.0f, -1.5f, 160.f - 4.f, 180.f, true, G.imgBall});
                G.lives = 5; G.score = 0; G.bonusBits=0; G.reverseTimer=G.lightsOffTimer=G.murderTimer=0; G.laserCharges=0; G.fireCooldown=0;
                // Re-init moving brick arrays for new layout
                int totalCells = levels_grid_width()*levels_grid_height();
                G.moving.assign(totalCells, { -1.f, 1.f, 0.f, 0.f });
                hw_log("DEBUG: level switched\n");
            }
        }
        #endif
    // Process bomb chain before physics so collisions see updated board
    process_bomb_events();
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
    update_moving_bricks();
    // Update particles
    for(auto &p : G.particles) {
        if(p.life<=0) continue;
        p.x += p.vx;
        p.y += p.vy;
        p.vy += 0.02f; // slight gravity
        p.life--;
    }
    // Compact occasionally
    if((int)G.particles.size() > 256) {
        G.particles.erase(std::remove_if(G.particles.begin(), G.particles.end(), [](const Particle& p){return p.life<=0;}), G.particles.end());
    }
        // Update ball(s)
        for(auto &b : G.balls) if(b.active) {
            b.px = b.x; b.py = b.y;
            b.x += b.vx; b.y += b.vy;
            // simple wall bounce
            if(b.x < 0) { b.x = 0; b.vx = -b.vx; }
            if(b.x > 320 - 8) { b.x = 320 - 8; b.vx = -b.vx; }
            if(b.y < 0) { b.y = 0; b.vy = -b.vy; }
            // bottom: lose life
            if(b.y > 240) {
                // Only lose a life if this was the last active ball.
                int activeCount = 0; for(const auto &bb : G.balls) if(bb.active) ++activeCount;
                if(activeCount > 1) {
                    // Deactivate this ball; others continue. No life lost.
                    b.active = false;
                    continue;
                }
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
                    G.mode = Mode::Title; 
                    levels_reset_level(levels_current());
                    G.balls.clear(); G.balls.push_back({160.f - 4.f, 180.f, 0.0f, -1.5f, 160.f - 4.f, 180.f, true, G.imgBall});
                    G.lives = 5; G.score = 0; G.bonusBits=0; G.reverseTimer=G.lightsOffTimer=G.murderTimer=0; G.laserCharges=0; G.fireCooldown=0;
                    return; }
                b.x = G.bat.x + G.bat.width/2 - 4; b.y = G.bat.y - 8; b.px=b.x; b.py=b.y; b.vx = 0; b.vy = -1.5f; continue;
            }
            // Bat collision using legacy logical sizes (BATWIDTH/BATHEIGHT, BALLWIDTH/BALLHEIGHT)
            if(b.vy > 0) {
                constexpr float ballCollW = (float)BALLWIDTH;   // legacy collision width
                constexpr float ballCollH = (float)BALLHEIGHT;  // legacy collision height
                // We render an 8x8 ball; align collision box centered within sprite
                float ballCenterX = b.x + 4.f; float ballCenterYPrev = b.py + 4.f; float ballCenterY = b.y + 4.f;
                float ballHalfW = ballCollW * 0.5f; float ballHalfH = ballCollH * 0.5f;
                float ballBottomPrev = ballCenterYPrev + ballHalfH;
                float ballBottom = ballCenterY + ballHalfH;
                // Bat effective rectangle: shrink sprite to legacy BATWIDTH/BATHEIGHT centered
                float effBatW = (float)BATWIDTH;
                float effBatH = (float)BATHEIGHT;
                float batPadX = (G.bat.width  - effBatW) * 0.5f; if(batPadX < 0) batPadX = 0;
                float batPadY = (G.bat.height - effBatH);        if(batPadY < 0) batPadY = 0; // assume extra space above
                float batTop = G.bat.y + batPadY; // logical top surface
                float batLeft = G.bat.x + batPadX;
                float batRight = batLeft + effBatW;
                // Detect crossing of the bat top line this frame
                if(ballBottomPrev <= batTop && ballBottom >= batTop && ballCenterX + ballHalfW > batLeft && ballCenterX - ballHalfW < batRight) {
                    // Place ball just above logical top using full rendered sprite alignment
                    float adjust = (ballBottom - batTop);
                    b.y -= adjust; // shift up so that logical bottom sits on top line
                    b.vy = -b.vy;
                    // Angle based on horizontal offset inside effective bat width
                    float rel = (ballCenterX - (batLeft + effBatW * 0.5f)) / (effBatW * 0.5f);
                    if(rel < -1.f) rel = -1.f;
                    if(rel > 1.f) rel = 1.f;
                    b.vx = rel * 2.0f; // maintain existing speed scale
                }
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
    // Render static bricks first then overwrite dynamic moving bricks at their current x
    levels_render();
    int cols = levels_grid_width(); int rows = levels_grid_height(); int ls=levels_left(); int ts=levels_top(); int cw=levels_brick_width(); int ch=levels_brick_height();
    for(int r=0;r<rows;++r) for(int c=0;c<cols;++c) {
        int raw = levels_brick_at(c,r); if(!is_moving_type(raw)) continue; int idx = r*cols + c; int atlas = levels_atlas_index(raw); if(atlas<0) continue; float y = ts + r*ch; // clear original cell to avoid ghost
        C2D_DrawRectSolid(ls + c*cw, y, 0, cw, ch, C2D_Color32(0,0,0,255));
        float x = (idx < (int)G.moving.size() && G.moving[idx].pos >= 0.f) ? G.moving[idx].pos : (ls + c*cw);
        if(idx < (int)G.moving.size()) { if(x < G.moving[idx].minX) x = G.moving[idx].minX; if(x > G.moving[idx].maxX) x = G.moving[idx].maxX; }
        hw_draw_sprite(hw_image(atlas), x, y);
        if(raw == (int)BrickType::T5) {
            int hp = levels_brick_hp(c,r); if(hp>0) {
                // darker overlay increases with damage (hp ranges 1..5). When hp=5 (full) no overlay; hp=1 heavy overlay.
                int missing = 5 - hp; int alpha = 30 + missing * 40; if(alpha>180) alpha=180;
                C2D_DrawRectSolid(x, y, 0, cw, ch, C2D_Color32(255,0,0,(uint8_t)alpha));
            }
        }
    }
    // Simple particles
    for(auto &p : G.particles) {
        if(p.life<=0) continue;
        C2D_DrawRectSolid(p.x, p.y, 0, 2, 2, p.color);
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
        #if defined(DEBUG) && DEBUG
        // Draw logical bat collision rectangle (centered reduced width & height)
        float effBatW = (float)BATWIDTH;
        float effBatH = (float)BATHEIGHT;
        float batPadX = (G.bat.width  - effBatW) * 0.5f; if(batPadX < 0) batPadX = 0;
        float batPadY = (G.bat.height - effBatH);        if(batPadY < 0) batPadY = 0;
        float batLeft = G.bat.x + batPadX; float batTop = G.bat.y + batPadY;
        C2D_DrawRectSolid(batLeft, batTop, 0, effBatW, 1, C2D_Color32(255,0,0,180)); // top line
        C2D_DrawRectSolid(batLeft, batTop+effBatH-1, 0, effBatW, 1, C2D_Color32(255,0,0,80)); // bottom line
        C2D_DrawRectSolid(batLeft, batTop, 0, 1, effBatH, C2D_Color32(255,0,0,80)); // left
        C2D_DrawRectSolid(batLeft+effBatW-1, batTop, 0, 1, effBatH, C2D_Color32(255,0,0,80)); // right
        // Draw ball logical footprint(s)
        for(auto &b : G.balls) if(b.active) {
            float ballCenterX = b.x + 4.f; float ballCenterY = b.y + 4.f;
            float halfW = BALLWIDTH * 0.5f; float halfH = BALLHEIGHT * 0.5f;
            float lx = ballCenterX - halfW; float ly = ballCenterY - halfH;
            C2D_DrawRectSolid(lx, ly, 0, BALLWIDTH, BALLHEIGHT, C2D_Color32(0,255,0,90));
        }
        #endif
    // Draw balls (multi-hit bricks visual tweak: overlay HP number optional later)
    for(auto &b : G.balls) if(b.active) hw_draw_sprite(b.img, b.x, b.y);
        // Draw lasers
        for(auto &L : G.lasers) if(L.active) C2D_DrawRectSolid(L.x, L.y, 0, 2, 6, C2D_Color32(255,255,100,255));
    }
}

// Public facade
void game_init() { game::init(); }
void game_update(const InputState& in) { game::update(in); }
void game_render() { game::render(); }
