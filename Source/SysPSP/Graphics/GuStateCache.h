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

// GE state cache for the PSP renderer.
//
// The renderer sets a dozen GE states for every draw (enables, texture
// function/filter/wrap, blend and alpha functions, texture mode and image...)
// whether they changed or not. Each call runs a pspgu function (instruction
// cache misses) and writes a command word to the uncached display list. This
// header remembers the last value sent for each state and skips identical
// repeats. Include it after <pspgu.h> in a renderer source file: the macros
// at the end send that file's calls through the cache.
//
// GuCacheInvalidate() forgets everything; it runs at the start and end of
// every N64 display list (BaseRenderer::BeginScene / EndScene), so state set
// by other code between frames (UI, screen clears) is never trusted.
// Texture installs (sceGuTexImage also flushes the GE texture cache) are only
// skipped while gGuTextureEpoch is unchanged: it is bumped whenever texture
// memory may be written (CNativeTexture::SetData, writable GetData, render
// to texture).

#ifndef SYSPSP_GRAPHICS_GUSTATECACHE_H_
#define SYSPSP_GRAPHICS_GUSTATECACHE_H_

#include <pspgu.h>
#include "Base/Types.h"

extern u32 gGuTextureEpoch;
extern bool gGuCacheActive;	// true between BaseRenderer::BeginScene and EndScene

struct SGuStateCache
{
	u32			EnableKnown;		// bit n: state n's on/off is known
	u32			EnableValue;		// bit n: state n is on
	bool		TexFuncKnown;		int TexFunc[2];
	bool		TexEnvColorKnown;	u32 TexEnvColor;
	bool		TexWrapKnown;		int TexWrap[2];
	bool		TexFilterKnown;		int TexFilter[2];
	bool		BlendFuncKnown;		int BlendFunc[3]; u32 BlendFix[2];
	bool		AlphaFuncKnown;		int AlphaFunc[3];
	bool		DepthMaskKnown;		int DepthMask;
	bool		DepthFuncKnown;		int DepthFunc;
	bool		ShadeModelKnown;	int ShadeModel;
	bool		TexModeKnown;		int TexMode[4];
	bool		TexImageKnown;		int TexImage[4]; const void * TexImagePtr; u32 TexImageEpoch;
	bool		ClutKnown;			int ClutMode[4]; int ClutLoadNum; const void * ClutPtr; u32 ClutEpoch;
	bool		TexOffsetKnown;		float TexOffset[2];
	bool		TexScaleKnown;		float TexScale[2];
};
extern SGuStateCache gGuState;

inline void GuCacheInvalidate()
{
	gGuState = SGuStateCache();
}

// Outside an N64 display list (UI, screen clears) other code sets GE state
// directly, so the cache is bypassed there
#define GUC_BYPASS( call )	if( !gGuCacheActive ) { call; return; }

inline void GucEnable( int state )
{
	GUC_BYPASS( sceGuEnable( state ) );
	const u32 bit = 1u << state;
	if( ( gGuState.EnableKnown & bit ) && ( gGuState.EnableValue & bit ) )
		return;
	gGuState.EnableKnown |= bit;
	gGuState.EnableValue |= bit;
	sceGuEnable( state );
}

inline void GucDisable( int state )
{
	GUC_BYPASS( sceGuDisable( state ) );
	const u32 bit = 1u << state;
	if( ( gGuState.EnableKnown & bit ) && !( gGuState.EnableValue & bit ) )
		return;
	gGuState.EnableKnown |= bit;
	gGuState.EnableValue &= ~bit;
	sceGuDisable( state );
}

inline void GucTexFunc( int tfx, int tcc )
{
	GUC_BYPASS( sceGuTexFunc( tfx, tcc ) );
	if( gGuState.TexFuncKnown && gGuState.TexFunc[0] == tfx && gGuState.TexFunc[1] == tcc )
		return;
	gGuState.TexFuncKnown = true; gGuState.TexFunc[0] = tfx; gGuState.TexFunc[1] = tcc;
	sceGuTexFunc( tfx, tcc );
}

