; =============================================================================
; File:        score.inc
; Project:     Super Mario Engine (EL2003 COAL Semester Project)
; Description: Module 13 - 32-bit score arithmetic (ADD/ADC, clamped to
;              999,999), coin counter & 100-coin 1-UP (lives clamped to 99),
;              stomp-chain scoring table, flag pole zone scoring, and bonus
;              floating score pop-ups.
; =============================================================================

; Pop-up record field offsets (8 bytes per record)
POP_ACTIVE          equ 0           ; byte (0/1)
POP_TIMER           equ 1           ; byte (frames remaining)
POP_X               equ 2           ; word (world X)
POP_Y               equ 4           ; word (world Y)
POP_STR             equ 6           ; word (pointer to string)

; Lookup table of pop-up text pointers matching stomp_points_tbl (indices 0..9)
stomp_str_tbl:
    dw str_pop_100, str_pop_200, str_pop_400, str_pop_500, str_pop_800
    dw str_pop_1000, str_pop_2000, str_pop_4000, str_pop_5000, str_pop_8000

; -----------------------------------------------------------------------------
; Procedure: AddScore (Module 13, Section 5.4)
; Purpose:   Adds unsigned 16-bit points to the 32-bit score (score_hi:score_lo)
;            using ADD and ADC, clamping the result to 999,999 (000F423Fh).
; Inputs:    [bp+4] = points to add (unsigned word)
; Outputs:   Updates score_lo and score_hi
; Registers: All preserved
; -----------------------------------------------------------------------------
AddScore:
    push bp
    mov  bp, sp
    push ax

    mov  ax, [bp+4]
    add  [score_lo], ax
    adc  word [score_hi], 0

    ; Clamp to 999,999 (SCORE_MAX_HI : SCORE_MAX_LO)
    cmp  word [score_hi], SCORE_MAX_HI
    jb   .done
    ja   .clamp
    cmp  word [score_lo], SCORE_MAX_LO
    jbe  .done

.clamp:
    mov  word [score_hi], SCORE_MAX_HI
    mov  word [score_lo], SCORE_MAX_LO

.done:
    pop  ax
    pop  bp
    ret  2

; -----------------------------------------------------------------------------
; Procedure: AddLife (Module 13, Section 5.4)
; Purpose:   Increments lives_count, clamped to MAX_LIVES (99).
; Inputs:    None
; Outputs:   Updates lives_count
; Registers: All preserved
; -----------------------------------------------------------------------------
AddLife:
    cmp  byte [lives_count], MAX_LIVES
    jae  .done
    inc  byte [lives_count]
.done:
    ret

; -----------------------------------------------------------------------------
; Procedure: AddCoin (Module 13, Section 5.4)
; Purpose:   Increments coins_count. Reaching 100 coins awards +1 life and
;            resets coins_count to 0 (no extra points).
; Inputs:    None
; Outputs:   Updates coins_count (and lives_count on 100 coins)
; Registers: All preserved
; -----------------------------------------------------------------------------
AddCoin:
    inc  byte [coins_count]
    cmp  byte [coins_count], 100
    jb   .done
    mov  byte [coins_count], 0
    call AddLife
    ; Play 1-UP chime
    push ax
    mov  ax, 900
    push ax
    mov  ax, -60
    push ax
    mov  ax, 8
    push ax
    call PlaySound
    pop  ax
.done:
    ret

; -----------------------------------------------------------------------------
; Procedure: AwardStompPoints (Section 5.4)
; Purpose:   Awards stomp-chain points (100..8000 for stomps 1..10, or 1-UP
;            for stomp 11+) and increments hero_stomp_chain.
; Inputs:    [bp+6] = world_x for score pop-up
;            [bp+4] = world_y for score pop-up
; Outputs:   Updates score/lives and hero_stomp_chain; spawns score pop-up
; Registers: All preserved
; -----------------------------------------------------------------------------
AwardStompPoints:
    push bp
    mov  bp, sp
    push ax
    push bx

    xor  bh, bh
    mov  bl, [hero_stomp_chain]
    cmp  bl, 10
    jae  .stomp_1up

    ; Index word tables by chain * 2
    shl  bx, 1
    mov  ax, [stomp_points_tbl + bx]
    push ax
    call AddScore

    ; Spawn floating score pop-up above stomped enemy
    push word [bp+6]
    push word [bp+4]
    push word [stomp_str_tbl + bx]
    call SpawnPopup

    inc  byte [hero_stomp_chain]
    jmp  short .done

.stomp_1up:
    call AddLife
    push word [bp+6]
    push word [bp+4]
    mov  ax, str_pop_1up
    push ax
    call SpawnPopup

.done:
    pop  bx
    pop  ax
    pop  bp
    ret  4

; -----------------------------------------------------------------------------
; Procedure: AwardFlagPoints (Section 5.4 & 5.11)
; Purpose:   Awards 100..5000 points based on the map row of Mario's feet when
;            he touches the flag pole (rows 2-3: 5000, 4: 2000, 5: 800,
;            6-7: 400, 8+: 100).
; Inputs:    None (reads hero_y_int, hero_form)
; Outputs:   Adds flag points to score and spawns pop-up
; Registers: All preserved
; -----------------------------------------------------------------------------
AwardFlagPoints:
    push ax
    push bx
    push cx

    ; Calculate row of Mario's bottom edge (feet_y = y + height - 1)
    mov  ax, [hero_y_int]
    add  ax, HERO_SMALL_H - 1
    cmp  byte [hero_form], 0
    je   .calc_row
    add  ax, 16                     ; Super Mario is 32 px tall
