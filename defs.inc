; =============================================================================
; File:        defs.inc
; Project:     Super Mario Engine (EL2003 COAL Semester Project)
; Description: Module 1 - Named constants (EQU) for display, tiles, colours,
;              scan codes, game limits, physics (8.8 fixed-point), enemy
;              records, sprite flags, and game state machine indices.
; Assembler:   NASM (-f bin, 16-bit Real Mode .COM)
; =============================================================================

; -----------------------------------------------------------------------------
; Display and Tile Constants (Appendix A.1)
; -----------------------------------------------------------------------------
SCREEN_W            equ 320
SCREEN_H            equ 200
PLAY_TOP_Y          equ 16          ; Map row 0 (0..15) is reserved for HUD
PLAY_BOTTOM_Y       equ 192         ; 12 rows * 16 = 192 (bottom 8 px stay black)
TILE_SIZE           equ 16
VIS_COLS            equ 20          ; 320 / 16 = 20 visible columns
VIS_ROWS            equ 12          ; 192 / 16 = 12 tile rows

LEVEL_COLS          equ 60          ; 60 tiles wide (3 screens)
LEVEL_ROWS          equ 12          ; 12 tiles high
LEVEL_MAP_BYTES     equ 720         ; 60 * 12 bytes
LEVEL_WIDTH_PX      equ 960         ; 60 * 16 pixels
MAX_CAMERA_X        equ 640         ; 960 - 320
CAMERA_FOLLOW_X     equ 160         ; Screen midpoint where camera starts following
HERO_MAX_X          equ 944         ; 960 - 16 (right edge of level)

VGA_SEG             equ 0A000h      ; VGA Mode 13h video memory segment
FB_WORDS            equ 32000       ; 64000 bytes / 2 for REP STOSW / REP MOVSW
FB_PARAGRAPHS       equ 0FA0h       ; 4000 paragraphs = 64000 bytes

; -----------------------------------------------------------------------------
; Tile Codes (Appendix A.2 + visual castle end tiles)
; -----------------------------------------------------------------------------
TILE_SKY            equ 0           ; Non-solid sky
TILE_GROUND         equ 1           ; Solid ground (grass top on row 10, dirt row 11)
TILE_BRICK          equ 2           ; Solid brick (Super Mario breaks from below, +50)
TILE_Q_COIN         equ 3           ; Solid "?" block with coin (+200, -> empty)
TILE_Q_MUSH         equ 4           ; Solid "?" block with mushroom (-> empty)
TILE_EMPTY          equ 5           ; Solid used/empty block
TILE_PIPE_TL        equ 6           ; Solid pipe top-left
TILE_PIPE_TR        equ 7           ; Solid pipe top-right
TILE_PIPE_BL        equ 8           ; Solid pipe body-left
TILE_PIPE_BR        equ 9           ; Solid pipe body-right
TILE_STAIR          equ 10          ; Solid stair block
TILE_COIN           equ 11          ; Non-solid free map coin (+200, coins +1)
TILE_FLAG           equ 12          ; Non-solid flag pole (touch = level won)
TILE_CASTLE_WALL    equ 13          ; Non-solid decorative castle brick
TILE_CASTLE_TOP     equ 14          ; Non-solid decorative castle battlement
TILE_CASTLE_DOOR    equ 15          ; Non-solid decorative castle doorway

; -----------------------------------------------------------------------------
; Keyboard Scan Codes (Appendix A.3)
; -----------------------------------------------------------------------------
KEY_ESC             equ 01h
KEY_P               equ 19h
KEY_ENTER           equ 1Ch
KEY_LSHIFT          equ 2Ah
KEY_SPACE           equ 39h
KEY_F1              equ 3Bh         ; Debug key: set time to 5 s (documented in README)
KEY_F2              equ 3Ch         ; Debug key: set coins to 99 (documented in README)
KEY_UP              equ 48h
KEY_LEFT            equ 4Bh
KEY_RIGHT           equ 4Dh
KEY_DOWN            equ 50h

