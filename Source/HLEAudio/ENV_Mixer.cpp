#include "Base/Types.h"


#include <string.h>

#include "Utility/MathUtil.h"
#include "Core/Memory.h"
#include "Debug/DBGConsole.h"
#include "HLEAudio/HLEAudioInternal.h"
#include "HLEAudio/HLEAudioState.h"

extern bool & isMKABI;
extern bool & isZeldaABI;

// One aligned block (see AUDIO_ME_ALIGN)
static struct AUDIO_ME_ALIGN SEnvState { u32 t3 = 0, s5 = 0, s6 = 0; u16 env[8] = {}; } gEnvState;
static u32 & gEnv_t3 = gEnvState.t3;
static u32 & gEnv_s5 = gEnvState.s5;
static u32 & gEnv_s6 = gEnvState.s6;
static u16 (&env)[8] = gEnvState.env;

inline u16 Sample_Mask(u32 x) { return (u16)(x & 0xffff); }

void ENVSETUP1(AudioHLECommand command) {
  // fprintf (dfile, "ENVSETUP1: cmd0 = %08X, cmd1 = %08X\n", command.cmd0,command.cmd1);
  gEnv_t3 = command.cmd0 & 0xFFFF;
  u32 tmp = (command.cmd0 >> 0x8) & 0xFF00;
  env[4] = Sample_Mask(tmp);
  env[5] = Sample_Mask(tmp + gEnv_t3);
  gEnv_s5 = command.cmd1 >> 0x10;
  gEnv_s6 = command.cmd1 & 0xFFFF;
  // fprintf (dfile, "	gEnv_t3 = %X / gEnv_s5 = %X / gEnv_s6 = %X / env[4] = %X
  // / env[5] = %X\n", gEnv_t3, gEnv_s5, gEnv_s6, env[4], env[5]);
}

void ENVSETUP2(AudioHLECommand command) {
  // fprintf (dfile, "ENVSETUP2: cmd0 = %08X, cmd1 = %08X\n", command.cmd0,
  // command.cmd1);
  u32 tmp1 = (command.cmd1 >> 0x10);
  env[0] = Sample_Mask(tmp1);
  env[1] = Sample_Mask(tmp1 + gEnv_s5);

  u32 tmp2 = command.cmd1 & 0xffff;
  env[2] = Sample_Mask(tmp2);
  env[3] = Sample_Mask(tmp2 + gEnv_s6);
  // fprintf (dfile, "	env[0] = %X / env[1] = %X / env[2] = %X / env[3] =
  // %X\n", env[0], env[1], env[2], env[3]);
}

void ENVSETUP3(AudioHLECommand command [[maybe_unused]]) { 
  // Not implemented
}

void ENVMIXER(AudioHLECommand command) {
  // static int envmixcnt = 0;
  u8 flags(command.Abi1EnvMixer.Flags);
  u32 address(
      command.Abi1EnvMixer
          .Address); //+ gAudioHLEState.Segments[(command.cmd1>>24)&0xf]);

  gAudioHLEState.EnvMixer(flags, address);
}

void ENVMIXER_GE(AudioHLECommand command [[maybe_unused]]) {
  // Not implemented
}

// One 8 sample step of ENVMIXER2 (see below). The arithmetic is exactly that
// of the original loop, element by element in the same order (the buffers
// can overlap, so the order of the saturating adds matters).
#define DAEDALUS_ENVMIXER2_STEP(X, VOL_DRY, VOL_WET, VOL_FX)                     \
  {                                                                            \
    const u32 in = (u32)(s32)buffs3[(X) ^ 1];                                  \
    s16 vec9 = (s16)((in * (VOL_DRY)) >> 0x10) ^ m0;                           \
    s16 vec10 = (s16)((in * (VOL_WET)) >> 0x10) ^ m1;                          \
    bufft6[(X) ^ 1] = Saturate<s16>(bufft6[(X) ^ 1] + vec9);                   \
    bufft7[(X) ^ 1] = Saturate<s16>(bufft7[(X) ^ 1] + vec10);                  \
    vec9 = (s16)(((u32)(s32)vec9 * (VOL_FX)) >> 0x10) ^ m2;                    \
    vec10 = (s16)(((u32)(s32)vec10 * (VOL_FX)) >> 0x10) ^ m3;                  \
    if (kSwap) {                                                               \
      buffs0[(X) ^ 1] = Saturate<s16>(buffs0[(X) ^ 1] + vec10);                \
      buffs1[(X) ^ 1] = Saturate<s16>(buffs1[(X) ^ 1] + vec9);                 \
    } else {                                                                   \
      buffs0[(X) ^ 1] = Saturate<s16>(buffs0[(X) ^ 1] + vec9);                 \
      buffs1[(X) ^ 1] = Saturate<s16>(buffs1[(X) ^ 1] + vec10);                \
    }                                                                          \
  }

