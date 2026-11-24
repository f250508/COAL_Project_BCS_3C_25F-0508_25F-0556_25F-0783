; =============================================================================
; File:        entities.inc
; Project:     Super Mario Engine (EL2003 COAL Semester Project)
; Description: Modules 11 & 12 - Enemy table initialization & AI (Goomba, Koopa,
;              stationary/sliding shells), Mushroom power-up physics, brick &
;              "?" block triggers, map coin collection, Mario-vs-Entity
;              collision resolution (stomp, kick, shell kills, hurt, power-up),
;              and entity/hero sprite rendering.
; =============================================================================

; -----------------------------------------------------------------------------
; Procedure: InitEnemies (Module 11)
; Purpose:   Clears enemy_table and mushroom state, then populates Level 1-1
;            enemies from enemy_spawn_tbl in EST_WAITING state.
; Inputs:    None
; Outputs:   enemy_table initialized, mush_active = 0
; Registers: All preserved
; -----------------------------------------------------------------------------
InitEnemies:
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push es

    ; Clear enemy_table (MAX_ENEMIES * ENEMY_REC_SIZE bytes)
    push ds
    pop  es
    mov  di, enemy_table
    xor  ax, ax
    mov  cx, (MAX_ENEMIES * ENEMY_REC_SIZE) / 2
    cld
    rep  stosw

    mov  byte [mush_active], 0

    ; Populate from enemy_spawn_tbl: [type, col, row]
    mov  si, enemy_spawn_tbl
    mov  di, enemy_table
    mov  cx, enemy_spawn_count
.spawn_loop:
    mov  al, [si]                   ; type (ENT_GOOMBA / ENT_KOOPA)
    mov  [di + EN_TYPE], al
    mov  byte [di + EN_STATE], EST_WAITING
    mov  byte [di + EN_DIR], 1      ; Start walking left when activated
    mov  byte [di + EN_GROUND], 1

    ; x_int = col * 16
    xor  ah, ah
    mov  al, [si+1]
    push cx
    mov  cl, 4
    shl  ax, cl
    mov  [di + EN_X_INT], ax

    ; y_int = row * 16
    xor  ah, ah
    mov  al, [si+2]
    shl  ax, cl
    mov  [di + EN_Y_INT], ax
    pop  cx

    add  si, 3
    add  di, ENEMY_REC_SIZE
    loop .spawn_loop

    pop  es
    pop  di
    pop  si
    pop  dx
    pop  cx
    pop  bx
    pop  ax
    ret

; -----------------------------------------------------------------------------
; Procedure: CollectMapCoin (Module 12, Section 5.4 & 5.9)
; Purpose:   Removes a free coin from level_map_work, awards +200 points and
;            +1 coin (with 100-coin 1-UP check), and spawns a score pop-up.
; Inputs:    [bp+6] = col (0..59)
;            [bp+4] = row (0..11)
; Outputs:   Tile becomes TILE_SKY; score & coins updated
; Registers: All preserved
; -----------------------------------------------------------------------------
CollectMapCoin:
    push bp
    mov  bp, sp
    push ax
    push bx
    push cx

    ; Remove coin from working map
    push word [bp+6]
    push word [bp+4]
    mov  ax, TILE_SKY
    push ax
    call SetTile

    mov  ax, 200
    push ax
    call AddScore
    call AddCoin

    ; Spawn "200" pop-up at coin world position
    mov  ax, [bp+6]
    mov  bx, [bp+4]
    mov  cl, 4
    shl  ax, cl
    shl  bx, cl
    push ax
    push bx
    mov  ax, str_pop_200
    push ax
    call SpawnPopup

    ; Crisp coin chime SFX
    mov  ax, 1200
    push ax
    mov  ax, -180
    push ax
    mov  ax, 4
    push ax
    call PlaySound

    pop  cx
    pop  bx
    pop  ax
    pop  bp
    ret  4

; -----------------------------------------------------------------------------
; Procedure: TriggerBlock (Module 12, Section 5.9)
; Purpose:   Handles head-hit from below on Brick, "?" Coin, "?" Mushroom,
;            and Empty blocks in level_map_work.
; Inputs:    [bp+6] = col (0..59)
;            [bp+4] = row (0..11)
; Outputs:   Modifies working map, awards score/coins, or spawns mushroom
; Registers: All preserved
; -----------------------------------------------------------------------------
TriggerBlock:
    push bp
    mov  bp, sp
    push ax
    push bx
    push cx

    ; Read tile at (col*16, row*16) via GetTile
    mov  ax, [bp+6]
    mov  bx, [bp+4]
    mov  cl, 4
    shl  ax, cl
    shl  bx, cl
    push ax
    push bx
    call GetTile                    ; AL = tile code

    cmp  al, TILE_BRICK
    je   .hit_brick
    cmp  al, TILE_Q_COIN
    je   .hit_q_coin
    cmp  al, TILE_Q_MUSH
    jne  .no_trigger
    jmp  .hit_q_mush
