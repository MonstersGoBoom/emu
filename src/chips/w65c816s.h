#pragma once
/*#
    # w65c816s.h

    Western Design Center 65C816 CPU emulator.

    Project repo: https://github.com/floooh/chips/

    NOTE: this file is code-generated from w65816.template.h and w65816_gen.py
    in the 'codegen' directory.

    Do this:
    ~~~C
    #define CHIPS_IMPL
    ~~~
    before you include this file in *one* C or C++ file to create the
    implementation.

    Optionally provide the following macros with your own implementation
    ~~~C
    CHIPS_ASSERT(c)
    ~~~

    ## Emulated Pins

    ***********************************
    *           +-----------+         *
    *   IRQ --->|           |---> A0  *
    *   NMI --->|           |...      *
    *    RDY--->|           |---> A23 *
    *    RES--->|           |         *
    *    RW <---|           |         *
    *  SYNC <---|           |         *
    *           |           |<--> D0  *
    *           |           |...      *
    *           |           |<--> D7  *
    *           |           |         *
    *           +-----------+         *
    ***********************************

    If the RDY pin is active (1) the CPU will loop on the next read
    access until the pin goes inactive.

    ## Overview

    w65c816s.h implements a cycle-stepped 65816 CPU emulator, meaning
    that the emulation state can be ticked forward in clock cycles instead
    of full instructions.

    To initialize the emulator, fill out a w65816_desc_t structure with
    initialization parameters and then call w65816_init().

        ~~~C
        typedef struct {
            bool bcd_disabled;          // set to true if BCD mode is disabled
         } w65816_desc_t;
         ~~~

    At the end of w65816_init(), the CPU emulation will be at the start of
    RESET state, and the first 7 ticks will execute the reset sequence
    (loading the reset vector at address 0xFFFC and continuing execution
    there.

    w65816_init() will return a 64-bit pin mask which must be the input argument
    to the first call of w65816_tick().

    To execute instructions, call w65816_tick() in a loop. w65816_tick() takes
    a 64-bit pin mask as input, executes one clock tick, and returns
    a modified pin mask.

    After executing one tick, the pin mask must be inspected, a memory read
    or write operation must be performed, and the modified pin mask must be
    used for the next call to w65816_tick(). This 64-bit pin mask is how
    the CPU emulation communicates with the outside world.

    The simplest-possible execution loop would look like this:

        ~~~C
        // setup 16 MBytes of memory
        uint8_t mem[1<<24] = { ... };
        // initialize the CPU
        w65816_t cpu;
        uint64_t pins = w65816_init(&cpu, &(w65816_desc_t){...});
        while (...) {
            // run the CPU emulation for one tick
            pins = w65816_tick(&cpu, pins);
            // extract 24-bit address from pin mask
            const uint32_t addr = W65816_GET_ADDR(pins);
            // perform memory access
            if (pins & W65816_RW) {
                // a memory read
                W65816_SET_DATA(pins, mem[addr]);
            }
            else {
                // a memory write
                mem[addr] = W65816_GET_DATA(pins);
            }
        }
        ~~~

    To start a reset sequence, set the W65816_RES bit in the pin mask and
    continue calling the w65816_tick() function. At the start of the next
    instruction, the CPU will initiate the 7-tick reset sequence. You do NOT
    need to clear the W65816_RES bit, this will be cleared when the reset
    sequence starts.

    To request an interrupt, set the W65816_IRQ or W65816_NMI bits in the pin
    mask and continue calling the tick function. The interrupt sequence
    will be initiated at the end of the current or next instruction
    (depending on the exact cycle the interrupt pin has been set).

    Unlike the W65816_RES pin, you are also responsible for clearing the
    interrupt pins (typically, the interrupt lines are cleared by the chip
    which requested the interrupt once the CPU reads a chip's interrupt
    status register to check which chip requested the interrupt).

    To find out whether a new instruction is about to start, check if the
    both W65816_VPA and W65816_VDA pins are set.

    To "goto" a random address at any time, a 'prefetch' like this is
    necessary (this basically simulates a normal instruction fetch from
    address 'next_pc'). This is usually only needed in "trap code" which
    intercepts operating system calls, executes some native code to emulate
    the operating system call, and then continue execution somewhere else:

        ~~~C
        pins = W65816_VPA|W65816_VDA;
        W65816_SET_ADDR(pins, next_pc);
        W65816_SET_DATA(pins, mem[next_pc]);
        w65816_set_pc(next_pc);
        ~~~~

    ## Functions
    ~~~C
    uint64_t w65816_init(w65816_t* cpu, const w65816_desc_t* desc)
    ~~~
        Initialize a w65816_t instance, the desc structure provides
        initialization attributes:
            ~~~C
            typedef struct {
                bool bcd_disabled;              // set to true if BCD mode is disabled
            } w65816_desc_t;
            ~~~

    ~~~C
    uint64_t w65816_tick(w65816_t* cpu, uint64_t pins)
    ~~~
        Tick the CPU for one clock cycle. The 'pins' argument and return value
        is the current state of the CPU pins used to communicate with the
        outside world (see the Overview section above for details).

    ~~~C
    void w65816_set_x(w65816_t* cpu, uint8_t val)
    void w65816_set_xx(w65816_t* cpu, uint16_t val)
    uint8_t w65816_x(w65816_t* cpu)
    uint16_t w65816_xx(w65816_t* cpu)
    ~~~
        Set and get 658165 registers and flags.


    ## zlib/libpng license

    Copyright (c) 2018 Andre Weissflog
    This software is provided 'as-is', without any express or implied warranty.
    In no event will the authors be held liable for any damages arising from the
    use of this software.
    Permission is granted to anyone to use this software for any purpose,
    including commercial applications, and to alter it and redistribute it
    freely, subject to the following restrictions:
        1. The origin of this software must not be misrepresented; you must not
        claim that you wrote the original software. If you use this software in a
        product, an acknowledgment in the product documentation would be
        appreciated but is not required.
        2. Altered source versions must be plainly marked as such, and must not
        be misrepresented as being the original software.
        3. This notice may not be removed or altered from any source
        distribution.
#*/
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// address bus pins
#define W65816_PIN_A0    (0)
#define W65816_PIN_A1    (1)
#define W65816_PIN_A2    (2)
#define W65816_PIN_A3    (3)
#define W65816_PIN_A4    (4)
#define W65816_PIN_A5    (5)
#define W65816_PIN_A6    (6)
#define W65816_PIN_A7    (7)
#define W65816_PIN_A8    (8)
#define W65816_PIN_A9    (9)
#define W65816_PIN_A10   (10)
#define W65816_PIN_A11   (11)
#define W65816_PIN_A12   (12)
#define W65816_PIN_A13   (13)
#define W65816_PIN_A14   (14)
#define W65816_PIN_A15   (15)

// data bus pins
#define W65816_PIN_D0    (16)
#define W65816_PIN_D1    (17)
#define W65816_PIN_D2    (18)
#define W65816_PIN_D3    (19)
#define W65816_PIN_D4    (20)
#define W65816_PIN_D5    (21)
#define W65816_PIN_D6    (22)
#define W65816_PIN_D7    (23)

// control pins
#define W65816_PIN_RW    (24)      // out: memory read or write access
#define W65816_PIN_VPA   (25)      // out: valid program address
#define W65816_PIN_VDA   (26)      // out: valid data address
#define W65816_PIN_IRQ   (27)      // in: maskable interrupt requested
#define W65816_PIN_NMI   (28)      // in: non-maskable interrupt requested
#define W65816_PIN_RDY   (29)      // in: freeze execution at next read cycle
#define W65816_PIN_RES   (30)      // in: request RESET
#define W65816_PIN_ABORT (31)      // in: request ABORT (not implemented)

// bank address pins
#define W65816_PIN_A16   (32)
#define W65816_PIN_A17   (33)
#define W65816_PIN_A18   (34)
#define W65816_PIN_A19   (35)
#define W65816_PIN_A20   (36)
#define W65816_PIN_A21   (37)
#define W65816_PIN_A22   (38)
#define W65816_PIN_A23   (39)

// pin bit masks
#define W65816_A0    (1ULL<<W65816_PIN_A0)
#define W65816_A1    (1ULL<<W65816_PIN_A1)
#define W65816_A2    (1ULL<<W65816_PIN_A2)
#define W65816_A3    (1ULL<<W65816_PIN_A3)
#define W65816_A4    (1ULL<<W65816_PIN_A4)
#define W65816_A5    (1ULL<<W65816_PIN_A5)
#define W65816_A6    (1ULL<<W65816_PIN_A6)
#define W65816_A7    (1ULL<<W65816_PIN_A7)
#define W65816_A8    (1ULL<<W65816_PIN_A8)
#define W65816_A9    (1ULL<<W65816_PIN_A9)
#define W65816_A10   (1ULL<<W65816_PIN_A10)
#define W65816_A11   (1ULL<<W65816_PIN_A11)
#define W65816_A12   (1ULL<<W65816_PIN_A12)
#define W65816_A13   (1ULL<<W65816_PIN_A13)
#define W65816_A14   (1ULL<<W65816_PIN_A14)
#define W65816_A15   (1ULL<<W65816_PIN_A15)
#define W65816_A16   (1ULL<<W65816_PIN_A16)
#define W65816_A17   (1ULL<<W65816_PIN_A17)
#define W65816_A18   (1ULL<<W65816_PIN_A18)
#define W65816_A19   (1ULL<<W65816_PIN_A19)
#define W65816_A20   (1ULL<<W65816_PIN_A20)
#define W65816_A21   (1ULL<<W65816_PIN_A21)
#define W65816_A22   (1ULL<<W65816_PIN_A22)
#define W65816_A23   (1ULL<<W65816_PIN_A23)
#define W65816_D0    (1ULL<<W65816_PIN_D0)
#define W65816_D1    (1ULL<<W65816_PIN_D1)
#define W65816_D2    (1ULL<<W65816_PIN_D2)
#define W65816_D3    (1ULL<<W65816_PIN_D3)
#define W65816_D4    (1ULL<<W65816_PIN_D4)
#define W65816_D5    (1ULL<<W65816_PIN_D5)
#define W65816_D6    (1ULL<<W65816_PIN_D6)
#define W65816_D7    (1ULL<<W65816_PIN_D7)
#define W65816_RW    (1ULL<<W65816_PIN_RW)
#define W65816_VPA   (1ULL<<W65816_PIN_VPA)
#define W65816_VDA   (1ULL<<W65816_PIN_VDA)
#define W65816_IRQ   (1ULL<<W65816_PIN_IRQ)
#define W65816_NMI   (1ULL<<W65816_PIN_NMI)
#define W65816_RDY   (1ULL<<W65816_PIN_RDY)
#define W65816_RES   (1ULL<<W65816_PIN_RES)
#define W65816_ABORT (1ULL<<W65816_PIN_ABORT)

/* bit mask for all CPU pins (up to bit pos 40) */
#define W65816_PIN_MASK ((1ULL<<40)-1)

/* status indicator flags */
#define W65816_EF    (1<<0)  /* Emulation */
#define W65816_CF    (1<<0)  /* Carry */
#define W65816_ZF    (1<<1)  /* Zero */
#define W65816_IF    (1<<2)  /* IRQ disable */
#define W65816_DF    (1<<3)  /* Decimal mode */
#define W65816_BF    (1<<4)  /* BRK command (Emulation) */
#define W65816_XF    (1<<4)  /* Index Register Select (Native) */
#define W65816_UF    (1<<5)  /* Unused (Emulated) */
#define W65816_MF    (1<<5)  /* Memory Select (Native) */
#define W65816_VF    (1<<6)  /* Overflow */
#define W65816_NF    (1<<7)  /* Negative */

/* internal BRK state flags */
#define W65816_BRK_IRQ   (1<<0)  /* IRQ was triggered */
#define W65816_BRK_NMI   (1<<1)  /* NMI was triggered */
#define W65816_BRK_RESET (1<<2)  /* RES was triggered */

/* the desc structure provided to w65816_init() */
typedef struct {
    bool bcd_disabled;              /* set to true if BCD mode is disabled */
} w65816_desc_t;

/* CPU state */
typedef struct {
    uint16_t IR;        /* internal Instruction Register */
    uint16_t PC;        /* internal Program Counter register */
    uint16_t AD;        /* ADL/ADH internal register */
    uint16_t C;         /* BA=C accumulator */
    uint16_t X,Y;       /* index registers */
    uint8_t DBR,PBR;    /* Bank registers (Data, Program) */
    uint16_t D;         /* Direct register */
    uint8_t P;          /* Processor status register */
    uint16_t S;         /* Stack pointer */
    uint64_t PINS;      /* last stored pin state (do NOT modify) */
    uint16_t irq_pip;
    uint16_t nmi_pip;
    uint8_t emulation;  /* W65C02 Emulation mode */
    uint8_t brk_flags;  /* W65816_BRK_* */
    uint8_t bcd_enabled;
} w65816_t;

/* initialize a new w65816 instance and return initial pin mask */
uint64_t w65816_init(w65816_t* cpu, const w65816_desc_t* desc);
/* execute one tick */
uint64_t w65816_tick(w65816_t* cpu, uint64_t pins);
// prepare w65816_t snapshot for saving
void w65816_snapshot_onsave(w65816_t* snapshot);
// fixup w65816_t snapshot after loading
void w65816_snapshot_onload(w65816_t* snapshot, w65816_t* sys);

/* register access functions */
void w65816_set_a(w65816_t* cpu, uint8_t v);
void w65816_set_b(w65816_t* cpu, uint8_t v);
void w65816_set_c(w65816_t* cpu, uint16_t v);
void w65816_set_x(w65816_t* cpu, uint16_t v);
void w65816_set_y(w65816_t* cpu, uint16_t v);
void w65816_set_s(w65816_t* cpu, uint16_t v);
void w65816_set_p(w65816_t* cpu, uint8_t v);
void w65816_set_e(w65816_t* cpu, bool v);
void w65816_set_pc(w65816_t* cpu, uint16_t v);
void w65816_set_pb(w65816_t* cpu, uint8_t v);
void w65816_set_db(w65816_t* cpu, uint8_t v);
uint8_t w65816_a(w65816_t* cpu);
uint8_t w65816_b(w65816_t* cpu);
uint16_t w65816_c(w65816_t* cpu);
uint16_t w65816_x(w65816_t* cpu);
uint16_t w65816_y(w65816_t* cpu);
uint16_t w65816_s(w65816_t* cpu);
uint8_t w65816_p(w65816_t* cpu);
bool w65816_e(w65816_t* cpu);
uint16_t w65816_pc(w65816_t* cpu);
uint8_t w65816_pb(w65816_t* cpu);
uint8_t w65816_db(w65816_t* cpu);

/* extract 24-bit address bus from 64-bit pins */
#define W65816_GET_ADDR(p) ((uint32_t)((p)&0xFFFFULL)|(uint32_t)((p>>16)&0xFF0000ULL))
/* merge 24-bit address bus value into 64-bit pins */
#define W65816_SET_ADDR(p,a) {p=(((p)&~0xFF0000FFFFULL)|((a)&0xFFFFULL)|((a<<16)&0xFF00000000ULL));}
/* extract 8-bit bank value from 64-bit pins */
#define W65816_GET_BANK(p) ((uint8_t)(((p)&0xFF00000000ULL)>>32))
/* merge 8-bit bank value into 64-bit pins */
#define W65816_SET_BANK(p,a) {p=(((p)&~0xFF00000000ULL)|((a<<32)&0xFF00000000ULL));}
/* extract 8-bit data bus from 64-bit pins */
#define W65816_GET_DATA(p) ((uint8_t)(((p)&0xFF0000ULL)>>16))
/* merge 8-bit data bus value into 64-bit pins */
#define W65816_SET_DATA(p,d) {p=(((p)&~0xFF0000ULL)|(((d)<<16)&0xFF0000ULL));}
/* copy data bus value from other pin mask */
#define W65816_COPY_DATA(p0,p1) (((p0)&~0xFF0000ULL)|((p1)&0xFF0000ULL))
/* return a pin mask with control-pins, address and data bus */
#define W65816_MAKE_PINS(ctrl, addr, data) ((ctrl)|(((data)<<16)&0xFF0000ULL)|((addr)&0xFFFFULL)|((addr<<16)&0xFF00000000ULL))

#ifdef __cplusplus
} /* extern "C" */
#endif

/*-- IMPLEMENTATION ----------------------------------------------------------*/
#ifdef CHIPS_IMPL
#include <string.h>
#ifndef CHIPS_ASSERT
    #include <assert.h>
    #define CHIPS_ASSERT(c) assert(c)
#endif

#if defined(__GNUC__)
#define _W65816_UNREACHABLE __builtin_unreachable()
#elif defined(_MSC_VER)
#define _W65816_UNREACHABLE __assume(0)
#else
#define _W65816_UNREACHABLE
#endif

/* register access macros */
#define _E(c) ((bool)c->emulation)
#define _a8(c) ((_E(c)||(bool)(c->P&W65816_MF)))
#define _i8(c) ((_E(c)||(bool)(c->P&W65816_XF)))
#define _A(c) (*(((uint8_t*)(void*)&c->C)))
#define _B(c) (*(((uint8_t*)((void*)&c->C))+1))
#define _C(c) (*((uint16_t*)(&c->C)))
#define _X(c) (*(((uint8_t*)(void*)&c->X)))
#define _XH(c) (*(((uint8_t*)((void*)&c->X))+1))
#define _X16(c) (*(((uint16_t*)(void*)&c->X)))
#define _Y(c) (*(((uint8_t*)(void*)&c->Y)))
#define _YH(c) (*(((uint8_t*)((void*)&c->Y))+1))
#define _Y16(c) (*(((uint16_t*)(void*)&c->Y)))
#define _S(c) (*(((uint8_t*)(void*)&c->S)))

/* register access functions */
void w65816_set_a(w65816_t* cpu, uint8_t v) { _A(cpu) = v; }
void w65816_set_b(w65816_t* cpu, uint8_t v) { _B(cpu) = v; }
void w65816_set_c(w65816_t* cpu, uint16_t v) { cpu->C = v; }
void w65816_set_x(w65816_t* cpu, uint16_t v) { cpu->X = v; }
void w65816_set_y(w65816_t* cpu, uint16_t v) { cpu->Y = v; }
void w65816_set_s(w65816_t* cpu, uint16_t v) { cpu->S = v; }
void w65816_set_p(w65816_t* cpu, uint8_t v) { cpu->P = v; }
void w65816_set_e(w65816_t* cpu, bool v) { cpu->emulation = v; }
void w65816_set_pc(w65816_t* cpu, uint16_t v) { cpu->PC = v; }
void w65816_set_pb(w65816_t* cpu, uint8_t v) { cpu->PBR = v; }
void w65816_set_db(w65816_t* cpu, uint8_t v) { cpu->DBR = v; }
uint8_t w65816_a(w65816_t* cpu) { return _A(cpu); }
uint8_t w65816_b(w65816_t* cpu) { return _B(cpu); }
uint16_t w65816_c(w65816_t* cpu) { return cpu->C; }
uint16_t w65816_x(w65816_t* cpu) { return cpu->X; }
uint16_t w65816_y(w65816_t* cpu) { return cpu->Y; }
uint16_t w65816_s(w65816_t* cpu) { return cpu->S; }
uint8_t w65816_p(w65816_t* cpu) { return cpu->P; }
bool w65816_e(w65816_t* cpu) { return cpu->emulation; }
uint16_t w65816_pc(w65816_t* cpu) { return cpu->PC; }
uint8_t w65816_pb(w65816_t* cpu) { return cpu->PBR; }
uint8_t w65816_db(w65816_t* cpu) { return cpu->DBR; }

/* helper macros and functions for code-generated instruction decoder */
#define _W65816_NZ(p,v) ((p&~(W65816_NF|W65816_ZF))|((v&0xFF)?(v&W65816_NF):W65816_ZF))

static inline void _w65816_adc(w65816_t* cpu, uint8_t val) {
    if (cpu->bcd_enabled && (cpu->P & W65816_DF)) {
        /* decimal mode (credit goes to MAME) */
        uint8_t c = cpu->P & W65816_CF ? 1 : 0;
        cpu->P &= ~(W65816_NF|W65816_VF|W65816_ZF|W65816_CF);
        uint8_t al = (_A(cpu) & 0x0F) + (val & 0x0F) + c;
        if (al > 9) {
            al += 6;
        }
        uint8_t ah = (_A(cpu) >> 4) + (val >> 4) + (al > 0x0F);
        if (0 == (uint8_t)(_A(cpu) + val + c)) {
            cpu->P |= W65816_ZF;
        }
        else if (ah & 0x08) {
            cpu->P |= W65816_NF;
        }
        if (~(_A(cpu)^val) & (_A(cpu)^(ah<<4)) & 0x80) {
            cpu->P |= W65816_VF;
        }
        if (ah > 9) {
            ah += 6;
        }
        if (ah > 15) {
            cpu->P |= W65816_CF;
        }
        _A(cpu) = (ah<<4) | (al & 0x0F);
    }
    else {
        /* default mode */
        uint16_t sum = _A(cpu) + val + (cpu->P & W65816_CF ? 1:0);
        cpu->P &= ~(W65816_VF|W65816_CF);
        cpu->P = _W65816_NZ(cpu->P,sum);
        if (~(_A(cpu)^val) & (_A(cpu)^sum) & 0x80) {
            cpu->P |= W65816_VF;
        }
        if (sum & 0xFF00) {
            cpu->P |= W65816_CF;
        }
        _A(cpu) = sum & 0xFF;
    }
}

static inline void _w65816_sbc(w65816_t* cpu, uint8_t val) {
    if (cpu->bcd_enabled && (cpu->P & W65816_DF)) {
        /* decimal mode (credit goes to MAME) */
        uint8_t c = cpu->P & W65816_CF ? 0 : 1;
        cpu->P &= ~(W65816_NF|W65816_VF|W65816_ZF|W65816_CF);
        uint16_t diff = _A(cpu) - val - c;
        uint8_t al = (_A(cpu) & 0x0F) - (val & 0x0F) - c;
        if ((int8_t)al < 0) {
            al -= 6;
        }
        uint8_t ah = (_A(cpu)>>4) - (val>>4) - ((int8_t)al < 0);
        if (0 == (uint8_t)diff) {
            cpu->P |= W65816_ZF;
        }
        else if (diff & 0x80) {
            cpu->P |= W65816_NF;
        }
        if ((_A(cpu)^val) & (_A(cpu)^diff) & 0x80) {
            cpu->P |= W65816_VF;
        }
        if (!(diff & 0xFF00)) {
            cpu->P |= W65816_CF;
        }
        if (ah & 0x80) {
            ah -= 6;
        }
        _A(cpu) = (ah<<4) | (al & 0x0F);
    }
    else {
        /* default mode */
        uint16_t diff = _A(cpu) - val - (cpu->P & W65816_CF ? 0 : 1);
        cpu->P &= ~(W65816_VF|W65816_CF);
        cpu->P = _W65816_NZ(cpu->P, (uint8_t)diff);
        if ((_A(cpu)^val) & (_A(cpu)^diff) & 0x80) {
            cpu->P |= W65816_VF;
        }
        if (!(diff & 0xFF00)) {
            cpu->P |= W65816_CF;
        }
        _A(cpu) = diff & 0xFF;
    }
}

static inline void _w65816_cmp(w65816_t* cpu, uint8_t r, uint8_t v) {
    uint16_t t = r - v;
    cpu->P = (_W65816_NZ(cpu->P, (uint8_t)t) & ~W65816_CF) | ((t & 0xFF00) ? 0:W65816_CF);
}

