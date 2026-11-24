; =============================================================================
; File:        physics.inc
; Project:     Super Mario Engine (EL2003 COAL Semester Project)
; Description: Modules 9 & 10 - Fixed-point 16.8 + 8.8 arithmetic helper,
;              Mario movement & physics (walk, run, friction, braking, gravity,
;              ground-only jump, variable jump height, walking animation pose),
;              axis-separated tile collision, head-hit block triggering,
;              coin/flag overlap checks, and AABB rectangle overlap helper.
; =============================================================================

; -----------------------------------------------------------------------------
; Procedure: AddFixed88 (Module 9)
; Purpose:   Adds a signed 8.8 fixed-point velocity to a 16.8 position stored
;            as [word int, byte frac]. Handles negative two's-complement values
;            with full borrow/carry propagation across the 960-px level width.
; Inputs:    [bp+6] = address of (word int, byte frac)
;            [bp+4] = signed 8.8 velocity (word)
; Outputs:   Updates [ptr] (int) and [ptr+2] (frac) in memory
; Registers: All preserved
; -----------------------------------------------------------------------------
AddFixed88:
    push bp
    mov  bp, sp
    push ax
    push bx

    mov  bx, [bp+6]                 ; BX -> [int_word, frac_byte]
    mov  ax, [bp+4]                 ; AH = signed int delta, AL = frac delta
    add  [bx+2], al                 ; Add fractional byte; CF propagates below
    mov  al, ah
    cbw                             ; Sign-extend AH into AX (preserves CF!)
    adc  [bx], ax                   ; Add signed integer part + carry

    pop  bx
    pop  ax
    pop  bp
    ret  4

; -----------------------------------------------------------------------------
; Procedure: GetHeroHeight
; Purpose:   Returns Mario's current sprite/hitbox height (16 Small, 32 Super).
; Inputs:    None (reads hero_form)
; Outputs:   AX = 16 or 32
; Registers: AX returned; all others preserved
; -----------------------------------------------------------------------------
GetHeroHeight:
    mov  ax, HERO_SMALL_H
    cmp  byte [hero_form], 0
    je   .done
    mov  ax, HERO_SUPER_H
.done:
    ret

; -----------------------------------------------------------------------------
; Procedure: CheckRectOverlap (Module 10)
; Purpose:   Tests axis-aligned bounding box overlap between two rectangles.
; Inputs:    [bp+18] = x1, [bp+16] = y1, [bp+14] = w1, [bp+12] = h1
;            [bp+10] = x2, [bp+8]  = y2, [bp+6]  = w2, [bp+4]  = h2
; Outputs:   AX = 1 if overlapping, 0 otherwise
; Registers: AX returned; all others preserved
; -----------------------------------------------------------------------------
CheckRectOverlap:
    push bp
    mov  bp, sp
    push bx

    ; Check x1 < x2 + w2
    mov  ax, [bp+10]                ; x2
    add  ax, [bp+6]                 ; x2 + w2
    cmp  [bp+18], ax                ; x1 >= x2 + w2 ?
    jge  .no_hit

    ; Check x1 + w1 > x2
    mov  ax, [bp+18]                ; x1
    add  ax, [bp+14]                ; x1 + w1
    cmp  ax, [bp+10]                ; x1 + w1 <= x2 ?
    jle  .no_hit

    ; Check y1 < y2 + h2
    mov  ax, [bp+8]                 ; y2
    add  ax, [bp+4]                 ; y2 + h2
    cmp  [bp+16], ax                ; y1 >= y2 + h2 ?
    jge  .no_hit

    ; Check y1 + h1 > y2
    mov  ax, [bp+16]                ; y1
    add  ax, [bp+12]                ; y1 + h1
    cmp  ax, [bp+8]                 ; y1 + h1 <= y2 ?
    jle  .no_hit

    mov  ax, 1
    jmp  short .done

.no_hit:
    xor  ax, ax

.done:
    pop  bx
    pop  bp
    ret  16

