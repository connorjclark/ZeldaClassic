#ifndef BYTECODE_H
#define BYTECODE_H

#include "zasm/defines.h"
#include "ByteCode_gen.h"

/*
 SP - stack pointer
 D4 - stack frame pointer
 D6 - stack frame offset accumulator
 D2 - expression accumulator #1
 D3 - expression accumulator #2
 D0 - array index accumulator
 D1 - secondary array index accumulator
 D5 - pure SETR sink
 */
#define INDEX                   rINDEX
#define INDEX2                  rINDEX2
#define EXP1                    rEXP1
#define EXP2                    rEXP2
#define SFRAME                  rSFRAME
#define NUL                     rNUL
#define SFTEMP                  rSFTEMP
#define WHAT_NO_7               rWHAT_NO_7

//{ INTERNAL ARRAYS

#define INTARR_OFFS 65536
#define INTARR_SCREEN_NPC       (65536+0)
#define INTARR_SCREEN_ITEMSPR   (65536+1)
#define INTARR_SCREEN_LWPN      (65536+2)
#define INTARR_SCREEN_EWPN      (65536+3)
#define INTARR_SCREEN_FFC       (65536+4)
#define INTARR_SCREEN_PORTALS   (65536+5)
#define INTARR_SAVPRTL          (65536+6)

//} END INTERNAL ARRAYS

#endif
