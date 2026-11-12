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

#include "Base/Types.h"
#include "SysPSP/Utility/OverclockPSP.h"
#include "SysPSP/Utility/ModulePSP.h"
#include "Interface/Preferences.h"

#include <cstdio>

extern "C" int pspOverclockSet( int target_mhz );

static int	gOverclockModule = -1;
static int	gCurrentMHz = 333;

static bool LoadModule()
{
	if( gOverclockModule < 0 )
		gOverclockModule = CModule::Load( "Plugins/overclock.prx" );
	return gOverclockModule >= 0;
}

int PSP_ApplyOverclockPreference()
{
	const int wanted = gGlobalPreferences.Overclock400 ? 400 : 333;
	if( wanted == gCurrentMHz )
		return gCurrentMHz;
	if( !LoadModule() )
	{
		printf( "Overclock: could not load Plugins/overclock.prx\n" );
		return -1;
	}
	int result = pspOverclockSet( wanted );
	printf( "Overclock: asked for %d MHz, set %d MHz\n", wanted, result );
	if( result > 0 )
		gCurrentMHz = ( wanted == 333 ) ? 333 : wanted;
	return result;
}

void PSP_RestoreNormalClock()
{
	if( gCurrentMHz != 333 && gOverclockModule >= 0 )
	{
		pspOverclockSet( 333 );
		gCurrentMHz = 333;
	}
}
