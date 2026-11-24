; =============================================================================
; File:        gfx.inc
; Project:     Super Mario Engine (EL2003 COAL Semester Project)
; Description: Modules 2, 3, 4, 5, 6 & 13 - VGA Mode 13h setup, custom DAC
;              palette, off-screen frame buffer allocation/clear/flip using
;              REP STOSW / REP MOVSW, PutPixel, FillRect, DrawTile, DrawMap,
;              GetTile, SetTile, ResetMap (REP MOVSB), DrawSprite with
;              mirroring/clipping/blinking, parallax clouds/hills, bitmap font,
;              32-bit decimal number printer, HUD, and scrolling camera.
; =============================================================================

; -----------------------------------------------------------------------------
; Procedure: SetVideoMode
; Purpose:   Switches VGA video mode via BIOS INT 10h.
; Inputs:    [bp+4] = video mode (word, e.g. 0013h for 320x200x256, 0003h text)
; Outputs:   None
; Registers: All preserved
; -----------------------------------------------------------------------------
SetVideoMode:
    push bp
    mov  bp, sp
    push ax

    mov  ax, [bp+4]
    xor  ah, ah                     ; AH = 00h (Set Video Mode)
    int  10h

    pop  ax
    pop  bp
    ret  2

; -----------------------------------------------------------------------------
; Procedure: LoadCustomPalette
; Purpose:   Programs VGA DAC palette indices 16..45 with crisp Mario colours.
; Inputs:    None (reads custom_palette table)
; Outputs:   VGA DAC updated
; Registers: All preserved
; -----------------------------------------------------------------------------
LoadCustomPalette:
    push ax
    push cx
    push dx
    push si

    mov  dx, 03C8h                  ; VGA DAC Write Index register
    mov  al, PALETTE_START_IDX
    out  dx, al

    inc  dx                         ; 03C9h = VGA DAC Data register
    mov  si, custom_palette
    mov  cx, PALETTE_COUNT * 3      ; 3 bytes (R, G, B) per colour
    cld
.pal_loop:
    lodsb
    out  dx, al
    loop .pal_loop

    pop  si
    pop  dx
    pop  cx
    pop  ax
    ret

; -----------------------------------------------------------------------------
; Procedure: AllocFrameBuffer
; Purpose:   Shrinks .COM memory block to 64 KB and allocates a separate 64,000-
;            byte segment via DOS INT 21h AH=48h for flicker-free rendering.
; Inputs:    None
; Outputs:   frame_buf_seg = allocated segment, fb_allocated = 1 (or 0 fallback)
; Registers: All preserved
; -----------------------------------------------------------------------------
AllocFrameBuffer:
    push ax
    push bx
    push es

    ; Shrink current .COM segment to 64 KB (1000h paragraphs)
    mov  ax, cs
    mov  es, ax
    mov  bx, 1000h
    mov  ah, 4Ah                    ; DOS Resize Memory Block
    int  21h

    ; Request 4000 paragraphs (64,000 bytes) for off-screen frame buffer
    mov  bx, FB_PARAGRAPHS
    mov  ah, 48h                    ; DOS Allocate Memory Block
    int  21h
    jc   .fallback

    mov  [frame_buf_seg], ax
    mov  byte [fb_allocated], 1
    jmp  short .done

.fallback:
    ; Fallback: use free 64 KB segment directly above our .COM segment
    mov  ax, cs
    add  ax, 1000h
    mov  [frame_buf_seg], ax
    mov  byte [fb_allocated], 0

.done:
    pop  es
    pop  bx
    pop  ax
    ret

; -----------------------------------------------------------------------------
; Procedure: FreeFrameBuffer
; Purpose:   Releases the DOS-allocated frame buffer segment on exit.
; Inputs:    None
; Outputs:   fb_allocated = 0
; Registers: All preserved
; -----------------------------------------------------------------------------
FreeFrameBuffer:
    push ax
    push es

    cmp  byte [fb_allocated], 0
    je   .done

    mov  es, [frame_buf_seg]
    mov  ah, 49h                    ; DOS Free Memory Block
    int  21h
    mov  byte [fb_allocated], 0

.done:
    pop  es
    pop  ax
    ret

; -----------------------------------------------------------------------------
; Procedure: ClearFrameBuffer
; Purpose:   Fills the entire 320x200 off-screen buffer with one colour using
;            REP STOSW (Module 5).
; Inputs:    [bp+4] = colour (word, low byte used)
; Outputs:   Frame buffer cleared
; Registers: All preserved
; -----------------------------------------------------------------------------
ClearFrameBuffer:
    push bp
    mov  bp, sp
    push ax
    push cx
    push di
    push es

    mov  es, [frame_buf_seg]
    xor  di, di
    mov  al, [bp+4]
    mov  ah, al
    mov  cx, FB_WORDS               ; 32,000 words = 64,000 bytes
    cld
    rep  stosw

    pop  es
    pop  di
    pop  cx
    pop  ax
    pop  bp
    ret  2

; -----------------------------------------------------------------------------
; Procedure: ClearPlayfield
; Purpose:   Clears HUD strip (rows 0..15) to black, playfield (16..191) to
;            COL_SKY, and bottom strip (192..199) to black via REP STOSW.
; Inputs:    None
; Outputs:   Frame buffer initialized for gameplay frame
; Registers: All preserved
; -----------------------------------------------------------------------------
ClearPlayfield:
    push ax
    push cx
    push di
    push es

    mov  es, [frame_buf_seg]
    xor  di, di
    cld

    ; Top 16 rows (HUD strip): black
    xor  ax, ax
    mov  cx, 2560                   ; 16 * 320 / 2
    rep  stosw

    ; Playfield rows 16..191: sky blue
    mov  al, COL_SKY
    mov  ah, al
    mov  cx, 28160                  ; 176 * 320 / 2
    rep  stosw

    ; Bottom 8 rows (192..199): stay black per Appendix A.1
    xor  ax, ax
    mov  cx, 1280                   ; 8 * 320 / 2
    rep  stosw

    pop  es
    pop  di
    pop  cx
    pop  ax
    ret

