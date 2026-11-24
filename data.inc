; =============================================================================
; File:        data.inc
; Project:     Super Mario Engine (EL2003 COAL Semester Project)
; Description: Modules 1 & 3 - Grouped runtime variables (hero, camera,
;              enemies, mushroom, score/HUD, clock, keyboard/interrupts,
;              state machine, pop-ups), lookup tables, UI strings, and the
;              60x12 original + working level maps.
; =============================================================================

; -----------------------------------------------------------------------------
; Video & Frame Buffer Variables
; -----------------------------------------------------------------------------
frame_buf_seg       dw 0            ; Allocated segment for 64,000-byte off-screen buffer
fb_allocated        db 0            ; 1 if allocated via DOS INT 21h AH=48h
frame_counter       dw 0            ; Incremented every rendered frame (for anim/blink)

; -----------------------------------------------------------------------------
; Interrupt & Timing Variables (Modules 7 & 8)
; -----------------------------------------------------------------------------
old_int09_off       dw 0
old_int09_seg       dw 0
old_int08_off       dw 0
old_int08_seg       dw 0
irq_installed       db 0            ; 1 once custom INT 08h/09h vectors are active

timer_ticks         dw 0            ; Incremented at 30 Hz by TimerISR
last_tick           dw 0            ; Last processed tick value in main loop
pit_accum           dw 0            ; Accumulator to chain BIOS INT 08h at 18.2 Hz
sec_tick_accum      db 0            ; Counts 0..29 ticks -> 1 real second

key_state           times 128 db 0  ; 1 = key currently held, 0 = released
key_pressed         times 128 db 0  ; Edge-triggered press flags (cleared each frame)

; -----------------------------------------------------------------------------
; PC Speaker Sound Effect Variables (Bonus - Timer Driven, Non-Blocking)
; -----------------------------------------------------------------------------
sfx_timer           db 0            ; Remaining frames for active sound effect
sfx_freq_step       dw 0            ; Per-frame frequency slide (signed)
sfx_cur_div         dw 0            ; Current PIT channel 2 divisor

; -----------------------------------------------------------------------------
; Game State Machine Variables (Module 14)
; -----------------------------------------------------------------------------
game_state          db STATE_TITLE  ; Current state (0..8)
menu_selection      db 0            ; 0 = START GAME, 1 = EXIT
state_timer         dw 0            ; Countdown timer for timed states
win_phase           db 0            ; 0 = sliding down flag, 1 = walking to castle
death_is_pit        db 0            ; 1 if pit death (skip upward hop)

; -----------------------------------------------------------------------------
; Hero Data (Modules 6, 9, 10 - Grouped Together per Module 9)
; Position uses 16.8 fixed point (word int + byte frac) so world X reaches 960;
; velocities use signed 8.8 fixed point matching Appendix A.5.
; -----------------------------------------------------------------------------
hero_x_int          dw HERO_START_X ; World X whole pixels (0..944)
hero_x_frac         db 0            ; World X fractional byte (0..255)
hero_y_int          dw HERO_START_Y ; World Y whole pixels (top of sprite)
hero_y_frac         db 0            ; World Y fractional byte (0..255)
hero_vx             dw 0            ; Horizontal velocity (8.8 signed)
hero_vy             dw 0            ; Vertical velocity (8.8 signed)
hero_prev_feet_y    dw HERO_START_Y + HERO_SMALL_H ; Feet Y before vertical move
hero_on_ground      db 1            ; 1 = standing on solid tile
hero_jump_lock      db 0            ; 1 = Space must be released before next jump
hero_facing         db 0            ; 0 = facing right, 1 = facing left
hero_pose           db POSE_STAND   ; 0 = stand, 1 = run, 2 = jump
hero_form           db 0            ; 0 = Small (16x16), 1 = Super (16x32)
hero_hp             db HP_SMALL     ; 1 = Small, 2 = Super
hero_invuln         db 0            ; Remaining invulnerability ticks (0..60)
hero_stomp_chain    db 0            ; Consecutive stomps without landing (0..11+)

; -----------------------------------------------------------------------------
; Camera Variable (Module 6)
; -----------------------------------------------------------------------------
camera_x            dw 0            ; World X of left edge of 320-px viewport (0..640)

; -----------------------------------------------------------------------------
; Score, Coins, Lives and Clock Variables (Module 13)
; Score is 32-bit unsigned (score_hi:score_lo) added with ADD/ADC.
; -----------------------------------------------------------------------------
score_lo            dw 0            ; Low 16 bits of 32-bit score
score_hi            dw 0            ; High 16 bits of 32-bit score
pre_bonus_lo        dw 0            ; Saved score before Level Complete time bonus
pre_bonus_hi        dw 0
time_bonus_val      dw 0            ; Computed time_left * 50 on Level Complete
coins_count         db 0            ; 0..99 (100 gives +1 life and resets to 0)
lives_count         db START_LIVES  ; 0..99
game_time           dw START_TIME   ; 300 down to 0