.no_trigger:
    jmp  .done                      ; Empty block / ground / stair -> nothing

.hit_brick:
    ; Small Mario bumping a brick gives nothing (Section 5.4 & 5.9)
    cmp  byte [hero_form], 0
    jne  .break_brick
    ; Optional soft bump sound
    mov  ax, 3800
    push ax
    mov  ax, 200
    push ax
    mov  ax, 3
    push ax
    call PlaySound
    jmp  .done

.break_brick:
    ; Super Mario destroys brick -> TILE_SKY, +50 points
    push word [bp+6]
    push word [bp+4]
    mov  ax, TILE_SKY
    push ax
    call SetTile

    mov  ax, 50
    push ax
    call AddScore

    mov  ax, [bp+6]
    mov  bx, [bp+4]
    mov  cl, 4
    shl  ax, cl
    shl  bx, cl
    push ax
    push bx
    mov  ax, str_pop_50
    push ax
    call SpawnPopup

    mov  ax, 2600
    push ax
    mov  ax, 350
    push ax
    mov  ax, 4
    push ax
    call PlaySound
    jmp  .done

.hit_q_coin:
    ; "?" coin block -> empty block, +200 points, coins +1
    push word [bp+6]
    push word [bp+4]
    mov  ax, TILE_EMPTY
    push ax
    call SetTile

    mov  ax, 200
    push ax
    call AddScore
    call AddCoin

    ; Pop-up above the block
    mov  ax, [bp+6]
    mov  bx, [bp+4]
    dec  bx
    mov  cl, 4
    shl  ax, cl
    shl  bx, cl
    push ax
    push bx
    mov  ax, str_pop_200
    push ax
    call SpawnPopup

    mov  ax, 1200
    push ax
    mov  ax, -180
    push ax
    mov  ax, 4
    push ax
    call PlaySound
    jmp  short .done

.hit_q_mush:
    ; "?" mushroom block -> empty block, spawn mushroom on top moving right
    push word [bp+6]
    push word [bp+4]
    mov  ax, TILE_EMPTY
    push ax
    call SetTile

    mov  byte [mush_active], 1
    mov  ax, [bp+6]
    mov  bx, [bp+4]
    dec  bx                         ; One tile above the "?" block
    mov  cl, 4
    shl  ax, cl
    shl  bx, cl
    mov  [mush_x_int], ax
    mov  byte [mush_x_frac], 0
    mov  [mush_y_int], bx
    mov  byte [mush_y_frac], 0
    mov  word [mush_vx], PHYS_MUSH_SPD  ; Moves right at 1.0 px/frame
    mov  word [mush_vy], 0

    mov  ax, 2800
    push ax
    mov  ax, -220
    push ax
    mov  ax, 7
    push ax
    call PlaySound

.done:
    pop  cx
    pop  bx
    pop  ax
    pop  bp
    ret  4

; -----------------------------------------------------------------------------
; Procedure: MakeSuperMario (Section 5.1 & 5.9)
; Purpose:   Awards +1000 points for collecting a mushroom. If Mario is Small,
;            upgrades him to Super Mario (HP = 2, height 32) keeping feet fixed.
; Inputs:    None
; Outputs:   Updates score, hero_form, hero_hp, hero_y_int
; Registers: All preserved
; -----------------------------------------------------------------------------
MakeSuperMario:
    push ax

    mov  ax, 1000
    push ax
    call AddScore

    push word [hero_x_int]
    mov  ax, [hero_y_int]
    sub  ax, 8
    push ax
    mov  ax, str_pop_1000
    push ax
    call SpawnPopup

    cmp  byte [hero_form], 0
    jne  .play_sfx                  ; Already Super: points only, HP stays 2

    mov  byte [hero_form], 1
    mov  byte [hero_hp], HP_SUPER
    ; Keep feet fixed: y_top = y_feet - 32 = old_y_top - 16
    sub  word [hero_y_int], 16
    cmp  word [hero_y_int], PLAY_TOP_Y
    jge  .play_sfx
    mov  word [hero_y_int], PLAY_TOP_Y