; -----------------------------------------------------------------------------
; Procedure: FlipFrameBuffer
; Purpose:   Copies the 64,000-byte off-screen frame buffer to VGA video memory
;            (0A000h:0000) in one block move using REP MOVSW (Module 5).
; Inputs:    None
; Outputs:   Screen updated without flicker
; Registers: All preserved
; -----------------------------------------------------------------------------
FlipFrameBuffer:
    push ax
    push cx
    push si
    push di
    push ds
    push es

    mov  ax, VGA_SEG
    mov  es, ax
    mov  ds, [frame_buf_seg]
    xor  si, si
    xor  di, di
    mov  cx, FB_WORDS               ; 32,000 words
    cld
    rep  movsw

    pop  es
    pop  ds
    pop  di
    pop  si
    pop  cx
    pop  ax
    ret

; -----------------------------------------------------------------------------
; Procedure: PutPixel (Module 2)
; Purpose:   Plots a single pixel in the off-screen frame buffer.
;            Computes offset y*320 + x using shifts: (y << 8) + (y << 6) + x.
; Inputs:    [bp+8] = x (word, signed)
;            [bp+6] = y (word, signed)
;            [bp+4] = colour (word, low byte)
; Outputs:   Writes pixel to frame_buf_seg
; Registers: All preserved
; -----------------------------------------------------------------------------
PutPixel:
    push bp
    mov  bp, sp
    push ax
    push bx
    push cx
    push di
    push es

    mov  ax, [bp+8]                 ; x
    cmp  ax, 0
    jl   .out_of_bounds
    cmp  ax, SCREEN_W
    jge  .out_of_bounds

    mov  bx, [bp+6]                 ; y
    cmp  bx, 0
    jl   .out_of_bounds
    cmp  bx, SCREEN_H
    jge  .out_of_bounds

    ; Offset = (y << 8) + (y << 6) + x
    mov  di, bx
    mov  cl, 8
    shl  di, cl                     ; DI = y * 256
    mov  cl, 6
    shl  bx, cl                     ; BX = y * 64
    add  di, bx                     ; DI = y * 320
    add  di, ax                     ; DI = y * 320 + x

    mov  es, [frame_buf_seg]
    mov  al, [bp+4]
    mov  [es:di], al

.out_of_bounds:
    pop  es
    pop  di
    pop  cx
    pop  bx
    pop  ax
    pop  bp
    ret  6

; -----------------------------------------------------------------------------
; Procedure: FillRect (Module 2)
; Purpose:   Draws a clipped filled rectangle into the off-screen frame buffer.
;            Uses shift arithmetic for row address and REP STOSB per scanline.
; Inputs:    [bp+12] = x (signed word)
;            [bp+10] = y (signed word)
;            [bp+8]  = width (word)
;            [bp+6]  = height (word)
;            [bp+4]  = colour (word, low byte)
; Outputs:   Draws rectangle to frame_buf_seg
; Registers: All preserved
; -----------------------------------------------------------------------------
FillRect:
    push bp
    mov  bp, sp
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push es

    mov  ax, [bp+12]                ; x1
    mov  bx, [bp+10]                ; y1
    mov  cx, [bp+8]                 ; w
    mov  dx, [bp+6]                 ; h

    cmp  cx, 0
    jle  .clip_exit
    cmp  dx, 0
    jle  .clip_exit

    ; Clip left edge (x1 < 0)
    cmp  ax, 0
    jge  .chk_right
    add  cx, ax                     ; reduce width
    xor  ax, ax
    cmp  cx, 0
    jle  .clip_exit

.chk_right:
    cmp  ax, SCREEN_W
    jge  .clip_exit
    mov  si, ax
    add  si, cx
    cmp  si, SCREEN_W
    jle  .chk_top
    mov  cx, SCREEN_W
    sub  cx, ax                     ; clamp width to right edge

.chk_top:
    cmp  bx, 0
    jge  .chk_bottom
    add  dx, bx                     ; reduce height
    xor  bx, bx
    cmp  dx, 0
    jle  .clip_exit

.chk_bottom:
    cmp  bx, SCREEN_H
    jge  .clip_exit
    mov  si, bx
    add  si, dx
    cmp  si, SCREEN_H
    jle  .do_draw
    mov  dx, SCREEN_H
    sub  dx, bx                     ; clamp height to bottom edge
    jmp  short .do_draw

.clip_exit:
    jmp  .done

.do_draw:
    ; Compute starting offset = (y << 8) + (y << 6) + x
    mov  di, bx
    mov  si, bx
    push cx                         ; save clipped width
    mov  cl, 8
    shl  di, cl
    mov  cl, 6
    shl  si, cl
    add  di, si
    add  di, ax                     ; DI = start offset
    pop  bx                         ; BX = clipped width

    mov  es, [frame_buf_seg]
    mov  al, [bp+4]                 ; colour
    mov  si, SCREEN_W
    sub  si, bx                     ; SI = stride to next row
    cld

.row_loop:
    mov  cx, bx
    rep  stosb
    add  di, si
    dec  dx
    jnz  .row_loop

.done:
    pop  es
    pop  di
    pop  si
    pop  dx
    pop  cx
    pop  bx
    pop  ax
    pop  bp
    ret  10