; -----------------------------------------------------------------------------
; Procedure: CheckHeroSpecialTile (Internal helper for coin & flag overlap)
; Purpose:   Tests world point (px, py) for map Coin (11) or Flag Pole (12).
; Inputs:    [bp+6] = px (world X)
;            [bp+4] = py (world Y)
; Outputs:   Collects coin or triggers win sequence if touched
; Registers: All preserved
; -----------------------------------------------------------------------------
CheckHeroSpecialTile:
    push bp
    mov  bp, sp
    push ax
    push bx
    push cx

    push word [bp+6]
    push word [bp+4]
    call GetTile                    ; AX = tile code

    cmp  al, TILE_COIN
    jne  .chk_flag

    ; Collect free map coin: col = px >> 4, row = py >> 4
    mov  ax, [bp+6]
    mov  bx, [bp+4]
    mov  cl, 4
    shr  ax, cl
    shr  bx, cl
    push ax
    push bx
    call CollectMapCoin
    jmp  short .done

.chk_flag:
    cmp  al, TILE_FLAG
    jne  .done
    cmp  byte [game_state], STATE_PLAYING
    jne  .done
    call StartWinSequence

.done:
    pop  cx
    pop  bx
    pop  ax
    pop  bp
    ret  4

; -----------------------------------------------------------------------------
; Procedure: UpdateMario (Modules 9 & 10)
; Purpose:   Updates Mario's horizontal/vertical velocity from the key_state
;            table, applies gravity and variable jump height, moves Mario one
;            axis at a time with tile collision, checks head-hits, coins,
;            flag pole, and updates facing direction and animation pose.
; Inputs:    None (reads key_state, updates hero_* variables)
; Outputs:   Updated hero position, velocity, ground/pose state
; Registers: All preserved
; -----------------------------------------------------------------------------
UpdateMario:
    push ax
    push bx
    push cx
    push dx
    push si

    ; Save previous feet Y before vertical movement (for stomp test, Sec 5.6)
    call GetHeroHeight              ; AX = 16 or 32
    mov  si, ax                     ; SI = current hero_h throughout procedure
    add  ax, [hero_y_int]
    mov  [hero_prev_feet_y], ax

    ; =========================================================================
    ; 1. HORIZONTAL VELOCITY (Walk, Run, Brake, Friction)
    ; =========================================================================
    mov  bx, PHYS_MAX_WALK          ; Default max speed = 1.5 px/frame
    cmp  byte [key_state + KEY_LSHIFT], 0
    je   .spd_limit_ok
    mov  bx, PHYS_MAX_RUN           ; Shift held: max speed = 2.5 px/frame
.spd_limit_ok:

    mov  al, [key_state + KEY_RIGHT]
    mov  ah, [key_state + KEY_LEFT]

    ; Right pressed (and not Left)?
    cmp  al, 0
    je   .chk_left
    cmp  ah, 0
    jne  .apply_friction

    mov  byte [hero_facing], 0      ; Facing right
    mov  ax, [hero_vx]
    cmp  ax, 0
    jge  .accel_right
    ; Braking from leftward motion (faster than friction)
    add  ax, PHYS_BRAKE
    jmp  .store_vx

.accel_right:
    cmp  ax, bx                     ; Already faster than current max (Shift released)?
    jle  .do_inc_right
    sub  ax, PHYS_ACCEL             ; Smoothly slow down to walk cap
    cmp  ax, bx
    jge  .store_vx
    mov  ax, bx
    jmp  .store_vx

.do_inc_right:
    add  ax, PHYS_ACCEL
    cmp  ax, bx
    jle  .store_vx
    mov  ax, bx
    jmp  .store_vx

.chk_left:
    cmp  ah, 0
    je   .apply_friction

    mov  byte [hero_facing], 1      ; Facing left
    neg  bx                         ; BX = -max_speed
    mov  ax, [hero_vx]
    cmp  ax, 0
    jle  .accel_left
    ; Braking from rightward motion
    sub  ax, PHYS_BRAKE
    jmp  .store_vx

