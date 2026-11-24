# Super Mario Engine — 16-Bit x86 Assembly (NASM + DOSBox)

A complete 16-bit Real Mode side-scrolling platform game written from scratch in x86 Assembly language (`MARIO.COM`) for **EL2003 – Computer Organization and Assembly Language**.

## Team Members & Module Division

| Member | Roll Number | Modules Built |
| :--- | :--- | :--- |
| Student 1 | 25F-0508 | Modules 1–5 (Skeleton, VGA Mode 13h, Maps/Tiles, Sprites, Frame Buffer) |
| Student 2 | 25F-0566 | Modules 6–10 (Camera, Keyboard ISR, Timer ISR, Physics, Tile/AABB Collision) |
| Student 3 | 25F-0783 | Modules 11–15 (Enemies/Shells, Blocks/Mushroom, Score/HUD, State Machine, Clean Exit) |

## Project Structure
```
super_mario_engine/
├── build.bat          # Assembles MARIO.COM with NASM
├── README.md          # Project documentation
└── src/
    ├── mario.asm      # Module 1: ORG 100h entry point, main loop, clean exit
    ├── defs.inc       # Module 1: Named EQU constants (display, tiles, keys, physics, states)
    ├── irq.inc        # Modules 7–8: Custom INT 09h keyboard ISR, 30 Hz INT 08h timer ISR, SFX
    ├── gfx.inc        # Modules 2–6 & 13: VGA Mode 13h, off-screen buffer, tiles, sprites, HUD, camera
    ├── score.inc      # Module 13: 32-bit score (ADD/ADC), coins/1-UP, stomp chain, flag zones, pop-ups
    ├── physics.inc    # Modules 9–10: 16.8/8.8 fixed-point Mario physics & axis-separated collision
    ├── entities.inc   # Modules 11–12: Goomba, Koopa, shell behaviour, mushroom, block triggers
    ├── states.inc     # Modules 14–15: Jump-table state machine, screens, ResetLevel, NewGame
    ├── data.inc       # Modules 1 & 3: Runtime state variables, lookup tables, 60x12 original & working maps
    └── assets.inc     # Modules 2, 4 & 13: Custom VGA DAC palette, 16x16 tiles, 16x16 & 16x32 sprites, 8x8 font
```

## Build & Run Steps

### 1. Assemble with NASM (Windows Command Prompt or DOSBox)
Run `build.bat`, or invoke NASM directly:
```bat
nasm -f bin -O2 -w+all -I src/ src/mario.asm -o MARIO.COM
```

### 2. Run in DOSBox (v0.74-3 or later)
Inside DOSBox:
```dos
mount c C:\path\to\super_mario_engine
c:
MARIO.COM
```

## Controls (Appendix A.3)

| Key | Scan Code | Action |
| :--- | :--- | :--- |
| **Left / Right Arrow** | `4Bh` / `4Dh` | Move left / right |
| **Left Shift** | `2Ah` | Hold to run faster (2.5 px/frame vs 1.5 px/frame walk) |
| **Space bar** | `39h` | Jump (ground only; hold for higher jump, tap for short hop) |
| **Up / Down Arrow** | `48h` / `50h` | Move highlight on Title menu |
| **Enter** | `1Ch` | Select menu option / return to Title on end screens |
| **P** | `19h` | Pause / resume gameplay |
| **Esc** | `01h` | Return to Title from Playing/Paused, or exit from Title |

### Documented Debug Keys (for Live Demo Testing, Section 7.7)
Active only during the `PLAYING` state:
- **F1** (`3Bh`): Sets the level timer to **5 seconds** (to verify timeout death rule).
- **F2** (`3Ch`): Sets the coin counter to **99 coins** (so collecting 1 more coin verifies the 100-coin 1-UP rule and reset to `00`).