; -----------------------------------------------------------------------------
; Procedure: DrawBorderBox
; Purpose:   Draws a filled UI panel with a 2-pixel coloured border.
; Inputs:    [bp+14] = x, [bp+12] = y, [bp+10] = w, [bp+8] = h,
;            [bp+6] = fill colour, [bp+4] = border colour
; Outputs:   Draws bordered box into frame buffer
; Registers: All preserved
; -----------------------------------------------------------------------------
DrawBorderBox:
    push bp
    mov  bp, sp
    push ax

    ; Outer border rectangle
    push word [bp+14]
    push word [bp+12]
    push word [bp+10]
    push word [bp+8]
    push word [bp+4]
    call FillRect

    ; Inner filled rectangle (inset by 2 px on all sides)
    mov  ax, [bp+14]
    add  ax, 2
    push ax
    mov  ax, [bp+12]
    add  ax, 2
    push ax
    mov  ax, [bp+10]
    sub  ax, 4
    push ax
    mov  ax, [bp+8]
    sub  ax, 4
    push ax
    push word [bp+6]
    call FillRect

    pop  ax
    pop  bp
    ret  12

; -----------------------------------------------------------------------------
; Procedure: ResetMap (Module 3)
; Purpose:   Restores level_map_work from level_map_orig in one REP MOVSB.
; Inputs:    None
; Outputs:   level_map_work matches level_map_orig
; Registers: All preserved
; -----------------------------------------------------------------------------
ResetMap:
    push cx
    push si
    push di
    push es

    push ds
    pop  es
    mov  si, level_map_orig
    mov  di, level_map_work
    mov  cx, LEVEL_MAP_BYTES
    cld
    rep  movsb

    pop  es
    pop  di
    pop  si
    pop  cx
    ret

; -----------------------------------------------------------------------------
; Procedure: GetTile (Module 3)
; Purpose:   Converts world pixel coordinates (px, py) to the tile code in
;            level_map_work. Used by all collision and block logic.
; Inputs:    [bp+6] = px (signed world pixel X)
;            [bp+4] = py (signed world pixel Y)
; Outputs:   AX = tile code (0..15)
; Registers: AX returned; all other registers preserved
; -----------------------------------------------------------------------------
GetTile:
    push bp
    mov  bp, sp
    push bx
    push cx
    push dx

    mov  ax, [bp+6]                 ; px
    cmp  ax, 0
    jl   .ret_solid_wall
    cmp  ax, LEVEL_WIDTH_PX
    jge  .ret_solid_wall

    mov  bx, [bp+4]                 ; py
    cmp  bx, PLAY_TOP_Y
    jl   .ret_sky
    cmp  bx, PLAY_BOTTOM_Y
    jge  .ret_sky

    ; col = px >> 4, row = py >> 4
    mov  cl, 4
    shr  ax, cl                     ; AX = col (0..59)
    shr  bx, cl                     ; BX = row (1..11)

    ; index = row * 60 + col
    mov  dx, LEVEL_COLS
    push ax                         ; save col
    mov  ax, bx
    mul  dl                         ; AX = row * 60
    pop  bx                         ; BX = col
    add  bx, ax                     ; BX = row * 60 + col

    xor  ah, ah
    mov  al, [level_map_work + bx]
    jmp  short .done

.ret_solid_wall:
    mov  ax, TILE_GROUND
    jmp  short .done

.ret_sky:
    xor  ax, ax                     ; TILE_SKY (non-solid)

.done:
    pop  dx
    pop  cx
    pop  bx
    pop  bp
    ret  4

; -----------------------------------------------------------------------------
; Procedure: SetTile (Module 3 & 12)
; Purpose:   Writes a tile code into level_map_work at (col, row).
; Inputs:    [bp+8] = col (0..59)
;            [bp+6] = row (0..11)
;            [bp+4] = tile code (word, low byte used)
; Outputs:   Modifies level_map_work
; Registers: All preserved
; -----------------------------------------------------------------------------
SetTile:
    push bp
    mov  bp, sp
    push ax
    push bx
    push dx

    mov  bx, [bp+8]                 ; col
    cmp  bx, 0
    jl   .done
    cmp  bx, LEVEL_COLS
    jge  .done

    mov  ax, [bp+6]                 ; row
    cmp  ax, 0
    jl   .done
    cmp  ax, LEVEL_ROWS
    jge  .done

    mov  dl, LEVEL_COLS
    mul  dl                         ; AX = row * 60
    add  bx, ax
    mov  al, [bp+4]
    mov  [level_map_work + bx], al

.done:
    pop  dx
    pop  bx
    pop  ax
    pop  bp
    ret  6

; -----------------------------------------------------------------------------
; Procedure: IsTileSolid (Module 10)
; Purpose:   Checks whether a tile code blocks movement.
; Inputs:    [bp+4] = tile code (word)
; Outputs:   AX = 1 if solid (codes 1..10), 0 otherwise
; Registers: AX returned; all other registers preserved
; -----------------------------------------------------------------------------
IsTileSolid:
    push bp
    mov  bp, sp

    mov  ax, [bp+4]
    cmp  al, TILE_GROUND            ; 1
    jb   .not_solid
    cmp  al, TILE_STAIR             ; 10
    ja   .not_solid
    mov  ax, 1
    jmp  short .done

.not_solid:
    xor  ax, ax

.done:
    pop  bp
    ret  2

; -----------------------------------------------------------------------------
; Procedure: DrawTile (Module 3)
; Purpose:   Draws a 16x16 tile at (screen_x, screen_y) with left/right screen
;            clipping and colour-0 transparency (for coins, flagpole, etc.).
; Inputs:    [bp+8] = screen_x (signed word, -15..319)
;            [bp+6] = screen_y (word, 0..176)
;            [bp+4] = tile code (word)
; Outputs:   Draws tile pixels into frame_buf_seg
; Registers: All preserved
; -----------------------------------------------------------------------------
DrawTile:
    push bp
    mov  bp, sp
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push es

    mov  al, [bp+4]                 ; tile code
    cmp  al, TILE_SKY
    jne  .not_sky
    jmp  .done