.play_sfx:
    mov  ax, 2400
    push ax
    mov  ax, -160
    push ax
    mov  ax, 9
    push ax
    call PlaySound

    pop  ax
    ret

; -----------------------------------------------------------------------------
; Procedure: HurtMario (Section 5.7)
; Purpose:   Applies enemy contact damage. Ignored during invulnerability.
;            Super Mario shrinks to Small (feet fixed) with 2 s blink;
;            Small Mario dies immediately.
; Inputs:    None
; Outputs:   Updates hero_hp, hero_form, hero_invuln, or enters STATE_DYING
; Registers: All preserved
; -----------------------------------------------------------------------------
HurtMario:
    push ax

    cmp  byte [hero_invuln], 0
    jne  .done                      ; Invulnerable -> no effect

    cmp  byte [hero_hp], HP_SUPER
    jb   .kill_small

    ; Super Mario -> Small Mario (HP 2 -> 1), feet fixed, 2 s invulnerability
    mov  byte [hero_hp], HP_SMALL
    mov  byte [hero_form], 0
    add  word [hero_y_int], 16      ; Keep feet fixed (height 32 -> 16)
    mov  byte [hero_invuln], INVULN_TICKS

    mov  ax, 1400
    push ax
    mov  ax, 260
    push ax
    mov  ax, 8
    push ax
    call PlaySound
    jmp  short .done

.kill_small:
    mov  byte [hero_hp], 0
    xor  ax, ax                     ; is_pit = 0 (play upward death hop)
    push ax
    call StartDeath

.done:
    pop  ax
    ret

; -----------------------------------------------------------------------------
; Procedure: UpdateMushroom (Module 12, Section 5.9)
; Purpose:   Moves active mushroom at 1 px/frame, turns at walls, applies
;            gravity, lands on floors, and removes it on pit or left-of-camera.
; Inputs:    None
; Outputs:   Updates mush_* state
; Registers: All preserved
; -----------------------------------------------------------------------------
UpdateMushroom:
    push ax
    push dx

    cmp  byte [mush_active], 0
    jne  .is_active
    jmp  .done
.is_active:

    ; Horizontal move
    mov  ax, mush_x_int
    push ax
    push word [mush_vx]
    call AddFixed88

    ; Check solid wall on leading side
    cmp  word [mush_vx], 0
    jl   .mush_wall_l

    mov  dx, [mush_x_int]
    add  dx, 15
    push dx
    mov  ax, [mush_y_int]
    add  ax, 8
    push ax
    call GetTile
    push ax
    call IsTileSolid
    cmp  ax, 1
    jne  .mush_grav
    and  dx, 0FFF0h
    sub  dx, 16
    mov  [mush_x_int], dx
    neg  word [mush_vx]
    jmp  short .mush_grav

.mush_wall_l:
    mov  dx, [mush_x_int]
    push dx
    mov  ax, [mush_y_int]
    add  ax, 8
    push ax
    call GetTile
    push ax
    call IsTileSolid
    cmp  ax, 1
    jne  .mush_grav
    and  dx, 0FFF0h
    add  dx, 16
    mov  [mush_x_int], dx
    neg  word [mush_vx]

.mush_grav:
    mov  ax, [mush_vy]
    add  ax, PHYS_GRAVITY
    cmp  ax, PHYS_MAX_FALL
    jle  .mush_store_vy
    mov  ax, PHYS_MAX_FALL
.mush_store_vy:
    mov  [mush_vy], ax

    mov  ax, mush_y_int
    push ax
    push word [mush_vy]
    call AddFixed88

    ; Floor collision
    mov  dx, [mush_y_int]
    add  dx, 16
    mov  ax, [mush_x_int]
    add  ax, 2
    push ax
    push dx
    call GetTile
    push ax
    call IsTileSolid
    cmp  ax, 1
    je   .mush_land

    mov  ax, [mush_x_int]
    add  ax, 13
    push ax
    push dx
    call GetTile
    push ax
    call IsTileSolid
    cmp  ax, 1
    jne  .mush_bounds

.mush_land:
    and  dx, 0FFF0h
    sub  dx, 16
    mov  [mush_y_int], dx
    mov  byte [mush_y_frac], 0
    mov  word [mush_vy], 0

