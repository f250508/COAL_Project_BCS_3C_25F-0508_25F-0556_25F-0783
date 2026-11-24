; =============================================================================
; File:        states.inc
; Project:     Super Mario Engine (EL2003 COAL Semester Project)
; Description: Modules 14 & 15 - Jump-table state machine (Section 5.12),
;              frame execution order (Section 5.13), death & win sequences
;              (Sections 5.8 & 5.11), ResetLevel, NewGame, and all UI screens
;              (Title, Playing, Paused, Dying, Life-Lost, Level-Complete,
;              Game-Over).
; =============================================================================

; State Machine Jump Table indexed by game_state (0..8)
state_jump_table:
    dw StateTitle                   ; 0: STATE_TITLE
    dw StatePlaying                 ; 1: STATE_PLAYING
    dw StatePaused                  ; 2: STATE_PAUSED
    dw StateDying                   ; 3: STATE_DYING
    dw StateLifeLost                ; 4: STATE_LIFE_LOST
    dw StateWinSeq                  ; 5: STATE_WIN_SEQ
    dw StateLevelComp               ; 6: STATE_LEVEL_COMP
    dw StateGameOver                ; 7: STATE_GAME_OVER
    dw StateExitStub                ; 8: STATE_EXIT

; -----------------------------------------------------------------------------
; Procedure: RunCurrentState (Module 14)
; Purpose:   Dispatches the current frame to the active state handler via an
;            indexed jump table.
; Inputs:    None (reads game_state)
; Outputs:   Executes current state's update & render logic
; Registers: All preserved
; -----------------------------------------------------------------------------
RunCurrentState:
    push bx
    xor  bh, bh
    mov  bl, [game_state]
    shl  bx, 1
    call [state_jump_table + bx]
    pop  bx
    ret

; -----------------------------------------------------------------------------
; Procedure: ResetLevel (Module 15, Section 5.8)
; Purpose:   Resets Mario (Small, HP 1, start pos), camera (0), clock (300),
;            working map (from original via REP MOVSB), enemies, mushroom, and
;            pop-ups. Keeps score, coins, and lives intact.
; Inputs:    None
; Outputs:   Level ready to start/restart
; Registers: All preserved
; -----------------------------------------------------------------------------
ResetLevel:
    mov  word [hero_x_int], HERO_START_X
    mov  byte [hero_x_frac], 0
    mov  word [hero_y_int], HERO_START_Y
    mov  byte [hero_y_frac], 0
    mov  word [hero_vx], 0
    mov  word [hero_vy], 0
    mov  word [hero_prev_feet_y], HERO_START_Y + HERO_SMALL_H
    mov  byte [hero_on_ground], 1
    mov  byte [hero_jump_lock], 0
    mov  byte [hero_facing], 0
    mov  byte [hero_pose], POSE_STAND
    mov  byte [hero_form], 0
    mov  byte [hero_hp], HP_SMALL
    mov  byte [hero_invuln], 0
    mov  byte [hero_stomp_chain], 0

    mov  word [camera_x], 0
    mov  word [game_time], START_TIME
    mov  byte [sec_tick_accum], 0

    call ResetMap
    call InitEnemies
    call ClearPopups
    call StopSound
    ret

; -----------------------------------------------------------------------------
; Procedure: NewGame (Module 15, Section 5.8)
; Purpose:   Starts a brand-new game from scratch: score=0, coins=0, lives=3,
;            resets level, and enters STATE_PLAYING.
; Inputs:    None
; Outputs:   All game counters reset; game_state = STATE_PLAYING
; Registers: All preserved
; -----------------------------------------------------------------------------
NewGame:
    mov  word [score_lo], 0
    mov  word [score_hi], 0
    mov  byte [coins_count], 0
    mov  byte [lives_count], START_LIVES
    call ResetLevel
    mov  byte [game_state], STATE_PLAYING
    ret