.not_sky:

    ; Map tile code -> bitmap pointer in SI
    cmp  al, TILE_GROUND
    jne  .chk_brick
    cmp  word [bp+6], 160           ; Row 10 (y=160) gets grass top; Row 11 gets dirt
    je   .use_gnd_top
    mov  si, tile_ground_sub
    jmp  .have_bmp
.use_gnd_top:
    mov  si, tile_ground_top
    jmp  .have_bmp

.chk_brick:
    cmp  al, TILE_BRICK
    jne  .chk_q
    mov  si, tile_brick
    jmp  .have_bmp

.chk_q:
    cmp  al, TILE_Q_COIN
    je   .use_q
    cmp  al, TILE_Q_MUSH
    jne  .chk_empty
.use_q:
    mov  si, tile_q_block
    jmp  .have_bmp

.chk_empty:
    cmp  al, TILE_EMPTY
    jne  .chk_ptl
    mov  si, tile_empty
    jmp  .have_bmp

.chk_ptl:
    cmp  al, TILE_PIPE_TL
    jne  .chk_ptr
    mov  si, tile_pipe_tl
    jmp  .have_bmp

.chk_ptr:
    cmp  al, TILE_PIPE_TR
    jne  .chk_pbl
    mov  si, tile_pipe_tr
    jmp  .have_bmp

.chk_pbl:
    cmp  al, TILE_PIPE_BL
    jne  .chk_pbr
    mov  si, tile_pipe_bl
    jmp  .have_bmp

.chk_pbr:
    cmp  al, TILE_PIPE_BR
    jne  .chk_stair
    mov  si, tile_pipe_br
    jmp  .have_bmp

.chk_stair:
    cmp  al, TILE_STAIR
    jne  .chk_coin
    mov  si, tile_stair
    jmp  .have_bmp

.chk_coin:
    cmp  al, TILE_COIN
    jne  .chk_flag
    mov  si, tile_coin
    jmp  .have_bmp

.chk_flag:
    cmp  al, TILE_FLAG
    jne  .chk_cwall
    cmp  word [bp+6], 32            ; Row 2 (y=32) draws flag top & pennant
    je   .use_flag_top
    mov  si, tile_flag_pole
    jmp  .have_bmp
.use_flag_top:
    mov  si, tile_flag_top
    jmp  .have_bmp

.chk_cwall:
    cmp  al, TILE_CASTLE_WALL
    jne  .chk_ctop
    mov  si, tile_castle_wall
    jmp  .have_bmp

.chk_ctop:
    cmp  al, TILE_CASTLE_TOP
    jne  .chk_cdoor
    mov  si, tile_castle_top
    jmp  .have_bmp

.chk_cdoor:
    cmp  al, TILE_CASTLE_DOOR
    jne  .done
    mov  si, tile_castle_door

.have_bmp:
    ; Horizontal clipping: compute skip_left (AX), visible_w (BX), dest_x (DX)
    mov  dx, [bp+8]                 ; screen_x
    cmp  dx, SCREEN_W
    jge  .done
    cmp  dx, -15
    jl   .done

    xor  ax, ax                     ; skip_left = 0
    mov  bx, TILE_SIZE              ; visible_w = 16

    cmp  dx, 0
    jge  .clip_r
    ; screen_x < 0: skip (-screen_x) pixels on left
    mov  ax, dx
    neg  ax                         ; skip_left = -screen_x
    sub  bx, ax                     ; visible_w = 16 - skip_left
    xor  dx, dx                     ; dest_x = 0
    jmp  short .start_blit

.clip_r:
    mov  cx, dx
    add  cx, TILE_SIZE
    cmp  cx, SCREEN_W
    jle  .start_blit
    mov  bx, SCREEN_W
    sub  bx, dx                     ; visible_w = 320 - screen_x

.start_blit:
    ; Compute DI = (screen_y << 8) + (screen_y << 6) + dest_x
    mov  di, [bp+6]
    push bx                         ; save visible_w
    mov  bx, di
    mov  cl, 8
    shl  di, cl
    mov  cl, 6
    shl  bx, cl
    add  di, bx
    add  di, dx                     ; DI = start framebuffer offset
    pop  bx                         ; BX = visible_w

    add  si, ax                     ; Advance source ptr by skip_left
    mov  es, [frame_buf_seg]
    mov  dx, TILE_SIZE              ; 16 rows remaining
    mov  ah, TILE_SIZE
    sub  ah, bl                     ; AH = source row skip (16 - visible_w)

.row_loop:
    mov  cx, bx                     ; visible_w pixels this row
.col_loop:
    mov  al, [si]
    inc  si
    test al, al                     ; Skip colour 0 (transparent)
    jz   .skip_px
    mov  [es:di], al
.skip_px:
    inc  di
    loop .col_loop

    ; Advance SI to next tile row, DI to next screen scanline
    push ax
    xor  al, al
    xchg al, ah                     ; AX = src_row_skip
    add  si, ax
    pop  ax

    add  di, SCREEN_W
    sub  di, bx

    dec  dx
    jnz  .row_loop

.done:
    pop  es
    pop  di
    pop  si
    pop  dx
    pop  cx
    pop  bx
    pop  ax
    pop  bp
    ret  6

; -----------------------------------------------------------------------------
; Procedure: DrawParallaxBG (Bonus - Half-Speed Clouds and Bushes)
; Purpose:   Draws decorative clouds and distant green hills scrolling at half
;            the camera speed (camera_x >> 1) for visual depth.
; Inputs:    None (reads camera_x)
; Outputs:   Draws background scenery into frame_buf_seg
; Registers: All preserved
; -----------------------------------------------------------------------------
DrawParallaxBG:
    push ax
    push bx
    push cx
    push dx
    push si

    ; Parallax offset = camera_x / 2
    mov  bx, [camera_x]
    shr  bx, 1

    ; Draw 5 clouds spaced every 130 pixels in parallax space
    mov  cx, 5
    mov  ax, 36                     ; First cloud parallax X