.mush_bounds:
    ; Disappear in pit or >32 px left of camera
    cmp  word [mush_y_int], PLAY_BOTTOM_Y
    jge  .mush_kill
    mov  ax, [camera_x]
    sub  ax, 32
    cmp  [mush_x_int], ax
    jge  .done

.mush_kill:
    mov  byte [mush_active], 0

.done:
    pop  dx
    pop  ax
    ret

; -----------------------------------------------------------------------------
; Procedure: UpdateEnemies (Module 11, Section 5.5)
; Purpose:   Walks enemy_table and updates activation, movement, wall turning,
;            Koopa ledge turning, gravity, flat Goomba timer, shell grace timer,
;            sliding-shell kills (+500), enemy-vs-enemy turning, and cleanup.
; Inputs:    None
; Outputs:   Updates enemy_table and score (on shell kills)
; Registers: All preserved
; -----------------------------------------------------------------------------
UpdateEnemies:
    push ax
    push bx
    push cx
    push dx
    push si
    push di

    mov  si, enemy_table
    mov  cx, MAX_ENEMIES

.en_loop:
    mov  al, [si + EN_STATE]
    cmp  al, EST_DEAD
    jne  .not_dead
    jmp  .next_en
.not_dead:

    ; Check off-screen removal (pit or > 32 px left of camera)
    cmp  word [si + EN_Y_INT], PLAY_BOTTOM_Y
    jge  .kill_en
    mov  ax, [camera_x]
    sub  ax, 32
    cmp  [si + EN_X_INT], ax
    jl   .kill_en

    ; Dispatch by enemy state (reload AL since AX was used above)
    mov  al, [si + EN_STATE]
    cmp  al, EST_WAITING
    je   .do_waiting
    cmp  al, EST_FLAT
    je   .do_flat
    cmp  al, EST_SHELL_STILL
    je   .do_shell_still
    jmp  .do_moving                 ; EST_WALKING or EST_SHELL_SLIDE

.kill_en:
    mov  byte [si + EN_STATE], EST_DEAD
    jmp  .next_en

    ; --- EST_WAITING: stand still until within 1 tile (16 px) of screen ---
.do_waiting:
    mov  ax, [camera_x]
    add  ax, SCREEN_W + TILE_SIZE   ; camera_x + 336
    cmp  [si + EN_X_INT], ax
    jle  .activate_en
    jmp  .next_en
.activate_en:
    mov  byte [si + EN_STATE], EST_WALKING
    mov  byte [si + EN_DIR], 1      ; Start walking left
    cmp  byte [si + EN_TYPE], ENT_KOOPA
    je   .set_koopa_vx
    mov  word [si + EN_VX], -PHYS_GOOMBA_SPD
    jmp  .next_en
.set_koopa_vx:
    mov  word [si + EN_VX], -PHYS_KOOPA_SPD
    jmp  .next_en

    ; --- EST_FLAT: flattened Goomba stays ~1 s then removed ---
.do_flat:
    dec  byte [si + EN_TIMER]
    jz   .flat_done
    jmp  .next_en
.flat_done:
    mov  byte [si + EN_STATE], EST_DEAD
    jmp  .next_en

    ; --- EST_SHELL_STILL: count down grace timer, apply gravity ---
.do_shell_still:
    cmp  byte [si + EN_TIMER], 0
    je   .shell_still_grav
    dec  byte [si + EN_TIMER]
.shell_still_grav:
    jmp  .apply_en_grav

    ; --- EST_WALKING or EST_SHELL_SLIDE ---
.do_moving:
    ; Decrement shell grace timer if active
    cmp  byte [si + EN_TIMER], 0
    je   .move_en_x
    dec  byte [si + EN_TIMER]

.move_en_x:
    ; Horizontal move
    lea  ax, [si + EN_X_INT]
    push ax
    push word [si + EN_VX]
    call AddFixed88

    ; Check solid wall on leading edge
    cmp  byte [si + EN_DIR], 0
    jne  .en_chk_left_wall

    ; Moving right: check (x + 15, y + 8)
    mov  dx, [si + EN_X_INT]
    add  dx, 15
    push dx
    mov  ax, [si + EN_Y_INT]
    add  ax, 8
    push ax
    call GetTile
    push ax
    call IsTileSolid
    cmp  ax, 1
    jne  .en_chk_koopa_ledge
    and  dx, 0FFF0h
    sub  dx, 16
    mov  [si + EN_X_INT], dx
    mov  byte [si + EN_DIR], 1
    neg  word [si + EN_VX]
    jmp  short .apply_en_grav