; -----------------------------------------------------------------------------
; Game Limits and Timing Constants (Appendix A.4, 30 Hz Timer)
; -----------------------------------------------------------------------------
PIT_DIVISOR_30HZ    equ 39773       ; 1,193,182 / 30 = 39,772.7 -> ~30.00 FPS
TICKS_PER_SEC       equ 30          ; 30 timer ticks = 1 real second

START_LIVES         equ 3
MAX_LIVES           equ 99
START_TIME          equ 300
HP_SMALL            equ 1
HP_SUPER            equ 2

SCORE_MAX_HI        equ 000Fh       ; 999,999 = 000F423Fh
SCORE_MAX_LO        equ 423Fh

INVULN_TICKS        equ 60          ; 2 seconds invulnerability after damage
FLAT_GOOMBA_TICKS   equ 30          ; 1 second flattened Goomba display
SHELL_GRACE_TICKS   equ 8           ; 8 frames kick grace period
DEATH_ANIM_TICKS    equ 45          ; 1.5 seconds death animation
LIFE_LOST_TICKS     equ 60          ; 2 seconds life-lost screen
WIN_SLIDE_MIN_TICKS equ 30          ; ~1 second flag slide
WIN_WALK_TICKS      equ 30          ; ~1 second walk to castle

HERO_START_X        equ 32          ; World X = 32 (column 2)
HERO_START_Y        equ 144         ; World Y = 144 (stands on row 9, ground = row 10)
HERO_HITBOX_XOFF    equ 2           ; 12-px wide hitbox centred in 16-px sprite
HERO_HITBOX_W       equ 12
HERO_SMALL_H        equ 16
HERO_SUPER_H        equ 32

MAX_ENEMIES         equ 8           ; At most 8 active enemies
ENEMY_REC_SIZE      equ 16          ; 16 bytes per enemy record (shift by 4)
MAX_POPUPS          equ 4           ; Bonus: 4 simultaneous floating score pop-ups
POPUP_REC_SIZE      equ 8           ; 8 bytes per pop-up record

; -----------------------------------------------------------------------------
; Physics Constants in 8.8 Signed Fixed Point (Appendix A.5, 30 FPS)
; -----------------------------------------------------------------------------
PHYS_ACCEL          equ 0020h       ;  0.125 px/frame^2
PHYS_FRICTION       equ 0010h       ;  0.0625 px/frame^2
PHYS_BRAKE          equ 0040h       ;  0.25 px/frame^2 (faster braking when reversing)
PHYS_MAX_WALK       equ 0180h       ;  1.5 px/frame
PHYS_MAX_RUN        equ 0280h       ;  2.5 px/frame
PHYS_GRAVITY        equ 0040h       ;  0.25 px/frame^2
PHYS_JUMP_VEL       equ 0FA80h      ; -5.5 px/frame (about 3.8 tiles high)
PHYS_JUMP_CUT       equ 0FE00h      ; -2.0 px/frame (variable jump cut-off)
PHYS_MAX_FALL       equ 0400h       ;  4.0 px/frame
PHYS_STOMP_VEL      equ 0FD00h      ; -3.0 px/frame (normal stomp bounce)
PHYS_STOMP_HIGH     equ 0FC00h      ; -4.0 px/frame (stomp bounce with Space held)
PHYS_DEATH_HOP      equ 0FB20h      ; -4.875 px/frame (~48 px upward hop on death)

PHYS_GOOMBA_SPD     equ 0080h       ;  0.5 px/frame
PHYS_KOOPA_SPD      equ 00C0h       ;  0.75 px/frame
PHYS_SHELL_SPD      equ 0300h       ;  3.0 px/frame
PHYS_MUSH_SPD       equ 0100h       ;  1.0 px/frame

; -----------------------------------------------------------------------------
; Enemy Types, States and Record Field Offsets (Module 11)
; -----------------------------------------------------------------------------
ENT_NONE            equ 0
ENT_GOOMBA          equ 1
ENT_KOOPA           equ 2

EST_DEAD            equ 0
EST_WAITING         equ 1           ; Stands still until within 1 tile of screen
EST_WALKING         equ 2
EST_FLAT            equ 3           ; Flattened Goomba (1 s timer)
EST_SHELL_STILL     equ 4           ; Stationary Koopa shell
EST_SHELL_SLIDE     equ 5           ; Fast sliding Koopa shell