static inline uint8_t _w65816_asl(w65816_t* cpu, uint8_t v) {
    cpu->P = (_W65816_NZ(cpu->P, v<<1) & ~W65816_CF) | ((v & 0x80) ? W65816_CF:0);
    return v<<1;
}

static inline uint8_t _w65816_lsr(w65816_t* cpu, uint8_t v) {
    cpu->P = (_W65816_NZ(cpu->P, v>>1) & ~W65816_CF) | ((v & 0x01) ? W65816_CF:0);
    return v>>1;
}

static inline uint8_t _w65816_rol(w65816_t* cpu, uint8_t v) {
    bool carry = cpu->P & W65816_CF;
    cpu->P &= ~(W65816_NF|W65816_ZF|W65816_CF);
    if (v & 0x80) {
        cpu->P |= W65816_CF;
    }
    v <<= 1;
    if (carry) {
        v |= 1;
    }
    cpu->P = _W65816_NZ(cpu->P, v);
    return v;
}

static inline uint8_t _w65816_ror(w65816_t* cpu, uint8_t v) {
    bool carry = cpu->P & W65816_CF;
    cpu->P &= ~(W65816_NF|W65816_ZF|W65816_CF);
    if (v & 1) {
        cpu->P |= W65816_CF;
    }
    v >>= 1;
    if (carry) {
        v |= 0x80;
    }
    cpu->P = _W65816_NZ(cpu->P, v);
    return v;
}

static inline void _w65816_bit(w65816_t* cpu, uint8_t v) {
    uint8_t t = _A(cpu) & v;
    cpu->P &= ~(W65816_NF|W65816_VF|W65816_ZF);
    if (!t) {
        cpu->P |= W65816_ZF;
    }
    cpu->P |= v & (W65816_NF|W65816_VF);
}

static inline void _w65816_xce(w65816_t* cpu) {
    uint8_t e = cpu->emulation;
    cpu->emulation = cpu->P & W65816_CF;
    cpu->P &= ~W65816_CF;
    if (e) {
        cpu->P |= W65816_CF;
    }

    if (!cpu->emulation) {
        cpu->P |= W65816_MF|W65816_XF;
    }
}

static inline void _w65816_xba(w65816_t* cpu) {
    uint8_t t = _A(cpu);
    _A(cpu) = _B(cpu);
    _B(cpu) = t;
}
#undef _W65816_NZ

uint64_t w65816_init(w65816_t* c, const w65816_desc_t* desc) {
    CHIPS_ASSERT(c && desc);
    memset(c, 0, sizeof(*c));
    c->emulation = true; /* start in Emulation mode */
    c->P = W65816_ZF;
    c->bcd_enabled = !desc->bcd_disabled;
    c->PINS = W65816_RW | W65816_VPA | W65816_VDA | W65816_RES;
    return c->PINS;
}

void w65816_snapshot_onsave(w65816_t* snapshot) {
    CHIPS_ASSERT(snapshot);
}

void w65816_snapshot_onload(w65816_t* snapshot, w65816_t* sys) {
    CHIPS_ASSERT(snapshot && sys);
}

/* set 16-bit address in 64-bit pin mask */
#define _SA(addr) pins=(pins&~0xFFFF)|((addr)&0xFFFFULL)
/* extract 16-bit address from pin mask */
#define _GA() ((uint16_t)(pins&0xFFFFULL))
/* set 16-bit address and 8-bit data in 64-bit pin mask */
#define _SAD(addr,data) pins=(pins&~0xFFFFFF)|((((data)&0xFF)<<16)&0xFF0000ULL)|((addr)&0xFFFFULL)
/* set 24-bit address in 64-bit pin mask */
#define _SAL(addr) pins=(pins&~0xFF0000FFFF)|((addr)&0xFFFFULL)|(((addr)<<16)&0xFF00000000ULL)
/* set 8-bit bank address in 64-bit pin mask */
#define _SB(addr) pins=(pins&~0xFF00000000)|(((addr)&0xFFULL)<<32)
/* extract 24-bit address from pin mask */
#define _GAL() ((uint32_t)((pins&0xFFFFULL)|((pins>>16)&0xFF0000ULL)))
/* set 24-bit address and 8-bit data in 64-bit pin mask */
#define _SALD(addr,data) pins=(pins&~0xFFFFFF)|((((data)&0xFF)<<16)&0xFF0000ULL)|((addr)&0xFFFFULL)|(((addr)>>16)&0xFF0000ULL)
/* fetch next opcode byte */
#define _FETCH() _SA(c->PC);_ON(W65816_VPA|W65816_VDA);
/* signal valid program address */
#define _VPA() _ON(W65816_VPA);
/* signal valid data address */
#define _VDA() _ON(W65816_VDA);
/* set 8-bit data in 64-bit pin mask */
#define _SD(data) pins=((pins&~0xFF0000ULL)|(((data&0xFF)<<16)&0xFF0000ULL))
/* extract 8-bit data from 64-bit pin mask */
#define _GD() ((uint8_t)((pins&0xFF0000ULL)>>16))
/* enable control pins */
#define _ON(m) pins|=(m)
/* disable control pins */
#define _OFF(m) pins&=~(m)
/* a memory read tick */
#define _RD() _ON(W65816_RW);
/* a memory write tick */
#define _WR() _OFF(W65816_RW);
/* set N and Z flags depending on value */
#define _NZ(v) c->P=((c->P&~(W65816_NF|W65816_ZF))|((v&0xFF)?(v&W65816_NF):W65816_ZF))
#define _NZ16(v) c->P=((c->P&~(W65816_NF|W65816_ZF))|((v&0xFFFF)?((v>>8)&W65816_NF):W65816_ZF))
/* set Z flag depending on value */
#define _Z(v) c->P=((c->P&~W65816_ZF)|((v&0xFF)?(0):W65816_ZF))
#define _Z16(v) c->P=((c->P&~W65816_ZF)|((v&0xFFFF)?(0):W65816_ZF))

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable:4244)   /* conversion from 'uint16_t' to 'uint8_t', possible loss of data */
#endif

