/*
Copyright (C) 2006 StrmnNrmn

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

#include <psptypes.h>
#include <pspkernel.h>
#include <pspdebug.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <glm/gtc/type_ptr.hpp>  //
#include "UI/ColourPulser.h"

#include "Graphics/GraphicsContext.h"
#include "Graphics/NativeTexture.h"
#include "UI/DrawText.h"
#include "UI/UIContext.h"
#include "Utility/Translate.h"
#include "UI/DrawTextUtilities.h"
#include "Interface/Preferences.h"
#include <cmath>
#include <algorithm>


#define GL_TRUE                           1
#define GL_FALSE                          0

namespace
{
const u32				BACKGROUND_WIDTH = 480;
const u32				BACKGROUND_HEIGHT = 272;

struct BackgroundVtx
{
    glm::vec2	t0;
    glm::vec3	pos;

	static const u32 Flags = GU_TEXTURE_32BITF|GU_VERTEX_32BITF;
};

DAEDALUS_STATIC_ASSERT( sizeof(BackgroundVtx) == 20 );

struct BackgroundColourVtx
{
	c32		colour;
    glm::vec3		pos;

	static const u32 Flags = GU_COLOR_8888|GU_VERTEX_32BITF;
};

DAEDALUS_STATIC_ASSERT( sizeof(BackgroundColourVtx) == 16 );

struct BackgroundTextureVtx
{
    glm::vec2	t0;
	c32		colour;
    glm::vec3		pos;

	static const u32 Flags = GU_TEXTURE_32BITF|GU_COLOR_8888|GU_VERTEX_32BITF;
};

DAEDALUS_STATIC_ASSERT( sizeof(BackgroundTextureVtx) == 24 );


const u32		MS_PER_COLOUR_CYCLE = 1200;

const c32		COLOUR_SELECTION_DIM	= c32( 0xe0, 0xe0, 0x80 );
const c32		COLOUR_SELECTION_BRIGHT	= c32( 0xff, 0xff, 0x80 );
}


class IUIContext : public CUIContext
{
	public:

		IUIContext();
		~IUIContext();

		virtual void				BeginRender();
		virtual void				EndRender();

		virtual c32					GetBackgroundColour() const			{ return mBackgroundColour; }
		virtual void				SetBackgroundColour( c32 colour )	{ mBackgroundColour = colour; }

		virtual u32					GetScreenWidth() const				{ return BACKGROUND_WIDTH; }
		virtual u32					GetScreenHeight() const				{ return BACKGROUND_HEIGHT; }

		virtual c32					GetDefaultTextColour() const		{ return DrawTextUtilities::TextWhite; }
		virtual c32					GetSelectedTextColour() const		{ return mColourPulser.GetCurrentColour(); }

		virtual void				Update( float elapsed_time );

		virtual void				RenderTexture( const std::shared_ptr<CNativeTexture> texture, const glm::vec2 & tl, const glm::vec2 & wh, c32 colour );
		virtual void				RenderTexture( const std::shared_ptr<CNativeTexture> texture, s32 x, s32 y, c32 colour );
		virtual void				ClearBackground( c32 colour );
		virtual void				DrawRect( s32 x, s32 y, u32 w, u32 h, c32 colour );
		virtual void				DrawLine( s32 x0, s32 y0, s32 x1, s32 y1, c32 colour );

		virtual void				SetFontStyle( EFontStyle font_style );

		virtual u32					DrawText( s32 x, s32 y, const std::string text, u32 length, c32 colour );
		virtual u32					DrawText( s32 x, s32 y, const std::string, u32 length, c32 colour, c32 drop_colour );
		virtual u32					DrawTextScale( s32 x, s32 y, float scale, const std::string text, u32 length, c32 colour );
		virtual u32					DrawTextScale( s32 x, s32 y, float scale, const std::string text, u32 length, c32 colour, c32 drop_colour );
		virtual u32					DrawTextAlign( s32 min_x, s32 max_x, EAlignType align_type, s32 y, const std::string text, u32 length, c32 colour );
		virtual u32					DrawTextAlign( s32 min_x, s32 max_x, EAlignType align_type, s32 y, const std::string text, u32 length, c32 colour, c32 drop_colour );
		virtual s32					DrawTextArea( s32 left, s32 top, u32 width, u32 height, const std::string& text, c32 colour, EVerticalAlign vertical_align );

		virtual u32					GetFontHeight() const;
		virtual u32					GetTextWidth( const char * text ) const;

	private:
				s32					AlignText( s32 min_x, s32 max_x, const std::string p_str, u32 length, EAlignType align_type );

	private:
		CDrawText::EFont			mCurrentFont;
		CColourPulser				mColourPulser;

		// PS Vita style theme (GuiColor VITA)
		float						mThemeTime = 0.0f;			// seconds, moves the ribbons
		void						DrawVitaBackdrop();
		void						DrawGradient( s32 x, s32 y, s32 w, s32 h, c32 top, c32 bottom );
		void						DrawPill( s32 x, s32 y, s32 w, s32 h, c32 colour );
		virtual void				DrawRoundRect( s32 x, s32 y, u32 w, u32 h, u32 radius, c32 colour );
		c32							VitaSelected( s32 x, s32 y, u32 width, c32 colour );
		c32							mBackgroundColour;
};

CUIContext::~CUIContext() {}
CUIContext *	CUIContext::Create() { return new IUIContext; }



IUIContext::IUIContext()
:	mCurrentFont( CDrawText::F_REGULAR )
,	mColourPulser( COLOUR_SELECTION_DIM, COLOUR_SELECTION_BRIGHT, MS_PER_COLOUR_CYCLE )
,	mBackgroundColour( 0,0,0 )
{}


IUIContext::~IUIContext()
{
	// Clear everything to black
	for( u32 i = 0; i < 2; ++i )
	{
		CGraphicsContext::Get()->BeginFrame();
		CGraphicsContext::Get()->ClearToBlack();
		CGraphicsContext::Get()->EndFrame();
		CGraphicsContext::Get()->UpdateFrame( false );
	}
}


void	IUIContext::Update( float elapsed_time )
{
	s32		elapsed_ms = s32( 1000.0f * elapsed_time );
	mColourPulser.Update( elapsed_ms );
	mThemeTime += elapsed_time;
	if( mThemeTime > 1000.0f )
		mThemeTime -= 1000.0f;
}

//*****************************************************************************
// PS Vita style theme (Global Settings: GUI Color "Vita")
//*****************************************************************************
namespace
{
	inline bool VitaTheme()		{ return gGlobalPreferences.GuiColor == VITA; }

	// Untextured, blended, smooth shaded 2D drawing
	void BeginFlat2D()
	{
		sceGuDisable( GU_DEPTH_TEST );
		sceGuDisable( GU_CULL_FACE );	// the strips below wind both ways
		sceGuDepthMask( GL_TRUE );
		sceGuShadeModel( GU_SMOOTH );
		sceGuDisable( GU_TEXTURE_2D );
		sceGuDisable( GU_ALPHA_TEST );
		sceGuEnable( GU_BLEND );
		sceGuBlendFunc( GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0 );
		alignas(DATA_ALIGN) glm::mat4 identity = glm::mat4( 1.0f );
		sceGuSetMatrix( GU_PROJECTION, reinterpret_cast< const ScePspFMatrix4 * >( glm::value_ptr( identity ) ) );
	}

	inline void SetVtx( BackgroundColourVtx & v, float x, float y, c32 colour )
	{
		v.pos = glm::vec3( x, y, 0.0f );
		v.colour = colour;
	}
}

void IUIContext::DrawGradient( s32 x, s32 y, s32 w, s32 h, c32 top, c32 bottom )
{
	BackgroundColourVtx * v = (BackgroundColourVtx *)sceGuGetMemory( 4 * sizeof( BackgroundColourVtx ) );
	SetVtx( v[0], float( x ), float( y ), top );
	SetVtx( v[1], float( x + w ), float( y ), top );
	SetVtx( v[2], float( x ), float( y + h ), bottom );
	SetVtx( v[3], float( x + w ), float( y + h ), bottom );
	BeginFlat2D();
	sceGuDrawArray( GU_TRIANGLE_STRIP, BackgroundColourVtx::Flags|GU_TRANSFORM_2D, 4, nullptr, v );
}

// Rounded rectangle with half round ends (radius h/2), like the Vita's buttons
void IUIContext::DrawPill( s32 x, s32 y, s32 w, s32 h, c32 colour )
{
	const u32 kSegments = 8;			// per half circle
	const float r = h * 0.5f;
	const float cy = y + r;
	const float left = x + r;
	const float right = x + w - r;
	// One triangle fan around the centre line: right half circle, then left
	const u32 n = 2 + 2 * ( kSegments + 1 ) + 1;
	BackgroundColourVtx * v = (BackgroundColourVtx *)sceGuGetMemory( n * sizeof( BackgroundColourVtx ) );
	u32 i = 0;
	SetVtx( v[ i++ ], ( left + right ) * 0.5f, cy, colour );
	for( u32 k = 0; k <= kSegments; ++k )
	{
		const float a = -1.5707963f + 3.1415926f * k / kSegments;
		SetVtx( v[ i++ ], right + r * cosf( a ), cy + r * sinf( a ), colour );
	}
	for( u32 k = 0; k <= kSegments; ++k )
	{
		const float a = 1.5707963f + 3.1415926f * k / kSegments;
		SetVtx( v[ i++ ], left + r * cosf( a ), cy + r * sinf( a ), colour );
	}
	SetVtx( v[ i++ ], right, cy - r, colour );		// close the fan
	BeginFlat2D();
	sceGuDrawArray( GU_TRIANGLE_FAN, BackgroundColourVtx::Flags|GU_TRANSFORM_2D, i, nullptr, v );
}

// Rectangle with quarter circle corners: one fan from the centre around the outline
void IUIContext::DrawRoundRect( s32 x, s32 y, u32 w, u32 h, u32 radius, c32 colour )
{
	const u32 kSegments = 5;			// per corner
	const float r = float( radius < std::min( w, h ) / 2 ? radius : std::min( w, h ) / 2 );
	const float cx[ 4 ] = { x + w - r, x + w - r, x + r, x + r };		// corner centres, clockwise from top right
	const float cy[ 4 ] = { y + r, y + h - r, y + h - r, y + r };
	const u32 n = 1 + 4 * ( kSegments + 1 ) + 1;
	BackgroundColourVtx * v = (BackgroundColourVtx *)sceGuGetMemory( n * sizeof( BackgroundColourVtx ) );
	u32 i = 0;
	SetVtx( v[ i++ ], x + w * 0.5f, y + h * 0.5f, colour );
	for( u32 c = 0; c < 4; ++c )
	{
		for( u32 k = 0; k <= kSegments; ++k )
		{
			const float a = -1.5707963f + 1.5707963f * ( c + float( k ) / kSegments );
			SetVtx( v[ i++ ], cx[ c ] + r * cosf( a ), cy[ c ] + r * sinf( a ), colour );
		}
	}
	v[ i ] = v[ 1 ];	// close the outline
	++i;
	BeginFlat2D();
	sceGuDrawArray( GU_TRIANGLE_FAN, BackgroundColourVtx::Flags|GU_TRANSFORM_2D, i, nullptr, v );
}

void IUIContext::DrawVitaBackdrop()
{
	const s32 w = s32( GetScreenWidth() );
	const s32 h = s32( GetScreenHeight() );

	// Deep blue to near black, like the Vita's dark themes
	DrawGradient( 0, 0, w, h, c32( 0x12, 0x3e, 0x72, 0xff ), c32( 0x02, 0x08, 0x14, 0xff ) );

	// Soft light ribbons drifting across the screen
	struct SRibbon { float Base, Amp, Period, Speed, Phase, Thick; u8 Alpha; };
	static const SRibbon kRibbons[] =
	{
		{ 0.62f, 18.0f, 520.0f,  0.25f, 0.0f, 48.0f, 0x46 },
		{ 0.72f, 26.0f, 700.0f, -0.18f, 1.7f, 30.0f, 0x38 },
		{ 0.54f, 12.0f, 380.0f,  0.33f, 3.1f, 18.0f, 0x30 },
	};
	const u32 kColumns = 25;
	for( const SRibbon & rb : kRibbons )
	{
		BackgroundColourVtx * v = (BackgroundColourVtx *)sceGuGetMemory( kColumns * 3 * sizeof( BackgroundColourVtx ) * 2 );
		// Two strips: fade in above the centre line, fade out below it
		const c32 edge( 0xa0, 0xdc, 0xff, 0x00 );
		const c32 mid( 0xa0, 0xdc, 0xff, rb.Alpha );
		u32 n = 0;
		for( u32 half = 0; half < 2; ++half )
		{
			BackgroundColourVtx * strip = v + n;
			u32 m = 0;
			for( u32 c = 0; c < kColumns; ++c )
			{
				const float x = float( w ) * c / ( kColumns - 1 );
				const float yc = h * rb.Base + rb.Amp * sinf( 6.2831853f * x / rb.Period + rb.Phase + mThemeTime * rb.Speed );
				const float t = rb.Thick * 0.5f;
				if( half == 0 )
				{
					SetVtx( strip[ m++ ], x, yc - t, edge );
					SetVtx( strip[ m++ ], x, yc, mid );
				}
				else
				{
					SetVtx( strip[ m++ ], x, yc, mid );
					SetVtx( strip[ m++ ], x, yc + t, edge );
				}
			}
			BeginFlat2D();
			sceGuDrawArray( GU_TRIANGLE_STRIP, BackgroundColourVtx::Flags|GU_TRANSFORM_2D, m, nullptr, strip );
			n += m;
		}
	}

	// Status bar across the top, with a faint line under it
	DrawGradient( 0, 0, w, 27, c32( 0x00, 0x00, 0x00, 0x70 ), c32( 0x00, 0x00, 0x00, 0x48 ) );
	DrawGradient( 0, 27, w, 1, c32( 0xff, 0xff, 0xff, 0x30 ), c32( 0xff, 0xff, 0xff, 0x30 ) );
}

// Selected text (drawn in the pulsing selection colour) gets a glowing pill
// behind it and is drawn white on top, as on the Vita
c32 IUIContext::VitaSelected( s32 x, s32 y, u32 width, c32 colour )
{
	if( !VitaTheme() || colour.GetColour() != mColourPulser.GetCurrentColour().GetColour() )
		return colour;
	const s32 fh = s32( CDrawText::GetFontHeight( mCurrentFont ) );
	// Brightness follows the pulse, so the selection still breathes a little
	const u32 pulse = mColourPulser.GetCurrentColour().GetG();
	const u8 alpha = u8( 0x58 + ( ( pulse - 0xe0 ) & 0x1f ) * 2 );
	DrawPill( x - 8, y - fh + 1, s32( width ) + 16, fh + 5, c32( 0x28, 0x96, 0xff, alpha ) );
	return c32( 0xff, 0xff, 0xff, 0xff );
}


void	IUIContext::RenderTexture( const std::shared_ptr<CNativeTexture> texture, s32 x, s32 y, c32 colour )
{
	if(texture == NULL)
		return;

	glm::vec2		tl = glm::vec2( f32( x ), f32( y ) );
	glm::vec2		wh = glm::vec2( f32( texture->GetWidth() ), f32( texture->GetHeight() ) );

	RenderTexture( texture, tl, wh, colour );
}

void	IUIContext::RenderTexture( const std::shared_ptr<CNativeTexture> texture, const glm::vec2 & tl, const glm::vec2 & wh, c32 colour )
{
	if(texture == NULL)
		return;

	u32				num_verts( 2 );
	BackgroundTextureVtx*	p_verts = (BackgroundTextureVtx*)sceGuGetMemory(num_verts*sizeof(BackgroundTextureVtx));

	sceGuDisable(GU_DEPTH_TEST);
	sceGuDepthMask( GL_TRUE );	// GL_TRUE to disable z-writes
	sceGuShadeModel( GU_FLAT );

	sceGuTexFilter(GU_LINEAR,GU_LINEAR);
	sceGuTexScale(1.0f,1.0f);
	sceGuTexOffset(0.0f,0.0f);

	u32		width( texture->GetWidth() );
	u32		height( texture->GetHeight() );

	texture->InstallTexture();

	sceGuDisable(GU_ALPHA_TEST);
	sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
	sceGuEnable(GU_BLEND);
	sceGuTexFunc(GU_TFX_MODULATE,GU_TCC_RGBA);

	alignas(DATA_ALIGN) glm::mat4 identity = glm::mat4(1.0f);  // Create an identity matrix
	sceGuSetMatrix( GU_PROJECTION, reinterpret_cast< const ScePspFMatrix4 * >( glm::value_ptr(identity) ) );

	glm::vec2		tex_uv0( (float)0, (float)0 );
	glm::vec2		tex_uv1( (float)width, (float)height );

	p_verts[0].pos = glm::vec3( tl.x, tl.y, 0.0f );
	p_verts[0].t0 = glm::vec2( 0.0f, 0.0f );
	p_verts[0].colour = colour;

	p_verts[1].pos = glm::vec3( tl.x + wh.x, tl.y + wh.y, 0.0f );
	p_verts[1].t0 = glm::vec2( (float)width, (float)height );
	p_verts[1].colour = colour;

	sceGuDrawArray(GU_SPRITES,BackgroundTextureVtx::Flags|GU_TRANSFORM_2D,num_verts,NULL,p_verts);
}


void	IUIContext::ClearBackground( c32 colour )
{
	CGraphicsContext::Get()->ClearColBufferAndDepth( colour );
	if( VitaTheme() )
		DrawVitaBackdrop();
}


void	IUIContext::DrawRect( s32 x, s32 y, u32 w, u32 h, c32 colour )
{
	u32						num_verts( 2 );
	BackgroundColourVtx*	p_verts = (BackgroundColourVtx*)sceGuGetMemory(num_verts*sizeof(BackgroundColourVtx));

	sceGuDisable(GU_DEPTH_TEST);
	sceGuDepthMask( GL_TRUE );	// GL_TRUE to disable z-writes
	sceGuShadeModel( GU_FLAT );

	sceGuDisable(GU_TEXTURE_2D);

	sceGuDisable(GU_ALPHA_TEST);
	sceGuDisable(GU_BLEND);


	alignas(DATA_ALIGN) glm::mat4 identity = glm::mat4(1.0f);  // Create an identity matrix
	sceGuSetMatrix( GU_PROJECTION, reinterpret_cast< const ScePspFMatrix4 * >( glm::value_ptr(identity) ) );

	p_verts[0].pos = glm::vec3( float( x ), float( y ), 0.0f );
	p_verts[0].colour = colour;

	p_verts[1].pos = glm::vec3( float( x + w ), float( y + h ), 0.0f );
	p_verts[1].colour = colour;

	sceGuDrawArray(GU_SPRITES,BackgroundColourVtx::Flags|GU_TRANSFORM_2D,num_verts,NULL,p_verts);

}

void	IUIContext::DrawLine( s32 x0, s32 y0, s32 x1, s32 y1, c32 colour )
{
	u32						num_verts( 2 );
	BackgroundColourVtx*	p_verts = (BackgroundColourVtx*)sceGuGetMemory(num_verts*sizeof(BackgroundColourVtx));

	sceGuDisable(GU_DEPTH_TEST);
	sceGuDepthMask( GL_TRUE );	// GL_TRUE to disable z-writes
	sceGuShadeModel( GU_FLAT );

	sceGuDisable(GU_TEXTURE_2D);

	sceGuDisable(GU_ALPHA_TEST);
	sceGuDisable(GU_BLEND);

	alignas(DATA_ALIGN) glm::mat4 identity = glm::mat4(1.0f);  // Create an identity matrix
	sceGuSetMatrix( GU_PROJECTION, reinterpret_cast< const ScePspFMatrix4 * >( glm::value_ptr(identity) ) );

	p_verts[0].pos = glm::vec3( float( x0 ), float( y0 ), 0.0f );
	p_verts[0].colour = colour;

	p_verts[1].pos = glm::vec3( float( x1 ), float( y1 ), 0.0f );
	p_verts[1].colour = colour;

	sceGuDrawArray(GU_LINES,BackgroundColourVtx::Flags|GU_TRANSFORM_2D,num_verts,NULL,p_verts);

}


void	IUIContext::SetFontStyle( EFontStyle font_style )
{
	switch( font_style )
	{
	case FS_REGULAR: mCurrentFont = CDrawText::F_REGULAR;		return;
	case FS_HEADING: mCurrentFont = CDrawText::F_LARGE_BOLD;	return;
	}
	#ifdef DAEDALUS_DEBUG_CONSOLE
	DAEDALUS_ERROR( "Unhandled font style" );
  #endif
}

u32		IUIContext::GetFontHeight() const
{
	return CDrawText::GetFontHeight( mCurrentFont );
}

u32		IUIContext::GetTextWidth( const char * text ) const
{
	return CDrawText::GetTextWidth( mCurrentFont, text );
}

s32		IUIContext::AlignText( s32 min_x, s32 max_x, const std::string p_str, u32 length, EAlignType align_type )
{
	s32		x;

	switch( align_type )
	{
	case AT_LEFT:
		x = min_x;
		break;

	case AT_CENTRE:
		x = min_x + ((max_x - min_x) - CDrawText::GetTextWidth( mCurrentFont, p_str, length )) / 2;
		break;

	case AT_RIGHT:
		x = max_x - CDrawText::GetTextWidth( mCurrentFont, p_str, length );
		break;

	default:
  	#ifdef DAEDALUS_DEBUG_CONSOLE
		DAEDALUS_ERROR( "Unhandled alignment type" );
    #endif
		x = min_x;
		break;
	}

	return x;
}

u32	IUIContext::DrawText( s32 x, s32 y, const std::string text, u32 length, c32 colour )
{
	colour = VitaSelected( x, y, CDrawText::GetTextWidth( mCurrentFont, text, length ), colour );
	return CDrawText::Render( mCurrentFont, x, y, 1.0f, text, length, colour );
}


u32	IUIContext::DrawText( s32 x, s32 y, const std::string text, u32 length, c32 colour, c32 drop_colour )
{
	colour = VitaSelected( x, y, CDrawText::GetTextWidth( mCurrentFont, text, length ), colour );
	return CDrawText::Render( mCurrentFont, x, y, 1.0f, text, length, colour, drop_colour );
}

u32	IUIContext::DrawTextScale( s32 x, s32 y, float scale, const std::string text, u32 length, c32 colour )
{
	return CDrawText::Render( mCurrentFont, x, y, scale, text, length, colour );
}

u32	IUIContext::DrawTextScale( s32 x, s32 y, float scale, const std::string text, u32 length, c32 colour, c32 drop_colour )
{
	return CDrawText::Render( mCurrentFont, x, y, scale, text, length, colour, drop_colour );
}

u32	IUIContext::DrawTextAlign( s32 min_x, s32 max_x, EAlignType align_type, s32 y, const std::string text, u32 length, c32 colour )
{
	colour = VitaSelected( AlignText( min_x, max_x, text, length, align_type ), y, CDrawText::GetTextWidth( mCurrentFont, text, length ), colour );

	return CDrawText::Render( mCurrentFont, AlignText( min_x, max_x, text, length, align_type ), y, 1.0f, text, length, colour );
}

u32	IUIContext::DrawTextAlign( s32 min_x, s32 max_x, EAlignType align_type, s32 y, const std::string text, u32 length, c32 colour, c32 drop_colour )
{
	colour = VitaSelected( AlignText( min_x, max_x, text, length, align_type ), y, CDrawText::GetTextWidth( mCurrentFont, text, length ), colour );

	return CDrawText::Render( mCurrentFont, AlignText( min_x, max_x, text, length, align_type ), y, 1.0f, text, length, colour, drop_colour );
}


namespace
{
	s32		VerticalAlign( EVerticalAlign vertical_align, s32 top, u32 height, u32 text_height )
	{
		switch( vertical_align )
		{
			case VA_TOP:	return top;
			case VA_BOTTOM: return top + height - text_height;
		}
	#ifdef DAEDALUS_DEBUG_CONSOLE
		DAEDALUS_ERROR( "Unhandled vertical align" );
    #endif
		return top;
	}
}



s32		IUIContext::DrawTextArea( s32 left, s32 top, u32 width [[maybe_unused]], u32 height, const std::string& text, c32 colour, EVerticalAlign vertical_align )
{
	const u32			font_height( CDrawText::GetFontHeight( mCurrentFont ) );
	u32					length = text.length();
	std::vector<u32>	lengths;
	bool				match = false;
	// DrawTextUtilities::WrapText( mCurrentFont, width, Translate_Strings( text, length ), length, lengths, match );

	s32 x =  left;
	s32 y = VerticalAlign( vertical_align, top, height, lengths.size() * font_height );

	// Our built-in auto-linebreaking can't handle unicodes.
	// Fall back to use intrafont's manual linebreaking feature
	if( match )
	{
		y += font_height;
		DrawTextScale( x, y, 0.8f, text, length, colour );
		y += 2;
		return y - top;
	}
	return y - top;
}


void	IUIContext::BeginRender()
{
	CGraphicsContext::Get()->BeginFrame();
}


void	IUIContext::EndRender()
{
	CGraphicsContext::Get()->EndFrame();
	CGraphicsContext::Get()->UpdateFrame( true );
}