; -----------------------------------------------------------------------------
; Active Enemy Table (Module 11 - 8 fixed-size 16-byte records = 128 bytes)
; -----------------------------------------------------------------------------
enemy_table         times (MAX_ENEMIES * ENEMY_REC_SIZE) db 0

; -----------------------------------------------------------------------------
; Mushroom Power-Up Record (Module 12 - 1 active mushroom)
; -----------------------------------------------------------------------------
mush_active         db 0            ; 1 = active on map
mush_x_int          dw 0            ; World X whole pixels
mush_x_frac         db 0
mush_y_int          dw 0            ; World Y whole pixels
mush_y_frac         db 0
mush_vx             dw 0            ; 8.8 signed horizontal velocity
mush_vy             dw 0            ; 8.8 signed vertical velocity

; -----------------------------------------------------------------------------
; Floating Score Pop-Ups (Bonus - 4 records of 8 bytes each)
; Layout: +0 active(b), +1 timer(b), +2 x_int(w), +4 y_int(w), +6 str_ptr(w)
; -----------------------------------------------------------------------------
popup_table         times (MAX_POPUPS * POPUP_REC_SIZE) db 0

; -----------------------------------------------------------------------------
; Lookup Tables (Sections 5.4 & 5.5)
; -----------------------------------------------------------------------------
; Stomp chain table: index 0..9 -> 100..8000 points; >= 10 -> 1-UP (+1 life)
stomp_points_tbl    dw 100, 200, 400, 500, 800, 1000, 2000, 4000, 5000, 8000

; Initial enemy spawn list for Level 1-1 (count followed by [type, col, row])
; 4 Goombas + 2 Koopas = 6 enemies (meets Section 5.15 minimums)
enemy_spawn_count   equ 6
enemy_spawn_tbl:
    db ENT_GOOMBA, 10, 9            ; Goomba 1 (early stomp / damage test)
    db ENT_GOOMBA, 12, 9            ; Goomba 2 (close to Goomba 1 for stomp chain)
    db ENT_KOOPA,  23, 9            ; Koopa 1 (turns at Pit 1 edge, shell kick test)
    db ENT_GOOMBA, 27, 9            ; Goomba 3 (target for Koopa 1 sliding shell)
    db ENT_GOOMBA, 40, 9            ; Goomba 4 (after Pit 2)
    db ENT_KOOPA,  45, 9            ; Koopa 2 (patrols near end stairs)

; -----------------------------------------------------------------------------
; UI and HUD Text Strings (Zero-Terminated)
; -----------------------------------------------------------------------------
str_hud_score       db 'SCORE', 0
str_hud_coins       db 'COINS', 0
str_hud_world       db 'WORLD', 0
str_hud_time        db 'TIME', 0
str_hud_lives       db 'LIVES', 0
str_hud_health      db 'HEALTH', 0
str_world_11        db '1-1', 0
str_x_prefix        db 'X', 0

str_title_1         db 'SUPER MARIO', 0
str_title_2         db 'ENGINE', 0
str_menu_start      db 'START GAME', 0
str_menu_exit       db 'EXIT', 0
str_title_footer    db 'EL2003 COAL  NASM + DOSBOX', 0
str_title_hint      db 'ARROWS:MOVE  SHIFT:RUN  SPACE:JUMP', 0

str_paused          db 'PAUSED', 0
str_pause_resume    db 'PRESS P TO RESUME', 0
str_pause_title     db 'PRESS ESC FOR TITLE', 0

str_world_banner    db 'WORLD 1-1', 0
str_lives_left      db 'LIVES LEFT', 0

str_level_comp      db 'LEVEL COMPLETE!', 0
str_lc_score        db 'SCORE', 0
str_lc_coins        db 'COINS', 0
str_lc_time         db 'TIME LEFT', 0
str_lc_bonus        db 'TIME BONUS', 0
str_lc_final        db 'FINAL SCORE', 0
str_lc_x50          db ' X 50 = ', 0
str_lc_plus         db '+ ', 0
str_press_enter     db 'PRESS ENTER TO CONTINUE', 0

str_game_over       db 'GAME OVER', 0
str_final_score     db 'FINAL SCORE:', 0
str_go_coins        db 'COINS:', 0
str_go_time         db 'TIME:', 0
str_go_enter        db 'PRESS ENTER FOR THE TITLE', 0