; -----------------------------------------------------------------------------
; Procedure: StartDeath (Module 15, Section 5.8)
; Purpose:   Transitions into STATE_DYING, freezing enemies/clock/input and
;            setting upward hop (or continuing fall for pit death).
; Inputs:    [bp+4] = is_pit (1 = pit death, skip upward hop; 0 = normal death)
; Outputs:   game_state = STATE_DYING, state_timer = 45 ticks (1.5 s)
; Registers: All preserved
; -----------------------------------------------------------------------------
StartDeath:
    push bp
    mov  bp, sp
    push ax

    mov  byte [game_state], STATE_DYING
    mov  word [state_timer], DEATH_ANIM_TICKS
    mov  byte [hero_pose], POSE_JUMP
    mov  word [hero_vx], 0
    mov  byte [hero_invuln], 0

    mov  al, [bp+4]
    mov  [death_is_pit], al
    cmp  al, 0
    jne  .pit_fall
    mov  word [hero_vy], PHYS_DEATH_HOP ; Upward hop (~48 px)
    jmp  short .death_sfx
.pit_fall:
    mov  word [hero_vy], 0080h      ; Pit death: skip hop, keep falling

.death_sfx:
    mov  ax, 1800
    push ax
    mov  ax, 240
    push ax
    mov  ax, 15
    push ax
    call PlaySound

    pop  ax
    pop  bp
    ret  2

; -----------------------------------------------------------------------------
; Procedure: StartWinSequence (Module 15, Section 5.11)
; Purpose:   Triggered on touching the flag pole. Freezes clock/controls,
;            awards height-based flag points, snaps Mario to pole for slide.
; Inputs:    None
; Outputs:   game_state = STATE_WIN_SEQ, win_phase = 0
; Registers: All preserved
; -----------------------------------------------------------------------------
StartWinSequence:
    push ax

    call AwardFlagPoints

    mov  byte [game_state], STATE_WIN_SEQ
    mov  byte [win_phase], 0
    mov  word [state_timer], WIN_SLIDE_MIN_TICKS
    mov  word [hero_x_int], 54 * 16 - 4 ; Snap flush to flag pole column 54
    mov  byte [hero_x_frac], 0
    mov  word [hero_vx], 0
    mov  word [hero_vy], 0
    mov  byte [hero_facing], 0
    mov  byte [hero_pose], POSE_JUMP

    ; Victory rising chime
    mov  ax, 2600
    push ax
    mov  ax, -140
    push ax
    mov  ax, 12
    push ax
    call PlaySound

    pop  ax
    ret

; -----------------------------------------------------------------------------
; Procedure: RenderGameScene
; Purpose:   Draws a complete gameplay frame into frame_buf_seg in layered
;            order: playfield clear, parallax BG, map tiles, entities, Mario,
;            score pop-ups, and fixed top HUD.
; Inputs:    None
; Outputs:   frame_buf_seg populated (caller invokes FlipFrameBuffer)
; Registers: All preserved
; -----------------------------------------------------------------------------
RenderGameScene:
    call ClearPlayfield
    call DrawParallaxBG
    call DrawMap
    call DrawEntities
    call DrawHero
    call DrawPopups
    call DrawHUD
    ret

; =============================================================================
; STATE 0: TITLE SCREEN (Section 5.12 & Figure 1)
; =============================================================================
StateTitle:
    push ax
    push bx
    push cx

    ; Handle menu keys (edge-triggered)
    cmp  byte [key_pressed + KEY_UP], 0
    je   .chk_down
    mov  byte [menu_selection], 0
.chk_down:
    cmp  byte [key_pressed + KEY_DOWN], 0
    je   .chk_esc
    mov  byte [menu_selection], 1
.chk_esc:
    cmp  byte [key_pressed + KEY_ESC], 0
    je   .chk_enter
    mov  byte [game_state], STATE_EXIT
    jmp  .done
.chk_enter:
    cmp  byte [key_pressed + KEY_ENTER], 0
    je   .draw_title
    cmp  byte [menu_selection], 0
    jne  .choose_exit
    call NewGame
    jmp  .done
