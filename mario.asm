; =============================================================================
; File:        mario.asm
; Project:     Super Mario Engine (EL2003 COAL Semester Project)
; Description: Module 1 - Main .COM entry point (ORG 100h), initialization of
;              off-screen frame buffer, VGA Mode 13h, custom palette, custom
;              INT 08h/09h hardware interrupt handlers, 30 Hz state-machine
;              main loop, and clean exit path back to DOS text mode.
; Build:       nasm -f bin -O2 -I src/ src/mario.asm -o MARIO.COM
; Run:         DOSBox v0.74-3+ -> MARIO.COM
; =============================================================================

bits 16
org  100h

; Include named constants (emits 0 bytes)
%include "defs.inc"

; -----------------------------------------------------------------------------
; Entry Point: Main
; Purpose:   Initializes memory, VGA Mode 13h, custom palette, and custom
;            hardware interrupts; runs the 30 Hz game state loop until EXIT;
;            cleanly restores all system resources before returning to DOS.
; Inputs:    None (DOS .COM startup: CS=DS=ES=SS, SP=FFFEh)
; Outputs:   Exits to DOS with exit code 0 (INT 21h, AH=4Ch)
; Registers: N/A (program entry/exit)
; -----------------------------------------------------------------------------
Main:
    cld
    push cs
    pop  ds
    push cs
    pop  es

    ; 1. Allocate 64,000-byte off-screen frame buffer segment (Module 5)
    call AllocFrameBuffer

    ; 2. Switch to VGA Mode 13h (320x200, 256 colours) & load palette (Module 2)
    mov  ax, 0013h
    push ax
    call SetVideoMode
    call LoadCustomPalette

    ; 3. Clear frame buffer & initialize default level/title state
    xor  ax, ax
    push ax
    call ClearFrameBuffer
    call FlipFrameBuffer

    mov  byte [game_state], STATE_TITLE
    mov  byte [menu_selection], 0

    ; 4. Install custom Keyboard (INT 09h) & 30 Hz Timer (INT 08h) ISRs
    call InstallInterrupts

    ; 5. Main 30 FPS Timer-Driven Game Loop (No delay loops!)
.main_loop:
    call WaitNextTick               ; Synchronize to 30 Hz PIT interrupt
    inc  word [frame_counter]       ; Global animation & blink counter
    call UpdateSound                ; Advance non-blocking PC speaker SFX
    call RunCurrentState            ; Jump-table state machine (Section 5.12)
    call ClearKeyPressed            ; Clear edge-triggered key flags

    cmp  byte [game_state], STATE_EXIT
    jne  .main_loop

; -----------------------------------------------------------------------------
; Clean Exit Path (Modules 1, 2, 7, 8, 15)
; Always restores PIT rate, INT 08h, INT 09h, frees memory, restores Mode 03h.
; -----------------------------------------------------------------------------
CleanExit:
    call UninstallInterrupts        ; Restore PIT 18.2 Hz, INT 08h, INT 09h, silence SFX
    call FreeFrameBuffer            ; Release DOS-allocated frame buffer segment

    mov  ax, 0003h                  ; Restore standard 80x25 VGA text mode
    push ax
    call SetVideoMode

    mov  ax, 4C00h                  ; DOS Terminate Program (return code 0)
    int  21h

; =============================================================================
; Modular Source Includes (Phases 1 - 5 / Modules 1 - 15)
; =============================================================================
%include "irq.inc"                  ; Modules 7 & 8: Keyboard & Timer ISRs, SFX
%include "gfx.inc"                  ; Modules 2-6 & 13: VGA, Frame Buffer, Tiles, Sprites, HUD, Camera
%include "score.inc"                ; Module 13: 32-bit Score, Coins, 1-UP, Stomp/Flag Scoring, Pop-ups
%include "physics.inc"              ; Modules 9 & 10: Fixed-Point Physics & Tile/AABB Collision
%include "entities.inc"             ; Modules 11 & 12: Enemies, Shells, Mushroom, Blocks, Coins
%include "states.inc"               ; Modules 14 & 15: State Machine, Screens, Reset/NewGame
%include "data.inc"                 ; Modules 1 & 3: Variables, Tables, Strings, 60x12 Level Maps
%include "assets.inc"               ; Modules 2, 4 & 13: Palette, Tiles, Sprites, 8x8 Bitmap Font