.cloud_loop:
    mov  dx, ax
    sub  dx, bx                     ; Screen X of cloud
    cmp  dx, -48
    jl   .next_cloud
    cmp  dx, SCREEN_W
    jge  .next_cloud

    ; Cloud underside shadow
    push dx
    mov  si, 36
    push si
    mov  si, 40
    push si
    mov  si, 10
    push si
    mov  si, COL_CLOUD_SHD
    push si
    call FillRect

    ; Cloud main body
    push dx
    mov  si, 34
    push si
    mov  si, 40
    push si
    mov  si, 10
    push si
    mov  si, COL_WHITE
    push si
    call FillRect

    ; Cloud upper puff
    mov  si, dx
    add  si, 8
    push si
    mov  si, 26
    push si
    mov  si, 24
    push si
    mov  si, 10
    push si
    mov  si, COL_WHITE
    push si
    call FillRect

.next_cloud:
    add  ax, 130
    loop .cloud_loop

    ; Draw 5 distant hills/bushes near ground line (y = 148..160)
    mov  cx, 5
    mov  ax, 24
.bush_loop:
    mov  dx, ax
    sub  dx, bx
    cmp  dx, -48
    jl   .next_bush
    cmp  dx, SCREEN_W
    jge  .next_bush

    ; Base bush rectangle
    push dx
    mov  si, 152
    push si
    mov  si, 44
    push si
    mov  si, 8
    push si
    mov  si, COL_GRASS
    push si
    call FillRect

    ; Rounded bush crest
    mov  si, dx
    add  si, 8
    push si
    mov  si, 146
    push si
    mov  si, 28
    push si
    mov  si, 6
    push si
    mov  si, COL_GRASS_HI
    push si
    call FillRect

.next_bush:
    add  ax, 136
    loop .bush_loop

    pop  si
    pop  dx
    pop  cx
    pop  bx
    pop  ax
    ret

; -----------------------------------------------------------------------------
; Procedure: DrawMap (Modules 3 & 6)
; Purpose:   Draws the 20 visible tile columns + 1 partial edge column starting
;            at first_col = camera_x >> 4 and screen_x = -(camera_x & 15).
; Inputs:    None (reads camera_x and level_map_work)
; Outputs:   Renders visible level tiles to frame_buf_seg
; Registers: All preserved
; -----------------------------------------------------------------------------
DrawMap:
    push ax
    push bx
    push cx
    push dx
    push si
    push di

    ; first_col = camera_x >> 4
    mov  ax, [camera_x]
    mov  dx, ax
    mov  cl, 4
    shr  ax, cl                     ; AX = first_col (0..40)

    ; start_sx = -(camera_x & 15)
    and  dx, 0Fh
    neg  dx                         ; DX = start screen_x (-15..0)

    mov  si, 1                      ; Map row 1..11 (skip row 0 = HUD)
.row_loop:
    cmp  si, LEVEL_ROWS
    jge  .done

    ; Compute row base offset in level_map_work: DI = row * 60
    push ax                         ; save first_col
    mov  ax, si
    mov  bl, LEVEL_COLS
    mul  bl                         ; AX = row * 60
    mov  di, ax
    pop  ax

    ; Compute screen_y = row * 16 in BX
    mov  bx, si
    mov  cl, 4
    shl  bx, cl                     ; BX = screen_y

    ; Walk 21 columns (20 full + 1 partial right column)
    push ax                         ; save first_col
    push dx                         ; save start_sx
    push si                         ; save row index
    mov  cx, VIS_COLS + 1           ; 21 columns
.col_loop:
    cmp  ax, LEVEL_COLS
    jge  .next_col

    ; Fetch tile code from level_map_work[row*60 + col]
    mov  si, di
    add  si, ax
    push ax                         ; save col
    xor  ah, ah
    mov  al, [level_map_work + si]
    cmp  al, TILE_SKY
    je   .tile_skipped

    ; DrawTile(screen_x=DX, screen_y=BX, tile_code=AX)
    push dx
    push bx
    push ax
    call DrawTile

.tile_skipped:
    pop  ax                         ; restore col
.next_col:
    inc  ax
    add  dx, TILE_SIZE
    loop .col_loop

    pop  si
    pop  dx
    pop  ax
    inc  si
    jmp  short .row_loop

.done:
    pop  di
    pop  si
    pop  dx
    pop  cx
    pop  bx
    pop  ax
    ret

; -----------------------------------------------------------------------------
; Procedure: DrawSprite (Module 4)
; Purpose:   Draws a 16x16 or 16x32 sprite with colour-0 transparency,
;            horizontal mirroring, blinking (invulnerability), and full
;            clipping on all 4 edges (x: 0..319, y: 0..191).
; Inputs:    [bp+10] = screen_x (signed word)
;            [bp+8]  = screen_y (signed word)
;            [bp+6]  = sprite bitmap address (word)
;            [bp+4]  = flags (SPR_FLIP_X=1, SPR_TALL=2, SPR_BLINK=4)
; Outputs:   Draws sprite pixels into frame_buf_seg
; Registers: All preserved
; -----------------------------------------------------------------------------
DrawSprite:
    push bp
    mov  bp, sp
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push es

    ; Blink check: if SPR_BLINK set and (frame_counter & 2) != 0, skip draw
    mov  ax, [bp+4]
    test ax, SPR_BLINK
    jz   .no_blink
    test word [frame_counter], 2
    jnz  .early_exit
.no_blink:

    ; Determine sprite height (16 or 32)
    mov  dx, 16
    test word [bp+4], SPR_TALL
    jz   .height_ok
    mov  dx, 32
