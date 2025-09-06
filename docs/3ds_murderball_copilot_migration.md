# 3DS Murderball – Copilot Migration Instructions (DOS → 3DS with libctru + citro2d/citro3d)

You are helping port a 1990s DOS/Borland C++ 4.0 game to Nintendo 3DS. The project root is `3ds_murderball/` and uses **devkitPro**, **libctru**, **citro2d**, **citro3d**. 
The hardware‑specific layer lives at `source/platform/{platform}/hardware.cpp` (only `3ds` implemented for now).

**Key constraints**
- The game **renders only to the 3DS bottom screen** (320×240). 
- Controls: **stylus** to move the bat; **D‑Pad Up** to release balls / fire.
- Art pipeline: `.t3s → .t3x` via `tex3ds`. With the current Makefile, `.t3x` assets are **embedded** into the ELF and expose headers (not loaded from RomFS). PNG sources live in `gfx/images/`. Do **not** rely on RomFS unless explicitly switched on.

---

## 0) High‑level tasks Copilot must perform
1) **Modernize Borland-era code to C++17** (devkitARM compatible) and remove DOS/Borland dependencies.
2) **Migrate PCX/INF graphics** to **citro2d sprite sheets** generated from `.t3s` (headers + embedded `.t3x` data).
3) Implement the **3DS platform layer** in `source/platform/3ds/hardware.cpp` using **citro2d** on the **bottom screen**.
4) Wire **input** (stylus + D‑Pad Up) and **frame loop** (aptMainLoop).
5) Keep generic game logic platform‑agnostic under `source/` (outside platform folder).

---

## 1) Replace legacy Borland headers / APIs with standard C++
**Search for and remove** Borland/DOS headers (case-insensitive), replacing with C++ equivalents:
- Remove: `<graphics.h>`, `<conio.h>`, `<dos.h>`, `<mem.h>`, `<alloc.h>`, `<dir.h>`, `<bios.h>`, `<values.h>`, `<iostream.h>` (old iostream), `<fstream.h>`, `<string.h>` (keep `<cstring>` instead)
- Use:  
  - I/O: `<iostream>`, `<fstream>`  
  - Containers/strings: `<vector>`, `<array>`, `<map>`, `<string>`  
  - Fixed-width ints: `<cstdint>` (use `uint8_t`, `uint16_t`, `int32_t`, etc.)  
  - C interop: `<cstring>`, `<cstdlib>`  
  - Math: `<cmath>`  
  - Time/random (if needed): `<chrono>`, `<random>`

**Code modernizations:**
- Replace raw C arrays with `std::vector`/`std::array` where practical.
- Replace non‑portable typedefs with `<cstdint>` types.
- Replace DOS file I/O with standard C/C++ I/O only where still used for data formats; for runtime I/O on 3DS use standard `fopen/fread/fwrite` on `sdmc:/` as needed (but assets are embedded via `.t3x` headers in this project).

**Eliminate Borland‑specific functions:** `kbhit`, `getch`, `delay`, `clrscr`, BIOS calls, DOS interrupts, direct VRAM pokes, etc. Replace with 3DS input/render calls described below.

---

## 2) Graphics pipeline (PCX/INF → citro2d with .t3s/.t3x)
- The project already contains `gfx/*.t3s` manifests and `gfx/images/*.png`. The Makefile compiles `.t3s → .t3x` and also emits **headers** into the build include path.
- **Do not parse INF files anymore.** Atlas coordinates and image indices come from the `.t3x` metadata; access them via the **auto‑generated headers** and citro2d.

**How to use embedded `.t3x` with headers (current Makefile configuration):**
- Each `NAME.t3s` generates: 
  - `build/NAME.t3x` (linked into `.o` automatically), and 
  - `build/NAME.h` (exposed via `-I$(BUILD)`; Makefile already adds this include path).