; Floating pop-up strings
str_pop_50          db '50', 0
str_pop_100         db '100', 0
str_pop_200         db '200', 0
str_pop_400         db '400', 0
str_pop_500         db '500', 0
str_pop_800         db '800', 0
str_pop_1000        db '1000', 0
str_pop_2000        db '2000', 0
str_pop_4000        db '4000', 0
str_pop_5000        db '5000', 0
str_pop_8000        db '8000', 0
str_pop_1up         db '1UP', 0

; -----------------------------------------------------------------------------
; Level 1-1 Map: Original Copy (60 columns x 12 rows = 720 bytes, NEVER modified)
; Tile codes:
;   0=Sky, 1=Ground, 2=Brick, 3=?Coin, 4=?Mushroom, 5=Empty,
;   6/7/8/9=Pipe (TL/TR/BL/BR), 10=Stair, 11=Coin, 12=Flag, 13/14/15=Castle
; -----------------------------------------------------------------------------
level_map_orig:
; Row 0: HUD strip (always sky/empty)
;      00,01,02,03,04,05,06,07,08,09, 10,11,12,13,14,15,16,17,18,19, 20,21,22,23,24,25,26,27,28,29
    db  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    db  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0

; Row 1: Sky above playfield
    db  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    db  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0

; Row 2: Flag pole top (col 54)
    db  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    db  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0,12, 0, 0, 0, 0, 0

; Row 3: Upper ?Coin block (col 10), Flag pole (col 54)
    db  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  3, 0, 0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    db  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0,12, 0, 0, 0, 0, 0

; Row 4: High platform coins (cols 40..42), Flag pole (col 54)
    db  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    db  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 11,11,11, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0,12, 0, 0, 0, 0, 0

; Row 5: Mid-air coins (cols 26..28), Flag pole (col 54)
    db  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0,11,11,11, 0
    db  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0,12, 0, 0, 0, 0, 0

; Row 6: ?Mush(5), Brick/? group(8..12), Brick/?Mush(22..25), Coin(35), Bricks/?(39..43), Stair(51), Flag(54), Castle top(56..58)
    db  0, 0, 0, 0, 0, 4, 0, 0, 2, 3,  2, 3, 2, 0, 0, 0, 0, 0, 0, 0,  0, 0, 2, 2, 4, 2, 0, 0, 0, 0
    db  0, 0, 0, 0, 0,11, 0, 0, 0, 2,  3, 2, 2, 2, 0, 0, 0, 0, 0, 0,  0,10, 0, 0,12, 0,14,14,14, 0

; Row 7: Coins over Pit 1(18..20), Pipe 2 top(30..31), Coins Pit 2(34,36), Stairs(50..51), Flag(54), Castle(56..58)
    db  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,11,11, 11, 0, 0, 0, 0, 0, 0, 0, 0, 0
    db  6, 7, 0, 0,11, 0,11, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 10,10, 0, 0,12, 0,13,13,13, 0

; Row 8: Pipe 1 top(14..15), Pipe 2 body(30..31), Stairs(49..51), Flag(54), Castle door(56..58)
    db  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 6, 7, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    db  8, 9, 0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0, 0,10, 10,10, 0, 0,12, 0,13,15,13, 0

; Row 9: Pipe 1 body(14..15), Pipe 2 body(30..31), Stairs(48..51), Flag base(54), Castle door(56..58)
    db  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 8, 9, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    db  8, 9, 0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,10,10, 10,10, 0, 0,10, 0,13,15,13, 0

; Row 10: Ground with Pit 1 (cols 18..19) and Pit 2 (cols 34..36)
    db  1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  1, 1, 1, 1, 1, 1, 1, 1, 0, 0,  1, 1, 1, 1, 1, 1, 1, 1, 1, 1
    db  1, 1, 1, 1, 0, 0, 0, 1, 1, 1,  1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  1, 1, 1, 1, 1, 1, 1, 1, 1, 1

; Row 11: Lower ground with Pit 1 (cols 18..19) and Pit 2 (cols 34..36)
    db  1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  1, 1, 1, 1, 1, 1, 1, 1, 0, 0,  1, 1, 1, 1, 1, 1, 1, 1, 1, 1
    db  1, 1, 1, 1, 0, 0, 0, 1, 1, 1,  1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  1, 1, 1, 1, 1, 1, 1, 1, 1, 1

; -----------------------------------------------------------------------------
; Working Copy of Level Map (Restored from level_map_orig via REP MOVSB)
; -----------------------------------------------------------------------------
level_map_work      times LEVEL_MAP_BYTES db 0