.calc_row:
    cmp  ax, 0
    jge  .shift_row
    xor  ax, ax
.shift_row:
    mov  cl, 4
    shr  ax, cl                     ; AX = map row (2..9)

    cmp  ax, 3
    jbe  .zone_top                  ; Rows 2..3 -> 5000
    cmp  ax, 4
    je   .zone_4                    ; Row 4 -> 2000
    cmp  ax, 5
    je   .zone_5                    ; Row 5 -> 800
    cmp  ax, 7
    jbe  .zone_67                   ; Rows 6..7 -> 400

    mov  ax, 100                    ; Row 8+ -> 100
    mov  bx, str_pop_100
    jmp  short .apply

.zone_top:
    mov  ax, 5000
    mov  bx, str_pop_5000
    jmp  short .apply

.zone_4:
    mov  ax, 2000
    mov  bx, str_pop_2000
    jmp  short .apply

.zone_5:
    mov  ax, 800
    mov  bx, str_pop_800
    jmp  short .apply

.zone_67:
    mov  ax, 400
    mov  bx, str_pop_400

.apply:
    push ax
    call AddScore

    push word [hero_x_int]
    mov  ax, [hero_y_int]
    sub  ax, 8
    push ax
    push bx
    call SpawnPopup

    pop  cx
    pop  bx
    pop  ax
    ret

; -----------------------------------------------------------------------------
; Procedure: ClearPopups
; Purpose:   Deactivates all floating score pop-ups (on level reset).
; Inputs:    None
; Outputs:   popup_table cleared
; Registers: All preserved
; -----------------------------------------------------------------------------
ClearPopups:
    push ax
    push cx
    push di
    push es

    push ds
    pop  es
    mov  di, popup_table
    xor  ax, ax
    mov  cx, (MAX_POPUPS * POPUP_REC_SIZE) / 2
    cld
    rep  stosw

    pop  es
    pop  di
    pop  cx
    pop  ax
    ret

; -----------------------------------------------------------------------------
; Procedure: SpawnPopup (Bonus - Floating Score Pop-Up)
; Purpose:   Creates a short-lived floating score label at (world_x, world_y).
; Inputs:    [bp+8] = world_x (word)
;            [bp+6] = world_y (word)
;            [bp+4] = pointer to score text string (word)
; Outputs:   Allocates a slot in popup_table
; Registers: All preserved
; -----------------------------------------------------------------------------
SpawnPopup:
    push bp
    mov  bp, sp
    push ax
    push bx
    push cx

    mov  bx, popup_table
    mov  cx, MAX_POPUPS
.find_slot:
    cmp  byte [bx + POP_ACTIVE], 0
    je   .use_slot
    add  bx, POPUP_REC_SIZE
    loop .find_slot

    ; All slots active: overwrite first slot
    mov  bx, popup_table

.use_slot:
    mov  byte [bx + POP_ACTIVE], 1
    mov  byte [bx + POP_TIMER], 24  ; 0.8 seconds at 30 FPS
    mov  ax, [bp+8]
    mov  [bx + POP_X], ax
    mov  ax, [bp+6]
    mov  [bx + POP_Y], ax
    mov  ax, [bp+4]
    mov  [bx + POP_STR], ax

    pop  cx
    pop  bx
    pop  ax
    pop  bp
    ret  6

; -----------------------------------------------------------------------------
; Procedure: UpdatePopups
; Purpose:   Floats active pop-ups upward and expires them when timer hits 0.
; Inputs:    None
; Outputs:   Updates popup_table
; Registers: All preserved
; -----------------------------------------------------------------------------
UpdatePopups:
    push bx
    push cx

    mov  bx, popup_table
    mov  cx, MAX_POPUPS
.upd_loop:
    cmp  byte [bx + POP_ACTIVE], 0
    je   .next
    dec  byte [bx + POP_TIMER]
    jz   .expire
    ; Float upward 1 pixel every odd timer tick
    test byte [bx + POP_TIMER], 1
    jz   .next
    dec  word [bx + POP_Y]
    jmp  short .next
.expire:
    mov  byte [bx + POP_ACTIVE], 0
.next:
    add  bx, POPUP_REC_SIZE
    loop .upd_loop

    pop  cx
    pop  bx
    ret

; -----------------------------------------------------------------------------
; Procedure: DrawPopups
; Purpose:   Renders all visible floating score pop-ups in screen coordinates.
; Inputs:    None
; Outputs:   Draws pop-up text into frame_buf_seg
; Registers: All preserved
; -----------------------------------------------------------------------------
DrawPopups:
    push ax
    push bx
    push cx

    mov  bx, popup_table
    mov  cx, MAX_POPUPS
.drw_loop:
    cmp  byte [bx + POP_ACTIVE], 0
    je   .next

    mov  ax, [bx + POP_X]
    sub  ax, [camera_x]
    cmp  ax, 0
    jl   .next
    cmp  ax, SCREEN_W - 32
    jg   .next

    cmp  word [bx + POP_Y], PLAY_TOP_Y
    jl   .next

    push ax
    push word [bx + POP_Y]
    push word [bx + POP_STR]
    mov  ax, COL_WHITE
    push ax
    call DrawString

.next:
    add  bx, POPUP_REC_SIZE
    loop .drw_loop

    pop  cx
    pop  bx
    pop  ax
    ret