.height_ok:
    ; Fast-reject completely off-screen sprites
    mov  ax, [bp+10]                ; sx
    cmp  ax, SCREEN_W
    jge  .early_exit
    cmp  ax, -16
    jle  .early_exit

    mov  ax, [bp+8]                 ; sy
    cmp  ax, PLAY_BOTTOM_Y
    jge  .early_exit
    add  ax, dx                     ; sy + height
    cmp  ax, 0
    jg   .on_screen
.early_exit:
    jmp  .done

.on_screen:

    mov  es, [frame_buf_seg]
    mov  si, [bp+6]                 ; sprite source pointer
    mov  bx, [bp+8]                 ; current py = sy

.spr_row_loop:
    ; Clip Y to [0, PLAY_BOTTOM_Y - 1] (0..191; bottom 8 px stay black)
    cmp  bx, 0
    jl   .skip_entire_row
    cmp  bx, PLAY_BOTTOM_Y
    jge  .done                      ; Below playfield -> all remaining rows off-screen

    ; Compute row base offset DI = (py << 8) + (py << 6)
    mov  di, bx
    push bx
    mov  cl, 8
    shl  di, cl
    mov  cl, 6
    shl  bx, cl
    add  di, bx
    pop  bx

    ; Draw 16 pixels of this row (normal or mirrored)
    test word [bp+4], SPR_FLIP_X
    jnz  .row_mirrored

    ; --- Normal (left-to-right) row ---
    push dx                         ; save remaining rows
    mov  dx, [bp+10]                ; current px = sx
    mov  cx, 16
.col_norm:
    mov  al, [si]
    inc  si
    test al, al                     ; Colour 0 = transparent
    jz   .next_norm
    cmp  dx, 0
    jl   .next_norm
    cmp  dx, SCREEN_W
    jge  .next_norm
    push di
    add  di, dx
    mov  [es:di], al
    pop  di
.next_norm:
    inc  dx
    loop .col_norm
    pop  dx
    jmp  short .next_row

    ; --- Mirrored (right-to-left source) row ---
.row_mirrored:
    push dx                         ; save remaining rows
    push si                         ; save start of row
    add  si, 15                     ; start reading from rightmost pixel of row
    mov  dx, [bp+10]                ; current px = sx
    mov  cx, 16
.col_flip:
    mov  al, [si]
    dec  si
    test al, al                     ; Colour 0 = transparent
    jz   .next_flip
    cmp  dx, 0
    jl   .next_flip
    cmp  dx, SCREEN_W
    jge  .next_flip
    push di
    add  di, dx
    mov  [es:di], al
    pop  di
.next_flip:
    inc  dx
    loop .col_flip
    pop  si
    add  si, 16                     ; advance SI past this row
    pop  dx
    jmp  short .next_row

.skip_entire_row:
    add  si, 16

.next_row:
    inc  bx                         ; py++
    dec  dx                         ; rows remaining--
    jz   .done
    jmp  .spr_row_loop

.done:
    pop  es
    pop  di
    pop  si
    pop  dx
    pop  cx
    pop  bx
    pop  ax
    pop  bp
    ret  8

; -----------------------------------------------------------------------------
; Procedure: DrawHeart (Module 13)
; Purpose:   Draws an 8x8 filled or empty heart icon for the HUD health meter.
; Inputs:    [bp+8] = x (word)
;            [bp+6] = y (word)
;            [bp+4] = filled flag (1 = full red heart, 0 = empty grey heart)
; Outputs:   Draws heart into frame_buf_seg
; Registers: All preserved
; -----------------------------------------------------------------------------
DrawHeart:
    push bp
    mov  bp, sp
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push es

    mov  si, spr_heart_empty
    cmp  word [bp+4], 0
    je   .have_icon
    mov  si, spr_heart_full
.have_icon:
    ; DI = (y << 8) + (y << 6) + x
    mov  di, [bp+6]
    mov  bx, di
    mov  cl, 8
    shl  di, cl
    mov  cl, 6
    shl  bx, cl
    add  di, bx
    add  di, [bp+8]

    mov  es, [frame_buf_seg]
    mov  dx, 8                      ; 8 rows
.h_row:
    mov  cx, 8                      ; 8 cols
.h_col:
    mov  al, [si]
    inc  si
    test al, al
    jz   .h_skip
    mov  [es:di], al
.h_skip:
    inc  di
    loop .h_col
    add  di, SCREEN_W - 8
    dec  dx
    jnz  .h_row

    pop  es
    pop  di
    pop  si
    pop  dx
    pop  cx
    pop  bx
    pop  ax
    pop  bp
    ret  6

; -----------------------------------------------------------------------------
; Procedure: DrawChar (Module 13)
; Purpose:   Renders a single 8x8 character from font_8x8 into frame_buf_seg.
; Inputs:    [bp+10] = x (signed word)
;            [bp+8]  = y (signed word)
;            [bp+6]  = ASCII character (word, low byte)
;            [bp+4]  = colour (word, low byte)
; Outputs:   Draws glyph pixels into frame_buf_seg
; Registers: All preserved
; -----------------------------------------------------------------------------
DrawChar:
    push bp
    mov  bp, sp
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push es

    ; Validate glyph in ASCII 32..95 (convert lowercase a-z -> A-Z automatically)
    xor  ah, ah
    mov  al, [bp+6]
    cmp  al, 'a'
    jb   .chk_range
    cmp  al, 'z'
    ja   .chk_range
    sub  al, 32                     ; Uppercase conversion