.accel_left:
    cmp  ax, bx                     ; Already faster left than -max_speed?
    jge  .do_dec_left
    add  ax, PHYS_ACCEL             ; Smoothly slow down to -walk cap
    cmp  ax, bx
    jle  .store_vx
    mov  ax, bx
    jmp  .store_vx

.do_dec_left:
    sub  ax, PHYS_ACCEL
    cmp  ax, bx
    jge  .store_vx
    mov  ax, bx
    jmp  .store_vx

.apply_friction:
    mov  ax, [hero_vx]
    cmp  ax, 0
    je   .store_vx
    jg   .fric_pos
    ; Moving left: add friction toward 0
    add  ax, PHYS_FRICTION
    cmp  ax, 0
    jle  .store_vx
    xor  ax, ax
    jmp  short .store_vx

.fric_pos:
    sub  ax, PHYS_FRICTION
    cmp  ax, 0
    jge  .store_vx
    xor  ax, ax

.store_vx:
    mov  [hero_vx], ax

    ; =========================================================================
    ; 2. JUMP, VARIABLE JUMP HEIGHT, AND GRAVITY
    ; =========================================================================
    cmp  byte [key_state + KEY_SPACE], 0
    jne  .space_held

    ; Space released: unlock jump & apply variable jump cut-off while rising
    mov  byte [hero_jump_lock], 0
    mov  ax, [hero_vy]
    cmp  ax, PHYS_JUMP_CUT          ; Signed compare: rising faster than -2.0?
    jge  .apply_gravity
    mov  word [hero_vy], PHYS_JUMP_CUT
    jmp  short .apply_gravity

.space_held:
    cmp  byte [hero_on_ground], 1
    jne  .apply_gravity
    cmp  byte [hero_jump_lock], 0
    jne  .apply_gravity

    ; Start jump from solid ground (no double jump)
    mov  word [hero_vy], PHYS_JUMP_VEL
    mov  byte [hero_on_ground], 0
    mov  byte [hero_jump_lock], 1
    ; Jump sound effect (rising pitch sweep)
    mov  ax, 2200
    push ax
    mov  ax, -150
    push ax
    mov  ax, 6
    push ax
    call PlaySound

.apply_gravity:
    mov  ax, [hero_vy]
    add  ax, PHYS_GRAVITY
    cmp  ax, PHYS_MAX_FALL
    jle  .store_vy
    mov  ax, PHYS_MAX_FALL
.store_vy:
    mov  [hero_vy], ax

    ; =========================================================================
    ; 3. HORIZONTAL MOVE + WALL TILE COLLISION (Axis 1)
    ; =========================================================================
    mov  ax, hero_x_int
    push ax
    push word [hero_vx]
    call AddFixed88

    ; Check solid walls along leading hitbox edge (left = x+2, right = x+13)
    mov  ax, [hero_vx]
    cmp  ax, 0
    jne  .has_vx
    jmp  .horiz_done
.has_vx:
    jl   .chk_wall_left

    ; --- Moving Right: test right edge (x + 13) at top, mid (if Super), bottom ---
    mov  dx, [hero_x_int]
    add  dx, 13                     ; Right hitbox X

    ; Top point (y + 2)
    push dx
    mov  ax, [hero_y_int]
    add  ax, 2
    push ax
    call GetTile
    push ax
    call IsTileSolid
    cmp  ax, 1
    je   .hit_wall_right

    ; Bottom point (y + h - 2)
    push dx
    mov  ax, [hero_y_int]
    add  ax, si
    sub  ax, 2
    push ax
    call GetTile
    push ax
    call IsTileSolid
    cmp  ax, 1
    je   .hit_wall_right

    ; Middle point (y + 16) for Super Mario
    cmp  si, HERO_SUPER_H
    jne  .no_wall_r
    push dx
    mov  ax, [hero_y_int]
    add  ax, 16
    push ax
    call GetTile
    push ax
    call IsTileSolid
    cmp  ax, 1
    je   .hit_wall_right