.choose_exit:
    mov  byte [game_state], STATE_EXIT
    jmp  .done

.draw_title:
    ; Fill background with classic sky blue
    mov  ax, COL_SKY
    push ax
    call ClearFrameBuffer

    ; Draw decorative clouds on title screen
    mov  ax, 36
    push ax
    mov  ax, 30
    push ax
    mov  ax, 42
    push ax
    mov  ax, 12
    push ax
    mov  ax, COL_WHITE
    push ax
    call FillRect

    mov  ax, 236
    push ax
    mov  ax, 38
    push ax
    mov  ax, 46
    push ax
    mov  ax, 12
    push ax
    mov  ax, COL_WHITE
    push ax
    call FillRect

    ; Draw top brick border (row 0, cols 0..19)
    xor  bx, bx
    mov  cx, 20
.top_bricks:
    push bx
    xor  ax, ax
    push ax
    mov  ax, TILE_BRICK
    push ax
    call DrawTile
    add  bx, 16
    loop .top_bricks

    ; Draw left & right brick columns (rows 1..9)
    mov  bx, 16
    mov  cx, 9
.side_bricks:
    xor  ax, ax
    push ax
    push bx
    mov  ax, TILE_BRICK
    push ax
    call DrawTile

    mov  ax, 304
    push ax
    push bx
    mov  ax, TILE_BRICK
    push ax
    call DrawTile

    add  bx, 16
    loop .side_bricks

    ; Draw ground strip across bottom (rows 10 & 11)
    xor  bx, bx
    mov  cx, 20
.bot_ground:
    push bx
    mov  ax, 160
    push ax
    mov  ax, TILE_GROUND
    push ax
    call DrawTile

    push bx
    mov  ax, 176
    push ax
    mov  ax, TILE_GROUND
    push ax
    call DrawTile

    add  bx, 16
    loop .bot_ground

    ; Bottom dark footer strip (rows 192..199)
    xor  ax, ax
    push ax
    mov  ax, 192
    push ax
    mov  ax, SCREEN_W
    push ax
    mov  ax, 8
    push ax
    xor  ax, ax
    push ax
    call FillRect

    ; Title banner panel
    mov  ax, 60
    push ax
    mov  ax, 28
    push ax
    mov  ax, 200
    push ax
    mov  ax, 42
    push ax
    mov  ax, COL_BRICK
    push ax
    mov  ax, COL_GOLD
    push ax
    call DrawBorderBox

    ; Drop shadow + red/gold title text "SUPER MARIO"
    mov  ax, 117
    push ax
    mov  ax, 39
    push ax
    mov  ax, str_title_1
    push ax
    mov  ax, COL_OUTLINE
    push ax
    call DrawString

    mov  ax, 116
    push ax
    mov  ax, 38
    push ax
    mov  ax, str_title_1
    push ax
    mov  ax, COL_GOLD_HI
    push ax
    call DrawString

    ; Subtitle "ENGINE"
    mov  ax, 136
    push ax
    mov  ax, 54
    push ax
    mov  ax, str_title_2
    push ax
    mov  ax, COL_WHITE
    push ax
    call DrawString

    ; Menu selection panel
    mov  ax, 92
    push ax
    mov  ax, 84
    push ax
    mov  ax, 136
    push ax
    mov  ax, 48
    push ax
    mov  ax, COL_UI_BG
    push ax
    mov  ax, COL_WHITE
    push ax
    call DrawBorderBox

    ; Menu option 0: START GAME
    mov  bx, COL_WHITE
    cmp  byte [menu_selection], 0
    jne  .opt0_col_ok
    mov  bx, COL_GOLD_HI
    ; Draw ">" cursor
    mov  ax, 108
    push ax
    mov  ax, 96
    push ax
    mov  ax, '>'
    push ax
    push bx
    call DrawChar