- In code, **include the generated header** and create a sprite sheet **from memory**:
  ```cpp
  #include <citro2d.h>
  #include "IMAGE.h"   // auto-generated from gfx/IMAGE.t3s

  // These symbols come from IMAGE.h:
  //   extern const u8  IMAGE_t3x[];
  //   extern const u32 IMAGE_t3x_size;
  // Load atlas from embedded bytes:
  C2D_SpriteSheet sheet = C2D_SpriteSheetLoadFromMem(IMAGE_t3x, IMAGE_t3x_size);
  ```

- Use your generated indices header(s) (e.g., `include/sprite_indexes/image_indices.h`) to fetch sub-images deterministically:
  ```cpp
  #include "sprite_indexes/image_indices.h"  // defines SPR_IMAGE_BALL_SPRITE, etc.

  C2D_Image imgBall = C2D_SpriteSheetGetImage(sheet, SPR_IMAGE_BALL_SPRITE);
  ```

> If you later enable ROMFS, you can switch to `C2D_SpriteSheetLoad("romfs:/IMAGE.t3x")` and remove the header include. For now, keep the **embedded** path.

---

## 3) 3DS rendering loop (bottom screen only)
Implement **hardware.cpp** under `source/platform/3ds/` with these responsibilities:

**Initialization:**
```cpp
#include <3ds.h>
#include <citro2d.h>

static C3D_RenderTarget* s_bottom;
static C2D_SpriteSheet   s_sheet;

bool hw_init()
{
    gfxInitDefault();
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();

    s_bottom = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
    if(!s_bottom) return false;

    // Example: load one atlas
    #include "IMAGE.h"
    s_sheet = C2D_SpriteSheetLoadFromMem(IMAGE_t3x, IMAGE_t3x_size);
    return s_sheet != nullptr;
}
```

**Input: stylus + D‑Pad Up:**
```cpp
struct InputState {
    int  stylusX = -1, stylusY = -1;   // -1 when not touching
    bool touching = false;
    bool fireHeld = false;
};

void hw_poll_input(InputState& out)
{
    hidScanInput();
    u32 kHeld = hidKeysHeld();

    touchPosition tp{};
    out.touching = (kHeld & KEY_TOUCH) != 0;
    if(out.touching) {
        hidTouchRead(&tp);
        out.stylusX = tp.px;   // 0..319
        out.stylusY = tp.py;   // 0..239
    } else {
        out.stylusX = out.stylusY = -1;
    }

    out.fireHeld = (kHeld & KEY_DUP) != 0;
}
```

**Per‑frame drawing (to bottom screen):**
```cpp
void hw_begin_frame()
{
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
    C2D_TargetClear(s_bottom, C2D_Color32(0,0,0,255));
    C2D_SceneBegin(s_bottom);
}

void hw_draw_sprite(C2D_Image img, float x, float y, float z=0.0f, float sx=1.0f, float sy=1.0f)
{
    C2D_DrawImageAt(img, x, y, z, nullptr, sx, sy);
}

void hw_end_frame()
{
    C3D_FrameEnd(0);
}
```

**Shutdown:**
```cpp
void hw_shutdown()
{
    if(s_sheet) C2D_SpriteSheetFree(s_sheet);
    C2D_Fini();
    C3D_Fini();
    gfxExit();
}
```

**Loop sketch (from generic game code):**
```cpp
bool running = hw_init();
while(running && aptMainLoop())
{
    InputState in{};
    hw_poll_input(in);

    // TODO: pass 'in' to game logic to move bat and fire when in.fireHeld

    hw_begin_frame();
    // TODO: draw game scene using hw_draw_sprite(...)
    hw_end_frame();
}
hw_shutdown();
```

**Notes:**
- Bottom screen resolution is **320×240**. Your legacy art was **320×200**; center or letterbox vertically if needed.
- `KEY_TOUCH` is held only while stylus contacts the screen; use prior position for smoothing if needed.

---