.no_wall_r:
    jmp  .horiz_done

.hit_wall_right:
    ; Snap flush against left face of tile: hero_x_int = (right & ~15) - 14
    and  dx, 0FFF0h
    sub  dx, 14
    mov  [hero_x_int], dx
    mov  byte [hero_x_frac], 0
    mov  word [hero_vx], 0
    jmp  .horiz_done

.chk_wall_left:
    ; --- Moving Left: test left edge (x + 2) at top, mid (if Super), bottom ---
    mov  dx, [hero_x_int]
    add  dx, 2                      ; Left hitbox X

    ; Top point (y + 2)
    push dx
    mov  ax, [hero_y_int]
    add  ax, 2
    push ax
    call GetTile
    push ax
    call IsTileSolid
    cmp  ax, 1
    je   .hit_wall_left

    ; Bottom point (y + h - 2)
    push dx
    mov  ax, [hero_y_int]
    add  ax, si
    sub  ax, 2
    push ax
    call GetTile
    push ax
    call IsTileSolid
    cmp  ax, 1
    je   .hit_wall_left

    ; Middle point (y + 16) for Super Mario
    cmp  si, HERO_SUPER_H
    jne  .horiz_done
    push dx
    mov  ax, [hero_y_int]
    add  ax, 16
    push ax
    call GetTile
    push ax
    call IsTileSolid
    cmp  ax, 1
    jne  .horiz_done

.hit_wall_left:
    ; Snap flush against right face of tile: hero_x_int = ((left & ~15) + 16) - 2
    and  dx, 0FFF0h
    add  dx, 14
    mov  [hero_x_int], dx
    mov  byte [hero_x_frac], 0
    mov  word [hero_vx], 0

.horiz_done:

    ; =========================================================================
    ; 4. VERTICAL MOVE + CEILING / FLOOR TILE COLLISION (Axis 2)
    ; =========================================================================
    mov  ax, hero_y_int
    push ax
    push word [hero_vy]
    call AddFixed88

    ; Keep Mario below HUD strip (y >= 16)
    cmp  word [hero_y_int], PLAY_TOP_Y
    jge  .chk_vert_dir
    mov  word [hero_y_int], PLAY_TOP_Y
    mov  byte [hero_y_frac], 0
    cmp  word [hero_vy], 0
    jge  .chk_vert_dir
    mov  word [hero_vy], 0

.chk_vert_dir:
    cmp  word [hero_vy], 0
    jl   .is_rising
    jmp  .chk_floor
.is_rising:

    ; --- Rising: Check Ceiling / Head-Hit (Section 5.6 & 5.9) ---
    mov  byte [hero_on_ground], 0
    mov  dx, [hero_y_int]           ; Top of Mario's head

    ; Check top-left (x + 2, y)
    mov  ax, [hero_x_int]
    add  ax, 2
    push ax
    push dx
    call GetTile
    push ax
    call IsTileSolid
    mov  bx, ax                     ; BX = 1 if left head hit solid

    ; Check top-right (x + 13, y)
    mov  ax, [hero_x_int]
    add  ax, 13
    push ax
    push dx
    call GetTile
    push ax
    call IsTileSolid
    or   ax, bx
    cmp  ax, 0
    jne  .head_hit_solid
    jmp  .vert_done

.head_hit_solid:
    ; Head hit a solid tile! Snap below tile & stop upward speed
    mov  cx, dx
    mov  ax, cx
    and  ax, 0FFF0h
    add  ax, TILE_SIZE
    mov  [hero_y_int], ax
    mov  byte [hero_y_frac], 0
    mov  word [hero_vy], 0

    ; Section 5.9: Prefer tile under horizontal centre of Mario's head (x + 8)
    mov  ax, [hero_x_int]
    add  ax, 8
    push ax
    push cx
    call GetTile
    push ax
    call IsTileSolid
    cmp  ax, 1
    je   .trig_center

    ; Fall back to whichever corner actually struck the solid tile
    cmp  bx, 1
    je   .trig_left
    mov  ax, [hero_x_int]
    add  ax, 13
    jmp  short .do_trigger