.opt0_col_ok:
    mov  ax, 122
    push ax
    mov  ax, 96
    push ax
    mov  ax, str_menu_start
    push ax
    push bx
    call DrawString

    ; Menu option 1: EXIT
    mov  bx, COL_WHITE
    cmp  byte [menu_selection], 1
    jne  .opt1_col_ok
    mov  bx, COL_GOLD_HI
    mov  ax, 108
    push ax
    mov  ax, 114
    push ax
    mov  ax, '>'
    push ax
    push bx
    call DrawChar
.opt1_col_ok:
    mov  ax, 122
    push ax
    mov  ax, 114
    push ax
    mov  ax, str_menu_exit
    push ax
    push bx
    call DrawString

    ; Decorative sprites on ground (Mario left, Mushroom center, Goomba right)
    mov  ax, 44
    push ax
    mov  ax, 144
    push ax
    mov  ax, spr_mario_s_stand
    push ax
    xor  ax, ax
    push ax
    call DrawSprite

    mov  ax, 256
    push ax
    mov  ax, 144
    push ax
    mov  ax, spr_goomba_walk
    push ax
    xor  ax, ax
    push ax
    call DrawSprite

    ; Controls hint and footer
    mov  ax, 24
    push ax
    mov  ax, 178
    push ax
    mov  ax, str_title_hint
    push ax
    mov  ax, COL_GOLD_HI
    push ax
    call DrawString

    mov  ax, 56
    push ax
    mov  ax, 192
    push ax
    mov  ax, str_title_footer
    push ax
    mov  ax, COL_WHITE
    push ax
    call DrawString

    call FlipFrameBuffer

.done:
    pop  cx
    pop  bx
    pop  ax
    ret

; =============================================================================
; STATE 1: PLAYING (Exact Section 5.13 Frame Order)
; =============================================================================
StatePlaying:
    push ax

    ; Step 2: Check state-transition and documented debug keys
    cmp  byte [key_pressed + KEY_P], 0
    je   .chk_esc
    call StopSound
    mov  byte [game_state], STATE_PAUSED
    jmp  .done

.chk_esc:
    cmp  byte [key_pressed + KEY_ESC], 0
    je   .chk_debug
    call StopSound
    mov  byte [game_state], STATE_TITLE
    jmp  .done

.chk_debug:
    ; Documented demo debug keys (Section 7.7 & README):
    ; F1 = set clock to 5 seconds; F2 = set coins to 99
    cmp  byte [key_pressed + KEY_F1], 0
    je   .chk_f2
    mov  word [game_time], 5
    mov  byte [sec_tick_accum], 0
.chk_f2:
    cmp  byte [key_pressed + KEY_F2], 0
    je   .step3_hero
    mov  byte [coins_count], 99

    ; Step 3: Update Mario (physics + axis-separated tile collision + coins/flag)
.step3_hero:
    call UpdateMario
    cmp  byte [game_state], STATE_PLAYING
    jne  .render_now                ; Flag pole touched -> render & finish

    ; Step 4: Update enemies, shells, mushroom, and pop-ups
    call UpdateEnemies
    call UpdateMushroom
    call UpdatePopups

    ; Step 5: Resolve Mario against enemies & mushroom
    call ResolveEntityCollisions
    cmp  byte [game_state], STATE_PLAYING
    jne  .render_now                ; Died from enemy contact

    ; Step 6: Decrement invulnerability timer & 1-second level clock
    cmp  byte [hero_invuln], 0
    je   .tick_clock
    dec  byte [hero_invuln]

.tick_clock:
    inc  byte [sec_tick_accum]
    cmp  byte [sec_tick_accum], TICKS_PER_SEC
    jb   .step7_checks
    mov  byte [sec_tick_accum], 0
    cmp  word [game_time], 0
    je   .step7_checks
    dec  word [game_time]

    ; Step 7: Check pit death (y >= 192) and clock == 0