.en_chk_left_wall:
    ; Moving left: check (x, y + 8)
    mov  dx, [si + EN_X_INT]
    push dx
    mov  ax, [si + EN_Y_INT]
    add  ax, 8
    push ax
    call GetTile
    push ax
    call IsTileSolid
    cmp  ax, 1
    jne  .en_chk_koopa_ledge
    and  dx, 0FFF0h
    add  dx, 16
    mov  [si + EN_X_INT], dx
    mov  byte [si + EN_DIR], 0
    neg  word [si + EN_VX]
    jmp  short .apply_en_grav

.en_chk_koopa_ledge:
    ; Section 5.5: Walking Koopa turns around at platform edges (Goomba walks off)
    cmp  byte [si + EN_TYPE], ENT_KOOPA
    jne  .apply_en_grav
    cmp  byte [si + EN_STATE], EST_WALKING
    jne  .apply_en_grav
    cmp  byte [si + EN_GROUND], 1
    jne  .apply_en_grav

    mov  dx, [si + EN_X_INT]
    cmp  byte [si + EN_DIR], 0
    jne  .koopa_probe_ok
    add  dx, 15                     ; Leading right corner
.koopa_probe_ok:
    push dx
    mov  ax, [si + EN_Y_INT]
    add  ax, 17                     ; Just below leading foot
    push ax
    call GetTile
    push ax
    call IsTileSolid
    cmp  ax, 1
    je   .apply_en_grav

    ; No solid floor ahead -> reverse Koopa direction!
    xor  byte [si + EN_DIR], 1
    neg  word [si + EN_VX]

.apply_en_grav:
    mov  ax, [si + EN_VY]
    add  ax, PHYS_GRAVITY
    cmp  ax, PHYS_MAX_FALL
    jle  .en_store_vy
    mov  ax, PHYS_MAX_FALL
.en_store_vy:
    mov  [si + EN_VY], ax

    lea  ax, [si + EN_Y_INT]
    push ax
    push word [si + EN_VY]
    call AddFixed88

    ; Floor check at (x+2, y+16) and (x+13, y+16)
    mov  dx, [si + EN_Y_INT]
    add  dx, 16

    mov  ax, [si + EN_X_INT]
    add  ax, 2
    push ax
    push dx
    call GetTile
    push ax
    call IsTileSolid
    cmp  ax, 1
    je   .en_landed

    mov  ax, [si + EN_X_INT]
    add  ax, 13
    push ax
    push dx
    call GetTile
    push ax
    call IsTileSolid
    cmp  ax, 1
    je   .en_landed

    mov  byte [si + EN_GROUND], 0
    jmp  short .en_vs_en

.en_landed:
    and  dx, 0FFF0h
    sub  dx, 16
    mov  [si + EN_Y_INT], dx
    mov  byte [si + EN_Y_FRAC], 0
    mov  word [si + EN_VY], 0
    mov  byte [si + EN_GROUND], 1

    ; --- Enemy-vs-Enemy: sliding shell kills (+500); walking enemies turn ---
.en_vs_en:
    mov  al, [si + EN_STATE]
    cmp  al, EST_WALKING
    je   .do_ev_loop
    cmp  al, EST_SHELL_SLIDE
    je   .do_ev_loop
    jmp  .next_en

.do_ev_loop:
    push cx                         ; save outer loop counter
    mov  di, enemy_table
    mov  cx, MAX_ENEMIES
.inner_ev:
    cmp  di, si                     ; Skip self
    je   .skip_inner_far
    mov  bl, [di + EN_STATE]
    cmp  bl, EST_DEAD
    je   .skip_inner_far
    cmp  bl, EST_FLAT
    jne  .do_ev_overlap
.skip_inner_far:
    jmp  .next_inner

.do_ev_overlap:
    ; Overlap test between enemy SI and enemy DI
    mov  ax, [si + EN_X_INT]
    inc  ax
    push ax
    mov  ax, [si + EN_Y_INT]
    inc  ax
    push ax
    mov  ax, 14
    push ax
    push ax
    mov  ax, [di + EN_X_INT]
    inc  ax
    push ax
    mov  ax, [di + EN_Y_INT]
    inc  ax
    push ax
    mov  ax, 14
    push ax
    push ax
    call CheckRectOverlap
    cmp  ax, 0
    je   .next_inner

    ; Collision detected!
    cmp  byte [si + EN_STATE], EST_SHELL_SLIDE
    jne  .walk_turn_en

    ; Sliding shell defeats Goomba or Koopa (+500 each, Section 5.4 & 5.5)
    mov  byte [di + EN_STATE], EST_DEAD
    mov  ax, 500
    push ax
    call AddScore

    push word [di + EN_X_INT]
    mov  ax, [di + EN_Y_INT]
    sub  ax, 8
    push ax
    mov  ax, str_pop_500
    push ax
    call SpawnPopup

    mov  ax, 1600
    push ax
    mov  ax, -120
    push ax
    mov  ax, 4
    push ax
    call PlaySound
    jmp  short .next_inner

