; =============================================================================
; File:        irq.inc
; Project:     Super Mario Engine (EL2003 COAL Semester Project)
; Description: Modules 7 & 8 - Custom hardware interrupt handlers for Keyboard
;              (INT 09h, port 60h, 128-byte multi-key state table) and Timer
;              (INT 08h at 30 Hz via PIT Channel 0, chaining to BIOS at 18.2 Hz),
;              plus non-blocking timer-driven PC Speaker SFX.
; =============================================================================

; -----------------------------------------------------------------------------
; Procedure: InstallInterrupts
; Purpose:   Saves original INT 08h & INT 09h vectors, installs custom handlers,
;            and programs PIT Channel 0 for 30 Hz frame pacing.
; Inputs:    None
; Outputs:   irq_installed = 1
; Registers: All preserved
; -----------------------------------------------------------------------------
InstallInterrupts:
    push ax
    push bx
    push dx
    push es

    ; Save original Keyboard vector (INT 09h)
    mov  ax, 3509h
    int  21h
    mov  [old_int09_off], bx
    mov  [old_int09_seg], es

    ; Save original Timer vector (INT 08h)
    mov  ax, 3508h
    int  21h
    mov  [old_int08_off], bx
    mov  [old_int08_seg], es

    cli

    ; Install custom Keyboard ISR (INT 09h)
    mov  dx, KeyboardISR
    mov  ax, 2509h
    int  21h

    ; Install custom Timer ISR (INT 08h)
    mov  dx, TimerISR
    mov  ax, 2508h
    int  21h

    ; Program PIT 8253 Channel 0 (Mode 3, lobyte/hibyte) for ~30 Hz
    mov  al, 36h
    out  43h, al
    mov  ax, PIT_DIVISOR_30HZ
    out  40h, al                    ; Low byte of divisor
    mov  al, ah
    out  40h, al                    ; High byte of divisor

    mov  word [timer_ticks], 0
    mov  word [last_tick], 0
    mov  word [pit_accum], 0
    mov  byte [irq_installed], 1

    sti

    pop  es
    pop  dx
    pop  bx
    pop  ax
    ret

; -----------------------------------------------------------------------------
; Procedure: UninstallInterrupts
; Purpose:   Restores PIT Channel 0 to 18.2 Hz, restores original INT 08h and
;            INT 09h vectors, and silences the PC speaker on every exit path.
; Inputs:    None
; Outputs:   irq_installed = 0
; Registers: All preserved
; -----------------------------------------------------------------------------
UninstallInterrupts:
    push ax
    push dx

    cmp  byte [irq_installed], 0
    je   .done

    call StopSound

    cli

    ; Restore PIT Channel 0 default divisor (0000h = 65536 -> 18.2065 Hz)
    mov  al, 36h
    out  43h, al
    xor  al, al
    out  40h, al
    out  40h, al

    ; Restore original Timer vector (INT 08h)
    push ds
    mov  dx, [old_int08_off]
    mov  ds, [old_int08_seg]
    mov  ax, 2508h
    int  21h
    pop  ds

    ; Restore original Keyboard vector (INT 09h)
    push ds
    mov  dx, [old_int09_off]
    mov  ds, [old_int09_seg]
    mov  ax, 2509h
    int  21h
    pop  ds

    mov  byte [irq_installed], 0

    sti

.done:
    pop  dx
    pop  ax
    ret

; -----------------------------------------------------------------------------
; Interrupt Service Routine: KeyboardISR (INT 09h)
; Purpose:   Reads hardware scan code from port 60h, ignores extended prefix
;            E0h/E1h, updates 128-byte key_state & key_pressed tables, sends EOI.
; Inputs:    Hardware IRQ1 (port 60h)
; Outputs:   Updates key_state[code] and key_pressed[code]
; Registers: All preserved (IRET)
; -----------------------------------------------------------------------------
KeyboardISR:
    push ax
    push bx
    push ds

    ; Ensure DS points to our .COM code/data segment
    push cs
    pop  ds

    in   al, 60h                    ; Read keyboard scan code

    ; Ignore extended arrow prefix bytes (E0h / E1h)
    cmp  al, 0E0h
    je   .send_eoi
    cmp  al, 0E1h
    je   .send_eoi

    ; Bit 7 set means key release (break code)
    test al, 80h
    jnz  .key_release

    ; Key press (make code): record held state and edge-triggered press
    xor  bh, bh
    mov  bl, al
    cmp  byte [key_state + bx], 0
    jne  .already_held
    mov  byte [key_pressed + bx], 1
.already_held:
    mov  byte [key_state + bx], 1
    jmp  short .send_eoi

.key_release:
    and  al, 7Fh                    ; Clear release bit 7
    xor  bh, bh
    mov  bl, al
    mov  byte [key_state + bx], 0

.send_eoi:
    mov  al, 20h                    ; Non-specific End-Of-Interrupt to PIC1
    out  20h, al

    pop  ds
    pop  bx
    pop  ax
    iret