.chk_range:
    cmp  al, 32
    jb   .done
    cmp  al, 95
    ja   .done

    sub  al, 32
    mov  cl, 3
    shl  ax, cl                     ; glyph offset = (char - 32) * 8
    mov  si, font_8x8
    add  si, ax

    ; Bounds check (reject if off-screen)
    mov  ax, [bp+10]                ; x
    cmp  ax, 0
    jl   .done
    cmp  ax, SCREEN_W - 8
    jg   .done
    mov  bx, [bp+8]                 ; y
    cmp  bx, 0
    jl   .done
    cmp  bx, SCREEN_H - 8
    jg   .done

    ; Compute DI = (y << 8) + (y << 6) + x
    mov  di, bx
    mov  cl, 8
    shl  di, cl
    mov  cl, 6
    shl  bx, cl
    add  di, bx
    add  di, ax

    mov  es, [frame_buf_seg]
    mov  bl, [bp+4]                 ; foreground colour
    mov  dx, 8                      ; 8 rows

.c_row:
    mov  al, [si]                   ; 8-bit row bitmap
    inc  si
    mov  cx, 8
.c_col:
    shl  al, 1                      ; Shift MSB into CF
    jnc  .c_skip
    mov  [es:di], bl
.c_skip:
    inc  di
    loop .c_col
    add  di, SCREEN_W - 8
    dec  dx
    jnz  .c_row

.done:
    pop  es
    pop  di
    pop  si
    pop  dx
    pop  cx
    pop  bx
    pop  ax
    pop  bp
    ret  8

; -----------------------------------------------------------------------------
; Procedure: DrawString (Module 13)
; Purpose:   Draws a zero-terminated ASCII string at (x, y) using DrawChar.
; Inputs:    [bp+10] = x (word)
;            [bp+8]  = y (word)
;            [bp+6]  = pointer to zero-terminated string (word)
;            [bp+4]  = colour (word)
; Outputs:   Draws text string into frame_buf_seg
; Registers: All preserved
; -----------------------------------------------------------------------------
DrawString:
    push bp
    mov  bp, sp
    push ax
    push bx
    push si

    mov  bx, [bp+10]                ; current x
    mov  si, [bp+6]                 ; string pointer
.str_loop:
    xor  ah, ah
    mov  al, [si]
    test al, al
    jz   .done
    inc  si

    push bx
    push word [bp+8]
    push ax
    push word [bp+4]
    call DrawChar

    add  bx, 8
    jmp  short .str_loop

.done:
    pop  si
    pop  bx
    pop  ax
    pop  bp
    ret  8

; -----------------------------------------------------------------------------
; Procedure: DrawNumber32 (Module 13)
; Purpose:   Converts a 32-bit unsigned integer to decimal via repeated division
;            by 10, pushing remainders onto the stack and popping in reverse
;            order, zero-padded to min_digits.
; Inputs:    [bp+14] = x (word)
;            [bp+12] = y (word)
;            [bp+10] = value low word
;            [bp+8]  = value high word
;            [bp+6]  = minimum digit count (zero-padded, e.g. 6 for SCORE)
;            [bp+4]  = colour (word)
; Outputs:   Draws decimal digits into frame_buf_seg
; Registers: All preserved
; -----------------------------------------------------------------------------
DrawNumber32:
    push bp
    mov  bp, sp
    push ax
    push bx
    push cx
    push dx
    push si

    mov  ax, [bp+10]                ; val_lo
    mov  dx, [bp+8]                 ; val_hi
    xor  si, si                     ; digit counter = 0

.div_loop:
    ; 32-bit unsigned divide DX:AX by 10 -> quotient DX:AX, remainder BX
    mov  bx, ax                     ; save val_lo
    mov  ax, dx                     ; AX = val_hi
    xor  dx, dx                     ; DX:AX = 0 : val_hi
    mov  cx, 10
    div  cx                         ; AX = hi_quot, DX = hi_rem
    xchg ax, bx                     ; BX = hi_quot, AX = val_lo
    div  cx                         ; AX = lo_quot, DX = remainder (0..9)
    xchg dx, bx                     ; DX = hi_quot, AX = lo_quot, BX = remainder

    add  bl, '0'
    push bx                         ; Push ASCII digit onto stack
    inc  si

    ; Continue while (DX:AX != 0) OR (count < min_digits)
    mov  cx, ax
    or   cx, dx
    jnz  .div_loop
    cmp  si, [bp+6]
    jb   .div_loop

    ; Pop digits in reverse order and render left-to-right
    mov  bx, [bp+14]                ; current x
.print_loop:
    pop  ax                         ; AL = ASCII digit
    push bx
    push word [bp+12]               ; y
    push ax
    push word [bp+4]                ; colour
    call DrawChar
    add  bx, 8
    dec  si
    jnz  .print_loop

    pop  si
    pop  dx
    pop  cx
    pop  bx
    pop  ax
    pop  bp
    ret  12