.step7_checks:
    cmp  word [hero_y_int], PLAY_BOTTOM_Y
    jl   .chk_timeout
    mov  ax, 1                      ; is_pit = 1 (skip upward death hop)
    push ax
    call StartDeath
    jmp  short .render_now

.chk_timeout:
    cmp  word [game_time], 0
    jne  .step8_camera
    xor  ax, ax                     ; is_pit = 0 (timeout death with hop)
    push ax
    call StartDeath
    jmp  short .render_now

    ; Step 8: Update camera (Section 5.3)
.step8_camera:
    call UpdateCamera

    ; Steps 9 & 10: Draw scene into frame buffer & flip to VGA memory
.render_now:
    call RenderGameScene
    call FlipFrameBuffer

.done:
    pop  ax
    ret

; =============================================================================
; STATE 2: PAUSED (Section 5.12 & Figure 12)
; =============================================================================
StatePaused:
    push ax

    cmp  byte [key_pressed + KEY_P], 0
    je   .chk_esc
    mov  byte [game_state], STATE_PLAYING
    jmp  short .done

.chk_esc:
    cmp  byte [key_pressed + KEY_ESC], 0
    je   .draw_pause
    mov  byte [game_state], STATE_TITLE
    jmp  short .done

.draw_pause:
    ; Keep frozen gameplay picture visible underneath
    call RenderGameScene

    ; Centered PAUSED overlay box
    mov  ax, 68
    push ax
    mov  ax, 72
    push ax
    mov  ax, 184
    push ax
    mov  ax, 58
    push ax
    mov  ax, COL_UI_BG
    push ax
    mov  ax, COL_UI_BORDER
    push ax
    call DrawBorderBox

    mov  ax, 136
    push ax
    mov  ax, 82
    push ax
    mov  ax, str_paused
    push ax
    mov  ax, COL_GOLD_HI
    push ax
    call DrawString

    mov  ax, 92
    push ax
    mov  ax, 100
    push ax
    mov  ax, str_pause_resume
    push ax
    mov  ax, COL_WHITE
    push ax
    call DrawString

    mov  ax, 84
    push ax
    mov  ax, 114
    push ax
    mov  ax, str_pause_title
    push ax
    mov  ax, COL_GRAY
    push ax
    call DrawString

    call FlipFrameBuffer

.done:
    pop  ax
    ret

; =============================================================================
; STATE 3: DYING (Section 5.8 - 1.5 s death animation, no collisions)
; =============================================================================
StateDying:
    push ax

    ; Apply gravity to hero_y with no collisions
    mov  ax, [hero_vy]
    add  ax, PHYS_GRAVITY
    cmp  ax, PHYS_MAX_FALL
    jle  .vy_ok
    mov  ax, PHYS_MAX_FALL
.vy_ok:
    mov  [hero_vy], ax

    mov  ax, hero_y_int
    push ax
    push word [hero_vy]
    call AddFixed88

    call RenderGameScene
    call FlipFrameBuffer

    dec  word [state_timer]
    jnz  .done

    ; Death animation finished: lives - 1
    cmp  byte [lives_count], 0
    je   .no_lives
    dec  byte [lives_count]
    cmp  byte [lives_count], 0
    je   .no_lives

    ; Lives remaining -> Life-Lost screen for 2 s
    mov  byte [game_state], STATE_LIFE_LOST
    mov  word [state_timer], LIFE_LOST_TICKS
    jmp  short .done

.no_lives:
    mov  byte [game_state], STATE_GAME_OVER

.done:
    pop  ax
    ret