; -----------------------------------------------------------------------------
; Interrupt Service Routine: TimerISR (INT 08h)
; Purpose:   Increments 30 Hz game tick counter; chains to original BIOS INT 08h
;            every ~65536 PIT cycles (18.2 Hz) so DOS timekeeping stays intact.
; Inputs:    Hardware IRQ0
; Outputs:   Increments timer_ticks
; Registers: All preserved (IRET or chained via original handler)
; -----------------------------------------------------------------------------
TimerISR:
    push ax
    inc  word [cs:timer_ticks]
    add  word [cs:pit_accum], PIT_DIVISOR_30HZ
    jc   .chain_bios

    mov  al, 20h                    ; Send EOI when not chaining to BIOS
    out  20h, al
    pop  ax
    iret

.chain_bios:
    pop  ax
    jmp  far [cs:old_int08_off]     ; BIOS INT 08h updates clock, sends EOI, IRETs

; -----------------------------------------------------------------------------
; Procedure: WaitNextTick
; Purpose:   Synchronizes the main loop to the 30 Hz timer interrupt without
;            using busy delay loops.
; Inputs:    None
; Outputs:   Updates last_tick to match timer_ticks
; Registers: All preserved
; -----------------------------------------------------------------------------
WaitNextTick:
    push ax
    sti                             ; Ensure timer IRQ0 can fire
    mov  ax, [last_tick]
.wait_loop:
    cmp  ax, [timer_ticks]
    je   .wait_loop
    mov  ax, [timer_ticks]
    mov  [last_tick], ax
    pop  ax
    ret

; -----------------------------------------------------------------------------
; Procedure: ClearKeyPressed
; Purpose:   Clears the 128-byte edge-triggered key_pressed table via REP STOSW.
; Inputs:    None
; Outputs:   key_pressed[0..127] = 0
; Registers: All preserved
; -----------------------------------------------------------------------------
ClearKeyPressed:
    push ax
    push cx
    push di
    push es

    push ds
    pop  es
    mov  di, key_pressed
    xor  ax, ax
    mov  cx, 64                     ; 64 words = 128 bytes
    cld
    rep  stosw

    pop  es
    pop  di
    pop  cx
    pop  ax
    ret

; -----------------------------------------------------------------------------
; Procedure: PlaySound (Bonus - Non-Blocking PC Speaker SFX)
; Purpose:   Starts a timer-driven sound effect with optional pitch sweep.
; Inputs:    [bp+8] = initial PIT divisor (word)
;            [bp+6] = per-frame divisor delta (signed word)
;            [bp+4] = duration in frames (word, low byte used)
; Outputs:   Updates sfx_* state and enables PC speaker
; Registers: All preserved
; -----------------------------------------------------------------------------
PlaySound:
    push bp
    mov  bp, sp
    push ax

    mov  ax, [bp+8]
    mov  [sfx_cur_div], ax
    mov  ax, [bp+6]
    mov  [sfx_freq_step], ax
    mov  al, [bp+4]
    mov  [sfx_timer], al

    ; Program PIT Channel 2 square wave
    mov  al, 0B6h
    out  43h, al
    mov  ax, [sfx_cur_div]
    out  42h, al
    mov  al, ah
    out  42h, al

    ; Enable speaker gate (bits 0 and 1 of port 61h)
    in   al, 61h
    or   al, 03h
    out  61h, al

    pop  ax
    pop  bp
    ret  6

; -----------------------------------------------------------------------------
; Procedure: UpdateSound
; Purpose:   Advances active PC speaker pitch sweep and stops sound when timer
;            expires. Called once per 30 Hz frame.
; Inputs:    None
; Outputs:   Updates sfx_timer and speaker pitch
; Registers: All preserved
; -----------------------------------------------------------------------------
UpdateSound:
    push ax

    cmp  byte [sfx_timer], 0
    je   .done

    dec  byte [sfx_timer]
    jz   .turn_off

    mov  ax, [sfx_cur_div]
    add  ax, [sfx_freq_step]
    cmp  ax, 120                    ; Clamp minimum divisor (avoid inaudible high pitch)
    jb   .done
    cmp  ax, 20000                  ; Clamp maximum divisor
    ja   .done
    mov  [sfx_cur_div], ax
    out  42h, al
    mov  al, ah
    out  42h, al
    jmp  short .done

.turn_off:
    call StopSound

.done:
    pop  ax
    ret

; -----------------------------------------------------------------------------
; Procedure: StopSound
; Purpose:   Immediately silences the PC speaker.
; Inputs:    None
; Outputs:   sfx_timer = 0
; Registers: All preserved
; -----------------------------------------------------------------------------
StopSound:
    push ax
    in   al, 61h
    and  al, 0FCh
    out  61h, al
    mov  byte [sfx_timer], 0
    pop  ax
    ret