// The volumes (env[]) and steps are kept in locals: the original read them
// from memory for every sample, because the compiler cannot prove that the
// s16 sample stores do not change the u16 env[] array. Same results, about
// half the work per sample on the PSP. kSwap: command bit 0x10 (swaps the
// two effect outputs). kMK: Mario Kart ABI (8 samples per step, no ramp of
// the effect volume).
template <bool kSwap, bool kMK>
static void EnvMixer2Loop(s16 *buffs3, s16 *bufft6, s16 *bufft7, s16 *buffs0,
                          s16 *buffs1, s32 count, s16 m0, s16 m1, s16 m2,
                          s16 m3, u32 adder) {
  u32 e0 = env[0], e1 = env[1], e2 = env[2], e3 = env[3], e4 = env[4],
      e5 = env[5];
  const u32 s5 = gEnv_s5, s6 = gEnv_s6, t3 = gEnv_t3;

  // Fully unrolled: the sample offsets become constants and the compiler
  // can overlap the multiplies (each mult/mflo pair otherwise stalls)
  while (count > 0) {
#pragma GCC unroll 8
    for (int x = 0; x < 0x8; x++)
      DAEDALUS_ENVMIXER2_STEP(x, e0, e2, e4)

    if (!kMK)
#pragma GCC unroll 8
      for (int x = 0x8; x < 0x10; x++)
        DAEDALUS_ENVMIXER2_STEP(x, e1, e3, e5)

    bufft6 += adder;
    bufft7 += adder;
    buffs0 += adder;
    buffs1 += adder;
    buffs3 += adder;
    count -= adder;
    e0 = Sample_Mask(e0 + s5);
    e1 = Sample_Mask(e1 + s5);
    e2 = Sample_Mask(e2 + s6);
    e3 = Sample_Mask(e3 + s6);
    e4 = Sample_Mask(e4 + t3);
    e5 = Sample_Mask(e5 + t3);
  }

  env[0] = e0; env[1] = e1; env[2] = e2;
  env[3] = e3; env[4] = e4; env[5] = e5;
}
#undef DAEDALUS_ENVMIXER2_STEP

void ENVMIXER2(AudioHLECommand command) {
  s16 *buffs3 =
      (s16 *)(gAudioHLEState.Buffer + ((command.cmd0 >> 0x0c) & 0x0ff0));
  s16 *bufft6 =
      (s16 *)(gAudioHLEState.Buffer + ((command.cmd1 >> 0x14) & 0x0ff0));
  s16 *bufft7 =
      (s16 *)(gAudioHLEState.Buffer + ((command.cmd1 >> 0x0c) & 0x0ff0));
  s16 *buffs0 =
      (s16 *)(gAudioHLEState.Buffer + ((command.cmd1 >> 0x04) & 0x0ff0));
  s16 *buffs1 =
      (s16 *)(gAudioHLEState.Buffer + ((command.cmd1 << 0x04) & 0x0ff0));

  // Sign masks: xor with 0 keeps a value, xor with -1 inverts it
  const s16 m0 = 0 - (s16)((command.cmd0 & 0x2) >> 1);
  const s16 m1 = 0 - (s16)((command.cmd0 & 0x1));
  const s16 m2 = 0 - (s16)((command.cmd0 & 0x8) >> 1);
  const s16 m3 = 0 - (s16)((command.cmd0 & 0x4) >> 1);

  s32 count = (command.cmd0 >> 8) & 0xff;

  u32 adder;
  if (!isMKABI) {
    gEnv_s5 *= 2;
    gEnv_s6 *= 2;
    gEnv_t3 *= 2;
    adder = 0x10;
  } else {
    command.cmd0 = 0;
    adder = 0x8;
    gEnv_t3 = 0;
  }

  const bool swap = (command.cmd0 & 0x10) != 0;
  if (isMKABI)
    EnvMixer2Loop<false, true>(buffs3, bufft6, bufft7, buffs0, buffs1, count, m0, m1, m2, m3, adder);
  else if (swap)
    EnvMixer2Loop<true, false>(buffs3, bufft6, bufft7, buffs0, buffs1, count, m0, m1, m2, m3, adder);
  else
    EnvMixer2Loop<false, false>(buffs3, bufft6, bufft7, buffs0, buffs1, count, m0, m1, m2, m3, adder);
}