inline void GucTexEnvColor( unsigned int color )
{
	GUC_BYPASS( sceGuTexEnvColor( color ) );
	if( gGuState.TexEnvColorKnown && gGuState.TexEnvColor == color )
		return;
	gGuState.TexEnvColorKnown = true; gGuState.TexEnvColor = color;
	sceGuTexEnvColor( color );
}

inline void GucTexWrap( int u, int v )
{
	GUC_BYPASS( sceGuTexWrap( u, v ) );
	if( gGuState.TexWrapKnown && gGuState.TexWrap[0] == u && gGuState.TexWrap[1] == v )
		return;
	gGuState.TexWrapKnown = true; gGuState.TexWrap[0] = u; gGuState.TexWrap[1] = v;
	sceGuTexWrap( u, v );
}

inline void GucTexFilter( int min, int mag )
{
	GUC_BYPASS( sceGuTexFilter( min, mag ) );
	if( gGuState.TexFilterKnown && gGuState.TexFilter[0] == min && gGuState.TexFilter[1] == mag )
		return;
	gGuState.TexFilterKnown = true; gGuState.TexFilter[0] = min; gGuState.TexFilter[1] = mag;
	sceGuTexFilter( min, mag );
}

inline void GucBlendFunc( int op, int src, int dest, unsigned int srcfix, unsigned int destfix )
{
	GUC_BYPASS( sceGuBlendFunc( op, src, dest, srcfix, destfix ) );
	if( gGuState.BlendFuncKnown && gGuState.BlendFunc[0] == op && gGuState.BlendFunc[1] == src && gGuState.BlendFunc[2] == dest &&
		gGuState.BlendFix[0] == srcfix && gGuState.BlendFix[1] == destfix )
		return;
	gGuState.BlendFuncKnown = true;
	gGuState.BlendFunc[0] = op; gGuState.BlendFunc[1] = src; gGuState.BlendFunc[2] = dest;
	gGuState.BlendFix[0] = srcfix; gGuState.BlendFix[1] = destfix;
	sceGuBlendFunc( op, src, dest, srcfix, destfix );
}

inline void GucAlphaFunc( int func, int value, int mask )
{
	GUC_BYPASS( sceGuAlphaFunc( func, value, mask ) );
	if( gGuState.AlphaFuncKnown && gGuState.AlphaFunc[0] == func && gGuState.AlphaFunc[1] == value && gGuState.AlphaFunc[2] == mask )
		return;
	gGuState.AlphaFuncKnown = true; gGuState.AlphaFunc[0] = func; gGuState.AlphaFunc[1] = value; gGuState.AlphaFunc[2] = mask;
	sceGuAlphaFunc( func, value, mask );
}

inline void GucDepthMask( int mask )
{
	GUC_BYPASS( sceGuDepthMask( mask ) );
	if( gGuState.DepthMaskKnown && gGuState.DepthMask == mask )
		return;
	gGuState.DepthMaskKnown = true; gGuState.DepthMask = mask;
	sceGuDepthMask( mask );
}

inline void GucDepthFunc( int function )
{
	GUC_BYPASS( sceGuDepthFunc( function ) );
	if( gGuState.DepthFuncKnown && gGuState.DepthFunc == function )
		return;
	gGuState.DepthFuncKnown = true; gGuState.DepthFunc = function;
	sceGuDepthFunc( function );
}

inline void GucShadeModel( int mode )
{
	GUC_BYPASS( sceGuShadeModel( mode ) );
	if( gGuState.ShadeModelKnown && gGuState.ShadeModel == mode )
		return;
	gGuState.ShadeModelKnown = true; gGuState.ShadeModel = mode;
	sceGuShadeModel( mode );
}

inline void GucTexMode( int tpsm, int maxmips, int a2, int swizzle )
{
	GUC_BYPASS( sceGuTexMode( tpsm, maxmips, a2, swizzle ) );
	if( gGuState.TexModeKnown && gGuState.TexMode[0] == tpsm && gGuState.TexMode[1] == maxmips &&
		gGuState.TexMode[2] == a2 && gGuState.TexMode[3] == swizzle )
		return;
	gGuState.TexModeKnown = true;
	gGuState.TexMode[0] = tpsm; gGuState.TexMode[1] = maxmips; gGuState.TexMode[2] = a2; gGuState.TexMode[3] = swizzle;
	sceGuTexMode( tpsm, maxmips, a2, swizzle );
}