; =============================================================================
; STATE 4: LIFE LOST (Section 5.8 & Figure 7b - 2 s display, then ResetLevel)
; =============================================================================
StateLifeLost:
    push ax

    xor  ax, ax
    push ax
    call ClearFrameBuffer

    ; Neat gold border frame
    mov  ax, 52
    push ax
    mov  ax, 40
    push ax
    mov  ax, 216
    push ax
    mov  ax, 120
    push ax
    xor  ax, ax
    push ax
    mov  ax, COL_UI_BORDER
    push ax
    call DrawBorderBox

    ; "WORLD 1-1"
    mov  ax, 124
    push ax
    mov  ax, 60
    push ax
    mov  ax, str_world_banner
    push ax
    mov  ax, COL_WHITE
    push ax
    call DrawString

    ; Mario sprite icon
    mov  ax, 120
    push ax
    mov  ax, 86
    push ax
    mov  ax, spr_mario_s_stand
    push ax
    xor  ax, ax
    push ax
    call DrawSprite

    ; "X <lives>"
    mov  ax, 152
    push ax
    mov  ax, 92
    push ax
    mov  ax, str_x_prefix
    push ax
    mov  ax, COL_WHITE
    push ax
    call DrawString

    mov  ax, 172
    push ax
    mov  ax, 92
    push ax
    xor  ah, ah
    mov  al, [lives_count]
    push ax
    xor  ax, ax
    push ax
    mov  ax, 1
    push ax
    mov  ax, COL_WHITE
    push ax
    call DrawNumber32

    ; "LIVES LEFT"
    mov  ax, 120
    push ax
    mov  ax, 124
    push ax
    mov  ax, str_lives_left
    push ax
    mov  ax, COL_GOLD_HI
    push ax
    call DrawString

    call FlipFrameBuffer

    dec  word [state_timer]
    jnz  .done

    call ResetLevel
    mov  byte [game_state], STATE_PLAYING

.done:
    pop  ax
    ret

; =============================================================================
; STATE 5: WIN SEQUENCE (Section 5.11 - slide down pole + walk to castle)
; =============================================================================
StateWinSeq:
    push ax
    push bx
    push dx

    call UpdatePopups

    cmp  byte [win_phase], 0
    jne  .phase_walk

    ; --- Phase 0: Slide down flag pole to top of base block (row 9 = y 144) ---
    call GetHeroHeight
    mov  bx, 144                    ; Top of flagpole base block (row 9)
    sub  bx, ax                     ; Target Y (128 Small / 112 Super)

    cmp  [hero_y_int], bx
    jge  .at_pole_base
    add  word [hero_y_int], 2       ; Slide down 2 px/frame
    cmp  [hero_y_int], bx
    jle  .slide_timer
    mov  [hero_y_int], bx
.at_pole_base:
    mov  byte [hero_pose], POSE_STAND
.slide_timer:
    cmp  word [state_timer], 0
    je   .chk_slide_done
    dec  word [state_timer]
.chk_slide_done:
    cmp  [hero_y_int], bx
    jl   .render_win
    cmp  word [state_timer], 0
    jne  .render_win

    ; Switch to Phase 1: step off base block and walk toward castle (~1 s)
    mov  byte [win_phase], 1
    mov  word [state_timer], WIN_WALK_TICKS
    add  word [hero_y_int], 16      ; Step down from base block onto ground
    add  word [hero_x_int], 12      ; Step clear of pole
    jmp  short .render_win

.phase_walk:
    ; Walk right toward castle door (col 57 = x 912)
    cmp  word [hero_x_int], 57 * 16
    jge  .walk_timer
    inc  word [hero_x_int]
    test word [frame_counter], 4
    jz   .w_run
    mov  byte [hero_pose], POSE_STAND
    jmp  short .walk_timer
.w_run:
    mov  byte [hero_pose], POSE_RUN

.walk_timer:
    dec  word [state_timer]
    jnz  .render_win

    ; Win sequence complete! Snapshot score, add time * 50 bonus, show screen
    mov  ax, [score_lo]
    mov  [pre_bonus_lo], ax
    mov  ax, [score_hi]
    mov  [pre_bonus_hi], ax

    mov  ax, [game_time]
    mov  bx, 50
    mul  bx                         ; AX = time_left * 50 (max 15,000)
    mov  [time_bonus_val], ax
    push ax
    call AddScore

    mov  byte [game_state], STATE_LEVEL_COMP
    jmp  short .done