.trig_left:
    mov  ax, [hero_x_int]
    add  ax, 2
    jmp  short .do_trigger

.trig_center:
    mov  ax, [hero_x_int]
    add  ax, 8

.do_trigger:
    ; Convert (px=AX, py=CX) -> (col, row) and call TriggerBlock
    push cx
    mov  cl, 4
    shr  ax, cl                     ; col
    pop  dx                         ; py
    mov  cl, 4
    shr  dx, cl                     ; row
    push ax
    push dx
    call TriggerBlock
    jmp  short .vert_done

.chk_floor:
    ; --- Falling / Standing: Check Floor under Feet (y + height) ---
    mov  dx, [hero_y_int]
    add  dx, si                     ; DX = feet_y

    ; Check bottom-left (x + 2, feet_y)
    mov  ax, [hero_x_int]
    add  ax, 2
    push ax
    push dx
    call GetTile
    push ax
    call IsTileSolid
    cmp  ax, 1
    je   .landed

    ; Check bottom-right (x + 13, feet_y)
    mov  ax, [hero_x_int]
    add  ax, 13
    push ax
    push dx
    call GetTile
    push ax
    call IsTileSolid
    cmp  ax, 1
    je   .landed

    ; In mid-air
    mov  byte [hero_on_ground], 0
    jmp  short .vert_done

.landed:
    ; Snap feet flush on top of solid tile: hero_y_int = (feet_y & ~15) - hero_h
    and  dx, 0FFF0h
    sub  dx, si
    mov  [hero_y_int], dx
    mov  byte [hero_y_frac], 0
    mov  word [hero_vy], 0
    mov  byte [hero_on_ground], 1
    mov  byte [hero_stomp_chain], 0 ; Section 5.6: landing resets stomp chain!

.vert_done:

    ; =========================================================================
    ; 5. UPDATE POSE (Stand, Walking Animation, Jump)
    ; =========================================================================
    cmp  byte [hero_on_ground], 0
    jne  .pose_ground
    mov  byte [hero_pose], POSE_JUMP
    jmp  short .chk_specials

.pose_ground:
    cmp  word [hero_vx], 0
    je   .pose_stand
    ; Bonus walking animation: toggle run/stand every 4 frames while moving
    test word [frame_counter], 4
    jz   .pose_run
.pose_stand:
    mov  byte [hero_pose], POSE_STAND
    jmp  short .chk_specials
.pose_run:
    mov  byte [hero_pose], POSE_RUN

    ; =========================================================================
    ; 6. CHECK MAP COINS AND FLAG POLE OVERLAP
    ; =========================================================================
.chk_specials:
    ; Test 4 hitbox corners + torso center (covers both Small and Super Mario)
    mov  ax, [hero_x_int]
    add  ax, 3
    push ax
    mov  ax, [hero_y_int]
    add  ax, 2
    push ax
    call CheckHeroSpecialTile

    mov  ax, [hero_x_int]
    add  ax, 12
    push ax
    mov  ax, [hero_y_int]
    add  ax, 2
    push ax
    call CheckHeroSpecialTile

    mov  ax, [hero_x_int]
    add  ax, 3
    push ax
    mov  ax, [hero_y_int]
    add  ax, si
    sub  ax, 2
    push ax
    call CheckHeroSpecialTile

    mov  ax, [hero_x_int]
    add  ax, 12
    push ax
    mov  ax, [hero_y_int]
    add  ax, si
    sub  ax, 2
    push ax
    call CheckHeroSpecialTile

    cmp  si, HERO_SUPER_H
    jne  .done
    mov  ax, [hero_x_int]
    add  ax, 8
    push ax
    mov  ax, [hero_y_int]
    add  ax, 16
    push ax
    call CheckHeroSpecialTile

.done:
    pop  si
    pop  dx
    pop  cx
    pop  bx
    pop  ax
    ret