.walk_turn_en:
    ; Walking enemy turns away from the other enemy
    mov  ax, [si + EN_X_INT]
    cmp  ax, [di + EN_X_INT]
    jg   .face_right
    ; SI is left of DI -> walk left
    cmp  byte [si + EN_DIR], 1
    je   .next_inner
    mov  byte [si + EN_DIR], 1
    neg  word [si + EN_VX]
    jmp  short .next_inner
.face_right:
    cmp  byte [si + EN_DIR], 0
    je   .next_inner
    mov  byte [si + EN_DIR], 0
    neg  word [si + EN_VX]

.next_inner:
    add  di, ENEMY_REC_SIZE
    dec  cx
    jz   .inner_done
    jmp  .inner_ev
.inner_done:
    pop  cx

.next_en:
    add  si, ENEMY_REC_SIZE
    dec  cx
    jz   .done
    jmp  .en_loop

.done:
    pop  di
    pop  si
    pop  dx
    pop  cx
    pop  bx
    pop  ax
    ret

; -----------------------------------------------------------------------------
; Procedure: ResolveEntityCollisions (Modules 10, 11 & 12, Section 5.6)
; Purpose:   Resolves Mario against Mushroom and active Enemies in exact
;            Section 5.6 order: mushroom collect, stationary shell kick (+400),
;            stomp test (moving down & prev feet <= en_y + 8), else side hurt.
; Inputs:    None
; Outputs:   Updates score, hero form/HP/bounce, enemy states, or triggers death
; Registers: All preserved
; -----------------------------------------------------------------------------
ResolveEntityCollisions:
    push ax
    push bx
    push cx
    push dx
    push si

    call GetHeroHeight
    mov  dx, ax                     ; DX = hero hitbox height (16 or 32)

    ; --- 1. Check Mario vs Mushroom ---
    cmp  byte [mush_active], 0
    je   .chk_enemies

    mov  ax, [hero_x_int]
    add  ax, HERO_HITBOX_XOFF
    push ax
    push word [hero_y_int]
    mov  ax, HERO_HITBOX_W
    push ax
    push dx
    mov  ax, [mush_x_int]
    inc  ax
    push ax
    mov  ax, [mush_y_int]
    inc  ax
    push ax
    mov  ax, 14
    push ax
    push ax
    call CheckRectOverlap
    cmp  ax, 0
    je   .chk_enemies

    mov  byte [mush_active], 0
    call MakeSuperMario
    call GetHeroHeight
    mov  dx, ax                     ; Refresh height after growing!

    ; --- 2. Check Mario vs Enemies ---
.chk_enemies:
    mov  si, enemy_table
    mov  cx, MAX_ENEMIES

.col_loop:
    cmp  byte [game_state], STATE_PLAYING
    je   .still_playing
    jmp  .done
.still_playing:

    mov  al, [si + EN_STATE]
    cmp  al, EST_DEAD
    je   .skip_col_far
    cmp  al, EST_FLAT
    je   .skip_col_far

    ; AABB overlap: Mario(x+2, y, 12, h) vs Enemy(x+1, y+2, 14, 14)
    mov  ax, [hero_x_int]
    add  ax, HERO_HITBOX_XOFF
    push ax
    push word [hero_y_int]
    mov  ax, HERO_HITBOX_W
    push ax
    push dx
    mov  ax, [si + EN_X_INT]
    inc  ax
    push ax
    mov  ax, [si + EN_Y_INT]
    add  ax, 2
    push ax
    mov  ax, 14
    push ax
    push ax
    call CheckRectOverlap
    cmp  ax, 0
    jne  .has_overlap
.skip_col_far:
    jmp  .next_col