void ENVMIXER3(AudioHLECommand command) {
  u8 flags = (u8)((command.cmd0 >> 16) & 0xff);
  u32 addy = (command.cmd1 & 0xFFFFFF);

  s16 *inp = (s16 *)(gAudioHLEState.Buffer + 0x4F0);
  s16 *out = (s16 *)(gAudioHLEState.Buffer + 0x9D0);
  s16 *aux1 = (s16 *)(gAudioHLEState.Buffer + 0xB40);
  s16 *aux2 = (s16 *)(gAudioHLEState.Buffer + 0xCB0);
  s16 *aux3 = (s16 *)(gAudioHLEState.Buffer + 0xE20);
  s32 MainL = 0, MainR = 0;
  s32 AuxL = 0, AuxR = 0;
  s32 i1 = 0, o1 = 0, a1 = 0, a2 = 0, a3 = 0;

  s32 LAdder = 0, LAcc = 0, LVol = 0;
  s32 RAdder = 0 , RAcc = 0, RVol = 0;
  s16 LSig = 0, RSig = 0; // Most significant part of the Ramp Value
  s16 Wet = 0, Dry = 0 ;
  s16 LTrg = 0, RTrg = 0;

  gAudioHLEState.VolRight = (s16)command.cmd0;

  s16 *buff = (s16 *)(rdram + addy);

  if (flags & A_INIT) {
    LAdder = gAudioHLEState.VolRampLeft / 8;
    LAcc = 0;
    LVol = gAudioHLEState.VolLeft;
    LSig = (s16)(gAudioHLEState.VolRampLeft >> 16);

    RAdder = gAudioHLEState.VolRampRight / 8;
    RAcc = 0;
    RVol = gAudioHLEState.VolRight;
    RSig = (s16)(gAudioHLEState.VolRampRight >> 16);

    Wet = gAudioHLEState.EnvWet;
    Dry = gAudioHLEState.EnvDry; // Save Wet/Dry values
    LTrg = gAudioHLEState.VolTrgLeft;
    RTrg = gAudioHLEState.VolTrgRight; // Save Current Left/Right Targets
  } else {
    Wet = *(s16 *)(buff + 0);     // 0-1
    Dry = *(s16 *)(buff + 2);     // 2-3
    LTrg = *(s16 *)(buff + 4);    // 4-5
    RTrg = *(s16 *)(buff + 6);    // 6-7
    LAdder = *(s32 *)(buff + 8);  // 8-9 (buff is a 16bit pointer)
    RAdder = *(s32 *)(buff + 10); // 10-11
    LAcc = *(s32 *)(buff + 12);   // 12-13
    RAcc = *(s32 *)(buff + 14);   // 14-15
    LVol = *(s32 *)(buff + 16);   // 16-17
    RVol = *(s32 *)(buff + 18);   // 18-19
    LSig = *(s16 *)(buff + 20);   // 20-21
    RSig = *(s16 *)(buff + 22);   // 22-23
                                  // u32 test  = *(s32 *)(buff + 24); // 22-23
    // if (test != 0x13371337)
    //	__asm int 3;
  }

  // if(!(flags&A_AUX)) {
  //	AuxIncRate=0;
  //	aux2=aux3=zero;
  // }

  for (s32 y = 0; y < (0x170 / 2); y++) {

    // Left
    LAcc += LAdder;
    LVol += (LAcc >> 16);
    LAcc &= 0xFFFF;

    // Right
    RAcc += RAdder;
    RVol += (RAcc >> 16);
    RAcc &= 0xFFFF;
    // ****************************************************************
    // Clamp Left
    if (LSig >= 0) { // VLT
      if (LVol > LTrg) {
        LVol = LTrg;
      }
    } else { // VGE
      if (LVol < LTrg) {
        LVol = LTrg;
      }
    }

    // Clamp Right
    if (RSig >= 0) { // VLT
      if (RVol > RTrg) {
        RVol = RTrg;
      }
    } else { // VGE
      if (RVol < RTrg) {
        RVol = RTrg;
      }
    }
    // ****************************************************************
    MainL = ((Dry * LVol) + 0x4000) >> 15;
    MainR = ((Dry * RVol) + 0x4000) >> 15;

    o1 = out[y ^ 1];
    a1 = aux1[y ^ 1];
    i1 = inp[y ^ 1];

    o1 += ((i1 * MainL) + 0x4000) >> 15;
    a1 += ((i1 * MainR) + 0x4000) >> 15;

    // ****************************************************************

    o1 = Saturate<s16>(o1);
    a1 = Saturate<s16>(a1);

    // ****************************************************************

    out[y ^ 1] = o1;
    aux1[y ^ 1] = a1;

    // ****************************************************************
    // if (!(flags&A_AUX)) {
    a2 = aux2[y ^ 1];
    a3 = aux3[y ^ 1];

    AuxL = ((Wet * LVol) + 0x4000) >> 15;
    AuxR = ((Wet * RVol) + 0x4000) >> 15;

    a2 += ((i1 * AuxL) + 0x4000) >> 15;
    a3 += ((i1 * AuxR) + 0x4000) >> 15;

    a2 = Saturate<s16>(a2);
    a3 = Saturate<s16>(a3);

    aux2[y ^ 1] = a2;
    aux3[y ^ 1] = a3;
  }
  //}

  *(s16 *)(buff + 0) = Wet;     // 0-1
  *(s16 *)(buff + 2) = Dry;     // 2-3
  *(s16 *)(buff + 4) = LTrg;    // 4-5
  *(s16 *)(buff + 6) = RTrg;    // 6-7
  *(s32 *)(buff + 8) = LAdder;  // 8-9 (buff is a 16bit pointer)
  *(s32 *)(buff + 10) = RAdder; // 10-11
  *(s32 *)(buff + 12) = LAcc;   // 12-13
  *(s32 *)(buff + 14) = RAcc;   // 14-15
  *(s32 *)(buff + 16) = LVol;   // 16-17
  *(s32 *)(buff + 18) = RVol;   // 18-19
  *(s16 *)(buff + 20) = LSig;   // 20-21
  *(s16 *)(buff + 22) = RSig;   // 22-23
                                //*(u32 *)(buff + 24) = 0x13371337; // 22-23
}

