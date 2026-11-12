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

// "CPU 400 MHz" setting. Loads Plugins/overclock.prx (a kernel module, see
// Source/SysPSP/PRX/Overclock/main.c) and asks it for about 400 MHz, or for
// the normal 333 MHz. Unofficial: a console that is not stable at that speed
// freezes and has to be restarted (hold the power switch).

#ifndef SYSPSP_UTILITY_OVERCLOCKPSP_H_
#define SYSPSP_UTILITY_OVERCLOCKPSP_H_

// Applies gGlobalPreferences.Overclock400. Returns the MHz now set, or a
// negative number if the module could not be loaded.
int		PSP_ApplyOverclockPreference();

// Back to 333 MHz (call before exiting)
void	PSP_RestoreNormalClock();

#endif // SYSPSP_UTILITY_OVERCLOCKPSP_H_