.has_overlap:
    ; Overlap confirmed! Branch by enemy state
    mov  al, [si + EN_STATE]
    cmp  al, EST_SHELL_STILL
    je   .kick_shell

    ; If sliding shell still has kick grace frames, ignore Mario
    cmp  al, EST_SHELL_SLIDE
    jne  .test_stomp
    cmp  byte [si + EN_TIMER], 0
    jne  .skip_col_far

.test_stomp:
    ; Section 5.6 Stomp test:
    ;   hero_vy > 0 (moving down) AND hero_prev_feet_y <= en_y + 8
    cmp  word [hero_vy], 0
    jle  .go_side_far
    mov  ax, [si + EN_Y_INT]
    add  ax, 8
    cmp  [hero_prev_feet_y], ax
    jle  .do_stomp
.go_side_far:
    jmp  .side_contact

.do_stomp:
    ; --- STOMP SUCCEEDED! ---
    push word [si + EN_X_INT]
    mov  ax, [si + EN_Y_INT]
    sub  ax, 8
    push ax
    call AwardStompPoints

    ; Bounce Mario upward (higher if Space is held)
    mov  word [hero_vy], PHYS_STOMP_VEL
    cmp  byte [key_state + KEY_SPACE], 0
    je   .stomp_bounce_ok
    mov  word [hero_vy], PHYS_STOMP_HIGH
.stomp_bounce_ok:
    mov  byte [hero_on_ground], 0

    ; Stomp SFX
    mov  ax, 1900
    push ax
    mov  ax, -220
    push ax
    mov  ax, 4
    push ax
    call PlaySound

    ; Update stomped enemy state
    cmp  byte [si + EN_TYPE], ENT_GOOMBA
    jne  .stomp_koopa
    mov  byte [si + EN_STATE], EST_FLAT
    mov  byte [si + EN_TIMER], FLAT_GOOMBA_TICKS
    mov  word [si + EN_VX], 0
    jmp  short .next_col

.stomp_koopa:
    ; Walking Koopa or sliding shell -> stationary shell
    mov  byte [si + EN_STATE], EST_SHELL_STILL
    mov  byte [si + EN_TIMER], SHELL_GRACE_TICKS
    mov  word [si + EN_VX], 0
    jmp  short .next_col

.kick_shell:
    ; Stationary shell touched/stomped (if grace timer expired) -> kick (+400)
    cmp  byte [si + EN_TIMER], 0
    jne  .next_col

    mov  byte [si + EN_STATE], EST_SHELL_SLIDE
    mov  byte [si + EN_TIMER], SHELL_GRACE_TICKS  ; 8 frames ignoring Mario

    mov  ax, [hero_x_int]
    cmp  ax, [si + EN_X_INT]
    jle  .kick_right
    ; Mario is right of shell -> slide left
    mov  byte [si + EN_DIR], 1
    mov  word [si + EN_VX], -PHYS_SHELL_SPD
    jmp  short .kick_score
.kick_right:
    mov  byte [si + EN_DIR], 0
    mov  word [si + EN_VX], PHYS_SHELL_SPD
.kick_score:
    mov  ax, 400
    push ax
    call AddScore

    push word [si + EN_X_INT]
    mov  ax, [si + EN_Y_INT]
    sub  ax, 8
    push ax
    mov  ax, str_pop_400
    push ax
    call SpawnPopup

    mov  ax, 1500
    push ax
    mov  ax, -160
    push ax
    mov  ax, 5
    push ax
    call PlaySound
    jmp  short .next_col

.side_contact:
    call HurtMario
    call GetHeroHeight
    mov  dx, ax                     ; Refresh height if shrunk

.next_col:
    add  si, ENEMY_REC_SIZE
    dec  cx
    jz   .done
    jmp  .col_loop

.done:
    pop  si
    pop  dx
    pop  cx
    pop  bx
    pop  ax
    ret

; -----------------------------------------------------------------------------
; Procedure: DrawEntities (Modules 4, 6, 11, 12)
; Purpose:   Draws active Mushroom and visible Enemies (-16 < screen_x < 320).
; Inputs:    None
; Outputs:   Renders entities into frame_buf_seg
; Registers: All preserved
; -----------------------------------------------------------------------------
DrawEntities:
    push ax
    push bx
    push cx
    push dx
    push si

    ; --- Draw Mushroom if active and visible ---
    cmp  byte [mush_active], 0
    je   .draw_en_list

    mov  ax, [mush_x_int]
    sub  ax, [camera_x]
    cmp  ax, -16
    jle  .draw_en_list
    cmp  ax, SCREEN_W
    jge  .draw_en_list

    push ax
    push word [mush_y_int]
    mov  ax, spr_mushroom
    push ax
    xor  ax, ax
    push ax
    call DrawSprite

    ; --- Draw Enemies ---