.render_win:
    call RenderGameScene
    call FlipFrameBuffer

.done:
    pop  dx
    pop  bx
    pop  ax
    ret

; =============================================================================
; STATE 6: LEVEL COMPLETE SCREEN (Section 5.11 & Figure 10)
; =============================================================================
StateLevelComp:
    push ax

    cmp  byte [key_pressed + KEY_ENTER], 0
    jne  .to_title
    cmp  byte [key_pressed + KEY_ESC], 0
    je   .draw_lc
.to_title:
    mov  byte [game_state], STATE_TITLE
    jmp  .done

.draw_lc:
    xor  ax, ax
    push ax
    call ClearFrameBuffer

    ; Outer navy/gold box
    mov  ax, 24
    push ax
    mov  ax, 18
    push ax
    mov  ax, 272
    push ax
    mov  ax, 164
    push ax
    mov  ax, COL_UI_BG
    push ax
    mov  ax, COL_UI_BORDER
    push ax
    call DrawBorderBox

    ; Header "LEVEL COMPLETE!"
    mov  ax, 100
    push ax
    mov  ax, 32
    push ax
    mov  ax, str_level_comp
    push ax
    mov  ax, COL_GOLD_HI
    push ax
    call DrawString

    ; Row 1: SCORE (pre-bonus)
    mov  ax, 44
    push ax
    mov  ax, 58
    push ax
    mov  ax, str_lc_score
    push ax
    mov  ax, COL_WHITE
    push ax
    call DrawString

    mov  ax, 148
    push ax
    mov  ax, 58
    push ax
    push word [pre_bonus_lo]
    push word [pre_bonus_hi]
    mov  ax, 6
    push ax
    mov  ax, COL_WHITE
    push ax
    call DrawNumber32

    ; Row 2: COINS
    mov  ax, 44
    push ax
    mov  ax, 74
    push ax
    mov  ax, str_lc_coins
    push ax
    mov  ax, COL_WHITE
    push ax
    call DrawString

    mov  ax, 148
    push ax
    mov  ax, 74
    push ax
    mov  ax, str_x_prefix
    push ax
    mov  ax, COL_GOLD
    push ax
    call DrawString

    mov  ax, 160
    push ax
    mov  ax, 74
    push ax
    xor  ah, ah
    mov  al, [coins_count]
    push ax
    xor  ax, ax
    push ax
    mov  ax, 2
    push ax
    mov  ax, COL_WHITE
    push ax
    call DrawNumber32

    ; Row 3: TIME LEFT   NNN X 50 = BBBBB
    mov  ax, 44
    push ax
    mov  ax, 90
    push ax
    mov  ax, str_lc_time
    push ax
    mov  ax, COL_WHITE
    push ax
    call DrawString

    mov  ax, 148
    push ax
    mov  ax, 90
    push ax
    push word [game_time]
    xor  ax, ax
    push ax
    mov  ax, 3
    push ax
    mov  ax, COL_WHITE
    push ax
    call DrawNumber32

    mov  ax, 172
    push ax
    mov  ax, 90
    push ax
    mov  ax, str_lc_x50
    push ax
    mov  ax, COL_GOLD_HI
    push ax
    call DrawString

    mov  ax, 236
    push ax
    mov  ax, 90
    push ax
    push word [time_bonus_val]
    xor  ax, ax
    push ax
    mov  ax, 1
    push ax
    mov  ax, COL_WHITE
    push ax
    call DrawNumber32

    ; Row 4: TIME BONUS   + BBBBB
    mov  ax, 44
    push ax
    mov  ax, 106
    push ax
    mov  ax, str_lc_bonus
    push ax
    mov  ax, COL_WHITE
    push ax
    call DrawString

    mov  ax, 148
    push ax
    mov  ax, 106
    push ax
    mov  ax, str_lc_plus
    push ax
    mov  ax, COL_GRASS_HI
    push ax
    call DrawString

    mov  ax, 164
    push ax
    mov  ax, 106
    push ax
    push word [time_bonus_val]
    xor  ax, ax
    push ax
    mov  ax, 1
    push ax
    mov  ax, COL_GRASS_HI
    push ax
    call DrawNumber32

    ; Row 5: FINAL SCORE
    mov  ax, 44
    push ax
    mov  ax, 126
    push ax
    mov  ax, str_lc_final
    push ax
    mov  ax, COL_GOLD_HI
    push ax
    call DrawString

    mov  ax, 148
    push ax
    mov  ax, 126
    push ax
    push word [score_lo]
    push word [score_hi]
    mov  ax, 6
    push ax
    mov  ax, COL_GOLD_HI
    push ax
    call DrawNumber32

    ; Footer prompt
    mov  ax, 68
    push ax
    mov  ax, 156
    push ax
    mov  ax, str_press_enter
    push ax
    mov  ax, COL_GRASS_HI
    push ax
    call DrawString

    call FlipFrameBuffer