; -----------------------------------------------------------------------------
; Procedure: DrawHUD (Module 13, Section 5.14)
; Purpose:   Draws the fixed top 16-pixel HUD strip showing SCORE (6 digits),
;            COINS (x00), WORLD (1-1), TIME (3 digits + hurry blink < 100),
;            LIVES (x3), and HEALTH (2 heart icons).
; Inputs:    None (reads score, coins, game_time, lives, hero_hp)
; Outputs:   Renders HUD into rows 0..15 of frame_buf_seg
; Registers: All preserved
; -----------------------------------------------------------------------------
DrawHUD:
    push ax
    push bx

    ; Clear HUD strip (y = 0..15) to solid black so sprites never overlap HUD
    xor  ax, ax
    push ax                         ; x = 0
    push ax                         ; y = 0
    mov  ax, SCREEN_W
    push ax                         ; w = 320
    mov  ax, PLAY_TOP_Y
    push ax                         ; h = 16
    xor  ax, ax
    push ax                         ; colour = black
    call FillRect

    ; --- SCORE ---
    mov  ax, 8
    push ax
    xor  ax, ax
    push ax
    mov  ax, str_hud_score
    push ax
    mov  ax, COL_GOLD_HI
    push ax
    call DrawString

    mov  ax, 8
    push ax
    mov  ax, 8
    push ax
    push word [score_lo]
    push word [score_hi]
    mov  ax, 6
    push ax
    mov  ax, COL_WHITE
    push ax
    call DrawNumber32

    ; --- COINS ---
    mov  ax, 72
    push ax
    xor  ax, ax
    push ax
    mov  ax, str_hud_coins
    push ax
    mov  ax, COL_GOLD_HI
    push ax
    call DrawString

    mov  ax, 76
    push ax
    mov  ax, 8
    push ax
    mov  ax, str_x_prefix
    push ax
    mov  ax, COL_GOLD
    push ax
    call DrawString

    mov  ax, 84
    push ax
    mov  ax, 8
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

    ; --- WORLD ---
    mov  ax, 124
    push ax
    xor  ax, ax
    push ax
    mov  ax, str_hud_world
    push ax
    mov  ax, COL_GOLD_HI
    push ax
    call DrawString

    mov  ax, 132
    push ax
    mov  ax, 8
    push ax
    mov  ax, str_world_11
    push ax
    mov  ax, COL_WHITE
    push ax
    call DrawString

    ; --- TIME (with Bonus "hurry" blink when time < 100) ---
    mov  bx, COL_WHITE
    cmp  word [game_time], 100
    jae  .time_color_ok
    test word [frame_counter], 8
    jz   .time_color_ok
    mov  bx, COL_RED
.time_color_ok:
    mov  ax, 176
    push ax
    xor  ax, ax
    push ax
    mov  ax, str_hud_time
    push ax
    mov  ax, COL_GOLD_HI
    push ax
    call DrawString

    mov  ax, 180
    push ax
    mov  ax, 8
    push ax
    push word [game_time]
    xor  ax, ax
    push ax
    mov  ax, 3
    push ax
    push bx                         ; colour (blinks red when < 100)
    call DrawNumber32

    ; --- LIVES ---
    mov  ax, 218
    push ax
    xor  ax, ax
    push ax
    mov  ax, str_hud_lives
    push ax
    mov  ax, COL_GOLD_HI
    push ax
    call DrawString

    mov  ax, 224
    push ax
    mov  ax, 8
    push ax
    mov  ax, str_x_prefix
    push ax
    mov  ax, COL_GRASS_HI
    push ax
    call DrawString

    mov  ax, 232
    push ax
    mov  ax, 8
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

    ; --- HEALTH (2 hearts: Small = 1 filled + 1 empty, Super = 2 filled) ---
    mov  ax, 266
    push ax
    xor  ax, ax
    push ax
    mov  ax, str_hud_health
    push ax
    mov  ax, COL_GOLD_HI
    push ax
    call DrawString

    ; Heart 1 (filled if hero_hp >= 1)
    mov  ax, 278
    push ax
    mov  ax, 8
    push ax
    xor  ax, ax
    cmp  byte [hero_hp], 1
    jb   .h1_push
    mov  ax, 1
.h1_push:
    push ax
    call DrawHeart

    ; Heart 2 (filled if hero_hp >= 2)
    mov  ax, 290
    push ax
    mov  ax, 8
    push ax
    xor  ax, ax
    cmp  byte [hero_hp], 2
    jb   .h2_push
    mov  ax, 1
.h2_push:
    push ax
    call DrawHeart

    pop  bx
    pop  ax
    ret

; -----------------------------------------------------------------------------
; Procedure: UpdateCamera (Module 6, Section 5.3)
; Purpose:   Updates camera_x once per frame following Section 5.3 rules:
;            1. screen_x = hero_x_int - camera_x
;            2. If screen_x > 160 -> new_camera = hero_x_int - 160
;            3. Clamp new_camera <= MAX_CAMERA_X (640)
;            4. Never decrease camera_x
;            5. Left invisible wall: hero_x_int >= camera_x (else clamp & vx=0)
;            6. Right level edge: hero_x_int <= HERO_MAX_X (944)
; Inputs:    None (reads/updates hero_x_int, hero_vx, camera_x)
; Outputs:   Updated camera_x and clamped hero_x_int
; Registers: All preserved
; -----------------------------------------------------------------------------
UpdateCamera:
    push ax
    push bx

    ; Step 1: screen_x = hero_x_int - camera_x
    mov  ax, [hero_x_int]
    mov  bx, [camera_x]
    sub  ax, bx
    cmp  ax, CAMERA_FOLLOW_X        ; screen_x > 160?
    jle  .chk_left_wall

    ; Step 2: new_camera = hero_x_int - 160
    mov  ax, [hero_x_int]
    sub  ax, CAMERA_FOLLOW_X

    ; Step 3: clamp to MAX_CAMERA_X (640)
    cmp  ax, MAX_CAMERA_X
    jle  .chk_monotonic
    mov  ax, MAX_CAMERA_X

.chk_monotonic:
    ; Step 4: never decrease camera_x
    cmp  ax, [camera_x]
    jle  .chk_left_wall
    mov  [camera_x], ax

.chk_left_wall:
    ; Step 5: left invisible wall at camera_x
    mov  ax, [camera_x]
    cmp  [hero_x_int], ax
    jge  .chk_right_edge
    mov  [hero_x_int], ax
    mov  byte [hero_x_frac], 0
    mov  word [hero_vx], 0

.chk_right_edge:
    ; Step 6: right end of level (944)
    cmp  word [hero_x_int], HERO_MAX_X
    jle  .done
    mov  word [hero_x_int], HERO_MAX_X
    mov  byte [hero_x_frac], 0
    mov  word [hero_vx], 0

.done:
    pop  bx
    pop  ax
    ret