## Memory Architecture
- **Program Segment (`.COM`)**: All code, original pixel-art bitmaps, 8×8 font, variables, and both copies of the 60×12 level map (`level_map_orig` and `level_map_work`) fit comfortably inside the single 64 KB `.COM` segment (~28 KB total, leaving >30 KB for the stack).
- **Off-Screen Frame Buffer (64,000 bytes)**: On startup, `AllocFrameBuffer` shrinks the `.COM` PSP memory block to 64 KB (`1000h` paragraphs) via `INT 21h, AH=4Ah` and allocates a separate 64,000-byte (`0FA0h` paragraphs) segment via `INT 21h, AH=48h` (with a safe fallback to `CS + 1000h` if conventional memory is already partitioned). Every frame is cleared with `REP STOSW`, rendered completely off-screen, and copied to VGA `0A000h:0000` in one `REP MOVSW` per 30 Hz timer tick. On exit, the segment is released with `INT 21h, AH=49h`.
- **Two Level Map Copies**: `level_map_orig` (720 bytes) is never modified; `level_map_work` (720 bytes) holds live block/coin changes and is restored in one `REP MOVSB` instruction inside `ResetMap` on every new life or new game.
- **Fixed-Point Coordinates**: Velocities use 8.8 signed two's-complement fixed-point words matching Appendix A.5 (`0020h`, `0180h`, `0280h`, `FA80h`, etc.). World positions use 16.8 fixed point (`word int` + `byte frac`) so horizontal coordinates span the full 960-pixel level width without overflowing an 8-bit integer byte.

## Game States (Section 5.12)
Driven by `game_state` and dispatched via the indexed jump table `state_jump_table`:
- **0 — `TITLE`**: Brick-framed title screen with `START GAME` and `EXIT`. `Up`/`Down` selects; `Enter` starts a new game or exits; `Esc` exits.
- **1 — `PLAYING`**: Runs the 10-step per-frame pipeline (timer sync, multi-key input, Mario physics & tile collision, enemy/shell/mushroom AI, entity collision/stomp/kick/collect, 1-second clock & invulnerability ticks, pit/timeout/flag checks, forward-only camera update, off-screen rendering, `REP MOVSW` flip).
- **2 — `PAUSED`**: Freezes all physics, entities, and the clock while displaying a bordered `PAUSED` overlay over the visible gameplay scene. `P` resumes; `Esc` returns to Title.
- **3 — `DYING`**: Freezes enemies, clock, and controls; plays the 1.5-second death animation (48 px upward hop + gravity fall without collisions; pit deaths skip the upward hop). Decrements lives and transitions to `LIFE LOST` (if lives > 0) or `GAME OVER` (if lives = 0).
- **4 — `LIFE LOST`**: Displays `WORLD 1-1`, Mario icon, and remaining lives for 2 seconds, then calls `ResetLevel` (preserving score & coins, restoring map, enemies, HP=1 Small, time=300, camera=0) and returns to `PLAYING`.
- **5 — `WIN SEQUENCE`**: Triggered on touching the flag pole. Freezes clock/input, awards height-based flag points (100–5000), slides Mario down the pole (~1 s), walks him to the castle (~1 s), adds `time_left × 50` bonus to score, and enters `LEVEL COMPLETE`.
- **6 — `LEVEL COMPLETE`**: Displays pre-bonus score, coins, `time × 50` bonus, and final 6-digit score. `Enter` returns to `TITLE`.
- **7 — `GAME OVER`**: Displays final score, coins, and remaining time. `Enter` returns to `TITLE`.
- **8 — `EXIT`**: Restores PIT Channel 0 to 18.2 Hz, restores original `INT 08h` and `INT 09h` vectors, silences PC speaker, frees the frame buffer segment, switches back to text mode `03h`, and terminates cleanly via `INT 21h, AH=4Ch`.

## Bonus Features Implemented
1. **Walking Animation**: Mario alternates between standing and running frames while moving on ground; Goombas waddle as they walk.
2. **Parallax Background**: Fluffy clouds and distant green hills scroll at half camera speed (`camera_x >> 1`).
3. **Floating Score Pop-Ups**: Points (`50`, `100`, `200`, `400`, `500`, `1000`, `5000`, `1UP`) rise above stomps, coins, bricks, shells, mushrooms, and the flag pole.
4. **"Hurry" Timer Blink**: HUD `TIME` digits blink red when fewer than 100 seconds remain.
5. **Non-Blocking PC Speaker Sound Effects**: Timer-driven pitch sweeps on PIT Channel 2 for jumping, collecting coins, breaking/bumping bricks, mushrooms, stomps, kicking shells, death, and winning (automatically silenced on pause/exit).

## Deviations & Known Limitations
- **Deviations**: None — every scoring, health, collision, camera, enemy, and state rule follows Section 5 exactly.
- **Known Limitations**:
  1. Single active mushroom power-up slot at a time (spawning a second mushroom while the first is still active replaces the first).
  2. One complete 60×12 level (`WORLD 1-1`); additional worlds can be added by extending `level_map_orig` and `enemy_spawn_tbl`.
  3. PC speaker SFX uses single-channel square waves without background music.