.done:
    pop  ax
    ret

; =============================================================================
; STATE 7: GAME OVER SCREEN (Section 5.12 & Figure 11)
; =============================================================================
StateGameOver:
    push ax

    cmp  byte [key_pressed + KEY_ENTER], 0
    jne  .to_title
    cmp  byte [key_pressed + KEY_ESC], 0
    je   .draw_go
.to_title:
    mov  byte [game_state], STATE_TITLE
    jmp  .done

.draw_go:
    xor  ax, ax
    push ax
    call ClearFrameBuffer

    ; Red-bordered Game Over frame
    mov  ax, 36
    push ax
    mov  ax, 28
    push ax
    mov  ax, 248
    push ax
    mov  ax, 144
    push ax
    xor  ax, ax
    push ax
    mov  ax, COL_RED
    push ax
    call DrawBorderBox

    ; "GAME OVER"
    mov  ax, 124
    push ax
    mov  ax, 54
    push ax
    mov  ax, str_game_over
    push ax
    mov  ax, COL_RED
    push ax
    call DrawString

    ; "FINAL SCORE: 000000"
    mov  ax, 84
    push ax
    mov  ax, 86
    push ax
    mov  ax, str_final_score
    push ax
    mov  ax, COL_WHITE
    push ax
    call DrawString

    mov  ax, 188
    push ax
    mov  ax, 86
    push ax
    push word [score_lo]
    push word [score_hi]
    mov  ax, 6
    push ax
    mov  ax, COL_GOLD_HI
    push ax
    call DrawNumber32

    ; "COINS: NN   TIME: NNN"
    mov  ax, 76
    push ax
    mov  ax, 108
    push ax
    mov  ax, str_go_coins
    push ax
    mov  ax, COL_GOLD
    push ax
    call DrawString

    mov  ax, 132
    push ax
    mov  ax, 108
    push ax
    xor  ah, ah
    mov  al, [coins_count]
    push ax
    xor  ax, ax
    push ax
    mov  ax, 2
    push ax
    mov  ax, COL_WHITE
    push ax
    call DrawNumber32

    mov  ax, 168
    push ax
    mov  ax, 108
    push ax
    mov  ax, str_go_time
    push ax
    mov  ax, COL_GOLD
    push ax
    call DrawString

    mov  ax, 216
    push ax
    mov  ax, 108
    push ax
    push word [game_time]
    xor  ax, ax
    push ax
    mov  ax, 3
    push ax
    mov  ax, COL_WHITE
    push ax
    call DrawNumber32

    ; "PRESS ENTER FOR THE TITLE"
    mov  ax, 60
    push ax
    mov  ax, 144
    push ax
    mov  ax, str_go_enter
    push ax
    mov  ax, COL_GRASS_HI
    push ax
    call DrawString

    call FlipFrameBuffer

.done:
    pop  ax
    ret

; =============================================================================
; STATE 8: EXIT STUB (Main loop breaks cleanly into CleanExit)
; =============================================================================
StateExitStub:
    ret