uint64_t w65816_tick(w65816_t* c, uint64_t pins) {
    if (pins & (W65816_VPA|W65816_VDA|W65816_IRQ|W65816_NMI|W65816_RDY|W65816_RES)) {
        // interrupt detection also works in RDY phases, but only NMI is "sticky"

        // NMI is edge-triggered
        if (0 != ((pins & (pins ^ c->PINS)) & W65816_NMI)) {
            c->nmi_pip |= 0x100;
        }
        // IRQ test is level triggered
        if ((pins & W65816_IRQ) && (0 == (c->P & W65816_IF))) {
            c->irq_pip |= 0x100;
        }

        // RDY pin is only checked during read cycles
        if ((pins & (W65816_RW|W65816_RDY)) == (W65816_RW|W65816_RDY)) {
            c->PINS = pins;
            c->irq_pip <<= 1;
            return pins;
        }
        if ((pins & W65816_VPA) && (pins & W65816_VDA)) {
            // load new instruction into 'instruction register' and restart tick counter
            c->IR = _GD()<<3;

            // check IRQ, NMI and RES state
            //  - IRQ is level-triggered and must be active in the full cycle
            //    before SYNC
            //  - NMI is edge-triggered, and the change must have happened in
            //    any cycle before SYNC
            //  - RES behaves slightly different than on a real 65816, we go
            //    into RES state as soon as the pin goes active, from there
            //    on, behaviour is 'standard'
            if (0 != (c->irq_pip & 0x400)) {
                c->brk_flags |= W65816_BRK_IRQ;
            }
            if (0 != (c->nmi_pip & 0xFC00)) {
                c->brk_flags |= W65816_BRK_NMI;
            }
            if (0 != (pins & W65816_RES)) {
                c->brk_flags |= W65816_BRK_RESET;
            }
            c->irq_pip &= 0x3FF;
            c->nmi_pip &= 0x3FF;

            // if interrupt or reset was requested, force a BRK instruction
            if (c->brk_flags) {
                c->IR = 0;
                c->P &= ~W65816_BF;
                pins &= ~W65816_RES;
            }
            else {
                c->PC++;
            }
        }
        // internal operation is default
        _OFF(W65816_VPA|W65816_VDA);
    }
    // reads are default, writes are special
    _RD();
    switch (c->IR++) {
    // <% decoder
    /* BRK s */
        case (0x00<<3)|0: if(0==c->brk_flags){_VPA();}_SA(c->PC);break;
        case (0x00<<3)|1: _VDA();if(0==(c->brk_flags&(W65816_BRK_IRQ|W65816_BRK_NMI))){c->PC++;}_SAD(0x0100|_S(c)--,c->PC>>8);if(0==(c->brk_flags&W65816_BRK_RESET)){_WR();}c->PBR=0;break;
        case (0x00<<3)|2: _VDA();_SAD(0x0100|_S(c)--,c->PC);if(0==(c->brk_flags&W65816_BRK_RESET)){_WR();}break;
        case (0x00<<3)|3: _VDA();_SAD(0x0100|_S(c)--,c->P|W65816_UF);if(c->brk_flags&W65816_BRK_RESET){c->AD=0xFFFC;}else{_WR();if(c->brk_flags&W65816_BRK_NMI){c->AD=0xFFFA;}else{c->AD=0xFFFE;}}break;
        case (0x00<<3)|4: _VDA();_SA(c->AD++);c->P|=(W65816_IF|W65816_BF);c->P&=W65816_DF;c->brk_flags=0; /* RES/NMI hijacking */break;
        case (0x00<<3)|5: _VDA();_SA(c->AD);c->AD=_GD(); /* NMI "half-hijacking" not possible */break;
        case (0x00<<3)|6: c->PC=(_GD()<<8)|c->AD;_FETCH();break;
        case (0x00<<3)|7: assert(false);break;
    /* ORA (d,x) */
        case (0x01<<3)|0: _VPA();_SA(c->PC++);break;
        case (0x01<<3)|1: c->AD=_GD();_SA(c->AD);break;
        case (0x01<<3)|2: _VDA();c->AD=(c->AD+_X(c))&0xFF;_SA(c->AD);break;
        case (0x01<<3)|3: _VDA();_SA((c->AD+1)&0xFF);c->AD=_GD();break;
        case (0x01<<3)|4: _VDA();_SA((_GD()<<8)|c->AD);break;
        case (0x01<<3)|5: _A(c)|=_GD();_NZ(_A(c));_FETCH();break;
        case (0x01<<3)|6: assert(false);break;
        case (0x01<<3)|7: assert(false);break;
    /* COP s */
        case (0x02<<3)|0: if(0==c->brk_flags){_VPA();}_SA(c->PC);break;
        case (0x02<<3)|1: _VDA();_SAD(0x0100|_S(c)--,c->PC>>8);_WR();c->PBR=0;break;
        case (0x02<<3)|2: _VDA();_SAD(0x0100|_S(c)--,c->PC);_WR();break;
        case (0x02<<3)|3: _VDA();_SAD(0x0100|_S(c)--,c->P|W65816_UF);_WR();c->AD=0xFFF4;break;
        case (0x02<<3)|4: _VDA();_SA(c->AD++);c->P|=W65816_IF;c->P&=W65816_DF;c->brk_flags=0; /* RES/NMI hijacking */break;
        case (0x02<<3)|5: _VDA();_SA(c->AD);c->AD=_GD(); /* NMI "half-hijacking" not possible */break;
        case (0x02<<3)|6: c->PC=(_GD()<<8)|c->AD;break;
        case (0x02<<3)|7: _FETCH();break;
    /* ORA d,s */
        case (0x03<<3)|0: /* (unimpl) */;break;
        case (0x03<<3)|1: _A(c)|=_GD();_NZ(_A(c));break;
        case (0x03<<3)|2: _FETCH();break;
        case (0x03<<3)|3: assert(false);break;
        case (0x03<<3)|4: assert(false);break;
        case (0x03<<3)|5: assert(false);break;
        case (0x03<<3)|6: assert(false);break;
        case (0x03<<3)|7: assert(false);break;
    /* TSB d */
        case (0x04<<3)|0: _VPA();_SA(c->PC++);break;
        case (0x04<<3)|1: _VDA();_SA(_GD());break;
        case (0x04<<3)|2: c->AD=_GD();if(_E(c)){_WR();}break;
        case (0x04<<3)|3: _VDA();_SD(_A(c)|c->AD);_WR();_Z(_A(c)&c->AD);break;
        case (0x04<<3)|4: _FETCH();break;
        case (0x04<<3)|5: assert(false);break;
        case (0x04<<3)|6: assert(false);break;
        case (0x04<<3)|7: assert(false);break;
    /* ORA d */
        case (0x05<<3)|0: _VPA();_SA(c->PC++);break;
        case (0x05<<3)|1: _VDA();_SA(_GD());break;
        case (0x05<<3)|2: _A(c)|=_GD();_NZ(_A(c));_FETCH();break;
        case (0x05<<3)|3: assert(false);break;
        case (0x05<<3)|4: assert(false);break;
        case (0x05<<3)|5: assert(false);break;
        case (0x05<<3)|6: assert(false);break;
        case (0x05<<3)|7: assert(false);break;
    /* ASL d */
        case (0x06<<3)|0: _VPA();_SA(c->PC++);break;
        case (0x06<<3)|1: _VDA();_SA(_GD());break;
        case (0x06<<3)|2: _VDA();c->AD=_GD();_WR();break;
        case (0x06<<3)|3: _VDA();_SD(_w65816_asl(c,c->AD));_WR();break;
        case (0x06<<3)|4: _FETCH();break;
        case (0x06<<3)|5: assert(false);break;
        case (0x06<<3)|6: assert(false);break;
        case (0x06<<3)|7: assert(false);break;
    /* ORA [d] */
        case (0x07<<3)|0: /* (unimpl) */;break;
        case (0x07<<3)|1: _A(c)|=_GD();_NZ(_A(c));break;
        case (0x07<<3)|2: _FETCH();break;
        case (0x07<<3)|3: assert(false);break;
        case (0x07<<3)|4: assert(false);break;
        case (0x07<<3)|5: assert(false);break;
        case (0x07<<3)|6: assert(false);break;
        case (0x07<<3)|7: assert(false);break;
    /* PHP s */
        case (0x08<<3)|0: _SA(c->PC);break;
        case (0x08<<3)|1: _VDA();_SAD(0x0100|_S(c)--,c->P|W65816_UF);_WR();break;
        case (0x08<<3)|2: _FETCH();break;
        case (0x08<<3)|3: assert(false);break;
        case (0x08<<3)|4: assert(false);break;
        case (0x08<<3)|5: assert(false);break;
        case (0x08<<3)|6: assert(false);break;
        case (0x08<<3)|7: assert(false);break;
    /* ORA # */
        case (0x09<<3)|0: _VPA();_SA(c->PC++);break;
        case (0x09<<3)|1: _A(c)|=_GD();_NZ(_A(c));_FETCH();break;
        case (0x09<<3)|2: assert(false);break;
        case (0x09<<3)|3: assert(false);break;
        case (0x09<<3)|4: assert(false);break;
        case (0x09<<3)|5: assert(false);break;
        case (0x09<<3)|6: assert(false);break;
        case (0x09<<3)|7: assert(false);break;
    /* ASL A */
        case (0x0A<<3)|0: _SA(c->PC);break;
        case (0x0A<<3)|1: _A(c)=_w65816_asl(c,_A(c));_FETCH();break;
        case (0x0A<<3)|2: assert(false);break;
        case (0x0A<<3)|3: assert(false);break;
        case (0x0A<<3)|4: assert(false);break;
        case (0x0A<<3)|5: assert(false);break;
        case (0x0A<<3)|6: assert(false);break;
        case (0x0A<<3)|7: assert(false);break;
    /* PHD s */
        case (0x0B<<3)|0: _SA(c->PC);break;
        case (0x0B<<3)|1: _VDA();_SAD(0x0100|_S(c)--,c->D>>8);_WR();break;
        case (0x0B<<3)|2: _VDA();_SAD(0x0100|_S(c)--,c->D);_WR();break;
        case (0x0B<<3)|3: _FETCH();break;
        case (0x0B<<3)|4: assert(false);break;
        case (0x0B<<3)|5: assert(false);break;
        case (0x0B<<3)|6: assert(false);break;
        case (0x0B<<3)|7: assert(false);break;
    /* TSB a */
        case (0x0C<<3)|0: _VPA();_SA(c->PC++);break;
        case (0x0C<<3)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0x0C<<3)|2: _SA((_GD()<<8)|c->AD);break;
        case (0x0C<<3)|3: c->AD=_GD();if(_E(c)){_WR();}break;
        case (0x0C<<3)|4: _VDA();_SD(_A(c)|c->AD);_WR();_Z(_A(c)&c->AD);break;
        case (0x0C<<3)|5: _FETCH();break;
        case (0x0C<<3)|6: assert(false);break;
        case (0x0C<<3)|7: assert(false);break;
    /* ORA a */
        case (0x0D<<3)|0: _VPA();_SA(c->PC++);break;
        case (0x0D<<3)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0x0D<<3)|2: _SA((_GD()<<8)|c->AD);break;
        case (0x0D<<3)|3: _A(c)|=_GD();_NZ(_A(c));_FETCH();break;
        case (0x0D<<3)|4: assert(false);break;
        case (0x0D<<3)|5: assert(false);break;
        case (0x0D<<3)|6: assert(false);break;
        case (0x0D<<3)|7: assert(false);break;
    /* ASL a */
        case (0x0E<<3)|0: _VPA();_SA(c->PC++);break;
        case (0x0E<<3)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0x0E<<3)|2: _SA((_GD()<<8)|c->AD);break;
        case (0x0E<<3)|3: _VDA();c->AD=_GD();_WR();break;
        case (0x0E<<3)|4: _VDA();_SD(_w65816_asl(c,c->AD));_WR();break;
        case (0x0E<<3)|5: _FETCH();break;
        case (0x0E<<3)|6: assert(false);break;
        case (0x0E<<3)|7: assert(false);break;
    /* ORA al */
        case (0x0F<<3)|0: /* (unimpl) */;break;
        case (0x0F<<3)|1: _A(c)|=_GD();_NZ(_A(c));break;
        case (0x0F<<3)|2: _FETCH();break;
        case (0x0F<<3)|3: assert(false);break;
        case (0x0F<<3)|4: assert(false);break;
        case (0x0F<<3)|5: assert(false);break;
        case (0x0F<<3)|6: assert(false);break;
        case (0x0F<<3)|7: assert(false);break;
    /* BPL r */
        case (0x10<<3)|0: _VPA();_SA(c->PC++);break;
        case (0x10<<3)|1: _SA(c->PC);c->AD=c->PC+(int8_t)_GD();if((c->P&0x80)!=0x0){_FETCH();};break;
        case (0x10<<3)|2: _SA((c->PC&0xFF00)|(c->AD&0x00FF));if((c->AD&0xFF00)==(c->PC&0xFF00)){c->PC=c->AD;c->irq_pip>>=1;c->nmi_pip>>=1;_FETCH();};break;
        case (0x10<<3)|3: c->PC=c->AD;_FETCH();break;
        case (0x10<<3)|4: assert(false);break;
        case (0x10<<3)|5: assert(false);break;
        case (0x10<<3)|6: assert(false);break;
        case (0x10<<3)|7: assert(false);break;
    /* ORA (d),y */
        case (0x11<<3)|0: _VPA();_SA(c->PC++);break;
        case (0x11<<3)|1: _VDA();c->AD=_GD();_SA(c->AD);break;
        case (0x11<<3)|2: _VDA();_SA((c->AD+1)&0xFF);c->AD=_GD();break;
        case (0x11<<3)|3: c->AD|=_GD()<<8;_SA((c->AD&0xFF00)|((c->AD+_Y(c))&0xFF));c->IR+=(~((c->AD>>8)-((c->AD+_Y(c))>>8)))&1;break;
        case (0x11<<3)|4: _VDA();_SA(c->AD+_Y(c));break;
        case (0x11<<3)|5: _A(c)|=_GD();_NZ(_A(c));_FETCH();break;
        case (0x11<<3)|6: assert(false);break;
        case (0x11<<3)|7: assert(false);break;
    /* ORA (d) */
        case (0x12<<3)|0: /* (unimpl) */;break;
        case (0x12<<3)|1: _A(c)|=_GD();_NZ(_A(c));break;
        case (0x12<<3)|2: _FETCH();break;
        case (0x12<<3)|3: assert(false);break;
        case (0x12<<3)|4: assert(false);break;
        case (0x12<<3)|5: assert(false);break;
        case (0x12<<3)|6: assert(false);break;
        case (0x12<<3)|7: assert(false);break;
    /* ORA (d,s),y */
        case (0x13<<3)|0: /* (unimpl) */;break;
        case (0x13<<3)|1: _A(c)|=_GD();_NZ(_A(c));break;
        case (0x13<<3)|2: _FETCH();break;
        case (0x13<<3)|3: assert(false);break;
        case (0x13<<3)|4: assert(false);break;
        case (0x13<<3)|5: assert(false);break;
        case (0x13<<3)|6: assert(false);break;
        case (0x13<<3)|7: assert(false);break;
    /* TRB d */
        case (0x14<<3)|0: _VPA();_SA(c->PC++);break;
        case (0x14<<3)|1: _VDA();_SA(_GD());break;
        case (0x14<<3)|2: c->AD=_GD();if(_E(c)){_WR();}break;
        case (0x14<<3)|3: _VDA();_SD(~_A(c)&c->AD);_WR();_Z(_A(c)&c->AD);break;
        case (0x14<<3)|4: _FETCH();break;
        case (0x14<<3)|5: assert(false);break;
        case (0x14<<3)|6: assert(false);break;
        case (0x14<<3)|7: assert(false);break;
    /* ORA d,x */
        case (0x15<<3)|0: _VPA();_SA(c->PC++);break;
        case (0x15<<3)|1: c->AD=_GD();_SA(c->AD);break;
        case (0x15<<3)|2: _VDA();if(_E(c)){_SA((c->AD+_X(c))&0x00FF);}else{_SA(c->AD+_X(c));}break;
        case (0x15<<3)|3: _A(c)|=_GD();_NZ(_A(c));_FETCH();break;
        case (0x15<<3)|4: assert(false);break;
        case (0x15<<3)|5: assert(false);break;
        case (0x15<<3)|6: assert(false);break;
        case (0x15<<3)|7: assert(false);break;
    /* ASL d,x */
        case (0x16<<3)|0: _VPA();_SA(c->PC++);break;
        case (0x16<<3)|1: c->AD=_GD();_SA(c->AD);break;
        case (0x16<<3)|2: _VDA();if(_E(c)){_SA((c->AD+_X(c))&0x00FF);}else{_SA(c->AD+_X(c));}break;
        case (0x16<<3)|3: _VDA();c->AD=_GD();_WR();break;
        case (0x16<<3)|4: _VDA();_SD(_w65816_asl(c,c->AD));_WR();break;
        case (0x16<<3)|5: _FETCH();break;
        case (0x16<<3)|6: assert(false);break;
        case (0x16<<3)|7: assert(false);break;
    /* ORA [d],y */
        case (0x17<<3)|0: /* (unimpl) */;break;
        case (0x17<<3)|1: _A(c)|=_GD();_NZ(_A(c));break;
        case (0x17<<3)|2: _FETCH();break;
        case (0x17<<3)|3: assert(false);break;
        case (0x17<<3)|4: assert(false);break;
        case (0x17<<3)|5: assert(false);break;
        case (0x17<<3)|6: assert(false);break;
        case (0x17<<3)|7: assert(false);break;
    /* CLE i */
        case (0x18<<3)|0: _SA(c->PC);break;
        case (0x18<<3)|1: c->P&=~0x1;_FETCH();break;
        case (0x18<<3)|2: assert(false);break;
        case (0x18<<3)|3: assert(false);break;
        case (0x18<<3)|4: assert(false);break;
        case (0x18<<3)|5: assert(false);break;
        case (0x18<<3)|6: assert(false);break;
        case (0x18<<3)|7: assert(false);break;
    /* ORA a,y */
        case (0x19<<3)|0: _VPA();_SA(c->PC++);break;
        case (0x19<<3)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0x19<<3)|2: c->AD|=_GD()<<8;_SA((c->AD&0xFF00)|((c->AD+_Y(c))&0xFF));c->IR+=(~((c->AD>>8)-((c->AD+_Y(c))>>8)))&1;break;
        case (0x19<<3)|3: _VDA();_SA(c->AD+_Y(c));break;
        case (0x19<<3)|4: _A(c)|=_GD();_NZ(_A(c));_FETCH();break;
        case (0x19<<3)|5: assert(false);break;
        case (0x19<<3)|6: assert(false);break;
        case (0x19<<3)|7: assert(false);break;
    /* INC A */
        case (0x1A<<3)|0: _SA(c->PC);break;
        case (0x1A<<3)|1: _A(c)++;_NZ(_A(c));_FETCH();break;
        case (0x1A<<3)|2: assert(false);break;
        case (0x1A<<3)|3: assert(false);break;
        case (0x1A<<3)|4: assert(false);break;
        case (0x1A<<3)|5: assert(false);break;
        case (0x1A<<3)|6: assert(false);break;
        case (0x1A<<3)|7: assert(false);break;
    /* TCS i */
        case (0x1B<<3)|0: _SA(c->PC);break;
        case (0x1B<<3)|1: c->S=c->C;_FETCH();break;
        case (0x1B<<3)|2: assert(false);break;
        case (0x1B<<3)|3: assert(false);break;
        case (0x1B<<3)|4: assert(false);break;
        case (0x1B<<3)|5: assert(false);break;
        case (0x1B<<3)|6: assert(false);break;
        case (0x1B<<3)|7: assert(false);break;
    /* TRB a */
        case (0x1C<<3)|0: _VPA();_SA(c->PC++);break;
        case (0x1C<<3)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0x1C<<3)|2: _SA((_GD()<<8)|c->AD);break;
        case (0x1C<<3)|3: c->AD=_GD();if(_E(c)){_WR();}break;
        case (0x1C<<3)|4: _VDA();_SD(~_A(c)&c->AD);_WR();_Z(_A(c)&c->AD);break;
        case (0x1C<<3)|5: _FETCH();break;
        case (0x1C<<3)|6: assert(false);break;
        case (0x1C<<3)|7: assert(false);break;
    /* ORA a,x */
        case (0x1D<<3)|0: _VPA();_SA(c->PC++);break;
        case (0x1D<<3)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0x1D<<3)|2: c->AD|=_GD()<<8;_SA((c->AD&0xFF00)|((c->AD+_X(c))&0xFF));c->IR+=(~((c->AD>>8)-((c->AD+_X(c))>>8)))&1;break;
        case (0x1D<<3)|3: _VDA();_SA(c->AD+_X(c));break;
        case (0x1D<<3)|4: _A(c)|=_GD();_NZ(_A(c));_FETCH();break;
        case (0x1D<<3)|5: assert(false);break;
        case (0x1D<<3)|6: assert(false);break;
        case (0x1D<<3)|7: assert(false);break;
    /* ASL a,x */
        case (0x1E<<3)|0: _VPA();_SA(c->PC++);break;
        case (0x1E<<3)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0x1E<<3)|2: c->AD|=_GD()<<8;_SA((c->AD&0xFF00)|((c->AD+_X(c))&0xFF));break;
        case (0x1E<<3)|3: _VDA();_SA(c->AD+_X(c));break;
        case (0x1E<<3)|4: _VDA();c->AD=_GD();_WR();break;
        case (0x1E<<3)|5: _VDA();_SD(_w65816_asl(c,c->AD));_WR();break;
        case (0x1E<<3)|6: _FETCH();break;
        case (0x1E<<3)|7: assert(false);break;
    /* ORA al,x */
        case (0x1F<<3)|0: /* (unimpl) */;break;
        case (0x1F<<3)|1: _A(c)|=_GD();_NZ(_A(c));break;
        case (0x1F<<3)|2: _FETCH();break;
        case (0x1F<<3)|3: assert(false);break;
        case (0x1F<<3)|4: assert(false);break;
        case (0x1F<<3)|5: assert(false);break;
        case (0x1F<<3)|6: assert(false);break;
        case (0x1F<<3)|7: assert(false);break;
    /* JSR a */
        case (0x20<<3)|0: _VPA();_SA(c->PC++);break;
        case (0x20<<3)|1: _SA(0x0100|_S(c));c->AD=_GD();break;
        case (0x20<<3)|2: _VDA();_SAD(0x0100|_S(c)--,c->PC>>8);_WR();break;
        case (0x20<<3)|3: _VDA();_SAD(0x0100|_S(c)--,c->PC);_WR();break;
        case (0x20<<3)|4: _VPA();_SA(c->PC);break;
        case (0x20<<3)|5: c->PC=(_GD()<<8)|c->AD;_FETCH();break;
        case (0x20<<3)|6: assert(false);break;
        case (0x20<<3)|7: assert(false);break;
    /* AND (d,x) */
        case (0x21<<3)|0: _VPA();_SA(c->PC++);break;
        case (0x21<<3)|1: c->AD=_GD();_SA(c->AD);break;
        case (0x21<<3)|2: _VDA();c->AD=(c->AD+_X(c))&0xFF;_SA(c->AD);break;
        case (0x21<<3)|3: _VDA();_SA((c->AD+1)&0xFF);c->AD=_GD();break;
        case (0x21<<3)|4: _VDA();_SA((_GD()<<8)|c->AD);break;
        case (0x21<<3)|5: _A(c)&=_GD();_NZ(_A(c));_FETCH();break;
        case (0x21<<3)|6: assert(false);break;
        case (0x21<<3)|7: assert(false);break;
    /* JSL al (unimpl) */
        case (0x22<<3)|0: /* (unimpl) */;break;
        case (0x22<<3)|1: break;
        case (0x22<<3)|2: _FETCH();break;
        case (0x22<<3)|3: assert(false);break;
        case (0x22<<3)|4: assert(false);break;
        case (0x22<<3)|5: assert(false);break;
        case (0x22<<3)|6: assert(false);break;
        case (0x22<<3)|7: assert(false);break;
    /* AND d,s */
        case (0x23<<3)|0: /* (unimpl) */;break;
        case (0x23<<3)|1: _A(c)&=_GD();_NZ(_A(c));break;
        case (0x23<<3)|2: _FETCH();break;
        case (0x23<<3)|3: assert(false);break;
        case (0x23<<3)|4: assert(false);break;
        case (0x23<<3)|5: assert(false);break;
        case (0x23<<3)|6: assert(false);break;
        case (0x23<<3)|7: assert(false);break;
    /* BIT d */
        case (0x24<<3)|0: _VPA();_SA(c->PC++);break;
        case (0x24<<3)|1: _VDA();_SA(_GD());break;
        case (0x24<<3)|2: _w65816_bit(c,_GD());_FETCH();break;
        case (0x24<<3)|3: assert(false);break;
        case (0x24<<3)|4: assert(false);break;
        case (0x24<<3)|5: assert(false);break;
        case (0x24<<3)|6: assert(false);break;
        case (0x24<<3)|7: assert(false);break;
    /* AND d */
        case (0x25<<3)|0: _VPA();_SA(c->PC++);break;
        case (0x25<<3)|1: _VDA();_SA(_GD());break;
        case (0x25<<3)|2: _A(c)&=_GD();_NZ(_A(c));_FETCH();break;
        case (0x25<<3)|3: assert(false);break;
        case (0x25<<3)|4: assert(false);break;
        case (0x25<<3)|5: assert(false);break;
        case (0x25<<3)|6: assert(false);break;
        case (0x25<<3)|7: assert(false);break;
    /* ROL d */
        case (0x26<<3)|0: _VPA();_SA(c->PC++);break;
        case (0x26<<3)|1: _VDA();_SA(_GD());break;
        case (0x26<<3)|2: _VDA();c->AD=_GD();_WR();break;
        case (0x26<<3)|3: _VDA();_SD(_w65816_rol(c,c->AD));_WR();break;
        case (0x26<<3)|4: _FETCH();break;
        case (0x26<<3)|5: assert(false);break;
        case (0x26<<3)|6: assert(false);break;
        case (0x26<<3)|7: assert(false);break;
    /* AND [d] */
        case (0x27<<3)|0: /* (unimpl) */;break;
        case (0x27<<3)|1: _A(c)&=_GD();_NZ(_A(c));break;
        case (0x27<<3)|2: _FETCH();break;
        case (0x27<<3)|3: assert(false);break;
        case (0x27<<3)|4: assert(false);break;
        case (0x27<<3)|5: assert(false);break;
        case (0x27<<3)|6: assert(false);break;
        case (0x27<<3)|7: assert(false);break;
    /* PLP s */
        case (0x28<<3)|0: _SA(c->PC);break;
        case (0x28<<3)|1: _SA(c->PC);break;
        case (0x28<<3)|2: _VDA();_SA(0x0100|++_S(c));break;
        case (0x28<<3)|3: c->P=(_GD()|W65816_BF)&~W65816_UF;_FETCH();break;
        case (0x28<<3)|4: assert(false);break;
        case (0x28<<3)|5: assert(false);break;
        case (0x28<<3)|6: assert(false);break;
        case (0x28<<3)|7: assert(false);break;
    /* AND # */
        case (0x29<<3)|0: _VPA();_SA(c->PC++);break;
        case (0x29<<3)|1: _A(c)&=_GD();_NZ(_A(c));_FETCH();break;
        case (0x29<<3)|2: assert(false);break;
        case (0x29<<3)|3: assert(false);break;
        case (0x29<<3)|4: assert(false);break;
        case (0x29<<3)|5: assert(false);break;
        case (0x29<<3)|6: assert(false);break;
        case (0x29<<3)|7: assert(false);break;
    /* ROL A */
        case (0x2A<<3)|0: _SA(c->PC);break;
        case (0x2A<<3)|1: _A(c)=_w65816_rol(c,_A(c));_FETCH();break;
        case (0x2A<<3)|2: assert(false);break;
        case (0x2A<<3)|3: assert(false);break;
        case (0x2A<<3)|4: assert(false);break;
        case (0x2A<<3)|5: assert(false);break;
        case (0x2A<<3)|6: assert(false);break;
        case (0x2A<<3)|7: assert(false);break;
    /* PLD s */
        case (0x2B<<3)|0: _SA(c->PC);break;
        case (0x2B<<3)|1: _SA(c->PC);break;
        case (0x2B<<3)|2: _VDA();_SA(0x0100|_S(c)++);break;
        case (0x2B<<3)|3: _VDA();_SA(0x0100|_S(c));c->AD=_GD();break;
        case (0x2B<<3)|4: c->D=(_GD()<<8)|c->AD;_FETCH();break;
        case (0x2B<<3)|5: assert(false);break;
        case (0x2B<<3)|6: assert(false);break;
        case (0x2B<<3)|7: assert(false);break;
    /* BIT a */
        case (0x2C<<3)|0: _VPA();_SA(c->PC++);break;
        case (0x2C<<3)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0x2C<<3)|2: _SA((_GD()<<8)|c->AD);break;
        case (0x2C<<3)|3: _w65816_bit(c,_GD());_FETCH();break;
        case (0x2C<<3)|4: assert(false);break;
        case (0x2C<<3)|5: assert(false);break;
        case (0x2C<<3)|6: assert(false);break;
        case (0x2C<<3)|7: assert(false);break;
    /* AND a */
        case (0x2D<<3)|0: _VPA();_SA(c->PC++);break;
        case (0x2D<<3)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0x2D<<3)|2: _SA((_GD()<<8)|c->AD);break;
        case (0x2D<<3)|3: _A(c)&=_GD();_NZ(_A(c));_FETCH();break;
        case (0x2D<<3)|4: assert(false);break;
        case (0x2D<<3)|5: assert(false);break;
        case (0x2D<<3)|6: assert(false);break;
        case (0x2D<<3)|7: assert(false);break;
    /* ROL a */
        case (0x2E<<3)|0: _VPA();_SA(c->PC++);break;
        case (0x2E<<3)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0x2E<<3)|2: _SA((_GD()<<8)|c->AD);break;
        case (0x2E<<3)|3: _VDA();c->AD=_GD();_WR();break;
        case (0x2E<<3)|4: _VDA();_SD(_w65816_rol(c,c->AD));_WR();break;
        case (0x2E<<3)|5: _FETCH();break;
        case (0x2E<<3)|6: assert(false);break;
        case (0x2E<<3)|7: assert(false);break;
    /* AND al */
        case (0x2F<<3)|0: /* (unimpl) */;break;
        case (0x2F<<3)|1: _A(c)&=_GD();_NZ(_A(c));break;
        case (0x2F<<3)|2: _FETCH();break;
        case (0x2F<<3)|3: assert(false);break;
        case (0x2F<<3)|4: assert(false);break;
        case (0x2F<<3)|5: assert(false);break;
        case (0x2F<<3)|6: assert(false);break;
        case (0x2F<<3)|7: assert(false);break;
    /* BMI r */
        case (0x30<<3)|0: _VPA();_SA(c->PC++);break;
        case (0x30<<3)|1: _SA(c->PC);c->AD=c->PC+(int8_t)_GD();if((c->P&0x80)!=0x80){_FETCH();};break;
        case (0x30<<3)|2: _SA((c->PC&0xFF00)|(c->AD&0x00FF));if((c->AD&0xFF00)==(c->PC&0xFF00)){c->PC=c->AD;c->irq_pip>>=1;c->nmi_pip>>=1;_FETCH();};break;
        case (0x30<<3)|3: c->PC=c->AD;_FETCH();break;
        case (0x30<<3)|4: assert(false);break;
        case (0x30<<3)|5: assert(false);break;
        case (0x30<<3)|6: assert(false);break;
        case (0x30<<3)|7: assert(false);break;
    /* AND (d),y */
        case (0x31<<3)|0: _VPA();_SA(c->PC++);break;
        case (0x31<<3)|1: _VDA();c->AD=_GD();_SA(c->AD);break;
        case (0x31<<3)|2: _VDA();_SA((c->AD+1)&0xFF);c->AD=_GD();break;
        case (0x31<<3)|3: c->AD|=_GD()<<8;_SA((c->AD&0xFF00)|((c->AD+_Y(c))&0xFF));c->IR+=(~((c->AD>>8)-((c->AD+_Y(c))>>8)))&1;break;
        case (0x31<<3)|4: _VDA();_SA(c->AD+_Y(c));break;
        case (0x31<<3)|5: _A(c)&=_GD();_NZ(_A(c));_FETCH();break;
        case (0x31<<3)|6: assert(false);break;
        case (0x31<<3)|7: assert(false);break;
    /* AND (d) */
        case (0x32<<3)|0: /* (unimpl) */;break;
        case (0x32<<3)|1: _A(c)&=_GD();_NZ(_A(c));break;
        case (0x32<<3)|2: _FETCH();break;
        case (0x32<<3)|3: assert(false);break;
        case (0x32<<3)|4: assert(false);break;
        case (0x32<<3)|5: assert(false);break;
        case (0x32<<3)|6: assert(false);break;
        case (0x32<<3)|7: assert(false);break;
    /* AND (d,s),y */
        case (0x33<<3)|0: /* (unimpl) */;break;
        case (0x33<<3)|1: _A(c)&=_GD();_NZ(_A(c));break;
        case (0x33<<3)|2: _FETCH();break;
        case (0x33<<3)|3: assert(false);break;
        case (0x33<<3)|4: assert(false);break;
        case (0x33<<3)|5: assert(false);break;
        case (0x33<<3)|6: assert(false);break;
        case (0x33<<3)|7: assert(false);break;
    /* BIT d,x */
        case (0x34<<3)|0: _VPA();_SA(c->PC++);break;
        case (0x34<<3)|1: c->AD=_GD();_SA(c->AD);break;
        case (0x34<<3)|2: _VDA();if(_E(c)){_SA((c->AD+_X(c))&0x00FF);}else{_SA(c->AD+_X(c));}break;
        case (0x34<<3)|3: _w65816_bit(c,_GD());_FETCH();break;
        case (0x34<<3)|4: assert(false);break;
        case (0x34<<3)|5: assert(false);break;
        case (0x34<<3)|6: assert(false);break;
        case (0x34<<3)|7: assert(false);break;
    /* AND d,x */
        case (0x35<<3)|0: _VPA();_SA(c->PC++);break;
        case (0x35<<3)|1: c->AD=_GD();_SA(c->AD);break;
        case (0x35<<3)|2: _VDA();if(_E(c)){_SA((c->AD+_X(c))&0x00FF);}else{_SA(c->AD+_X(c));}break;
        case (0x35<<3)|3: _A(c)&=_GD();_NZ(_A(c));_FETCH();break;
        case (0x35<<3)|4: assert(false);break;
        case (0x35<<3)|5: assert(false);break;
        case (0x35<<3)|6: assert(false);break;
        case (0x35<<3)|7: assert(false);break;
    /* ROL d,x */
        case (0x36<<3)|0: _VPA();_SA(c->PC++);break;
        case (0x36<<3)|1: c->AD=_GD();_SA(c->AD);break;
        case (0x36<<3)|2: _VDA();if(_E(c)){_SA((c->AD+_X(c))&0x00FF);}else{_SA(c->AD+_X(c));}break;
        case (0x36<<3)|3: _VDA();c->AD=_GD();_WR();break;
        case (0x36<<3)|4: _VDA();_SD(_w65816_rol(c,c->AD));_WR();break;
        case (0x36<<3)|5: _FETCH();break;
        case (0x36<<3)|6: assert(false);break;
        case (0x36<<3)|7: assert(false);break;
    /* AND [d],y */
        case (0x37<<3)|0: /* (unimpl) */;break;
        case (0x37<<3)|1: _A(c)&=_GD();_NZ(_A(c));break;
        case (0x37<<3)|2: _FETCH();break;
        case (0x37<<3)|3: assert(false);break;
        case (0x37<<3)|4: assert(false);break;
        case (0x37<<3)|5: assert(false);break;
        case (0x37<<3)|6: assert(false);break;
        case (0x37<<3)|7: assert(false);break;
    /* SEE i */
        case (0x38<<3)|0: _SA(c->PC);break;
        case (0x38<<3)|1: c->P|=0x1;_FETCH();break;
        case (0x38<<3)|2: assert(false);break;
        case (0x38<<3)|3: assert(false);break;
        case (0x38<<3)|4: assert(false);break;
        case (0x38<<3)|5: assert(false);break;
        case (0x38<<3)|6: assert(false);break;
        case (0x38<<3)|7: assert(false);break;
    /* AND a,y */
        case (0x39<<3)|0: _VPA();_SA(c->PC++);break;
        case (0x39<<3)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0x39<<3)|2: c->AD|=_GD()<<8;_SA((c->AD&0xFF00)|((c->AD+_Y(c))&0xFF));c->IR+=(~((c->AD>>8)-((c->AD+_Y(c))>>8)))&1;break;
        case (0x39<<3)|3: _VDA();_SA(c->AD+_Y(c));break;
        case (0x39<<3)|4: _A(c)&=_GD();_NZ(_A(c));_FETCH();break;
        case (0x39<<3)|5: assert(false);break;
        case (0x39<<3)|6: assert(false);break;
        case (0x39<<3)|7: assert(false);break;
    /* DEC A */
        case (0x3A<<3)|0: _SA(c->PC);break;
        case (0x3A<<3)|1: _A(c)--;_NZ(_A(c));_FETCH();break;
        case (0x3A<<3)|2: assert(false);break;
        case (0x3A<<3)|3: assert(false);break;
        case (0x3A<<3)|4: assert(false);break;
        case (0x3A<<3)|5: assert(false);break;
        case (0x3A<<3)|6: assert(false);break;
        case (0x3A<<3)|7: assert(false);break;
    /* TSC i */
        case (0x3B<<3)|0: _SA(c->PC);break;
        case (0x3B<<3)|1: c->C=c->S;_NZ(c->C);break;
        case (0x3B<<3)|2: _FETCH();break;
        case (0x3B<<3)|3: assert(false);break;
        case (0x3B<<3)|4: assert(false);break;
        case (0x3B<<3)|5: assert(false);break;
        case (0x3B<<3)|6: assert(false);break;
        case (0x3B<<3)|7: assert(false);break;
    /* BIT a,x */
        case (0x3C<<3)|0: _VPA();_SA(c->PC++);break;
        case (0x3C<<3)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0x3C<<3)|2: c->AD|=_GD()<<8;_SA((c->AD&0xFF00)|((c->AD+_X(c))&0xFF));c->IR+=(~((c->AD>>8)-((c->AD+_X(c))>>8)))&1;break;
        case (0x3C<<3)|3: _VDA();_SA(c->AD+_X(c));break;
        case (0x3C<<3)|4: _w65816_bit(c,_GD());_FETCH();break;
        case (0x3C<<3)|5: assert(false);break;
        case (0x3C<<3)|6: assert(false);break;
        case (0x3C<<3)|7: assert(false);break;
    /* AND a,x */
        case (0x3D<<3)|0: _VPA();_SA(c->PC++);break;
        case (0x3D<<3)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0x3D<<3)|2: c->AD|=_GD()<<8;_SA((c->AD&0xFF00)|((c->AD+_X(c))&0xFF));c->IR+=(~((c->AD>>8)-((c->AD+_X(c))>>8)))&1;break;
        case (0x3D<<3)|3: _VDA();_SA(c->AD+_X(c));break;
        case (0x3D<<3)|4: _A(c)&=_GD();_NZ(_A(c));_FETCH();break;
        case (0x3D<<3)|5: assert(false);break;
        case (0x3D<<3)|6: assert(false);break;
        case (0x3D<<3)|7: assert(false);break;
    /* ROL a,x */
        case (0x3E<<3)|0: _VPA();_SA(c->PC++);break;
        case (0x3E<<3)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0x3E<<3)|2: c->AD|=_GD()<<8;_SA((c->AD&0xFF00)|((c->AD+_X(c))&0xFF));break;
        case (0x3E<<3)|3: _VDA();_SA(c->AD+_X(c));break;
        case (0x3E<<3)|4: _VDA();c->AD=_GD();_WR();break;
        case (0x3E<<3)|5: _VDA();_SD(_w65816_rol(c,c->AD));_WR();break;
        case (0x3E<<3)|6: _FETCH();break;
        case (0x3E<<3)|7: assert(false);break;
    /* AND al,x */
        case (0x3F<<3)|0: /* (unimpl) */;break;
        case (0x3F<<3)|1: _A(c)&=_GD();_NZ(_A(c));break;
        case (0x3F<<3)|2: _FETCH();break;
        case (0x3F<<3)|3: assert(false);break;
        case (0x3F<<3)|4: assert(false);break;
        case (0x3F<<3)|5: assert(false);break;
        case (0x3F<<3)|6: assert(false);break;
        case (0x3F<<3)|7: assert(false);break;
    /* RTI s */
        case (0x40<<3)|0: _SA(c->PC);break;
        case (0x40<<3)|1: _SA(0x0100|_S(c)++);break;
        case (0x40<<3)|2: _VDA();_SA(0x0100|_S(c)++);break;
        case (0x40<<3)|3: _VDA();_SA(0x0100|_S(c)++);c->P=(_GD()|W65816_BF)&~W65816_UF;break;
        case (0x40<<3)|4: _VDA();_SA(0x0100|_S(c));c->AD=_GD();break;
        case (0x40<<3)|5: c->PC=(_GD()<<8)|c->AD;_FETCH();break;
        case (0x40<<3)|6: assert(false);break;
        case (0x40<<3)|7: assert(false);break;
    /* EOR (d,x) */
        case (0x41<<3)|0: _VPA();_SA(c->PC++);break;
        case (0x41<<3)|1: c->AD=_GD();_SA(c->AD);break;
        case (0x41<<3)|2: _VDA();c->AD=(c->AD+_X(c))&0xFF;_SA(c->AD);break;
        case (0x41<<3)|3: _VDA();_SA((c->AD+1)&0xFF);c->AD=_GD();break;
        case (0x41<<3)|4: _VDA();_SA((_GD()<<8)|c->AD);break;
        case (0x41<<3)|5: _A(c)^=_GD();_NZ(_A(c));_FETCH();break;
        case (0x41<<3)|6: assert(false);break;
        case (0x41<<3)|7: assert(false);break;
    /* WDM # */
        case (0x42<<3)|0: _VPA();_SA(c->PC++);break;
        case (0x42<<3)|1: _FETCH();break;
        case (0x42<<3)|2: assert(false);break;
        case (0x42<<3)|3: assert(false);break;
        case (0x42<<3)|4: assert(false);break;
        case (0x42<<3)|5: assert(false);break;
        case (0x42<<3)|6: assert(false);break;
        case (0x42<<3)|7: assert(false);break;
    /* EOR d,s */
        case (0x43<<3)|0: /* (unimpl) */;break;
        case (0x43<<3)|1: _A(c)^=_GD();_NZ(_A(c));break;
        case (0x43<<3)|2: _FETCH();break;
        case (0x43<<3)|3: assert(false);break;
        case (0x43<<3)|4: assert(false);break;
        case (0x43<<3)|5: assert(false);break;
        case (0x43<<3)|6: assert(false);break;
        case (0x43<<3)|7: assert(false);break;
    /* MVP xyc */
        case (0x44<<3)|0: _VPA();_SA(c->PC++);break;
        case (0x44<<3)|1: _VPA();c->DBR=_GD();_SA(c->PC);break;
        case (0x44<<3)|2: _VDA();_SB(_GD());_SA(c->X--);break;
        case (0x44<<3)|3: _VDA();_SB(c->DBR);_SA(c->Y--);_WR();break;
        case (0x44<<3)|4: if(c->C){c->PC--;}break;
        case (0x44<<3)|5: c->C--?c->PC--:c->PC++;break;
        case (0x44<<3)|6: _FETCH();break;
        case (0x44<<3)|7: assert(false);break;
    /* EOR d */
        case (0x45<<3)|0: _VPA();_SA(c->PC++);break;
        case (0x45<<3)|1: _VDA();_SA(_GD());break;
        case (0x45<<3)|2: _A(c)^=_GD();_NZ(_A(c));_FETCH();break;
        case (0x45<<3)|3: assert(false);break;
        case (0x45<<3)|4: assert(false);break;
        case (0x45<<3)|5: assert(false);break;
        case (0x45<<3)|6: assert(false);break;
        case (0x45<<3)|7: assert(false);break;
    /* LSR d */
        case (0x46<<3)|0: _VPA();_SA(c->PC++);break;
        case (0x46<<3)|1: _VDA();_SA(_GD());break;
        case (0x46<<3)|2: _VDA();c->AD=_GD();_WR();break;
        case (0x46<<3)|3: _VDA();_SD(_w65816_lsr(c,c->AD));_WR();break;
        case (0x46<<3)|4: _FETCH();break;
        case (0x46<<3)|5: assert(false);break;
        case (0x46<<3)|6: assert(false);break;
        case (0x46<<3)|7: assert(false);break;
    /* EOR [d] */
        case (0x47<<3)|0: /* (unimpl) */;break;
        case (0x47<<3)|1: _A(c)^=_GD();_NZ(_A(c));break;
        case (0x47<<3)|2: _FETCH();break;
        case (0x47<<3)|3: assert(false);break;
        case (0x47<<3)|4: assert(false);break;
        case (0x47<<3)|5: assert(false);break;
        case (0x47<<3)|6: assert(false);break;
        case (0x47<<3)|7: assert(false);break;
    /* PHA s */
        case (0x48<<3)|0: _SA(c->PC);break;
        case (0x48<<3)|1: _VDA();_SAD(0x0100|_S(c)--,_A(c));_WR();break;
        case (0x48<<3)|2: _FETCH();break;
        case (0x48<<3)|3: assert(false);break;
        case (0x48<<3)|4: assert(false);break;
        case (0x48<<3)|5: assert(false);break;
        case (0x48<<3)|6: assert(false);break;
        case (0x48<<3)|7: assert(false);break;
    /* EOR # */
        case (0x49<<3)|0: _VPA();_SA(c->PC++);break;
        case (0x49<<3)|1: _A(c)^=_GD();_NZ(_A(c));_FETCH();break;
        case (0x49<<3)|2: assert(false);break;
        case (0x49<<3)|3: assert(false);break;
        case (0x49<<3)|4: assert(false);break;
        case (0x49<<3)|5: assert(false);break;
        case (0x49<<3)|6: assert(false);break;
        case (0x49<<3)|7: assert(false);break;
    /* LSR A */
        case (0x4A<<3)|0: _SA(c->PC);break;
        case (0x4A<<3)|1: _A(c)=_w65816_lsr(c,_A(c));_FETCH();break;
        case (0x4A<<3)|2: assert(false);break;
        case (0x4A<<3)|3: assert(false);break;
        case (0x4A<<3)|4: assert(false);break;
        case (0x4A<<3)|5: assert(false);break;
        case (0x4A<<3)|6: assert(false);break;
        case (0x4A<<3)|7: assert(false);break;
    /* PHK s */
        case (0x4B<<3)|0: _SA(c->PC);break;
        case (0x4B<<3)|1: _VDA();_SAD(0x0100|_S(c)--,c->PBR);_WR();break;
        case (0x4B<<3)|2: _FETCH();break;
        case (0x4B<<3)|3: assert(false);break;
        case (0x4B<<3)|4: assert(false);break;
        case (0x4B<<3)|5: assert(false);break;
        case (0x4B<<3)|6: assert(false);break;
        case (0x4B<<3)|7: assert(false);break;
    /* JMP a */
        case (0x4C<<3)|0: _VPA();_SA(c->PC++);break;
        case (0x4C<<3)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0x4C<<3)|2: c->PC=(_GD()<<8)|c->AD;_FETCH();break;
        case (0x4C<<3)|3: assert(false);break;
        case (0x4C<<3)|4: assert(false);break;
        case (0x4C<<3)|5: assert(false);break;
        case (0x4C<<3)|6: assert(false);break;
        case (0x4C<<3)|7: assert(false);break;
    /* EOR a */
        case (0x4D<<3)|0: _VPA();_SA(c->PC++);break;
        case (0x4D<<3)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0x4D<<3)|2: _SA((_GD()<<8)|c->AD);break;
        case (0x4D<<3)|3: _A(c)^=_GD();_NZ(_A(c));_FETCH();break;
        case (0x4D<<3)|4: assert(false);break;
        case (0x4D<<3)|5: assert(false);break;
        case (0x4D<<3)|6: assert(false);break;
        case (0x4D<<3)|7: assert(false);break;
    /* LSR a */
        case (0x4E<<3)|0: _VPA();_SA(c->PC++);break;
        case (0x4E<<3)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0x4E<<3)|2: _SA((_GD()<<8)|c->AD);break;
        case (0x4E<<3)|3: _VDA();c->AD=_GD();_WR();break;
        case (0x4E<<3)|4: _VDA();_SD(_w65816_lsr(c,c->AD));_WR();break;
        case (0x4E<<3)|5: _FETCH();break;
        case (0x4E<<3)|6: assert(false);break;
        case (0x4E<<3)|7: assert(false);break;
    /* EOR al */
        case (0x4F<<3)|0: /* (unimpl) */;break;
        case (0x4F<<3)|1: _A(c)^=_GD();_NZ(_A(c));break;
        case (0x4F<<3)|2: _FETCH();break;
        case (0x4F<<3)|3: assert(false);break;
        case (0x4F<<3)|4: assert(false);break;
        case (0x4F<<3)|5: assert(false);break;
        case (0x4F<<3)|6: assert(false);break;
        case (0x4F<<3)|7: assert(false);break;
    /* BVC r */
        case (0x50<<3)|0: _VPA();_SA(c->PC++);break;
        case (0x50<<3)|1: _SA(c->PC);c->AD=c->PC+(int8_t)_GD();if((c->P&0x40)!=0x0){_FETCH();};break;
        case (0x50<<3)|2: _SA((c->PC&0xFF00)|(c->AD&0x00FF));if((c->AD&0xFF00)==(c->PC&0xFF00)){c->PC=c->AD;c->irq_pip>>=1;c->nmi_pip>>=1;_FETCH();};break;
        case (0x50<<3)|3: c->PC=c->AD;_FETCH();break;
        case (0x50<<3)|4: assert(false);break;
        case (0x50<<3)|5: assert(false);break;
        case (0x50<<3)|6: assert(false);break;
        case (0x50<<3)|7: assert(false);break;
    /* EOR (d),y */
        case (0x51<<3)|0: _VPA();_SA(c->PC++);break;
        case (0x51<<3)|1: _VDA();c->AD=_GD();_SA(c->AD);break;
        case (0x51<<3)|2: _VDA();_SA((c->AD+1)&0xFF);c->AD=_GD();break;
        case (0x51<<3)|3: c->AD|=_GD()<<8;_SA((c->AD&0xFF00)|((c->AD+_Y(c))&0xFF));c->IR+=(~((c->AD>>8)-((c->AD+_Y(c))>>8)))&1;break;
        case (0x51<<3)|4: _VDA();_SA(c->AD+_Y(c));break;
        case (0x51<<3)|5: _A(c)^=_GD();_NZ(_A(c));_FETCH();break;
        case (0x51<<3)|6: assert(false);break;
        case (0x51<<3)|7: assert(false);break;
    /* EOR (d) */
        case (0x52<<3)|0: /* (unimpl) */;break;
        case (0x52<<3)|1: _A(c)^=_GD();_NZ(_A(c));break;
        case (0x52<<3)|2: _FETCH();break;
        case (0x52<<3)|3: assert(false);break;
        case (0x52<<3)|4: assert(false);break;
        case (0x52<<3)|5: assert(false);break;
        case (0x52<<3)|6: assert(false);break;
        case (0x52<<3)|7: assert(false);break;
    /* EOR (d,s),y */
        case (0x53<<3)|0: /* (unimpl) */;break;
        case (0x53<<3)|1: _A(c)^=_GD();_NZ(_A(c));break;
        case (0x53<<3)|2: _FETCH();break;
        case (0x53<<3)|3: assert(false);break;
        case (0x53<<3)|4: assert(false);break;
        case (0x53<<3)|5: assert(false);break;
        case (0x53<<3)|6: assert(false);break;
        case (0x53<<3)|7: assert(false);break;
    /* MVN xyc */
        case (0x54<<3)|0: _VPA();_SA(c->PC++);break;
        case (0x54<<3)|1: _VPA();c->DBR=_GD();_SA(c->PC);break;
        case (0x54<<3)|2: _VDA();_SB(_GD());_SA(c->X++);break;
        case (0x54<<3)|3: _VDA();_SB(c->DBR);_SA(c->Y++);_WR();break;
        case (0x54<<3)|4: if(c->C){c->PC--;}break;
        case (0x54<<3)|5: c->C--?c->PC--:c->PC++;break;
        case (0x54<<3)|6: _FETCH();break;
        case (0x54<<3)|7: assert(false);break;
    /* EOR d,x */
        case (0x55<<3)|0: _VPA();_SA(c->PC++);break;
        case (0x55<<3)|1: c->AD=_GD();_SA(c->AD);break;
        case (0x55<<3)|2: _VDA();if(_E(c)){_SA((c->AD+_X(c))&0x00FF);}else{_SA(c->AD+_X(c));}break;
        case (0x55<<3)|3: _A(c)^=_GD();_NZ(_A(c));_FETCH();break;
        case (0x55<<3)|4: assert(false);break;
        case (0x55<<3)|5: assert(false);break;
        case (0x55<<3)|6: assert(false);break;
        case (0x55<<3)|7: assert(false);break;
    /* LSR d,x */
        case (0x56<<3)|0: _VPA();_SA(c->PC++);break;
        case (0x56<<3)|1: c->AD=_GD();_SA(c->AD);break;
        case (0x56<<3)|2: _VDA();if(_E(c)){_SA((c->AD+_X(c))&0x00FF);}else{_SA(c->AD+_X(c));}break;
        case (0x56<<3)|3: _VDA();c->AD=_GD();_WR();break;
        case (0x56<<3)|4: _VDA();_SD(_w65816_lsr(c,c->AD));_WR();break;
        case (0x56<<3)|5: _FETCH();break;
        case (0x56<<3)|6: assert(false);break;
        case (0x56<<3)|7: assert(false);break;
    /* EOR [d],y */
        case (0x57<<3)|0: /* (unimpl) */;break;
        case (0x57<<3)|1: _A(c)^=_GD();_NZ(_A(c));break;
        case (0x57<<3)|2: _FETCH();break;
        case (0x57<<3)|3: assert(false);break;
        case (0x57<<3)|4: assert(false);break;
        case (0x57<<3)|5: assert(false);break;
        case (0x57<<3)|6: assert(false);break;
        case (0x57<<3)|7: assert(false);break;
    /* CLI i */
        case (0x58<<3)|0: _SA(c->PC);break;
        case (0x58<<3)|1: c->P&=~0x4;_FETCH();break;
        case (0x58<<3)|2: assert(false);break;
        case (0x58<<3)|3: assert(false);break;
        case (0x58<<3)|4: assert(false);break;
        case (0x58<<3)|5: assert(false);break;
        case (0x58<<3)|6: assert(false);break;
        case (0x58<<3)|7: assert(false);break;
    /* EOR a,y */
        case (0x59<<3)|0: _VPA();_SA(c->PC++);break;
        case (0x59<<3)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0x59<<3)|2: c->AD|=_GD()<<8;_SA((c->AD&0xFF00)|((c->AD+_Y(c))&0xFF));c->IR+=(~((c->AD>>8)-((c->AD+_Y(c))>>8)))&1;break;
        case (0x59<<3)|3: _VDA();_SA(c->AD+_Y(c));break;
        case (0x59<<3)|4: _A(c)^=_GD();_NZ(_A(c));_FETCH();break;
        case (0x59<<3)|5: assert(false);break;
        case (0x59<<3)|6: assert(false);break;
        case (0x59<<3)|7: assert(false);break;
    /* PHY s */
        case (0x5A<<3)|0: _SA(c->PC);break;
        case (0x5A<<3)|1: _VDA();_SAD(0x0100|_S(c)--,_Y(c));_WR();break;
        case (0x5A<<3)|2: _FETCH();break;
        case (0x5A<<3)|3: assert(false);break;
        case (0x5A<<3)|4: assert(false);break;
        case (0x5A<<3)|5: assert(false);break;
        case (0x5A<<3)|6: assert(false);break;
        case (0x5A<<3)|7: assert(false);break;
    /* TCD i */
        case (0x5B<<3)|0: _SA(c->PC);break;
        case (0x5B<<3)|1: c->D=c->C;_NZ(c->C);_FETCH();break;
        case (0x5B<<3)|2: assert(false);break;
        case (0x5B<<3)|3: assert(false);break;
        case (0x5B<<3)|4: assert(false);break;
        case (0x5B<<3)|5: assert(false);break;
        case (0x5B<<3)|6: assert(false);break;
        case (0x5B<<3)|7: assert(false);break;
    /* JMP al */
        case (0x5C<<3)|0: /* (unimpl) */;break;
        case (0x5C<<3)|1: _VPA();_SA(c->PC++);break;
        case (0x5C<<3)|2: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0x5C<<3)|3: c->PC=(_GD()<<8)|c->AD;_FETCH();break;
        case (0x5C<<3)|4: assert(false);break;
        case (0x5C<<3)|5: assert(false);break;
        case (0x5C<<3)|6: assert(false);break;
        case (0x5C<<3)|7: assert(false);break;
    /* EOR a,x */
        case (0x5D<<3)|0: _VPA();_SA(c->PC++);break;
        case (0x5D<<3)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0x5D<<3)|2: c->AD|=_GD()<<8;_SA((c->AD&0xFF00)|((c->AD+_X(c))&0xFF));c->IR+=(~((c->AD>>8)-((c->AD+_X(c))>>8)))&1;break;
        case (0x5D<<3)|3: _VDA();_SA(c->AD+_X(c));break;
        case (0x5D<<3)|4: _A(c)^=_GD();_NZ(_A(c));_FETCH();break;
        case (0x5D<<3)|5: assert(false);break;
        case (0x5D<<3)|6: assert(false);break;
        case (0x5D<<3)|7: assert(false);break;
    /* LSR a,x */
        case (0x5E<<3)|0: _VPA();_SA(c->PC++);break;
        case (0x5E<<3)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0x5E<<3)|2: c->AD|=_GD()<<8;_SA((c->AD&0xFF00)|((c->AD+_X(c))&0xFF));break;
        case (0x5E<<3)|3: _VDA();_SA(c->AD+_X(c));break;
        case (0x5E<<3)|4: _VDA();c->AD=_GD();_WR();break;
        case (0x5E<<3)|5: _VDA();_SD(_w65816_lsr(c,c->AD));_WR();break;
        case (0x5E<<3)|6: _FETCH();break;
        case (0x5E<<3)|7: assert(false);break;
    /* EOR al,x */
        case (0x5F<<3)|0: /* (unimpl) */;break;
        case (0x5F<<3)|1: _A(c)^=_GD();_NZ(_A(c));break;
        case (0x5F<<3)|2: _FETCH();break;
        case (0x5F<<3)|3: assert(false);break;
        case (0x5F<<3)|4: assert(false);break;
        case (0x5F<<3)|5: assert(false);break;
        case (0x5F<<3)|6: assert(false);break;
        case (0x5F<<3)|7: assert(false);break;
    /* RTS s */
        case (0x60<<3)|0: _SA(c->PC);break;
        case (0x60<<3)|1: _SA(0x0100|_S(c)++);break;
        case (0x60<<3)|2: _VDA();_SA(0x0100|_S(c)++);break;
        case (0x60<<3)|3: _VDA();_SA(0x0100|_S(c));c->AD=_GD();break;
        case (0x60<<3)|4: c->PC=(_GD()<<8)|c->AD;_SA(c->PC++);break;
        case (0x60<<3)|5: _FETCH();break;
        case (0x60<<3)|6: assert(false);break;
        case (0x60<<3)|7: assert(false);break;
    /* ADC (d,x) */
        case (0x61<<3)|0: _VPA();_SA(c->PC++);break;
        case (0x61<<3)|1: c->AD=_GD();_SA(c->AD);break;
        case (0x61<<3)|2: _VDA();c->AD=(c->AD+_X(c))&0xFF;_SA(c->AD);break;
        case (0x61<<3)|3: _VDA();_SA((c->AD+1)&0xFF);c->AD=_GD();break;
        case (0x61<<3)|4: _VDA();_SA((_GD()<<8)|c->AD);break;
        case (0x61<<3)|5: _w65816_adc(c,_GD());_FETCH();break;
        case (0x61<<3)|6: assert(false);break;
        case (0x61<<3)|7: assert(false);break;
    /* PER s (unimpl) */
        case (0x62<<3)|0: _SA(c->PC);break;
        case (0x62<<3)|1: break;
        case (0x62<<3)|2: _FETCH();break;
        case (0x62<<3)|3: assert(false);break;
        case (0x62<<3)|4: assert(false);break;
        case (0x62<<3)|5: assert(false);break;
        case (0x62<<3)|6: assert(false);break;
        case (0x62<<3)|7: assert(false);break;
    /* ADC d,s */
        case (0x63<<3)|0: /* (unimpl) */;break;
        case (0x63<<3)|1: _w65816_adc(c,_GD());break;
        case (0x63<<3)|2: _FETCH();break;
        case (0x63<<3)|3: assert(false);break;
        case (0x63<<3)|4: assert(false);break;
        case (0x63<<3)|5: assert(false);break;
        case (0x63<<3)|6: assert(false);break;
        case (0x63<<3)|7: assert(false);break;
    /* STZ d */
        case (0x64<<3)|0: _VPA();_SA(c->PC++);break;
        case (0x64<<3)|1: _VDA();_SA(_GD());_VDA();_SD(0);_WR();break;
        case (0x64<<3)|2: if(_a8(c)){_FETCH();}else{_VDA();_SALD(_GAL()+1,0);_WR();}break;
        case (0x64<<3)|3: _FETCH();break;
        case (0x64<<3)|4: assert(false);break;
        case (0x64<<3)|5: assert(false);break;
        case (0x64<<3)|6: assert(false);break;
        case (0x64<<3)|7: assert(false);break;
    /* ADC d */
        case (0x65<<3)|0: _VPA();_SA(c->PC++);break;
        case (0x65<<3)|1: _VDA();_SA(_GD());break;
        case (0x65<<3)|2: _w65816_adc(c,_GD());_FETCH();break;
        case (0x65<<3)|3: assert(false);break;
        case (0x65<<3)|4: assert(false);break;
        case (0x65<<3)|5: assert(false);break;
        case (0x65<<3)|6: assert(false);break;
        case (0x65<<3)|7: assert(false);break;
    /* ROR d */
        case (0x66<<3)|0: _VPA();_SA(c->PC++);break;
        case (0x66<<3)|1: _VDA();_SA(_GD());break;
        case (0x66<<3)|2: _VDA();c->AD=_GD();_WR();break;
        case (0x66<<3)|3: _VDA();_SD(_w65816_ror(c,c->AD));_WR();break;
        case (0x66<<3)|4: _FETCH();break;
        case (0x66<<3)|5: assert(false);break;
        case (0x66<<3)|6: assert(false);break;
        case (0x66<<3)|7: assert(false);break;
    /* ADC [d] */
        case (0x67<<3)|0: /* (unimpl) */;break;
        case (0x67<<3)|1: _w65816_adc(c,_GD());break;
        case (0x67<<3)|2: _FETCH();break;
        case (0x67<<3)|3: assert(false);break;
        case (0x67<<3)|4: assert(false);break;
        case (0x67<<3)|5: assert(false);break;
        case (0x67<<3)|6: assert(false);break;
        case (0x67<<3)|7: assert(false);break;
    /* PLA s */
        case (0x68<<3)|0: _SA(c->PC);break;
        case (0x68<<3)|1: _SA(c->PC);break;
        case (0x68<<3)|2: _VDA();_SA(0x0100|++_S(c));break;
        case (0x68<<3)|3: _A(c)=_GD();_NZ(_A(c));_FETCH();break;
        case (0x68<<3)|4: assert(false);break;
        case (0x68<<3)|5: assert(false);break;
        case (0x68<<3)|6: assert(false);break;
        case (0x68<<3)|7: assert(false);break;
    /* ADC # */
        case (0x69<<3)|0: _VPA();_SA(c->PC++);break;
        case (0x69<<3)|1: _w65816_adc(c,_GD());_FETCH();break;
        case (0x69<<3)|2: assert(false);break;
        case (0x69<<3)|3: assert(false);break;
        case (0x69<<3)|4: assert(false);break;
        case (0x69<<3)|5: assert(false);break;
        case (0x69<<3)|6: assert(false);break;
        case (0x69<<3)|7: assert(false);break;
    /* ROR A */
        case (0x6A<<3)|0: _SA(c->PC);break;
        case (0x6A<<3)|1: _A(c)=_w65816_ror(c,_A(c));_FETCH();break;
        case (0x6A<<3)|2: assert(false);break;
        case (0x6A<<3)|3: assert(false);break;
        case (0x6A<<3)|4: assert(false);break;
        case (0x6A<<3)|5: assert(false);break;
        case (0x6A<<3)|6: assert(false);break;
        case (0x6A<<3)|7: assert(false);break;
    /* RTL s (unimpl) */
        case (0x6B<<3)|0: _SA(c->PC);break;
        case (0x6B<<3)|1: _SA(0x0100|_S(c)++);_FETCH();break;
        case (0x6B<<3)|2: assert(false);break;
        case (0x6B<<3)|3: assert(false);break;
        case (0x6B<<3)|4: assert(false);break;
        case (0x6B<<3)|5: assert(false);break;
        case (0x6B<<3)|6: assert(false);break;
        case (0x6B<<3)|7: assert(false);break;
    /* JMP (a) */
        case (0x6C<<3)|0: _VPA();_SA(c->PC++);break;
        case (0x6C<<3)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0x6C<<3)|2: _VDA();c->AD|=_GD()<<8;_SA(c->AD);break;
        case (0x6C<<3)|3: _VDA();_SA((c->AD&0xFF00)|((c->AD+1)&0x00FF));c->AD=_GD();break;
        case (0x6C<<3)|4: c->PC=(_GD()<<8)|c->AD;_FETCH();break;
        case (0x6C<<3)|5: assert(false);break;
        case (0x6C<<3)|6: assert(false);break;
        case (0x6C<<3)|7: assert(false);break;
    /* ADC a */
        case (0x6D<<3)|0: _VPA();_SA(c->PC++);break;
        case (0x6D<<3)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0x6D<<3)|2: _SA((_GD()<<8)|c->AD);break;
        case (0x6D<<3)|3: _w65816_adc(c,_GD());_FETCH();break;
        case (0x6D<<3)|4: assert(false);break;
        case (0x6D<<3)|5: assert(false);break;
        case (0x6D<<3)|6: assert(false);break;
        case (0x6D<<3)|7: assert(false);break;
    /* ROR a */
        case (0x6E<<3)|0: _VPA();_SA(c->PC++);break;
        case (0x6E<<3)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0x6E<<3)|2: _SA((_GD()<<8)|c->AD);break;
        case (0x6E<<3)|3: _VDA();c->AD=_GD();_WR();break;
        case (0x6E<<3)|4: _VDA();_SD(_w65816_ror(c,c->AD));_WR();break;
        case (0x6E<<3)|5: _FETCH();break;
        case (0x6E<<3)|6: assert(false);break;
        case (0x6E<<3)|7: assert(false);break;
    /* ADC al */
        case (0x6F<<3)|0: /* (unimpl) */;break;
        case (0x6F<<3)|1: _w65816_adc(c,_GD());break;
        case (0x6F<<3)|2: _FETCH();break;
        case (0x6F<<3)|3: assert(false);break;
        case (0x6F<<3)|4: assert(false);break;
        case (0x6F<<3)|5: assert(false);break;
        case (0x6F<<3)|6: assert(false);break;
        case (0x6F<<3)|7: assert(false);break;
    /* BVS r */
        case (0x70<<3)|0: _VPA();_SA(c->PC++);break;
        case (0x70<<3)|1: _SA(c->PC);c->AD=c->PC+(int8_t)_GD();if((c->P&0x40)!=0x40){_FETCH();};break;
        case (0x70<<3)|2: _SA((c->PC&0xFF00)|(c->AD&0x00FF));if((c->AD&0xFF00)==(c->PC&0xFF00)){c->PC=c->AD;c->irq_pip>>=1;c->nmi_pip>>=1;_FETCH();};break;
        case (0x70<<3)|3: c->PC=c->AD;_FETCH();break;
        case (0x70<<3)|4: assert(false);break;
        case (0x70<<3)|5: assert(false);break;
        case (0x70<<3)|6: assert(false);break;
        case (0x70<<3)|7: assert(false);break;
    /* ADC (d),y */
        case (0x71<<3)|0: _VPA();_SA(c->PC++);break;
        case (0x71<<3)|1: _VDA();c->AD=_GD();_SA(c->AD);break;
        case (0x71<<3)|2: _VDA();_SA((c->AD+1)&0xFF);c->AD=_GD();break;
        case (0x71<<3)|3: c->AD|=_GD()<<8;_SA((c->AD&0xFF00)|((c->AD+_Y(c))&0xFF));c->IR+=(~((c->AD>>8)-((c->AD+_Y(c))>>8)))&1;break;
        case (0x71<<3)|4: _VDA();_SA(c->AD+_Y(c));break;
        case (0x71<<3)|5: _w65816_adc(c,_GD());_FETCH();break;
        case (0x71<<3)|6: assert(false);break;
        case (0x71<<3)|7: assert(false);break;
    /* ADC (d) */
        case (0x72<<3)|0: /* (unimpl) */;break;
        case (0x72<<3)|1: _w65816_adc(c,_GD());break;
        case (0x72<<3)|2: _FETCH();break;
        case (0x72<<3)|3: assert(false);break;
        case (0x72<<3)|4: assert(false);break;
        case (0x72<<3)|5: assert(false);break;
        case (0x72<<3)|6: assert(false);break;
        case (0x72<<3)|7: assert(false);break;
    /* ADC (d,s),y */
        case (0x73<<3)|0: /* (unimpl) */;break;
        case (0x73<<3)|1: _w65816_adc(c,_GD());break;
        case (0x73<<3)|2: _FETCH();break;
        case (0x73<<3)|3: assert(false);break;
        case (0x73<<3)|4: assert(false);break;
        case (0x73<<3)|5: assert(false);break;
        case (0x73<<3)|6: assert(false);break;
        case (0x73<<3)|7: assert(false);break;
    /* STZ d,x */
        case (0x74<<3)|0: _VPA();_SA(c->PC++);break;
        case (0x74<<3)|1: c->AD=_GD();_SA(c->AD);break;
        case (0x74<<3)|2: _VDA();if(_E(c)){_SA((c->AD+_X(c))&0x00FF);}else{_SA(c->AD+_X(c));}_VDA();_SD(0);_WR();break;
        case (0x74<<3)|3: if(_a8(c)){_FETCH();}else{_VDA();_SALD(_GAL()+1,0);_WR();}break;
        case (0x74<<3)|4: _FETCH();break;
        case (0x74<<3)|5: assert(false);break;
        case (0x74<<3)|6: assert(false);break;
        case (0x74<<3)|7: assert(false);break;
    /* ADC d,x */
        case (0x75<<3)|0: _VPA();_SA(c->PC++);break;
        case (0x75<<3)|1: c->AD=_GD();_SA(c->AD);break;
        case (0x75<<3)|2: _VDA();if(_E(c)){_SA((c->AD+_X(c))&0x00FF);}else{_SA(c->AD+_X(c));}break;
        case (0x75<<3)|3: _w65816_adc(c,_GD());_FETCH();break;
        case (0x75<<3)|4: assert(false);break;
        case (0x75<<3)|5: assert(false);break;
        case (0x75<<3)|6: assert(false);break;
        case (0x75<<3)|7: assert(false);break;
    /* ROR d,x */
        case (0x76<<3)|0: _VPA();_SA(c->PC++);break;
        case (0x76<<3)|1: c->AD=_GD();_SA(c->AD);break;
        case (0x76<<3)|2: _VDA();if(_E(c)){_SA((c->AD+_X(c))&0x00FF);}else{_SA(c->AD+_X(c));}break;
        case (0x76<<3)|3: _VDA();c->AD=_GD();_WR();break;
        case (0x76<<3)|4: _VDA();_SD(_w65816_ror(c,c->AD));_WR();break;
        case (0x76<<3)|5: _FETCH();break;
        case (0x76<<3)|6: assert(false);break;
        case (0x76<<3)|7: assert(false);break;
    /* ADC [d],y */
        case (0x77<<3)|0: /* (unimpl) */;break;
        case (0x77<<3)|1: _w65816_adc(c,_GD());break;
        case (0x77<<3)|2: _FETCH();break;
        case (0x77<<3)|3: assert(false);break;
        case (0x77<<3)|4: assert(false);break;
        case (0x77<<3)|5: assert(false);break;
        case (0x77<<3)|6: assert(false);break;
        case (0x77<<3)|7: assert(false);break;
    /* SEI i */
        case (0x78<<3)|0: _SA(c->PC);break;
        case (0x78<<3)|1: c->P|=0x4;_FETCH();break;
        case (0x78<<3)|2: assert(false);break;
        case (0x78<<3)|3: assert(false);break;
        case (0x78<<3)|4: assert(false);break;
        case (0x78<<3)|5: assert(false);break;
        case (0x78<<3)|6: assert(false);break;
        case (0x78<<3)|7: assert(false);break;
    /* ADC a,y */
        case (0x79<<3)|0: _VPA();_SA(c->PC++);break;
        case (0x79<<3)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0x79<<3)|2: c->AD|=_GD()<<8;_SA((c->AD&0xFF00)|((c->AD+_Y(c))&0xFF));c->IR+=(~((c->AD>>8)-((c->AD+_Y(c))>>8)))&1;break;
        case (0x79<<3)|3: _VDA();_SA(c->AD+_Y(c));break;
        case (0x79<<3)|4: _w65816_adc(c,_GD());_FETCH();break;
        case (0x79<<3)|5: assert(false);break;
        case (0x79<<3)|6: assert(false);break;
        case (0x79<<3)|7: assert(false);break;
    /* PLY s */
        case (0x7A<<3)|0: _SA(c->PC);break;
        case (0x7A<<3)|1: _SA(c->PC);break;
        case (0x7A<<3)|2: _VDA();_SA(0x0100|++_S(c));break;
        case (0x7A<<3)|3: _Y(c)=_GD();_NZ(_Y(c));_FETCH();break;
        case (0x7A<<3)|4: assert(false);break;
        case (0x7A<<3)|5: assert(false);break;
        case (0x7A<<3)|6: assert(false);break;
        case (0x7A<<3)|7: assert(false);break;
    /* TDC i */
        case (0x7B<<3)|0: _SA(c->PC);break;
        case (0x7B<<3)|1: c->C=c->D;_NZ(c->C);_FETCH();break;
        case (0x7B<<3)|2: assert(false);break;
        case (0x7B<<3)|3: assert(false);break;
        case (0x7B<<3)|4: assert(false);break;
        case (0x7B<<3)|5: assert(false);break;
        case (0x7B<<3)|6: assert(false);break;
        case (0x7B<<3)|7: assert(false);break;
    /* JMP (a,x) */
        case (0x7C<<3)|0: _VPA();_SA(c->PC++);break;
        case (0x7C<<3)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0x7C<<3)|2: _VDA();c->AD|=_GD()<<8;_SA(c->AD);break;
        case (0x7C<<3)|3: _VDA();_SA((c->AD&0xFF00)|((c->AD+1)&0x00FF));c->AD=_GD();break;
        case (0x7C<<3)|4: c->PC=(_GD()<<8)|c->AD;_FETCH();break;
        case (0x7C<<3)|5: assert(false);break;
        case (0x7C<<3)|6: assert(false);break;
        case (0x7C<<3)|7: assert(false);break;
    /* ADC a,x */
        case (0x7D<<3)|0: _VPA();_SA(c->PC++);break;
        case (0x7D<<3)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0x7D<<3)|2: c->AD|=_GD()<<8;_SA((c->AD&0xFF00)|((c->AD+_X(c))&0xFF));c->IR+=(~((c->AD>>8)-((c->AD+_X(c))>>8)))&1;break;
        case (0x7D<<3)|3: _VDA();_SA(c->AD+_X(c));break;
        case (0x7D<<3)|4: _w65816_adc(c,_GD());_FETCH();break;
        case (0x7D<<3)|5: assert(false);break;
        case (0x7D<<3)|6: assert(false);break;
        case (0x7D<<3)|7: assert(false);break;
    /* ROR a,x */
        case (0x7E<<3)|0: _VPA();_SA(c->PC++);break;
        case (0x7E<<3)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0x7E<<3)|2: c->AD|=_GD()<<8;_SA((c->AD&0xFF00)|((c->AD+_X(c))&0xFF));break;
        case (0x7E<<3)|3: _VDA();_SA(c->AD+_X(c));break;
        case (0x7E<<3)|4: _VDA();c->AD=_GD();_WR();break;
        case (0x7E<<3)|5: _VDA();_SD(_w65816_ror(c,c->AD));_WR();break;
        case (0x7E<<3)|6: _FETCH();break;
        case (0x7E<<3)|7: assert(false);break;
    /* ADC al,x */
        case (0x7F<<3)|0: /* (unimpl) */;break;
        case (0x7F<<3)|1: _w65816_adc(c,_GD());break;
        case (0x7F<<3)|2: _FETCH();break;
        case (0x7F<<3)|3: assert(false);break;
        case (0x7F<<3)|4: assert(false);break;
        case (0x7F<<3)|5: assert(false);break;
        case (0x7F<<3)|6: assert(false);break;
        case (0x7F<<3)|7: assert(false);break;
    /* BRA r */
        case (0x80<<3)|0: _VPA();_SA(c->PC++);break;
        case (0x80<<3)|1: _SA(c->PC);c->AD=c->PC+(int8_t)_GD();break;
        case (0x80<<3)|2: _SA((c->PC&0xFF00)|(c->AD&0x00FF));if((c->AD&0xFF00)==(c->PC&0xFF00)){c->PC=c->AD;c->irq_pip>>=1;c->nmi_pip>>=1;_FETCH();};break;
        case (0x80<<3)|3: c->PC=c->AD;_FETCH();break;
        case (0x80<<3)|4: assert(false);break;
        case (0x80<<3)|5: assert(false);break;
        case (0x80<<3)|6: assert(false);break;
        case (0x80<<3)|7: assert(false);break;
    /* STA (d,x) */
        case (0x81<<3)|0: _VPA();_SA(c->PC++);break;
        case (0x81<<3)|1: c->AD=_GD();_SA(c->AD);break;
        case (0x81<<3)|2: _VDA();c->AD=(c->AD+_X(c))&0xFF;_SA(c->AD);break;
        case (0x81<<3)|3: _VDA();_SA((c->AD+1)&0xFF);c->AD=_GD();break;
        case (0x81<<3)|4: _VDA();_SA((_GD()<<8)|c->AD);_VDA();_SD(_A(c));_WR();break;
        case (0x81<<3)|5: if(_a8(c)){_FETCH();}else{_VDA();_SALD(_GAL()+1,_B(c));_WR();}break;
        case (0x81<<3)|6: _FETCH();break;
        case (0x81<<3)|7: assert(false);break;
    /* BRL rl (unimpl) */
        case (0x82<<3)|0: _FETCH();break;
        case (0x82<<3)|1: assert(false);break;
        case (0x82<<3)|2: assert(false);break;
        case (0x82<<3)|3: assert(false);break;
        case (0x82<<3)|4: assert(false);break;
        case (0x82<<3)|5: assert(false);break;
        case (0x82<<3)|6: assert(false);break;
        case (0x82<<3)|7: assert(false);break;
    /* STA d,s */
        case (0x83<<3)|0: /* (unimpl) */;_VDA();_SD(_A(c));_WR();break;
        case (0x83<<3)|1: if(_a8(c)){_FETCH();}else{_VDA();_SALD(_GAL()+1,_B(c));_WR();}break;
        case (0x83<<3)|2: _FETCH();break;
        case (0x83<<3)|3: assert(false);break;
        case (0x83<<3)|4: assert(false);break;
        case (0x83<<3)|5: assert(false);break;
        case (0x83<<3)|6: assert(false);break;
        case (0x83<<3)|7: assert(false);break;
    /* STY d */
        case (0x84<<3)|0: _VPA();_SA(c->PC++);break;
        case (0x84<<3)|1: _VDA();_SA(_GD());_VDA();_SD(_Y(c));_WR();break;
        case (0x84<<3)|2: if(_i8(c)){_FETCH();}else{_VDA();_SALD(_GAL()+1,_YH(c));_WR();}break;
        case (0x84<<3)|3: _FETCH();break;
        case (0x84<<3)|4: assert(false);break;
        case (0x84<<3)|5: assert(false);break;
        case (0x84<<3)|6: assert(false);break;
        case (0x84<<3)|7: assert(false);break;
    /* STA d */
        case (0x85<<3)|0: _VPA();_SA(c->PC++);break;
        case (0x85<<3)|1: _VDA();_SA(_GD());_VDA();_SD(_A(c));_WR();break;
        case (0x85<<3)|2: if(_a8(c)){_FETCH();}else{_VDA();_SALD(_GAL()+1,_B(c));_WR();}break;
        case (0x85<<3)|3: _FETCH();break;
        case (0x85<<3)|4: assert(false);break;
        case (0x85<<3)|5: assert(false);break;
        case (0x85<<3)|6: assert(false);break;
        case (0x85<<3)|7: assert(false);break;
    /* STX d */
        case (0x86<<3)|0: _VPA();_SA(c->PC++);break;
        case (0x86<<3)|1: _VDA();_SA(_GD());_VDA();_SD(_X(c));_WR();break;
        case (0x86<<3)|2: if(_i8(c)){_FETCH();}else{_VDA();_SALD(_GAL()+1,_XH(c));_WR();}break;
        case (0x86<<3)|3: _FETCH();break;
        case (0x86<<3)|4: assert(false);break;
        case (0x86<<3)|5: assert(false);break;
        case (0x86<<3)|6: assert(false);break;
        case (0x86<<3)|7: assert(false);break;
    /* STA [d] */
        case (0x87<<3)|0: /* (unimpl) */;_VDA();_SD(_A(c));_WR();break;
        case (0x87<<3)|1: if(_a8(c)){_FETCH();}else{_VDA();_SALD(_GAL()+1,_B(c));_WR();}break;
        case (0x87<<3)|2: _FETCH();break;
        case (0x87<<3)|3: assert(false);break;
        case (0x87<<3)|4: assert(false);break;
        case (0x87<<3)|5: assert(false);break;
        case (0x87<<3)|6: assert(false);break;
        case (0x87<<3)|7: assert(false);break;
    /* DEY i */
        case (0x88<<3)|0: _SA(c->PC);break;
        case (0x88<<3)|1: _Y(c)--;_NZ(_Y(c));_FETCH();break;
        case (0x88<<3)|2: assert(false);break;
        case (0x88<<3)|3: assert(false);break;
        case (0x88<<3)|4: assert(false);break;
        case (0x88<<3)|5: assert(false);break;
        case (0x88<<3)|6: assert(false);break;
        case (0x88<<3)|7: assert(false);break;
    /* BIT # */
        case (0x89<<3)|0: _VPA();_SA(c->PC++);break;
        case (0x89<<3)|1: _w65816_bit(c,_GD());_FETCH();break;
        case (0x89<<3)|2: assert(false);break;
        case (0x89<<3)|3: assert(false);break;
        case (0x89<<3)|4: assert(false);break;
        case (0x89<<3)|5: assert(false);break;
        case (0x89<<3)|6: assert(false);break;
        case (0x89<<3)|7: assert(false);break;
    /* TXA i */
        case (0x8A<<3)|0: _SA(c->PC);break;
        case (0x8A<<3)|1: _A(c)=_X(c);_NZ(_A(c));_FETCH();break;
        case (0x8A<<3)|2: assert(false);break;
        case (0x8A<<3)|3: assert(false);break;
        case (0x8A<<3)|4: assert(false);break;
        case (0x8A<<3)|5: assert(false);break;
        case (0x8A<<3)|6: assert(false);break;
        case (0x8A<<3)|7: assert(false);break;
    /* PHB s */
        case (0x8B<<3)|0: _SA(c->PC);break;
        case (0x8B<<3)|1: _VDA();_SAD(0x0100|_S(c)--,c->DBR);_WR();break;
        case (0x8B<<3)|2: _FETCH();break;
        case (0x8B<<3)|3: assert(false);break;
        case (0x8B<<3)|4: assert(false);break;
        case (0x8B<<3)|5: assert(false);break;
        case (0x8B<<3)|6: assert(false);break;
        case (0x8B<<3)|7: assert(false);break;
    /* STY a */
        case (0x8C<<3)|0: _VPA();_SA(c->PC++);break;
        case (0x8C<<3)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0x8C<<3)|2: _SA((_GD()<<8)|c->AD);_VDA();_SD(_Y(c));_WR();break;
        case (0x8C<<3)|3: if(_i8(c)){_FETCH();}else{_VDA();_SALD(_GAL()+1,_YH(c));_WR();}break;
        case (0x8C<<3)|4: _FETCH();break;
        case (0x8C<<3)|5: assert(false);break;
        case (0x8C<<3)|6: assert(false);break;
        case (0x8C<<3)|7: assert(false);break;
    /* STA a */
        case (0x8D<<3)|0: _VPA();_SA(c->PC++);break;
        case (0x8D<<3)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0x8D<<3)|2: _SA((_GD()<<8)|c->AD);_VDA();_SD(_A(c));_WR();break;
        case (0x8D<<3)|3: if(_a8(c)){_FETCH();}else{_VDA();_SALD(_GAL()+1,_B(c));_WR();}break;
        case (0x8D<<3)|4: _FETCH();break;
        case (0x8D<<3)|5: assert(false);break;
        case (0x8D<<3)|6: assert(false);break;
        case (0x8D<<3)|7: assert(false);break;
    /* STX a */
        case (0x8E<<3)|0: _VPA();_SA(c->PC++);break;
        case (0x8E<<3)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0x8E<<3)|2: _SA((_GD()<<8)|c->AD);_VDA();_SD(_X(c));_WR();break;
        case (0x8E<<3)|3: if(_i8(c)){_FETCH();}else{_VDA();_SALD(_GAL()+1,_XH(c));_WR();}break;
        case (0x8E<<3)|4: _FETCH();break;
        case (0x8E<<3)|5: assert(false);break;
        case (0x8E<<3)|6: assert(false);break;
        case (0x8E<<3)|7: assert(false);break;
    /* STA al */
        case (0x8F<<3)|0: /* (unimpl) */;_VDA();_SD(_A(c));_WR();break;
        case (0x8F<<3)|1: if(_a8(c)){_FETCH();}else{_VDA();_SALD(_GAL()+1,_B(c));_WR();}break;
        case (0x8F<<3)|2: _FETCH();break;
        case (0x8F<<3)|3: assert(false);break;
        case (0x8F<<3)|4: assert(false);break;
        case (0x8F<<3)|5: assert(false);break;
        case (0x8F<<3)|6: assert(false);break;
        case (0x8F<<3)|7: assert(false);break;
    /* BCC r */
        case (0x90<<3)|0: _VPA();_SA(c->PC++);break;
        case (0x90<<3)|1: _SA(c->PC);c->AD=c->PC+(int8_t)_GD();if((c->P&0x1)!=0x0){_FETCH();};break;
        case (0x90<<3)|2: _SA((c->PC&0xFF00)|(c->AD&0x00FF));if((c->AD&0xFF00)==(c->PC&0xFF00)){c->PC=c->AD;c->irq_pip>>=1;c->nmi_pip>>=1;_FETCH();};break;
        case (0x90<<3)|3: c->PC=c->AD;_FETCH();break;
        case (0x90<<3)|4: assert(false);break;
        case (0x90<<3)|5: assert(false);break;
        case (0x90<<3)|6: assert(false);break;
        case (0x90<<3)|7: assert(false);break;
    /* STA (d),y */
        case (0x91<<3)|0: _VPA();_SA(c->PC++);break;
        case (0x91<<3)|1: _VDA();c->AD=_GD();_SA(c->AD);break;
        case (0x91<<3)|2: _VDA();_SA((c->AD+1)&0xFF);c->AD=_GD();break;
        case (0x91<<3)|3: c->AD|=_GD()<<8;_SA((c->AD&0xFF00)|((c->AD+_Y(c))&0xFF));break;
        case (0x91<<3)|4: _VDA();_SA(c->AD+_Y(c));_VDA();_SD(_A(c));_WR();break;
        case (0x91<<3)|5: if(_a8(c)){_FETCH();}else{_VDA();_SALD(_GAL()+1,_B(c));_WR();}break;
        case (0x91<<3)|6: _FETCH();break;
        case (0x91<<3)|7: assert(false);break;
    /* STA (d) */
        case (0x92<<3)|0: /* (unimpl) */;_VDA();_SD(_A(c));_WR();break;
        case (0x92<<3)|1: if(_a8(c)){_FETCH();}else{_VDA();_SALD(_GAL()+1,_B(c));_WR();}break;
        case (0x92<<3)|2: _FETCH();break;
        case (0x92<<3)|3: assert(false);break;
        case (0x92<<3)|4: assert(false);break;
        case (0x92<<3)|5: assert(false);break;
        case (0x92<<3)|6: assert(false);break;
        case (0x92<<3)|7: assert(false);break;
    /* STA (d,s),y */
        case (0x93<<3)|0: /* (unimpl) */;_VDA();_SD(_A(c));_WR();break;
        case (0x93<<3)|1: if(_a8(c)){_FETCH();}else{_VDA();_SALD(_GAL()+1,_B(c));_WR();}break;
        case (0x93<<3)|2: _FETCH();break;
        case (0x93<<3)|3: assert(false);break;
        case (0x93<<3)|4: assert(false);break;
        case (0x93<<3)|5: assert(false);break;
        case (0x93<<3)|6: assert(false);break;
        case (0x93<<3)|7: assert(false);break;
    /* STY d,x */
        case (0x94<<3)|0: _VPA();_SA(c->PC++);break;
        case (0x94<<3)|1: c->AD=_GD();_SA(c->AD);break;
        case (0x94<<3)|2: _VDA();if(_E(c)){_SA((c->AD+_X(c))&0x00FF);}else{_SA(c->AD+_X(c));}_VDA();_SD(_Y(c));_WR();break;
        case (0x94<<3)|3: if(_i8(c)){_FETCH();}else{_VDA();_SALD(_GAL()+1,_YH(c));_WR();}break;
        case (0x94<<3)|4: _FETCH();break;
        case (0x94<<3)|5: assert(false);break;
        case (0x94<<3)|6: assert(false);break;
        case (0x94<<3)|7: assert(false);break;
    /* STA d,x */
        case (0x95<<3)|0: _VPA();_SA(c->PC++);break;
        case (0x95<<3)|1: c->AD=_GD();_SA(c->AD);break;
        case (0x95<<3)|2: _VDA();if(_E(c)){_SA((c->AD+_X(c))&0x00FF);}else{_SA(c->AD+_X(c));}_VDA();_SD(_A(c));_WR();break;
        case (0x95<<3)|3: if(_a8(c)){_FETCH();}else{_VDA();_SALD(_GAL()+1,_B(c));_WR();}break;
        case (0x95<<3)|4: _FETCH();break;
        case (0x95<<3)|5: assert(false);break;
        case (0x95<<3)|6: assert(false);break;
        case (0x95<<3)|7: assert(false);break;
    /* STX d,y */
        case (0x96<<3)|0: _VPA();_SA(c->PC++);break;
        case (0x96<<3)|1: c->AD=_GD();_SA(c->AD);break;
        case (0x96<<3)|2: _VDA();if(_E(c)){_SA((c->AD+_Y(c))&0x00FF);}else{_SA(c->AD+_Y(c));}_VDA();_SD(_X(c));_WR();break;
        case (0x96<<3)|3: if(_i8(c)){_FETCH();}else{_VDA();_SALD(_GAL()+1,_XH(c));_WR();}break;
        case (0x96<<3)|4: _FETCH();break;
        case (0x96<<3)|5: assert(false);break;
        case (0x96<<3)|6: assert(false);break;
        case (0x96<<3)|7: assert(false);break;
    /* STA [d],y */
        case (0x97<<3)|0: /* (unimpl) */;_VDA();_SD(_A(c));_WR();break;
        case (0x97<<3)|1: if(_a8(c)){_FETCH();}else{_VDA();_SALD(_GAL()+1,_B(c));_WR();}break;
        case (0x97<<3)|2: _FETCH();break;
        case (0x97<<3)|3: assert(false);break;
        case (0x97<<3)|4: assert(false);break;
        case (0x97<<3)|5: assert(false);break;
        case (0x97<<3)|6: assert(false);break;
        case (0x97<<3)|7: assert(false);break;
    /* TYA i */
        case (0x98<<3)|0: _SA(c->PC);break;
        case (0x98<<3)|1: _A(c)=_Y(c);_NZ(_A(c));_FETCH();break;
        case (0x98<<3)|2: assert(false);break;
        case (0x98<<3)|3: assert(false);break;
        case (0x98<<3)|4: assert(false);break;
        case (0x98<<3)|5: assert(false);break;
        case (0x98<<3)|6: assert(false);break;
        case (0x98<<3)|7: assert(false);break;
    /* STA a,y */
        case (0x99<<3)|0: _VPA();_SA(c->PC++);break;
        case (0x99<<3)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0x99<<3)|2: c->AD|=_GD()<<8;_SA((c->AD&0xFF00)|((c->AD+_Y(c))&0xFF));break;
        case (0x99<<3)|3: _VDA();_SA(c->AD+_Y(c));_VDA();_SD(_A(c));_WR();break;
        case (0x99<<3)|4: if(_a8(c)){_FETCH();}else{_VDA();_SALD(_GAL()+1,_B(c));_WR();}break;
        case (0x99<<3)|5: _FETCH();break;
        case (0x99<<3)|6: assert(false);break;
        case (0x99<<3)|7: assert(false);break;
    /* TXS i */
        case (0x9A<<3)|0: _SA(c->PC);break;
        case (0x9A<<3)|1: _S(c)=_X(c);_FETCH();break;
        case (0x9A<<3)|2: assert(false);break;
        case (0x9A<<3)|3: assert(false);break;
        case (0x9A<<3)|4: assert(false);break;
        case (0x9A<<3)|5: assert(false);break;
        case (0x9A<<3)|6: assert(false);break;
        case (0x9A<<3)|7: assert(false);break;
    /* TXY i */
        case (0x9B<<3)|0: _SA(c->PC);break;
        case (0x9B<<3)|1: _Y(c)=_X(c);_NZ(_Y(c));_FETCH();break;
        case (0x9B<<3)|2: assert(false);break;
        case (0x9B<<3)|3: assert(false);break;
        case (0x9B<<3)|4: assert(false);break;
        case (0x9B<<3)|5: assert(false);break;
        case (0x9B<<3)|6: assert(false);break;
        case (0x9B<<3)|7: assert(false);break;
    /* STZ a */
        case (0x9C<<3)|0: _VPA();_SA(c->PC++);break;
        case (0x9C<<3)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0x9C<<3)|2: _SA((_GD()<<8)|c->AD);_VDA();_SD(0);_WR();break;
        case (0x9C<<3)|3: if(_a8(c)){_FETCH();}else{_VDA();_SALD(_GAL()+1,0);_WR();}break;
        case (0x9C<<3)|4: _FETCH();break;
        case (0x9C<<3)|5: assert(false);break;
        case (0x9C<<3)|6: assert(false);break;
        case (0x9C<<3)|7: assert(false);break;
    /* STA a,x */
        case (0x9D<<3)|0: _VPA();_SA(c->PC++);break;
        case (0x9D<<3)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0x9D<<3)|2: c->AD|=_GD()<<8;_SA((c->AD&0xFF00)|((c->AD+_X(c))&0xFF));break;
        case (0x9D<<3)|3: _VDA();_SA(c->AD+_X(c));_VDA();_SD(_A(c));_WR();break;
        case (0x9D<<3)|4: if(_a8(c)){_FETCH();}else{_VDA();_SALD(_GAL()+1,_B(c));_WR();}break;
        case (0x9D<<3)|5: _FETCH();break;
        case (0x9D<<3)|6: assert(false);break;
        case (0x9D<<3)|7: assert(false);break;
    /* STZ a,x */
        case (0x9E<<3)|0: _VPA();_SA(c->PC++);break;
        case (0x9E<<3)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0x9E<<3)|2: c->AD|=_GD()<<8;_SA((c->AD&0xFF00)|((c->AD+_X(c))&0xFF));break;
        case (0x9E<<3)|3: _VDA();_SA(c->AD+_X(c));_VDA();_SD(0);_WR();break;
        case (0x9E<<3)|4: if(_a8(c)){_FETCH();}else{_VDA();_SALD(_GAL()+1,0);_WR();}break;
        case (0x9E<<3)|5: _FETCH();break;
        case (0x9E<<3)|6: assert(false);break;
        case (0x9E<<3)|7: assert(false);break;
    /* STA al,x */
        case (0x9F<<3)|0: /* (unimpl) */;_VDA();_SD(_A(c));_WR();break;
        case (0x9F<<3)|1: if(_a8(c)){_FETCH();}else{_VDA();_SALD(_GAL()+1,_B(c));_WR();}break;
        case (0x9F<<3)|2: _FETCH();break;
        case (0x9F<<3)|3: assert(false);break;
        case (0x9F<<3)|4: assert(false);break;
        case (0x9F<<3)|5: assert(false);break;
        case (0x9F<<3)|6: assert(false);break;
        case (0x9F<<3)|7: assert(false);break;
    /* LDY # */
        case (0xA0<<3)|0: _VPA();_SA(c->PC++);break;
        case (0xA0<<3)|1: _Y(c)=_GD();if(_i8(c)){_NZ(_Y(c));_FETCH();}else{_VPA();_SA(c->PC++);}break;
        case (0xA0<<3)|2: _YH(c)=_GD();_NZ16(_Y(c));_FETCH();break;
        case (0xA0<<3)|3: assert(false);break;
        case (0xA0<<3)|4: assert(false);break;
        case (0xA0<<3)|5: assert(false);break;
        case (0xA0<<3)|6: assert(false);break;
        case (0xA0<<3)|7: assert(false);break;
    /* LDA (d,x) */
        case (0xA1<<3)|0: _VPA();_SA(c->PC++);break;
        case (0xA1<<3)|1: c->AD=_GD();_SA(c->AD);break;
        case (0xA1<<3)|2: _VDA();c->AD=(c->AD+_X(c))&0xFF;_SA(c->AD);break;
        case (0xA1<<3)|3: _VDA();_SA((c->AD+1)&0xFF);c->AD=_GD();break;
        case (0xA1<<3)|4: _VDA();_SA((_GD()<<8)|c->AD);break;
        case (0xA1<<3)|5: _A(c)=_GD();if(_a8(c)){_NZ(_A(c));_FETCH();}else{_VDA();_SAL(_GAL()+1);}break;
        case (0xA1<<3)|6: _B(c)=_GD();_NZ16(_C(c));_FETCH();break;
        case (0xA1<<3)|7: assert(false);break;
    /* LDX # */
        case (0xA2<<3)|0: _VPA();_SA(c->PC++);break;
        case (0xA2<<3)|1: _X(c)=_GD();if(_i8(c)){_NZ(_X(c));_FETCH();}else{_VPA();_SA(c->PC++);}break;
        case (0xA2<<3)|2: _XH(c)=_GD();_NZ16(_X(c));_FETCH();break;
        case (0xA2<<3)|3: assert(false);break;
        case (0xA2<<3)|4: assert(false);break;
        case (0xA2<<3)|5: assert(false);break;
        case (0xA2<<3)|6: assert(false);break;
        case (0xA2<<3)|7: assert(false);break;
    /* LDA d,s */
        case (0xA3<<3)|0: /* (unimpl) */;break;
        case (0xA3<<3)|1: _A(c)=_GD();if(_a8(c)){_NZ(_A(c));_FETCH();}else{_VDA();_SAL(_GAL()+1);}break;
        case (0xA3<<3)|2: _B(c)=_GD();_NZ16(_C(c));_FETCH();break;
        case (0xA3<<3)|3: assert(false);break;
        case (0xA3<<3)|4: assert(false);break;
        case (0xA3<<3)|5: assert(false);break;
        case (0xA3<<3)|6: assert(false);break;
        case (0xA3<<3)|7: assert(false);break;
    /* LDY d */
        case (0xA4<<3)|0: _VPA();_SA(c->PC++);break;
        case (0xA4<<3)|1: _VDA();_SA(_GD());break;
        case (0xA4<<3)|2: _Y(c)=_GD();if(_i8(c)){_NZ(_Y(c));_FETCH();}else{_VDA();_SAL(_GAL()+1);}break;
        case (0xA4<<3)|3: _YH(c)=_GD();_NZ16(_Y(c));_FETCH();break;
        case (0xA4<<3)|4: assert(false);break;
        case (0xA4<<3)|5: assert(false);break;
        case (0xA4<<3)|6: assert(false);break;
        case (0xA4<<3)|7: assert(false);break;
    /* LDA d */
        case (0xA5<<3)|0: _VPA();_SA(c->PC++);break;
        case (0xA5<<3)|1: _VDA();_SA(_GD());break;
        case (0xA5<<3)|2: _A(c)=_GD();if(_a8(c)){_NZ(_A(c));_FETCH();}else{_VDA();_SAL(_GAL()+1);}break;
        case (0xA5<<3)|3: _B(c)=_GD();_NZ16(_C(c));_FETCH();break;
        case (0xA5<<3)|4: assert(false);break;
        case (0xA5<<3)|5: assert(false);break;
        case (0xA5<<3)|6: assert(false);break;
        case (0xA5<<3)|7: assert(false);break;
    /* LDX d */
        case (0xA6<<3)|0: _VPA();_SA(c->PC++);break;
        case (0xA6<<3)|1: _VDA();_SA(_GD());break;
        case (0xA6<<3)|2: _X(c)=_GD();if(_i8(c)){_NZ(_X(c));_FETCH();}else{_VDA();_SAL(_GAL()+1);}break;
        case (0xA6<<3)|3: _XH(c)=_GD();_NZ16(_X(c));_FETCH();break;
        case (0xA6<<3)|4: assert(false);break;
        case (0xA6<<3)|5: assert(false);break;
        case (0xA6<<3)|6: assert(false);break;
        case (0xA6<<3)|7: assert(false);break;
    /* LDA [d] */
        case (0xA7<<3)|0: /* (unimpl) */;break;
        case (0xA7<<3)|1: _A(c)=_GD();if(_a8(c)){_NZ(_A(c));_FETCH();}else{_VDA();_SAL(_GAL()+1);}break;
        case (0xA7<<3)|2: _B(c)=_GD();_NZ16(_C(c));_FETCH();break;
        case (0xA7<<3)|3: assert(false);break;
        case (0xA7<<3)|4: assert(false);break;
        case (0xA7<<3)|5: assert(false);break;
        case (0xA7<<3)|6: assert(false);break;
        case (0xA7<<3)|7: assert(false);break;
    /* TAY i */
        case (0xA8<<3)|0: _SA(c->PC);break;
        case (0xA8<<3)|1: _Y(c)=_A(c);_NZ(_Y(c));_FETCH();break;
        case (0xA8<<3)|2: assert(false);break;
        case (0xA8<<3)|3: assert(false);break;
        case (0xA8<<3)|4: assert(false);break;
        case (0xA8<<3)|5: assert(false);break;
        case (0xA8<<3)|6: assert(false);break;
        case (0xA8<<3)|7: assert(false);break;
    /* LDA # */
        case (0xA9<<3)|0: _VPA();_SA(c->PC++);break;
        case (0xA9<<3)|1: _A(c)=_GD();if(_a8(c)){_NZ(_A(c));_FETCH();}else{_VPA();_SA(c->PC++);}break;
        case (0xA9<<3)|2: _B(c)=_GD();_NZ16(_C(c));_FETCH();break;
        case (0xA9<<3)|3: assert(false);break;
        case (0xA9<<3)|4: assert(false);break;
        case (0xA9<<3)|5: assert(false);break;
        case (0xA9<<3)|6: assert(false);break;
        case (0xA9<<3)|7: assert(false);break;
    /* TAX i */
        case (0xAA<<3)|0: _SA(c->PC);break;
        case (0xAA<<3)|1: _X(c)=_A(c);_NZ(_X(c));_FETCH();break;
        case (0xAA<<3)|2: assert(false);break;
        case (0xAA<<3)|3: assert(false);break;
        case (0xAA<<3)|4: assert(false);break;
        case (0xAA<<3)|5: assert(false);break;
        case (0xAA<<3)|6: assert(false);break;
        case (0xAA<<3)|7: assert(false);break;
    /* PLB s */
        case (0xAB<<3)|0: _SA(c->PC);break;
        case (0xAB<<3)|1: _SA(c->PC);break;
        case (0xAB<<3)|2: _VDA();_SA(0x0100|++_S(c));break;
        case (0xAB<<3)|3: c->DBR=_GD();_NZ(c->DBR);_FETCH();break;
        case (0xAB<<3)|4: assert(false);break;
        case (0xAB<<3)|5: assert(false);break;
        case (0xAB<<3)|6: assert(false);break;
        case (0xAB<<3)|7: assert(false);break;
    /* LDY a */
        case (0xAC<<3)|0: _VPA();_SA(c->PC++);break;
        case (0xAC<<3)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0xAC<<3)|2: _SA((_GD()<<8)|c->AD);break;
        case (0xAC<<3)|3: _Y(c)=_GD();if(_i8(c)){_NZ(_Y(c));_FETCH();}else{_VDA();_SAL(_GAL()+1);}break;
        case (0xAC<<3)|4: _YH(c)=_GD();_NZ16(_Y(c));_FETCH();break;
        case (0xAC<<3)|5: assert(false);break;
        case (0xAC<<3)|6: assert(false);break;
        case (0xAC<<3)|7: assert(false);break;
    /* LDA a */
        case (0xAD<<3)|0: _VPA();_SA(c->PC++);break;
        case (0xAD<<3)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0xAD<<3)|2: _SA((_GD()<<8)|c->AD);break;
        case (0xAD<<3)|3: _A(c)=_GD();if(_a8(c)){_NZ(_A(c));_FETCH();}else{_VDA();_SAL(_GAL()+1);}break;
        case (0xAD<<3)|4: _B(c)=_GD();_NZ16(_C(c));_FETCH();break;
        case (0xAD<<3)|5: assert(false);break;
        case (0xAD<<3)|6: assert(false);break;
        case (0xAD<<3)|7: assert(false);break;
    /* LDX a */
        case (0xAE<<3)|0: _VPA();_SA(c->PC++);break;
        case (0xAE<<3)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0xAE<<3)|2: _SA((_GD()<<8)|c->AD);break;
        case (0xAE<<3)|3: _X(c)=_GD();if(_i8(c)){_NZ(_X(c));_FETCH();}else{_VDA();_SAL(_GAL()+1);}break;
        case (0xAE<<3)|4: _XH(c)=_GD();_NZ16(_X(c));_FETCH();break;
        case (0xAE<<3)|5: assert(false);break;
        case (0xAE<<3)|6: assert(false);break;
        case (0xAE<<3)|7: assert(false);break;
    /* LDA al */
        case (0xAF<<3)|0: /* (unimpl) */;break;
        case (0xAF<<3)|1: _A(c)=_GD();if(_a8(c)){_NZ(_A(c));_FETCH();}else{_VDA();_SAL(_GAL()+1);}break;
        case (0xAF<<3)|2: _B(c)=_GD();_NZ16(_C(c));_FETCH();break;
        case (0xAF<<3)|3: assert(false);break;
        case (0xAF<<3)|4: assert(false);break;
        case (0xAF<<3)|5: assert(false);break;
        case (0xAF<<3)|6: assert(false);break;
        case (0xAF<<3)|7: assert(false);break;
    /* BCS r */
        case (0xB0<<3)|0: _VPA();_SA(c->PC++);break;
        case (0xB0<<3)|1: _SA(c->PC);c->AD=c->PC+(int8_t)_GD();if((c->P&0x1)!=0x1){_FETCH();};break;
        case (0xB0<<3)|2: _SA((c->PC&0xFF00)|(c->AD&0x00FF));if((c->AD&0xFF00)==(c->PC&0xFF00)){c->PC=c->AD;c->irq_pip>>=1;c->nmi_pip>>=1;_FETCH();};break;
        case (0xB0<<3)|3: c->PC=c->AD;_FETCH();break;
        case (0xB0<<3)|4: assert(false);break;
        case (0xB0<<3)|5: assert(false);break;
        case (0xB0<<3)|6: assert(false);break;
        case (0xB0<<3)|7: assert(false);break;
    /* LDA (d),y */
        case (0xB1<<3)|0: _VPA();_SA(c->PC++);break;
        case (0xB1<<3)|1: _VDA();c->AD=_GD();_SA(c->AD);break;
        case (0xB1<<3)|2: _VDA();_SA((c->AD+1)&0xFF);c->AD=_GD();break;
        case (0xB1<<3)|3: c->AD|=_GD()<<8;_SA((c->AD&0xFF00)|((c->AD+_Y(c))&0xFF));c->IR+=(~((c->AD>>8)-((c->AD+_Y(c))>>8)))&1;break;
        case (0xB1<<3)|4: _VDA();_SA(c->AD+_Y(c));break;
        case (0xB1<<3)|5: _A(c)=_GD();if(_a8(c)){_NZ(_A(c));_FETCH();}else{_VDA();_SAL(_GAL()+1);}break;
        case (0xB1<<3)|6: _B(c)=_GD();_NZ16(_C(c));_FETCH();break;
        case (0xB1<<3)|7: assert(false);break;
    /* LDA (d) */
        case (0xB2<<3)|0: /* (unimpl) */;break;
        case (0xB2<<3)|1: _A(c)=_GD();if(_a8(c)){_NZ(_A(c));_FETCH();}else{_VDA();_SAL(_GAL()+1);}break;
        case (0xB2<<3)|2: _B(c)=_GD();_NZ16(_C(c));_FETCH();break;
        case (0xB2<<3)|3: assert(false);break;
        case (0xB2<<3)|4: assert(false);break;
        case (0xB2<<3)|5: assert(false);break;
        case (0xB2<<3)|6: assert(false);break;
        case (0xB2<<3)|7: assert(false);break;
    /* LDA (d,s),y */
        case (0xB3<<3)|0: /* (unimpl) */;break;
        case (0xB3<<3)|1: _A(c)=_GD();if(_a8(c)){_NZ(_A(c));_FETCH();}else{_VDA();_SAL(_GAL()+1);}break;
        case (0xB3<<3)|2: _B(c)=_GD();_NZ16(_C(c));_FETCH();break;
        case (0xB3<<3)|3: assert(false);break;
        case (0xB3<<3)|4: assert(false);break;
        case (0xB3<<3)|5: assert(false);break;
        case (0xB3<<3)|6: assert(false);break;
        case (0xB3<<3)|7: assert(false);break;
    /* LDY d,x */
        case (0xB4<<3)|0: _VPA();_SA(c->PC++);break;
        case (0xB4<<3)|1: c->AD=_GD();_SA(c->AD);break;
        case (0xB4<<3)|2: _VDA();if(_E(c)){_SA((c->AD+_X(c))&0x00FF);}else{_SA(c->AD+_X(c));}break;
        case (0xB4<<3)|3: _Y(c)=_GD();if(_i8(c)){_NZ(_Y(c));_FETCH();}else{_VDA();_SAL(_GAL()+1);}break;
        case (0xB4<<3)|4: _YH(c)=_GD();_NZ16(_Y(c));_FETCH();break;
        case (0xB4<<3)|5: assert(false);break;
        case (0xB4<<3)|6: assert(false);break;
        case (0xB4<<3)|7: assert(false);break;
    /* LDA d,x */
        case (0xB5<<3)|0: _VPA();_SA(c->PC++);break;
        case (0xB5<<3)|1: c->AD=_GD();_SA(c->AD);break;
        case (0xB5<<3)|2: _VDA();if(_E(c)){_SA((c->AD+_X(c))&0x00FF);}else{_SA(c->AD+_X(c));}break;
        case (0xB5<<3)|3: _A(c)=_GD();if(_a8(c)){_NZ(_A(c));_FETCH();}else{_VDA();_SAL(_GAL()+1);}break;
        case (0xB5<<3)|4: _B(c)=_GD();_NZ16(_C(c));_FETCH();break;
        case (0xB5<<3)|5: assert(false);break;
        case (0xB5<<3)|6: assert(false);break;
        case (0xB5<<3)|7: assert(false);break;
    /* LDX d,y */
        case (0xB6<<3)|0: _VPA();_SA(c->PC++);break;
        case (0xB6<<3)|1: c->AD=_GD();_SA(c->AD);break;
        case (0xB6<<3)|2: _VDA();if(_E(c)){_SA((c->AD+_Y(c))&0x00FF);}else{_SA(c->AD+_Y(c));}break;
        case (0xB6<<3)|3: _X(c)=_GD();if(_i8(c)){_NZ(_X(c));_FETCH();}else{_VDA();_SAL(_GAL()+1);}break;
        case (0xB6<<3)|4: _XH(c)=_GD();_NZ16(_X(c));_FETCH();break;
        case (0xB6<<3)|5: assert(false);break;
        case (0xB6<<3)|6: assert(false);break;
        case (0xB6<<3)|7: assert(false);break;
    /* LDA [d],y */
        case (0xB7<<3)|0: /* (unimpl) */;break;
        case (0xB7<<3)|1: _A(c)=_GD();if(_a8(c)){_NZ(_A(c));_FETCH();}else{_VDA();_SAL(_GAL()+1);}break;
        case (0xB7<<3)|2: _B(c)=_GD();_NZ16(_C(c));_FETCH();break;
        case (0xB7<<3)|3: assert(false);break;
        case (0xB7<<3)|4: assert(false);break;
        case (0xB7<<3)|5: assert(false);break;
        case (0xB7<<3)|6: assert(false);break;
        case (0xB7<<3)|7: assert(false);break;
    /* CLV i */
        case (0xB8<<3)|0: _SA(c->PC);break;
        case (0xB8<<3)|1: c->P&=~0x40;_FETCH();break;
        case (0xB8<<3)|2: assert(false);break;
        case (0xB8<<3)|3: assert(false);break;
        case (0xB8<<3)|4: assert(false);break;
        case (0xB8<<3)|5: assert(false);break;
        case (0xB8<<3)|6: assert(false);break;
        case (0xB8<<3)|7: assert(false);break;
    /* LDA a,y */
        case (0xB9<<3)|0: _VPA();_SA(c->PC++);break;
        case (0xB9<<3)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0xB9<<3)|2: c->AD|=_GD()<<8;_SA((c->AD&0xFF00)|((c->AD+_Y(c))&0xFF));c->IR+=(~((c->AD>>8)-((c->AD+_Y(c))>>8)))&1;break;
        case (0xB9<<3)|3: _VDA();_SA(c->AD+_Y(c));break;
        case (0xB9<<3)|4: _A(c)=_GD();if(_a8(c)){_NZ(_A(c));_FETCH();}else{_VDA();_SAL(_GAL()+1);}break;
        case (0xB9<<3)|5: _B(c)=_GD();_NZ16(_C(c));_FETCH();break;
        case (0xB9<<3)|6: assert(false);break;
        case (0xB9<<3)|7: assert(false);break;
    /* TSX i */
        case (0xBA<<3)|0: _SA(c->PC);break;
        case (0xBA<<3)|1: _X(c)=_S(c);_NZ(_X(c));_FETCH();break;
        case (0xBA<<3)|2: assert(false);break;
        case (0xBA<<3)|3: assert(false);break;
        case (0xBA<<3)|4: assert(false);break;
        case (0xBA<<3)|5: assert(false);break;
        case (0xBA<<3)|6: assert(false);break;
        case (0xBA<<3)|7: assert(false);break;
    /* TYX i */
        case (0xBB<<3)|0: _SA(c->PC);break;
        case (0xBB<<3)|1: _X(c)=_Y(c);_NZ(_X(c));_FETCH();break;
        case (0xBB<<3)|2: assert(false);break;
        case (0xBB<<3)|3: assert(false);break;
        case (0xBB<<3)|4: assert(false);break;
        case (0xBB<<3)|5: assert(false);break;
        case (0xBB<<3)|6: assert(false);break;
        case (0xBB<<3)|7: assert(false);break;
    /* LDY a,x */
        case (0xBC<<3)|0: _VPA();_SA(c->PC++);break;
        case (0xBC<<3)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0xBC<<3)|2: c->AD|=_GD()<<8;_SA((c->AD&0xFF00)|((c->AD+_X(c))&0xFF));c->IR+=(~((c->AD>>8)-((c->AD+_X(c))>>8)))&1;break;
        case (0xBC<<3)|3: _VDA();_SA(c->AD+_X(c));break;
        case (0xBC<<3)|4: _Y(c)=_GD();if(_i8(c)){_NZ(_Y(c));_FETCH();}else{_VDA();_SAL(_GAL()+1);}break;
        case (0xBC<<3)|5: _YH(c)=_GD();_NZ16(_Y(c));_FETCH();break;
        case (0xBC<<3)|6: assert(false);break;
        case (0xBC<<3)|7: assert(false);break;
    /* LDA a,x */
        case (0xBD<<3)|0: _VPA();_SA(c->PC++);break;
        case (0xBD<<3)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0xBD<<3)|2: c->AD|=_GD()<<8;_SA((c->AD&0xFF00)|((c->AD+_X(c))&0xFF));c->IR+=(~((c->AD>>8)-((c->AD+_X(c))>>8)))&1;break;
        case (0xBD<<3)|3: _VDA();_SA(c->AD+_X(c));break;
        case (0xBD<<3)|4: _A(c)=_GD();if(_a8(c)){_NZ(_A(c));_FETCH();}else{_VDA();_SAL(_GAL()+1);}break;
        case (0xBD<<3)|5: _B(c)=_GD();_NZ16(_C(c));_FETCH();break;
        case (0xBD<<3)|6: assert(false);break;
        case (0xBD<<3)|7: assert(false);break;
    /* LDX a,y */
        case (0xBE<<3)|0: _VPA();_SA(c->PC++);break;
        case (0xBE<<3)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0xBE<<3)|2: c->AD|=_GD()<<8;_SA((c->AD&0xFF00)|((c->AD+_Y(c))&0xFF));c->IR+=(~((c->AD>>8)-((c->AD+_Y(c))>>8)))&1;break;
        case (0xBE<<3)|3: _VDA();_SA(c->AD+_Y(c));break;
        case (0xBE<<3)|4: _X(c)=_GD();if(_i8(c)){_NZ(_X(c));_FETCH();}else{_VDA();_SAL(_GAL()+1);}break;
        case (0xBE<<3)|5: _XH(c)=_GD();_NZ16(_X(c));_FETCH();break;
        case (0xBE<<3)|6: assert(false);break;
        case (0xBE<<3)|7: assert(false);break;
    /* LDA al,x */
        case (0xBF<<3)|0: /* (unimpl) */;break;
        case (0xBF<<3)|1: _A(c)=_GD();if(_a8(c)){_NZ(_A(c));_FETCH();}else{_VDA();_SAL(_GAL()+1);}break;
        case (0xBF<<3)|2: _B(c)=_GD();_NZ16(_C(c));_FETCH();break;
        case (0xBF<<3)|3: assert(false);break;
        case (0xBF<<3)|4: assert(false);break;
        case (0xBF<<3)|5: assert(false);break;
        case (0xBF<<3)|6: assert(false);break;
        case (0xBF<<3)|7: assert(false);break;
    /* CPY # */
        case (0xC0<<3)|0: _VPA();_SA(c->PC++);break;
        case (0xC0<<3)|1: _w65816_cmp(c, _Y(c), _GD());_FETCH();break;
        case (0xC0<<3)|2: assert(false);break;
        case (0xC0<<3)|3: assert(false);break;
        case (0xC0<<3)|4: assert(false);break;
        case (0xC0<<3)|5: assert(false);break;
        case (0xC0<<3)|6: assert(false);break;
        case (0xC0<<3)|7: assert(false);break;
    /* CMP (d,x) */
        case (0xC1<<3)|0: _VPA();_SA(c->PC++);break;
        case (0xC1<<3)|1: c->AD=_GD();_SA(c->AD);break;
        case (0xC1<<3)|2: _VDA();c->AD=(c->AD+_X(c))&0xFF;_SA(c->AD);break;
        case (0xC1<<3)|3: _VDA();_SA((c->AD+1)&0xFF);c->AD=_GD();break;
        case (0xC1<<3)|4: _VDA();_SA((_GD()<<8)|c->AD);break;
        case (0xC1<<3)|5: _w65816_cmp(c, _A(c), _GD());_FETCH();break;
        case (0xC1<<3)|6: assert(false);break;
        case (0xC1<<3)|7: assert(false);break;
    /* REP # */
        case (0xC2<<3)|0: _VPA();_SA(c->PC++);break;
        case (0xC2<<3)|1: c->P&=~_GD();_SA(c->PC);break;
        case (0xC2<<3)|2: _FETCH();break;
        case (0xC2<<3)|3: assert(false);break;
        case (0xC2<<3)|4: assert(false);break;
        case (0xC2<<3)|5: assert(false);break;
        case (0xC2<<3)|6: assert(false);break;
        case (0xC2<<3)|7: assert(false);break;
    /* CMP d,s */
        case (0xC3<<3)|0: /* (unimpl) */;break;
        case (0xC3<<3)|1: _w65816_cmp(c, _A(c), _GD());break;
        case (0xC3<<3)|2: _FETCH();break;
        case (0xC3<<3)|3: assert(false);break;
        case (0xC3<<3)|4: assert(false);break;
        case (0xC3<<3)|5: assert(false);break;
        case (0xC3<<3)|6: assert(false);break;
        case (0xC3<<3)|7: assert(false);break;
    /* CPY d */
        case (0xC4<<3)|0: _VPA();_SA(c->PC++);break;
        case (0xC4<<3)|1: _VDA();_SA(_GD());break;
        case (0xC4<<3)|2: _w65816_cmp(c, _Y(c), _GD());_FETCH();break;
        case (0xC4<<3)|3: assert(false);break;
        case (0xC4<<3)|4: assert(false);break;
        case (0xC4<<3)|5: assert(false);break;
        case (0xC4<<3)|6: assert(false);break;
        case (0xC4<<3)|7: assert(false);break;
    /* CMP d */
        case (0xC5<<3)|0: _VPA();_SA(c->PC++);break;
        case (0xC5<<3)|1: _VDA();_SA(_GD());break;
        case (0xC5<<3)|2: _w65816_cmp(c, _A(c), _GD());_FETCH();break;
        case (0xC5<<3)|3: assert(false);break;
        case (0xC5<<3)|4: assert(false);break;
        case (0xC5<<3)|5: assert(false);break;
        case (0xC5<<3)|6: assert(false);break;
        case (0xC5<<3)|7: assert(false);break;
    /* DEC d */
        case (0xC6<<3)|0: _VPA();_SA(c->PC++);break;
        case (0xC6<<3)|1: _VDA();_SA(_GD());break;
        case (0xC6<<3)|2: c->AD=_GD();if(_E(c)){_WR();}break;
        case (0xC6<<3)|3: _VDA();c->AD--;_NZ(c->AD);_SD(c->AD);_WR();break;
        case (0xC6<<3)|4: _FETCH();break;
        case (0xC6<<3)|5: assert(false);break;
        case (0xC6<<3)|6: assert(false);break;
        case (0xC6<<3)|7: assert(false);break;
    /* CMP [d] */
        case (0xC7<<3)|0: /* (unimpl) */;break;
        case (0xC7<<3)|1: _w65816_cmp(c, _A(c), _GD());break;
        case (0xC7<<3)|2: _FETCH();break;
        case (0xC7<<3)|3: assert(false);break;
        case (0xC7<<3)|4: assert(false);break;
        case (0xC7<<3)|5: assert(false);break;
        case (0xC7<<3)|6: assert(false);break;
        case (0xC7<<3)|7: assert(false);break;
    /* INY i */
        case (0xC8<<3)|0: _SA(c->PC);break;
        case (0xC8<<3)|1: _Y(c)++;_NZ(_Y(c));_FETCH();break;
        case (0xC8<<3)|2: assert(false);break;
        case (0xC8<<3)|3: assert(false);break;
        case (0xC8<<3)|4: assert(false);break;
        case (0xC8<<3)|5: assert(false);break;
        case (0xC8<<3)|6: assert(false);break;
        case (0xC8<<3)|7: assert(false);break;
    /* CMP # */
        case (0xC9<<3)|0: _VPA();_SA(c->PC++);break;
        case (0xC9<<3)|1: _w65816_cmp(c, _A(c), _GD());_FETCH();break;
        case (0xC9<<3)|2: assert(false);break;
        case (0xC9<<3)|3: assert(false);break;
        case (0xC9<<3)|4: assert(false);break;
        case (0xC9<<3)|5: assert(false);break;
        case (0xC9<<3)|6: assert(false);break;
        case (0xC9<<3)|7: assert(false);break;
    /* DEX i */
        case (0xCA<<3)|0: _SA(c->PC);break;
        case (0xCA<<3)|1: _X(c)--;_NZ(_X(c));_FETCH();break;
        case (0xCA<<3)|2: assert(false);break;
        case (0xCA<<3)|3: assert(false);break;
        case (0xCA<<3)|4: assert(false);break;
        case (0xCA<<3)|5: assert(false);break;
        case (0xCA<<3)|6: assert(false);break;
        case (0xCA<<3)|7: assert(false);break;
    /* WAI i (unimpl) */
        case (0xCB<<3)|0: _SA(c->PC);break;
        case (0xCB<<3)|1: _FETCH();break;
        case (0xCB<<3)|2: assert(false);break;
        case (0xCB<<3)|3: assert(false);break;
        case (0xCB<<3)|4: assert(false);break;
        case (0xCB<<3)|5: assert(false);break;
        case (0xCB<<3)|6: assert(false);break;
        case (0xCB<<3)|7: assert(false);break;
    /* CPY a */
        case (0xCC<<3)|0: _VPA();_SA(c->PC++);break;
        case (0xCC<<3)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0xCC<<3)|2: _SA((_GD()<<8)|c->AD);break;
        case (0xCC<<3)|3: _w65816_cmp(c, _Y(c), _GD());_FETCH();break;
        case (0xCC<<3)|4: assert(false);break;
        case (0xCC<<3)|5: assert(false);break;
        case (0xCC<<3)|6: assert(false);break;
        case (0xCC<<3)|7: assert(false);break;
    /* CMP a */
        case (0xCD<<3)|0: _VPA();_SA(c->PC++);break;
        case (0xCD<<3)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0xCD<<3)|2: _SA((_GD()<<8)|c->AD);break;
        case (0xCD<<3)|3: _w65816_cmp(c, _A(c), _GD());_FETCH();break;
        case (0xCD<<3)|4: assert(false);break;
        case (0xCD<<3)|5: assert(false);break;
        case (0xCD<<3)|6: assert(false);break;
        case (0xCD<<3)|7: assert(false);break;
    /* DEC a */
        case (0xCE<<3)|0: _VPA();_SA(c->PC++);break;
        case (0xCE<<3)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0xCE<<3)|2: _SA((_GD()<<8)|c->AD);break;
        case (0xCE<<3)|3: c->AD=_GD();if(_E(c)){_WR();}break;
        case (0xCE<<3)|4: _VDA();c->AD--;_NZ(c->AD);_SD(c->AD);_WR();break;
        case (0xCE<<3)|5: _FETCH();break;
        case (0xCE<<3)|6: assert(false);break;
        case (0xCE<<3)|7: assert(false);break;
    /* CMP al */
        case (0xCF<<3)|0: /* (unimpl) */;break;
        case (0xCF<<3)|1: _w65816_cmp(c, _A(c), _GD());break;
        case (0xCF<<3)|2: _FETCH();break;
        case (0xCF<<3)|3: assert(false);break;
        case (0xCF<<3)|4: assert(false);break;
        case (0xCF<<3)|5: assert(false);break;
        case (0xCF<<3)|6: assert(false);break;
        case (0xCF<<3)|7: assert(false);break;
    /* BNE r */
        case (0xD0<<3)|0: _VPA();_SA(c->PC++);break;
        case (0xD0<<3)|1: _SA(c->PC);c->AD=c->PC+(int8_t)_GD();if((c->P&0x2)!=0x0){_FETCH();};break;
        case (0xD0<<3)|2: _SA((c->PC&0xFF00)|(c->AD&0x00FF));if((c->AD&0xFF00)==(c->PC&0xFF00)){c->PC=c->AD;c->irq_pip>>=1;c->nmi_pip>>=1;_FETCH();};break;
        case (0xD0<<3)|3: c->PC=c->AD;_FETCH();break;
        case (0xD0<<3)|4: assert(false);break;
        case (0xD0<<3)|5: assert(false);break;
        case (0xD0<<3)|6: assert(false);break;
        case (0xD0<<3)|7: assert(false);break;
    /* CMP (d),y */
        case (0xD1<<3)|0: _VPA();_SA(c->PC++);break;
        case (0xD1<<3)|1: _VDA();c->AD=_GD();_SA(c->AD);break;
        case (0xD1<<3)|2: _VDA();_SA((c->AD+1)&0xFF);c->AD=_GD();break;
        case (0xD1<<3)|3: c->AD|=_GD()<<8;_SA((c->AD&0xFF00)|((c->AD+_Y(c))&0xFF));c->IR+=(~((c->AD>>8)-((c->AD+_Y(c))>>8)))&1;break;
        case (0xD1<<3)|4: _VDA();_SA(c->AD+_Y(c));break;
        case (0xD1<<3)|5: _w65816_cmp(c, _A(c), _GD());_FETCH();break;
        case (0xD1<<3)|6: assert(false);break;
        case (0xD1<<3)|7: assert(false);break;
    /* CMP (d) */
        case (0xD2<<3)|0: /* (unimpl) */;break;
        case (0xD2<<3)|1: _w65816_cmp(c, _A(c), _GD());break;
        case (0xD2<<3)|2: _FETCH();break;
        case (0xD2<<3)|3: assert(false);break;
        case (0xD2<<3)|4: assert(false);break;
        case (0xD2<<3)|5: assert(false);break;
        case (0xD2<<3)|6: assert(false);break;
        case (0xD2<<3)|7: assert(false);break;
    /* CMP (d,s),y */
        case (0xD3<<3)|0: /* (unimpl) */;break;
        case (0xD3<<3)|1: _w65816_cmp(c, _A(c), _GD());break;
        case (0xD3<<3)|2: _FETCH();break;
        case (0xD3<<3)|3: assert(false);break;
        case (0xD3<<3)|4: assert(false);break;
        case (0xD3<<3)|5: assert(false);break;
        case (0xD3<<3)|6: assert(false);break;
        case (0xD3<<3)|7: assert(false);break;
    /* PEI s (unimpl) */
        case (0xD4<<3)|0: _SA(c->PC);break;
        case (0xD4<<3)|1: _FETCH();break;
        case (0xD4<<3)|2: assert(false);break;
        case (0xD4<<3)|3: assert(false);break;
        case (0xD4<<3)|4: assert(false);break;
        case (0xD4<<3)|5: assert(false);break;
        case (0xD4<<3)|6: assert(false);break;
        case (0xD4<<3)|7: assert(false);break;
    /* CMP d,x */
        case (0xD5<<3)|0: _VPA();_SA(c->PC++);break;
        case (0xD5<<3)|1: c->AD=_GD();_SA(c->AD);break;
        case (0xD5<<3)|2: _VDA();if(_E(c)){_SA((c->AD+_X(c))&0x00FF);}else{_SA(c->AD+_X(c));}break;
        case (0xD5<<3)|3: _w65816_cmp(c, _A(c), _GD());_FETCH();break;
        case (0xD5<<3)|4: assert(false);break;
        case (0xD5<<3)|5: assert(false);break;
        case (0xD5<<3)|6: assert(false);break;
        case (0xD5<<3)|7: assert(false);break;
    /* DEC d,x */
        case (0xD6<<3)|0: _VPA();_SA(c->PC++);break;
        case (0xD6<<3)|1: c->AD=_GD();_SA(c->AD);break;
        case (0xD6<<3)|2: _VDA();if(_E(c)){_SA((c->AD+_X(c))&0x00FF);}else{_SA(c->AD+_X(c));}break;
        case (0xD6<<3)|3: c->AD=_GD();if(_E(c)){_WR();}break;
        case (0xD6<<3)|4: _VDA();c->AD--;_NZ(c->AD);_SD(c->AD);_WR();break;
        case (0xD6<<3)|5: _FETCH();break;
        case (0xD6<<3)|6: assert(false);break;
        case (0xD6<<3)|7: assert(false);break;
    /* CMP [d],y */
        case (0xD7<<3)|0: /* (unimpl) */;break;
        case (0xD7<<3)|1: _w65816_cmp(c, _A(c), _GD());break;
        case (0xD7<<3)|2: _FETCH();break;
        case (0xD7<<3)|3: assert(false);break;
        case (0xD7<<3)|4: assert(false);break;
        case (0xD7<<3)|5: assert(false);break;
        case (0xD7<<3)|6: assert(false);break;
        case (0xD7<<3)|7: assert(false);break;
    /* CLD i */
        case (0xD8<<3)|0: _SA(c->PC);break;
        case (0xD8<<3)|1: c->P&=~0x8;_FETCH();break;
        case (0xD8<<3)|2: assert(false);break;
        case (0xD8<<3)|3: assert(false);break;
        case (0xD8<<3)|4: assert(false);break;
        case (0xD8<<3)|5: assert(false);break;
        case (0xD8<<3)|6: assert(false);break;
        case (0xD8<<3)|7: assert(false);break;
    /* CMP a,y */
        case (0xD9<<3)|0: _VPA();_SA(c->PC++);break;
        case (0xD9<<3)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0xD9<<3)|2: c->AD|=_GD()<<8;_SA((c->AD&0xFF00)|((c->AD+_Y(c))&0xFF));c->IR+=(~((c->AD>>8)-((c->AD+_Y(c))>>8)))&1;break;
        case (0xD9<<3)|3: _VDA();_SA(c->AD+_Y(c));break;
        case (0xD9<<3)|4: _w65816_cmp(c, _A(c), _GD());_FETCH();break;
        case (0xD9<<3)|5: assert(false);break;
        case (0xD9<<3)|6: assert(false);break;
        case (0xD9<<3)|7: assert(false);break;
    /* PHX s */
        case (0xDA<<3)|0: _SA(c->PC);break;
        case (0xDA<<3)|1: _VDA();_SAD(0x0100|_S(c)--,_X(c));_WR();break;
        case (0xDA<<3)|2: _FETCH();break;
        case (0xDA<<3)|3: assert(false);break;
        case (0xDA<<3)|4: assert(false);break;
        case (0xDA<<3)|5: assert(false);break;
        case (0xDA<<3)|6: assert(false);break;
        case (0xDA<<3)|7: assert(false);break;
    /* STP i (unimpl) */
        case (0xDB<<3)|0: _SA(c->PC);break;
        case (0xDB<<3)|1: break;
        case (0xDB<<3)|2: _FETCH();break;
        case (0xDB<<3)|3: assert(false);break;
        case (0xDB<<3)|4: assert(false);break;
        case (0xDB<<3)|5: assert(false);break;
        case (0xDB<<3)|6: assert(false);break;
        case (0xDB<<3)|7: assert(false);break;
    /* JML (a) (unimpl) */
        case (0xDC<<3)|0: _FETCH();break;
        case (0xDC<<3)|1: assert(false);break;
        case (0xDC<<3)|2: assert(false);break;
        case (0xDC<<3)|3: assert(false);break;
        case (0xDC<<3)|4: assert(false);break;
        case (0xDC<<3)|5: assert(false);break;
        case (0xDC<<3)|6: assert(false);break;
        case (0xDC<<3)|7: assert(false);break;
    /* CMP a,x */
        case (0xDD<<3)|0: _VPA();_SA(c->PC++);break;
        case (0xDD<<3)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0xDD<<3)|2: c->AD|=_GD()<<8;_SA((c->AD&0xFF00)|((c->AD+_X(c))&0xFF));c->IR+=(~((c->AD>>8)-((c->AD+_X(c))>>8)))&1;break;
        case (0xDD<<3)|3: _VDA();_SA(c->AD+_X(c));break;
        case (0xDD<<3)|4: _w65816_cmp(c, _A(c), _GD());_FETCH();break;
        case (0xDD<<3)|5: assert(false);break;
        case (0xDD<<3)|6: assert(false);break;
        case (0xDD<<3)|7: assert(false);break;
    /* DEC a,x */
        case (0xDE<<3)|0: _VPA();_SA(c->PC++);break;
        case (0xDE<<3)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0xDE<<3)|2: c->AD|=_GD()<<8;_SA((c->AD&0xFF00)|((c->AD+_X(c))&0xFF));break;
        case (0xDE<<3)|3: _VDA();_SA(c->AD+_X(c));break;
        case (0xDE<<3)|4: c->AD=_GD();if(_E(c)){_WR();}break;
        case (0xDE<<3)|5: _VDA();c->AD--;_NZ(c->AD);_SD(c->AD);_WR();break;
        case (0xDE<<3)|6: _FETCH();break;
        case (0xDE<<3)|7: assert(false);break;
    /* CMP al,x */
        case (0xDF<<3)|0: /* (unimpl) */;break;
        case (0xDF<<3)|1: _w65816_cmp(c, _A(c), _GD());break;
        case (0xDF<<3)|2: _FETCH();break;
        case (0xDF<<3)|3: assert(false);break;
        case (0xDF<<3)|4: assert(false);break;
        case (0xDF<<3)|5: assert(false);break;
        case (0xDF<<3)|6: assert(false);break;
        case (0xDF<<3)|7: assert(false);break;
    /* CPX # */
        case (0xE0<<3)|0: _VPA();_SA(c->PC++);break;
        case (0xE0<<3)|1: _w65816_cmp(c, _X(c), _GD());_FETCH();break;
        case (0xE0<<3)|2: assert(false);break;
        case (0xE0<<3)|3: assert(false);break;
        case (0xE0<<3)|4: assert(false);break;
        case (0xE0<<3)|5: assert(false);break;
        case (0xE0<<3)|6: assert(false);break;
        case (0xE0<<3)|7: assert(false);break;
    /* SBC (d,x) */
        case (0xE1<<3)|0: _VPA();_SA(c->PC++);break;
        case (0xE1<<3)|1: c->AD=_GD();_SA(c->AD);break;
        case (0xE1<<3)|2: _VDA();c->AD=(c->AD+_X(c))&0xFF;_SA(c->AD);break;
        case (0xE1<<3)|3: _VDA();_SA((c->AD+1)&0xFF);c->AD=_GD();break;
        case (0xE1<<3)|4: _VDA();_SA((_GD()<<8)|c->AD);break;
        case (0xE1<<3)|5: _w65816_sbc(c,_GD());_FETCH();break;
        case (0xE1<<3)|6: assert(false);break;
        case (0xE1<<3)|7: assert(false);break;
    /* SEP # */
        case (0xE2<<3)|0: _VPA();_SA(c->PC++);break;
        case (0xE2<<3)|1: c->P|=_GD();_SA(c->PC);break;
        case (0xE2<<3)|2: _FETCH();break;
        case (0xE2<<3)|3: assert(false);break;
        case (0xE2<<3)|4: assert(false);break;
        case (0xE2<<3)|5: assert(false);break;
        case (0xE2<<3)|6: assert(false);break;
        case (0xE2<<3)|7: assert(false);break;
    /* SBC d,s */
        case (0xE3<<3)|0: /* (unimpl) */;break;
        case (0xE3<<3)|1: _w65816_sbc(c,_GD());break;
        case (0xE3<<3)|2: _FETCH();break;
        case (0xE3<<3)|3: assert(false);break;
        case (0xE3<<3)|4: assert(false);break;
        case (0xE3<<3)|5: assert(false);break;
        case (0xE3<<3)|6: assert(false);break;
        case (0xE3<<3)|7: assert(false);break;
    /* CPX d */
        case (0xE4<<3)|0: _VPA();_SA(c->PC++);break;
        case (0xE4<<3)|1: _VDA();_SA(_GD());break;
        case (0xE4<<3)|2: _w65816_cmp(c, _X(c), _GD());_FETCH();break;
        case (0xE4<<3)|3: assert(false);break;
        case (0xE4<<3)|4: assert(false);break;
        case (0xE4<<3)|5: assert(false);break;
        case (0xE4<<3)|6: assert(false);break;
        case (0xE4<<3)|7: assert(false);break;
    /* SBC d */
        case (0xE5<<3)|0: _VPA();_SA(c->PC++);break;
        case (0xE5<<3)|1: _VDA();_SA(_GD());break;
        case (0xE5<<3)|2: _w65816_sbc(c,_GD());_FETCH();break;
        case (0xE5<<3)|3: assert(false);break;
        case (0xE5<<3)|4: assert(false);break;
        case (0xE5<<3)|5: assert(false);break;
        case (0xE5<<3)|6: assert(false);break;
        case (0xE5<<3)|7: assert(false);break;
    /* INC d */
        case (0xE6<<3)|0: _VPA();_SA(c->PC++);break;
        case (0xE6<<3)|1: _VDA();_SA(_GD());break;
        case (0xE6<<3)|2: c->AD=_GD();if(_E(c)){_WR();}break;
        case (0xE6<<3)|3: _VDA();c->AD++;_NZ(c->AD);_SD(c->AD);_WR();break;
        case (0xE6<<3)|4: _FETCH();break;
        case (0xE6<<3)|5: assert(false);break;
        case (0xE6<<3)|6: assert(false);break;
        case (0xE6<<3)|7: assert(false);break;
    /* SBC [d] */
        case (0xE7<<3)|0: /* (unimpl) */;break;
        case (0xE7<<3)|1: _w65816_sbc(c,_GD());break;
        case (0xE7<<3)|2: _FETCH();break;
        case (0xE7<<3)|3: assert(false);break;
        case (0xE7<<3)|4: assert(false);break;
        case (0xE7<<3)|5: assert(false);break;
        case (0xE7<<3)|6: assert(false);break;
        case (0xE7<<3)|7: assert(false);break;
    /* INX i */
        case (0xE8<<3)|0: _SA(c->PC);break;
        case (0xE8<<3)|1: _X(c)++;_NZ(_X(c));_FETCH();break;
        case (0xE8<<3)|2: assert(false);break;
        case (0xE8<<3)|3: assert(false);break;
        case (0xE8<<3)|4: assert(false);break;
        case (0xE8<<3)|5: assert(false);break;
        case (0xE8<<3)|6: assert(false);break;
        case (0xE8<<3)|7: assert(false);break;
    /* SBC # */
        case (0xE9<<3)|0: _VPA();_SA(c->PC++);break;
        case (0xE9<<3)|1: _w65816_sbc(c,_GD());_FETCH();break;
        case (0xE9<<3)|2: assert(false);break;
        case (0xE9<<3)|3: assert(false);break;
        case (0xE9<<3)|4: assert(false);break;
        case (0xE9<<3)|5: assert(false);break;
        case (0xE9<<3)|6: assert(false);break;
        case (0xE9<<3)|7: assert(false);break;
    /* NOP i */
        case (0xEA<<3)|0: _SA(c->PC);break;
        case (0xEA<<3)|1: _FETCH();break;
        case (0xEA<<3)|2: assert(false);break;
        case (0xEA<<3)|3: assert(false);break;
        case (0xEA<<3)|4: assert(false);break;
        case (0xEA<<3)|5: assert(false);break;
        case (0xEA<<3)|6: assert(false);break;
        case (0xEA<<3)|7: assert(false);break;
    /* XBA i */
        case (0xEB<<3)|0: _SA(c->PC);break;
        case (0xEB<<3)|1: _SA(c->PC);break;
        case (0xEB<<3)|2: _w65816_xba(c);_FETCH();break;
        case (0xEB<<3)|3: assert(false);break;
        case (0xEB<<3)|4: assert(false);break;
        case (0xEB<<3)|5: assert(false);break;
        case (0xEB<<3)|6: assert(false);break;
        case (0xEB<<3)|7: assert(false);break;
    /* CPX a */
        case (0xEC<<3)|0: _VPA();_SA(c->PC++);break;
        case (0xEC<<3)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0xEC<<3)|2: _SA((_GD()<<8)|c->AD);break;
        case (0xEC<<3)|3: _w65816_cmp(c, _X(c), _GD());_FETCH();break;
        case (0xEC<<3)|4: assert(false);break;
        case (0xEC<<3)|5: assert(false);break;
        case (0xEC<<3)|6: assert(false);break;
        case (0xEC<<3)|7: assert(false);break;
    /* SBC a */
        case (0xED<<3)|0: _VPA();_SA(c->PC++);break;
        case (0xED<<3)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0xED<<3)|2: _SA((_GD()<<8)|c->AD);break;
        case (0xED<<3)|3: _w65816_sbc(c,_GD());_FETCH();break;
        case (0xED<<3)|4: assert(false);break;
        case (0xED<<3)|5: assert(false);break;
        case (0xED<<3)|6: assert(false);break;
        case (0xED<<3)|7: assert(false);break;
    /* INC a */
        case (0xEE<<3)|0: _VPA();_SA(c->PC++);break;
        case (0xEE<<3)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0xEE<<3)|2: _SA((_GD()<<8)|c->AD);break;
        case (0xEE<<3)|3: c->AD=_GD();if(_E(c)){_WR();}break;
        case (0xEE<<3)|4: _VDA();c->AD++;_NZ(c->AD);_SD(c->AD);_WR();break;
        case (0xEE<<3)|5: _FETCH();break;
        case (0xEE<<3)|6: assert(false);break;
        case (0xEE<<3)|7: assert(false);break;
    /* SBC al */
        case (0xEF<<3)|0: /* (unimpl) */;break;
        case (0xEF<<3)|1: _w65816_sbc(c,_GD());break;
        case (0xEF<<3)|2: _FETCH();break;
        case (0xEF<<3)|3: assert(false);break;
        case (0xEF<<3)|4: assert(false);break;
        case (0xEF<<3)|5: assert(false);break;
        case (0xEF<<3)|6: assert(false);break;
        case (0xEF<<3)|7: assert(false);break;
    /* BEQ r */
        case (0xF0<<3)|0: _VPA();_SA(c->PC++);break;
        case (0xF0<<3)|1: _SA(c->PC);c->AD=c->PC+(int8_t)_GD();if((c->P&0x2)!=0x2){_FETCH();};break;
        case (0xF0<<3)|2: _SA((c->PC&0xFF00)|(c->AD&0x00FF));if((c->AD&0xFF00)==(c->PC&0xFF00)){c->PC=c->AD;c->irq_pip>>=1;c->nmi_pip>>=1;_FETCH();};break;
        case (0xF0<<3)|3: c->PC=c->AD;_FETCH();break;
        case (0xF0<<3)|4: assert(false);break;
        case (0xF0<<3)|5: assert(false);break;
        case (0xF0<<3)|6: assert(false);break;
        case (0xF0<<3)|7: assert(false);break;
    /* SBC (d),y */
        case (0xF1<<3)|0: _VPA();_SA(c->PC++);break;
        case (0xF1<<3)|1: _VDA();c->AD=_GD();_SA(c->AD);break;
        case (0xF1<<3)|2: _VDA();_SA((c->AD+1)&0xFF);c->AD=_GD();break;
        case (0xF1<<3)|3: c->AD|=_GD()<<8;_SA((c->AD&0xFF00)|((c->AD+_Y(c))&0xFF));c->IR+=(~((c->AD>>8)-((c->AD+_Y(c))>>8)))&1;break;
        case (0xF1<<3)|4: _VDA();_SA(c->AD+_Y(c));break;
        case (0xF1<<3)|5: _w65816_sbc(c,_GD());_FETCH();break;
        case (0xF1<<3)|6: assert(false);break;
        case (0xF1<<3)|7: assert(false);break;
    /* SBC (d) */
        case (0xF2<<3)|0: /* (unimpl) */;break;
        case (0xF2<<3)|1: _w65816_sbc(c,_GD());break;
        case (0xF2<<3)|2: _FETCH();break;
        case (0xF2<<3)|3: assert(false);break;
        case (0xF2<<3)|4: assert(false);break;
        case (0xF2<<3)|5: assert(false);break;
        case (0xF2<<3)|6: assert(false);break;
        case (0xF2<<3)|7: assert(false);break;
    /* SBC (d,s),y */
        case (0xF3<<3)|0: /* (unimpl) */;break;
        case (0xF3<<3)|1: _w65816_sbc(c,_GD());break;
        case (0xF3<<3)|2: _FETCH();break;
        case (0xF3<<3)|3: assert(false);break;
        case (0xF3<<3)|4: assert(false);break;
        case (0xF3<<3)|5: assert(false);break;
        case (0xF3<<3)|6: assert(false);break;
        case (0xF3<<3)|7: assert(false);break;
    /* PEA s (unimpl) */
        case (0xF4<<3)|0: _SA(c->PC);break;
        case (0xF4<<3)|1: _FETCH();break;
        case (0xF4<<3)|2: assert(false);break;
        case (0xF4<<3)|3: assert(false);break;
        case (0xF4<<3)|4: assert(false);break;
        case (0xF4<<3)|5: assert(false);break;
        case (0xF4<<3)|6: assert(false);break;
        case (0xF4<<3)|7: assert(false);break;
    /* SBC d,x */
        case (0xF5<<3)|0: _VPA();_SA(c->PC++);break;
        case (0xF5<<3)|1: c->AD=_GD();_SA(c->AD);break;
        case (0xF5<<3)|2: _VDA();if(_E(c)){_SA((c->AD+_X(c))&0x00FF);}else{_SA(c->AD+_X(c));}break;
        case (0xF5<<3)|3: _w65816_sbc(c,_GD());_FETCH();break;
        case (0xF5<<3)|4: assert(false);break;
        case (0xF5<<3)|5: assert(false);break;
        case (0xF5<<3)|6: assert(false);break;
        case (0xF5<<3)|7: assert(false);break;
    /* INC d,x */
        case (0xF6<<3)|0: _VPA();_SA(c->PC++);break;
        case (0xF6<<3)|1: c->AD=_GD();_SA(c->AD);break;
        case (0xF6<<3)|2: _VDA();if(_E(c)){_SA((c->AD+_X(c))&0x00FF);}else{_SA(c->AD+_X(c));}break;
        case (0xF6<<3)|3: c->AD=_GD();if(_E(c)){_WR();}break;
        case (0xF6<<3)|4: _VDA();c->AD++;_NZ(c->AD);_SD(c->AD);_WR();break;
        case (0xF6<<3)|5: _FETCH();break;
        case (0xF6<<3)|6: assert(false);break;
        case (0xF6<<3)|7: assert(false);break;
    /* SBC [d],y */
        case (0xF7<<3)|0: /* (unimpl) */;break;
        case (0xF7<<3)|1: _w65816_sbc(c,_GD());break;
        case (0xF7<<3)|2: _FETCH();break;
        case (0xF7<<3)|3: assert(false);break;
        case (0xF7<<3)|4: assert(false);break;
        case (0xF7<<3)|5: assert(false);break;
        case (0xF7<<3)|6: assert(false);break;
        case (0xF7<<3)|7: assert(false);break;
    /* SED i */
        case (0xF8<<3)|0: _SA(c->PC);break;
        case (0xF8<<3)|1: c->P|=0x8;_FETCH();break;
        case (0xF8<<3)|2: assert(false);break;
        case (0xF8<<3)|3: assert(false);break;
        case (0xF8<<3)|4: assert(false);break;
        case (0xF8<<3)|5: assert(false);break;
        case (0xF8<<3)|6: assert(false);break;
        case (0xF8<<3)|7: assert(false);break;
    /* SBC a,y */
        case (0xF9<<3)|0: _VPA();_SA(c->PC++);break;
        case (0xF9<<3)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0xF9<<3)|2: c->AD|=_GD()<<8;_SA((c->AD&0xFF00)|((c->AD+_Y(c))&0xFF));c->IR+=(~((c->AD>>8)-((c->AD+_Y(c))>>8)))&1;break;
        case (0xF9<<3)|3: _VDA();_SA(c->AD+_Y(c));break;
        case (0xF9<<3)|4: _w65816_sbc(c,_GD());_FETCH();break;
        case (0xF9<<3)|5: assert(false);break;
        case (0xF9<<3)|6: assert(false);break;
        case (0xF9<<3)|7: assert(false);break;
    /* PLX s */
        case (0xFA<<3)|0: _SA(c->PC);break;
        case (0xFA<<3)|1: _SA(c->PC);break;
        case (0xFA<<3)|2: _VDA();_SA(0x0100|++_S(c));break;
        case (0xFA<<3)|3: _X(c)=_GD();_NZ(_X(c));_FETCH();break;
        case (0xFA<<3)|4: assert(false);break;
        case (0xFA<<3)|5: assert(false);break;
        case (0xFA<<3)|6: assert(false);break;
        case (0xFA<<3)|7: assert(false);break;
    /* XCE i */
        case (0xFB<<3)|0: _SA(c->PC);break;
        case (0xFB<<3)|1: _w65816_xce(c);_FETCH();break;
        case (0xFB<<3)|2: assert(false);break;
        case (0xFB<<3)|3: assert(false);break;
        case (0xFB<<3)|4: assert(false);break;
        case (0xFB<<3)|5: assert(false);break;
        case (0xFB<<3)|6: assert(false);break;
        case (0xFB<<3)|7: assert(false);break;
    /* JSR (a,x) */
        case (0xFC<<3)|0: _VPA();_SA(c->PC++);break;
        case (0xFC<<3)|1: _VDA();_SAD(0x0100|_S(c)--,c->PC>>8);_WR();break;
        case (0xFC<<3)|2: _VDA();_SAD(0x0100|_S(c)--,c->PC);_WR();break;
        case (0xFC<<3)|3: _VPA();_SA(c->PC);break;
        case (0xFC<<3)|4: _SA(c->PC);c->AD=(_GD()<<8)|c->AD;break;
        case (0xFC<<3)|5: _VDA();_SA(c->AD+_X(c));break;
        case (0xFC<<3)|6: _VDA();_SA(c->AD+_X(c)+1);c->AD=_GD();break;
        case (0xFC<<3)|7: c->PC=(_GD()<<8)|c->AD;_FETCH();break;
    /* SBC a,x */
        case (0xFD<<3)|0: _VPA();_SA(c->PC++);break;
        case (0xFD<<3)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0xFD<<3)|2: c->AD|=_GD()<<8;_SA((c->AD&0xFF00)|((c->AD+_X(c))&0xFF));c->IR+=(~((c->AD>>8)-((c->AD+_X(c))>>8)))&1;break;
        case (0xFD<<3)|3: _VDA();_SA(c->AD+_X(c));break;
        case (0xFD<<3)|4: _w65816_sbc(c,_GD());_FETCH();break;
        case (0xFD<<3)|5: assert(false);break;
        case (0xFD<<3)|6: assert(false);break;
        case (0xFD<<3)|7: assert(false);break;
    /* INC a,x */
        case (0xFE<<3)|0: _VPA();_SA(c->PC++);break;
        case (0xFE<<3)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0xFE<<3)|2: c->AD|=_GD()<<8;_SA((c->AD&0xFF00)|((c->AD+_X(c))&0xFF));break;
        case (0xFE<<3)|3: _VDA();_SA(c->AD+_X(c));break;
        case (0xFE<<3)|4: c->AD=_GD();if(_E(c)){_WR();}break;
        case (0xFE<<3)|5: _VDA();c->AD++;_NZ(c->AD);_SD(c->AD);_WR();break;
        case (0xFE<<3)|6: _FETCH();break;
        case (0xFE<<3)|7: assert(false);break;
    /* SBC al,x */
        case (0xFF<<3)|0: /* (unimpl) */;break;
        case (0xFF<<3)|1: _w65816_sbc(c,_GD());break;
        case (0xFF<<3)|2: _FETCH();break;
        case (0xFF<<3)|3: assert(false);break;
        case (0xFF<<3)|4: assert(false);break;
        case (0xFF<<3)|5: assert(false);break;
        case (0xFF<<3)|6: assert(false);break;
        case (0xFF<<3)|7: assert(false);break;
    // %>
        default: _W65816_UNREACHABLE;
    }
    c->PINS = pins;
    c->irq_pip <<= 1;
    c->nmi_pip <<= 1;
    if (c->emulation) {
        // CPU is in Emulation mode
        // Stack is confined to page 01
        c->S = 0x0100 | (c->S&0xFF);
        // Unused flag is always 1
        c->P |= W65816_UF;
    }
    if (c->emulation | (c->P & W65816_XF)) {
        // CPU is in Emulation mode or registers are in eight-bit mode (X=1)
        // the index registers high byte are zero
        c->X = c->X & 0xFF;
        c->Y = c->Y & 0xFF;
    }
    return pins;
}
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#undef _SA
#undef _SAD
#undef _FETCH
#undef _SD
#undef _GD
#undef _ON
#undef _OFF
#undef _RD
#undef _WR
#undef _NZ
#endif /* CHIPS_IMPL */