## 4) Replace legacy sprite access with citro2d images
- Remove any PCX/INF parsing and sprite rect math in generic code.
- For each atlas (e.g., `IMAGE.t3s`), include the generated `IMAGE.h` (embedded bytes) and the indices header(s) you created (e.g., `image_indices.h`).
- Replace any code that referenced `x,y,w,h` with direct `C2D_Image` handles from `C2D_SpriteSheetGetImage(sheet, index)`.

**Example replacement:**
```cpp
// OLD (PCX/INF):
// drawSprite(image, x, y, srcX, srcY, w, h);

// NEW (citro2d):
C2D_Image img = C2D_SpriteSheetGetImage(s_sheet, SPR_IMAGE_YELLOW_BRICK);
C2D_DrawImageAt(img, x, y, 0.0f);
```

---

## 5) Input & game mapping
- **Stylus → bat X coordinate**: clamp to playfield; optionally smooth/predict.
- **KEY_DUP** (held) → continuous fire/release; for single‑shot use `hidKeysDown()` instead.
- Consider adding a configurable **dead‑zone** near edges to avoid accidental releases.

---

## 6) File I/O (levels)
- Keep **default** level packs as embedded (.t3x covers art only; level data can be shipped in RomFS if you enable it later, or as binary blobs compiled in).
- For **editable** user level packs, use the SD card path: `sdmc:/3ds/Murderball/levels/*.dat`
  ```cpp
  fsInit();
  mkdir("sdmc:/3ds", 0777);
  mkdir("sdmc:/3ds/Murderball", 0777);
  mkdir("sdmc:/3ds/Murderball/levels", 0777);
  // fopen("sdmc:/3ds/Murderball/levels/user_pack.dat","wb");
  fsExit();
  ```
- Do **not** attempt to write to RomFS (read‑only).

---

## 7) Headers and interfaces
- Define a clean interface in `include/hardware.hpp` that your generic game code calls; implement those functions only in `source/platform/3ds/hardware.cpp`.
- Suggested signatures:
  ```cpp
  struct InputState { int stylusX, stylusY; bool touching; bool fireHeld; };

  bool hw_init();
  void hw_shutdown();

  void hw_poll_input(InputState& out);

  void hw_begin_frame();
  void hw_end_frame();

  // drawing helpers
  void hw_draw_sprite(C2D_Image img, float x, float y, float z=0.0f, float sx=1.0f, float sy=1.0f);
  ```

---

## 8) Makefile / assets expectations
- The current Makefile embeds `.t3x` assets (no RomFS). Keep this behavior.
- Ensure `GRAPHICS := gfx` and `GFXBUILD := $(BUILD)` are set (default template already does this).
- The rule already generates `$(BUILD)/NAME.h` and links `NAME.t3x` as binary. Include `#include "NAME.h"` to access the embedded symbols.

---

## 9) Acceptance criteria (Copilot must satisfy)
- No deprecated Borland headers or APIs remain.
- Project compiles with devkitARM using **C++11+** (project uses `-std=gnu++11`; feel free to use C++17‑safe features that compile under this flag or adjust to `gnu++17` if needed).
- `source/platform/3ds/hardware.cpp` implements the full platform layer as above.
- All gameplay renders to **bottom screen**; **top screen is unused**.
- Sprites draw via **citro2d** using **embedded `.t3x` + generated headers** (no PCX/INF parsing).
- Input works: stylus controls bat; **D‑Pad Up** triggers release/fire.
- No warnings at `-O2 -Wall` (or no new warnings).

---

## 10) Optional enhancements (if time permits)
- Add a compile‑time **PLATFORM** define (`-DPLATFORM_3DS`) and ensure non‑3DS stubs continue to build.
- Add a simple FPS limiter/measure using `svcSleepThread` or `gspWaitForVBlank` if needed.
- Add a minimal **asset manager** wrapper that owns `C2D_SpriteSheet` and provides name→image lookups using your generated index headers.