inline void GucTexImage( int mipmap, int width, int height, int tbw, const void * tbp )
{
	GUC_BYPASS( sceGuTexImage( mipmap, width, height, tbw, tbp ) );
	if( gGuState.TexImageKnown && gGuState.TexImageEpoch == gGuTextureEpoch && gGuState.TexImagePtr == tbp &&
		gGuState.TexImage[0] == mipmap && gGuState.TexImage[1] == width && gGuState.TexImage[2] == height && gGuState.TexImage[3] == tbw )
		return;
	gGuState.TexImageKnown = true; gGuState.TexImageEpoch = gGuTextureEpoch; gGuState.TexImagePtr = tbp;
	gGuState.TexImage[0] = mipmap; gGuState.TexImage[1] = width; gGuState.TexImage[2] = height; gGuState.TexImage[3] = tbw;
	sceGuTexImage( mipmap, width, height, tbw, tbp );
}

inline void GucClutMode( unsigned int cpsm, unsigned int shift, unsigned int mask, unsigned int a3 )
{
	GUC_BYPASS( sceGuClutMode( cpsm, shift, mask, a3 ) );
	if( gGuState.ClutKnown && gGuState.ClutMode[0] == (int)cpsm && gGuState.ClutMode[1] == (int)shift &&
		gGuState.ClutMode[2] == (int)mask && gGuState.ClutMode[3] == (int)a3 )
		return;
	gGuState.ClutKnown = false;		// the next ClutLoad must be sent
	gGuState.ClutMode[0] = cpsm; gGuState.ClutMode[1] = shift; gGuState.ClutMode[2] = mask; gGuState.ClutMode[3] = a3;
	sceGuClutMode( cpsm, shift, mask, a3 );
}

inline void GucClutLoad( int num_blocks, const void * cbp )
{
	GUC_BYPASS( sceGuClutLoad( num_blocks, cbp ) );
	if( gGuState.ClutKnown && gGuState.ClutEpoch == gGuTextureEpoch && gGuState.ClutPtr == cbp && gGuState.ClutLoadNum == num_blocks )
		return;
	gGuState.ClutKnown = true; gGuState.ClutEpoch = gGuTextureEpoch; gGuState.ClutPtr = cbp; gGuState.ClutLoadNum = num_blocks;
	sceGuClutLoad( num_blocks, cbp );
}

inline void GucTexOffset( float u, float v )
{
	GUC_BYPASS( sceGuTexOffset( u, v ) );
	if( gGuState.TexOffsetKnown && gGuState.TexOffset[0] == u && gGuState.TexOffset[1] == v )
		return;
	gGuState.TexOffsetKnown = true; gGuState.TexOffset[0] = u; gGuState.TexOffset[1] = v;
	sceGuTexOffset( u, v );
}

inline void GucTexScale( float u, float v )
{
	GUC_BYPASS( sceGuTexScale( u, v ) );
	if( gGuState.TexScaleKnown && gGuState.TexScale[0] == u && gGuState.TexScale[1] == v )
		return;
	gGuState.TexScaleKnown = true; gGuState.TexScale[0] = u; gGuState.TexScale[1] = v;
	sceGuTexScale( u, v );
}

#define sceGuEnable			GucEnable
#define sceGuDisable		GucDisable
#define sceGuTexFunc		GucTexFunc
#define sceGuTexEnvColor	GucTexEnvColor
#define sceGuTexWrap		GucTexWrap
#define sceGuTexFilter		GucTexFilter
#define sceGuBlendFunc		GucBlendFunc
#define sceGuAlphaFunc		GucAlphaFunc
#define sceGuDepthMask		GucDepthMask
#define sceGuDepthFunc		GucDepthFunc
#define sceGuShadeModel		GucShadeModel
#define sceGuTexMode		GucTexMode
#define sceGuTexImage		GucTexImage
#define sceGuClutMode		GucClutMode
#define sceGuClutLoad		GucClutLoad
#define sceGuTexOffset		GucTexOffset
#define sceGuTexScale		GucTexScale

#endif // SYSPSP_GRAPHICS_GUSTATECACHE_H_
