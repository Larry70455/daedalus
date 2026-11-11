/*
Copyright (C) 2026 DaedalusX64 Team

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

// Interpreter vs dynarec correctness check (compile time option
// DAEDALUS_STATE_CHECK, off by default).
//
// In plain words:
//   If a file called statecheck.txt exists next to the emulator, the emulator
//   skips the menu, boots the ROM named in that file (optionally loads a
//   savestate), forces either the interpreter or the dynarec, and at every N64
//   vertical blank writes the CPU registers plus a checksum of N64 RAM to
//   statecheck_<mode>.txt. After the requested number of frames it exits.
//
//   Running it once with mode=interpreter and once with mode=dynarec and
//   comparing the two output files (Tools/statecheck_compare.py) shows the
//   first frame where the dynarec's result differs from the interpreter's.
//
// statecheck.txt format (one key=value per line, # starts a comment):
//   rom=Roms/test.z64
//   mode=interpreter        (or dynarec)
//   frames=300
//   savestate=SaveStates/x.ss   (optional)
//   audio=0|1|2             (optional: 0 audio off, 1 async on the Media Engine,
//                            2 sync on the main CPU; default keeps the ROM setting,
//                            which is off on the PSP)
//   ramhash=0               (optional: skip the RAM checksum. It costs tens
//                            of milliseconds per frame on a PSP, so turn it
//                            off when the run is used for timing)
//
// When DAEDALUS_STATE_CHECK is not defined, the macros below do nothing.

#ifndef UTILITY_STATECHECK_H_
#define UTILITY_STATECHECK_H_

#ifdef DAEDALUS_STATE_CHECK

// Returns true if statecheck.txt was found and the ROM is open and ready for
// CPU_Run(). The caller then runs the CPU; StateCheck exits the program when
// the requested number of frames has been recorded.
bool	StateCheck_Start();
void	StateCheck_OnVerticalBlank();

#define DAEDALUS_STATE_CHECK_VBL()	StateCheck_OnVerticalBlank()

#else

#define DAEDALUS_STATE_CHECK_VBL()	do {} while( 0 )

#endif // DAEDALUS_STATE_CHECK

#endif // UTILITY_STATECHECK_H_