void SETVOL(AudioHLECommand command) {
  // Might be better to unpack these depending on the flags...
  u8 flags = (u8)((command.cmd0 >> 16) & 0xff);
  s16 vol = (s16)(command.cmd0 & 0xffff);
  //	u16 voltgt =(u16)((command.cmd1 >> 16)&0xffff);
  u16 volrate = (u16)((command.cmd1 & 0xffff));

  if (flags & A_AUX) {
    gAudioHLEState.EnvDry = vol;          // m_MainVol
    gAudioHLEState.EnvWet = (s16)volrate; // m_AuxVol
  } else if (flags & A_VOL) {
    // Set the Source(start) Volumes
    if (flags & A_LEFT) {
      gAudioHLEState.VolLeft = vol;
    } else {
      // A_RIGHT
      gAudioHLEState.VolRight = vol;
    }
  } else {
    // Set the Ramping values Target, Ramp
    if (flags & A_LEFT) {
      gAudioHLEState.VolTrgLeft = (s16)(command.cmd0 & 0xffff); // m_LeftVol
      gAudioHLEState.VolRampLeft = command.cmd1;
    } else {
      // A_RIGHT
      gAudioHLEState.VolTrgRight = (s16)(command.cmd0 & 0xffff); // m_RightVol
      gAudioHLEState.VolRampRight = command.cmd1;
    }
  }
}

void SETVOL3(AudioHLECommand command) {
  u8 Flags = (u8)(command.cmd0 >> 0x10);
  if (Flags & 0x4) {                                     // 288
    if (Flags & 0x2) {                                   // 290
      gAudioHLEState.VolLeft = (s16)command.cmd0;        // 0x50
      gAudioHLEState.EnvDry = (s16)(command.cmd1 >> 16); // 0x4E
      gAudioHLEState.EnvWet = (s16)command.cmd1;         // 0x4C
    } else {
      gAudioHLEState.VolTrgRight = (s16)command.cmd0; // 0x46
      // gAudioHLEState.VolRampRight = (u16)(command.cmd1 >> 16) |
      // (s32)(s16)(command.cmd1 << 0x10);
      gAudioHLEState.VolRampRight = command.cmd1; // 0x48/0x4A
    }
  } else {
    gAudioHLEState.VolTrgLeft = (s16)command.cmd0; // 0x40
    gAudioHLEState.VolRampLeft = command.cmd1;     // 0x42/0x44
  }
}
