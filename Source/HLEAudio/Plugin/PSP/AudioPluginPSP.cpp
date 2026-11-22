/*
Copyright (C) 2003 Azimer
Copyright (C) 2001,2006 StrmnNrmn

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

//
//	N.B. This source code is derived from Azimer's Audio plugin (v0.55?)
//	and modified by StrmnNrmn to work with Daedalus PSP. Thanks Azimer!
//	Drop me a line if you get chance :)
//


#include "Base/Types.h"
#include <stdio.h>
#include <new>

#include <pspkernel.h>
#include <pspaudiolib.h>
#include <pspaudio.h>

#include "Interface/ConfigOptions.h"
#include "Core/CPU.h"
#include "Core/Interrupt.h"
#include "Core/Memory.h"
#include "Core/ROM.h"
#include "Core/RSP_HLE.h"
#include "Debug/DBGConsole.h"
#include "HLEAudio/HLEAudioInternal.h"
#include "HLEAudio/AudioBuffer.h"
#include "HLEAudio/AudioPlugin.h"
// #include "SysPSP/Utility/JobManager.h"
#include "SysPSP/Utility/CacheUtil.h"
// #include "SysPSP/Utility/JobManager.h"
#include "Utility/FramerateLimiter.h"
#include "System/Thread/Thread.h"

#include "Utility/PerfLog.h"
#ifdef DAEDALUS_PSP_USE_ME

#include "SysPSP/PRX/MediaEngine/me.h"
#include "SysPSP/Utility/ModulePSP.h"

bool gLoadedMediaEnginePRX {false};

volatile me_struct *mei;

bool InitialiseMediaEngine()
{
	if ( gLoadedMediaEnginePRX == false)
	{
		if( CModule::Load("Plugins/mediaengine.prx") < 0 )	return false;

		mei = (volatile struct me_struct *)malloc_64(sizeof(struct me_struct));
		mei = (volatile struct me_struct *)(make_uncached_ptr(mei));
		sceKernelDcacheWritebackInvalidateAll();

		if (InitME(mei) == 0)
		{
			gLoadedMediaEnginePRX = true;
			return true;
		}
		else
		{
			#ifdef DAEDALUS_DEBUG_CONSOLE
			printf(" Couldn't initialize MediaEngine Instance\n");
			#endif
			return false;
		}
	}
	else
	{
		printf("Media Engine already Initialised\n");
		return true;
	}
}

#endif

#define RSP_AUDIO_INTR_CYCLES     1
extern u32 gSoundSync;

static const u32	kOutputFrequency = 44100;
static const u32	MAX_OUTPUT_FREQUENCY = kOutputFrequency * 4;


static bool audio_open = false;


// Large kAudioBufferSize creates huge delay on sound //Corn
static const u32	kAudioBufferSize = 4096;	// About 92 ms. 2048 (46 ms) ran dry whenever one frame took longer than that, heard as crackle. Bigger means more audio delay.


class AudioPluginPSP : public CAudioPlugin
{
public:

 AudioPluginPSP();
	virtual ~AudioPluginPSP();
	virtual bool			StartEmulation();
	virtual void			StopEmulation();

	virtual void			DacrateChanged( int system_type );
	virtual void			LenChanged();
	virtual u32				ReadLength() {return 0;}
	virtual EProcessResult	ProcessAList();

	//virtual void SetFrequency(u32 frequency);
	virtual void AddBuffer( u8 * start, u32 length);
	virtual void FillBuffer( Sample * buffer, u32 num_samples);

	virtual void StopAudio();
	virtual void StartAudio();

public:
  CAudioBuffer *		mAudioBufferUncached;

private:
	CAudioBuffer * mAudioBuffer;
	bool mKeepRunning;
	bool mExitAudioThread;
	u32 mFrequency;
	s32 mAudioThread;
	s32 mSemaphore;
//	u32 mBufferLenMs;
};

static AudioPluginPSP * ac;

//
// Smooth mode: plays in sync like Synchronous, but checks once a second how
// much sound went missing because the game ran slower than real time. When
// more than a quarter was missing, it stops making sound for one second:
// the audio lists are skipped (about 3 ms of CPU time per frame in Pokemon
// Stadium 2), so the game catches up, then the sound fades back in.
// Skipping a list is what audio Disabled does all the time, so games are
// fine with it.
//
static const u32	kSmoothWindowUs = 1000000;	// how often to check
static const u32	kSmoothQuietUs = 1000000;	// how long to go quiet
static bool			gSmoothQuiet = false;
static u32			gSmoothQuietUntil = 0;
static u32			gSmoothWindowStart = 0;
static u32			gSmoothWindowPlayed = 0;
static u32			gSmoothWindowMissing = 0;

static void SmoothStartWindow( const CAudioBuffer * buffer, u32 now )
{
	gSmoothWindowStart = now;
	gSmoothWindowPlayed = buffer->GetPlayedSamples();
	gSmoothWindowMissing = buffer->GetMissingSamples();
}

// True while the sound should be skipped
static bool SmoothIsQuiet( const CAudioBuffer * buffer )
{
	const u32 now = sceKernelGetSystemTimeLow();
	if( gSmoothQuiet )
	{
		if( (s32)( now - gSmoothQuietUntil ) < 0 )
			return true;
		gSmoothQuiet = false;
		SmoothStartWindow( buffer, now );	// the quiet second itself does not count
		return false;
	}

	const u32 elapsed = now - gSmoothWindowStart;
	if( elapsed < kSmoothWindowUs )
		return false;

	const u32 played = buffer->GetPlayedSamples() - gSmoothWindowPlayed;
	const u32 missing = buffer->GetMissingSamples() - gSmoothWindowMissing;
	SmoothStartWindow( buffer, now );

	// A window much longer than a second means the emulator was paused (menu):
	// the sound ran dry then for a different reason, so ignore it
	if( elapsed > 2 * kSmoothWindowUs )
		return false;

	// missing / (played + missing) > 1/4
	if( missing * 3 > played )
	{
		DAEDALUS_PERF_STAT( kStatAudioSmoothQuiet );	// No-op unless DAEDALUS_PERF_LOG
		gSmoothQuiet = true;
		gSmoothQuietUntil = now + kSmoothQuietUs;
		return true;
	}
	return false;
}

void AudioPluginPSP::FillBuffer(Sample * buffer, u32 num_samples)
{
	sceKernelWaitSema( mSemaphore, 1, nullptr );

	mAudioBufferUncached->Drain( buffer, num_samples );

	sceKernelSignalSema( mSemaphore, 1 );
}


EAudioPluginMode gAudioPluginEnabled( APM_DISABLED );


AudioPluginPSP::AudioPluginPSP()
:mKeepRunning (false)
//: mAudioBuffer( kAudioBufferSize )
, mFrequency( 44100 )
,	mSemaphore( sceKernelCreateSema( "AudioPluginPSP", 0, 1, 1, nullptr ) )
//, mAudioThread ( kInvalidThreadHandle )
//, mKeepRunning( false )
//, mBufferLenMs ( 0 )
{
	// Allocate audio buffer with malloc_64 to avoid cached/uncached aliasing
	void * mem = malloc_64( sizeof( CAudioBuffer ) );
	mAudioBuffer = new( mem ) CAudioBuffer( kAudioBufferSize );
  mAudioBufferUncached = (CAudioBuffer*)make_uncached_ptr(mem);
	// Ideally we could just invalidate this range?
	dcache_wbinv_range_unaligned( mAudioBuffer, mAudioBuffer+sizeof( CAudioBuffer ) );

	#ifdef DAEDALUS_PSP_USE_ME
	InitialiseMediaEngine();
	#endif
}

AudioPluginPSP::~AudioPluginPSP( )
{
	mAudioBuffer->~CAudioBuffer();
  free(mAudioBuffer);
  sceKernelDeleteSema(mSemaphore);
  pspAudioEnd();
}

bool		AudioPluginPSP::StartEmulation()
{
	return true;
}


void	AudioPluginPSP::StopEmulation()
{
    Audio_Reset();
  	StopAudio();
    sceKernelDeleteSema(mSemaphore);
    pspAudioEndPre();
    sceKernelDelayThread(100000);
    pspAudioEnd();


}

void	AudioPluginPSP::DacrateChanged( int system_type )
{
u32 clock = (system_type == ST_NTSC) ? VI_NTSC_CLOCK : VI_PAL_CLOCK;
u32 dacrate = Memory_AI_GetRegister(AI_DACRATE_REG);
u32 frequency = clock / (dacrate + 1);

#ifdef DAEDALUS_DEBUG_CONSOLE
DBGConsole_Msg(0, "Audio frequency: %d", frequency);
#endif
mFrequency = frequency;
}


void	AudioPluginPSP::LenChanged()
{
	if( gAudioPluginEnabled > APM_DISABLED )
	{
		u32 address = Memory_AI_GetRegister(AI_DRAM_ADDR_REG) & 0xFFFFFF;
		u32	length = Memory_AI_GetRegister(AI_LEN_REG);

		AddBuffer( g_pu8RamBase + address, length );
	}
	else
	{
		StopAudio();
	}
}


EProcessResult	AudioPluginPSP::ProcessAList()
{
	DAEDALUS_PERF_SCOPE( kAudioHle );	// No-op unless DAEDALUS_PERF_LOG
	Memory_SP_SetRegisterBits(SP_STATUS_REG, SP_STATUS_HALT);

	EProcessResult	result = PR_NOT_STARTED;

	switch( gAudioPluginEnabled )
	{
		case APM_DISABLED:
			result = PR_COMPLETED;
			break;
		case APM_ENABLED_ASYNC:
			{
#ifdef DAEDALUS_PSP_USE_ME
				sceKernelDcacheWritebackInvalidateAll();
				// BeginME fails when the Media Engine is still busy with the
				// previous audio job; the job then runs on the main CPU.
				// An earlier change waited for the Media Engine here instead.
				// On a real PSP Go that stalled the main CPU so long that
				// Pokemon Stadium 2 stopped updating the screen with audio on,
				// so the original behaviour is back.
				if(BeginME( mei, (int)&Audio_Ucode, (int)NULL, -1, NULL, -1, NULL) < 0)
				{
					DAEDALUS_PERF_STAT( kStatAudioMeBusy );	// No-op unless DAEDALUS_PERF_LOG
					Audio_Ucode();
				}
				else
				{
					DAEDALUS_PERF_STAT( kStatAudioMeJobs );
				}
				result = PR_COMPLETED;
				break;
#else
				DAEDALUS_ERROR("Async audio is unimplemented");
				Audio_Ucode();
				result = PR_COMPLETED;
				break;
#endif
			}
			result = PR_COMPLETED;
			break;
		case APM_ENABLED_SYNC:
			Audio_Ucode();
			result = PR_COMPLETED;
			break;
		case APM_ENABLED_SMOOTH:
			if( !SmoothIsQuiet( mAudioBufferUncached ) )
				Audio_Ucode();
			result = PR_COMPLETED;
			break;
	}

	return result;
}


void audioCallback( void * buf, unsigned int length, void * userdata )
{
	AudioPluginPSP * ac( reinterpret_cast< AudioPluginPSP * >( userdata ) );

	ac->FillBuffer( reinterpret_cast< Sample * >( buf ), length );
}


void AudioPluginPSP::StartAudio()
{
	if (mKeepRunning)
		return;

	mKeepRunning = true;

	ac = this;


pspAudioInit();
pspAudioSetChannelCallback( 0, audioCallback, this );

	// Everything OK
	audio_open = true;
}

void AudioPluginPSP::AddBuffer( u8 *start, u32 length )
{
	if (length == 0)
		return;

	if (!mKeepRunning)
		StartAudio();

	u32 num_samples = length / sizeof( Sample );

	switch( gAudioPluginEnabled )
	{
	case APM_DISABLED:
		break;

	case APM_ENABLED_ASYNC:
		{

			mAudioBufferUncached->AddSamples( reinterpret_cast< const Sample * >( start ), num_samples, mFrequency, kOutputFrequency );
		
		}
		break;

	case APM_ENABLED_SYNC:
		{
			mAudioBufferUncached->SetSmooth( false );
			mAudioBufferUncached->AddSamples( reinterpret_cast< const Sample * >( start ), num_samples, mFrequency, kOutputFrequency );
		}
		break;

	case APM_ENABLED_SMOOTH:
		{
			mAudioBufferUncached->SetSmooth( true );
			if( !gSmoothQuiet )
				mAudioBufferUncached->AddSamples( reinterpret_cast< const Sample * >( start ), num_samples, mFrequency, kOutputFrequency );
		}
		break;
	}

	/*
	u32 remaining_samples = mAudioBuffer.GetNumBufferedSamples();
	mBufferLenMs = (1000 * remaining_samples) / kOutputFrequency);
	float ms = (float) num_samples * 1000.f / (float)mFrequency;
	#ifdef DAEDALUS_DEBUG_CONSOLE
	DPF_AUDIO("Queuing %d samples @%dHz - %.2fms - bufferlen now %d\n", num_samples, mFrequency, ms, mBufferLenMs);
	#endif
	*/
}

void AudioPluginPSP::StopAudio()
{
	if (!mKeepRunning)
		return;

	mKeepRunning = false;

	audio_open = false;
}

std::unique_ptr<CAudioPlugin> CreateAudioPlugin()
{
	return std::make_unique<AudioPluginPSP>();
}