.draw_en_list:
    mov  si, enemy_table
    mov  cx, MAX_ENEMIES
.drw_loop:
    mov  al, [si + EN_STATE]
    cmp  al, EST_DEAD
    je   .next_drw

    mov  bx, [si + EN_X_INT]
    sub  bx, [camera_x]             ; screen_x = world_x - camera_x
    cmp  bx, -16
    jle  .next_drw
    cmp  bx, SCREEN_W
    jge  .next_drw

    ; Select sprite pointer & flip flags
    xor  dx, dx                     ; flags = 0
    cmp  byte [si + EN_TYPE], ENT_GOOMBA
    jne  .drw_koopa

    cmp  al, EST_FLAT
    je   .drw_goomba_flat
    mov  ax, spr_goomba_walk
    ; Subtle Goomba waddle: mirror horizontally every 8 frames while walking
    cmp  al, EST_WALKING
    jne  .call_spr
    test word [frame_counter], 8
    jz   .call_spr
    or   dx, SPR_FLIP_X
    jmp  short .call_spr

.drw_goomba_flat:
    mov  ax, spr_goomba_flat
    jmp  short .call_spr

.drw_koopa:
    cmp  al, EST_SHELL_STILL
    je   .drw_shell
    cmp  al, EST_SHELL_SLIDE
    je   .drw_shell
    mov  ax, spr_koopa_walk
    cmp  byte [si + EN_DIR], 1      ; Facing left -> mirror horizontally
    jne  .call_spr
    or   dx, SPR_FLIP_X
    jmp  short .call_spr

.drw_shell:
    mov  ax, spr_koopa_shell

.call_spr:
    push bx                         ; screen_x
    push word [si + EN_Y_INT]       ; screen_y
    push ax                         ; sprite address
    push dx                         ; flags
    call DrawSprite

.next_drw:
    add  si, ENEMY_REC_SIZE
    dec  cx
    jz   .drw_done
    jmp  .drw_loop

.drw_done:
    pop  si
    pop  dx
    pop  cx
    pop  bx
    pop  ax
    ret

; -----------------------------------------------------------------------------
; Procedure: DrawHero (Module 4 & 6)
; Purpose:   Draws Mario at (hero_x_int - camera_x, hero_y_int) with correct
;            form (Small 16x16 / Super 16x32), pose (stand/run/jump), facing
;            mirror flag, and invulnerability blink flag.
; Inputs:    None
; Outputs:   Renders Mario into frame_buf_seg
; Registers: All preserved
; -----------------------------------------------------------------------------
DrawHero:
    push ax
    push bx
    push dx

    ; Build sprite flags in DX
    xor  dx, dx
    cmp  byte [hero_facing], 1
    jne  .chk_tall
    or   dx, SPR_FLIP_X
.chk_tall:
    cmp  byte [hero_form], 1
    jne  .chk_blink
    or   dx, SPR_TALL
.chk_blink:
    cmp  byte [hero_invuln], 0
    je   .sel_sprite
    or   dx, SPR_BLINK

.sel_sprite:
    cmp  byte [hero_form], 1
    je   .super_spr

    ; Small Mario sprites
    mov  al, [hero_pose]
    cmp  al, POSE_JUMP
    je   .s_jump
    cmp  al, POSE_RUN
    je   .s_run
    mov  ax, spr_mario_s_stand
    jmp  short .do_draw
.s_run:
    mov  ax, spr_mario_s_run
    jmp  short .do_draw
.s_jump:
    mov  ax, spr_mario_s_jump
    jmp  short .do_draw

.super_spr:
    ; Super Mario sprites (16x32)
    mov  al, [hero_pose]
    cmp  al, POSE_JUMP
    je   .l_jump
    cmp  al, POSE_RUN
    je   .l_run
    mov  ax, spr_mario_l_stand
    jmp  short .do_draw
.l_run:
    mov  ax, spr_mario_l_run
    jmp  short .do_draw
.l_jump:
    mov  ax, spr_mario_l_jump

.do_draw:
    mov  bx, [hero_x_int]
    sub  bx, [camera_x]             ; screen_x = world_x - camera_x
    push bx
    push word [hero_y_int]
    push ax
    push dx
    call DrawSprite

    pop  dx
    pop  bx
    pop  ax
    ret