EN_TYPE             equ 0           ; byte
EN_STATE            equ 1           ; byte
EN_X_INT            equ 2           ; word (signed world pixel X)
EN_X_FRAC           equ 4           ; byte
EN_DIR              equ 5           ; byte (0 = right, 1 = left)
EN_Y_INT            equ 6           ; word (signed world pixel Y)
EN_Y_FRAC           equ 8           ; byte
EN_TIMER            equ 9           ; byte (flat timer or kick grace timer)
EN_VX               equ 10          ; word (8.8 signed)
EN_VY               equ 12          ; word (8.8 signed)
EN_GROUND           equ 14          ; byte (0/1)
EN_ANIM             equ 15          ; byte (walk cycle toggle)

; -----------------------------------------------------------------------------
; Hero Poses and Sprite Drawing Flags (Module 4)
; -----------------------------------------------------------------------------
POSE_STAND          equ 0
POSE_RUN            equ 1
POSE_JUMP           equ 2

SPR_FLIP_X          equ 01h         ; Bit 0: mirror sprite horizontally
SPR_TALL            equ 02h         ; Bit 1: 16x32 sprite (default is 16x16)
SPR_BLINK           equ 04h         ; Bit 2: draw only on even frames (invulnerable)

; -----------------------------------------------------------------------------
; Game State Machine Constants (Section 5.12, Jump Table Indices)
; -----------------------------------------------------------------------------
STATE_TITLE         equ 0
STATE_PLAYING       equ 1
STATE_PAUSED        equ 2
STATE_DYING         equ 3
STATE_LIFE_LOST     equ 4
STATE_WIN_SEQ       equ 5
STATE_LEVEL_COMP    equ 6
STATE_GAME_OVER     equ 7
STATE_EXIT          equ 8

; -----------------------------------------------------------------------------
; Custom VGA Palette Indices (Loaded into DAC indices 16..45 in Module 2)
; Index 0 is strictly transparent for sprites and black for HUD/borders.
; -----------------------------------------------------------------------------
COL_BLACK           equ 0           ; Transparent in sprites / black background
COL_SKY             equ 16          ; Classic Mario sky blue
COL_BRICK           equ 17          ; Brick terracotta
COL_BRICK_DARK      equ 18          ; Brick mortar / shadow
COL_BRICK_HI        equ 19          ; Brick highlight
COL_GRASS           equ 20          ; Grass green
COL_GRASS_HI        equ 21          ; Bright grass / bush highlight
COL_DIRT            equ 22          ; Ground dirt brown
COL_DIRT_DARK       equ 23          ; Dirt dark speckle
COL_PIPE_DARK       equ 24          ; Pipe dark green
COL_PIPE_MID        equ 25          ; Pipe medium green
COL_PIPE_HI         equ 26          ; Pipe specular green
COL_GOLD            equ 27          ; Question block / coin gold
COL_GOLD_HI         equ 28          ; Bright gold glint
COL_GOLD_DARK       equ 29          ; Block / coin shadow
COL_SKIN            equ 30          ; Mario skin peach
COL_RED             equ 31          ; Mario red cap / heart / mushroom
COL_BLUE            equ 32          ; Mario blue overalls
COL_BROWN           equ 33          ; Hair / boots / Goomba body
COL_WHITE           equ 34          ; Pure white (text, eyes, clouds)
COL_GRAY            equ 35          ; Dark grey (empty heart, shadows)
COL_STAIR           equ 36          ; Stair block tan
COL_STAIR_HI        equ 37          ; Stair highlight
COL_STAIR_DARK      equ 38          ; Stair shadow
COL_CASTLE          equ 39          ; Castle brick
COL_CLOUD_SHD       equ 40          ; Cloud soft underside
COL_UI_BG           equ 41          ; Deep navy UI panel
COL_UI_BORDER       equ 42          ; Gold UI border
COL_KOOPA_SH        equ 43          ; Koopa shell bright green
COL_KOOPA_HI        equ 44          ; Koopa shell highlight
COL_OUTLINE         equ 45          ; Solid near-black for sprite eyes/outlines
