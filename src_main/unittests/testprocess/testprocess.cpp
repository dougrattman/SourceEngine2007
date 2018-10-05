// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//

#include "base/include/windows/windows_light.h"
#include "tier0/include/icommandline.h"
#include <stdio.h>
#include "tier0/include/dbg.h"


static unsigned short g_InitialColor = 0xFFFF;
static unsigned short g_LastColor = 0xFFFF;
static unsigned short g_BadColor = 0xFFFF;
static WORD g_BackgroundFlags = 0xFFFF;

static void GetInitialColors( )
{
	// Get the old background attributes.
	CONSOLE_SCREEN_BUFFER_INFO oldInfo;
	GetConsoleScreenBufferInfo( GetStdHandle( STD_OUTPUT_HANDLE ), &oldInfo );
	g_InitialColor = g_LastColor = oldInfo.wAttributes & (FOREGROUND_RED|FOREGROUND_GREEN|FOREGROUND_BLUE|FOREGROUND_INTENSITY);
	g_BackgroundFlags = oldInfo.wAttributes & (BACKGROUND_RED|BACKGROUND_GREEN|BACKGROUND_BLUE|BACKGROUND_INTENSITY);

	g_BadColor = 0;
	if (g_BackgroundFlags & BACKGROUND_RED)
		g_BadColor |= FOREGROUND_RED;
	if (g_BackgroundFlags & BACKGROUND_GREEN)
		g_BadColor |= FOREGROUND_GREEN;
	if (g_BackgroundFlags & BACKGROUND_BLUE)
		g_BadColor |= FOREGROUND_BLUE;
	if (g_BackgroundFlags & BACKGROUND_INTENSITY)
		g_BadColor |= FOREGROUND_INTENSITY;
}

static WORD SetConsoleTextColor( int red, int green, int blue, int intensity )
{
	WORD ret = g_LastColor;
	
	g_LastColor = 0;
	if( red )	g_LastColor |= FOREGROUND_RED;
	if( green ) g_LastColor |= FOREGROUND_GREEN;
	if( blue )  g_LastColor |= FOREGROUND_BLUE;
	if( intensity ) g_LastColor |= FOREGROUND_INTENSITY;

	// Just use the initial color if there's a match...
	if (g_LastColor == g_BadColor)
		g_LastColor = g_InitialColor;

	SetConsoleTextAttribute( GetStdHandle( STD_OUTPUT_HANDLE ), g_LastColor | g_BackgroundFlags );
	return ret;
}


static void RestoreConsoleTextColor( WORD color )
{
	SetConsoleTextAttribute( GetStdHandle( STD_OUTPUT_HANDLE ), color | g_BackgroundFlags );
	g_LastColor = color;
}

void CmdLib_Exit( int exitCode )
{
	TerminateProcess( GetCurrentProcess(), 1 );
}	

CRITICAL_SECTION g_SpewCS;
bool g_bSpewCSInitted = false;
bool g_bSuppressPrintfOutput = false;

DbgReturn CmdLib_SpewOutputFunc( DbgLevel type, char const *pMsg )
{
	// Hopefully two threads won't call this simultaneously right at the start!
	if ( !g_bSpewCSInitted )
	{
		InitializeCriticalSection( &g_SpewCS );
		g_bSpewCSInitted = true;
	}

	WORD old;
	DbgReturn retVal;
	
	EnterCriticalSection( &g_SpewCS );
	{
		if (( type == kDbgLevelMessage ) || (type == kDbgLevelLog ))
		{
			old = SetConsoleTextColor( 1, 1, 1, 0 );
			retVal = kDbgContinue;
		}
		else if( type == kDbgLevelWarning )
		{
			old = SetConsoleTextColor( 1, 1, 0, 1 );
			retVal = kDbgContinue;
		}
		else if( type == kDbgLevelAssert )
		{
			old = SetConsoleTextColor( 1, 0, 0, 1 );
			retVal = kDbgBreak;

#ifdef MPI
			// VMPI workers don't want to bring up dialogs and suchlike.			
			if ( g_bUseMPI && !g_bMPIMaster )
			{
				VMPI_HandleCrash( pMsg, true );
				exit( 0 );
			}
#endif
		}
		else if( type == kDbgLevelError )
		{
			old = SetConsoleTextColor( 1, 0, 0, 1 );
			retVal = kDbgAbort; // doesn't matter.. we exit below so we can return an errorlevel (which dbg.dll doesn't do).
		}
		else
		{
			old = SetConsoleTextColor( 1, 1, 1, 1 );
			retVal = kDbgContinue;
		}

		if ( !g_bSuppressPrintfOutput || type == kDbgLevelError )
			printf( "%s", pMsg );

		OutputDebugString( pMsg );
		
		if ( type == kDbgLevelError )
		{
			printf( "\n" );
			OutputDebugString( "\n" );
		}

		RestoreConsoleTextColor( old );
	}
	LeaveCriticalSection( &g_SpewCS );

	if ( type == kDbgLevelError )
	{
		CmdLib_Exit( 1 );
	}

	return retVal;
}


void InstallSpewFunction()
{
	setvbuf( stdout, NULL, _IONBF, 0 );
	setvbuf( stderr, NULL, _IONBF, 0 );

	SetDbgOutputCallback( CmdLib_SpewOutputFunc );
	GetInitialColors();
}


//-----------------------------------------------------------------------------
// Tests the process.cpp stuff
//-----------------------------------------------------------------------------
int main( int argc, char **argv )
{
	CommandLine()->CreateCmdLine( argc, argv );
	InstallSpewFunction();

	float flDelay = CommandLine()->ParmValue( "-delay", 0.0f );
	const char *pEndMessage = CommandLine()->ParmValue( "-message", "Test Finished!\n" );
	int nEndExtraBytes = CommandLine()->ParmValue( "-extrabytes", 0 );

	if ( flDelay > 0.0f )
	{
		float t = Plat_FloatTime();
		while ( Plat_FloatTime() - t < flDelay )
		{
		}
	}

	Msg( pEndMessage );

	if ( nEndExtraBytes )
	{
		while( --nEndExtraBytes >= 0 )
		{
			Msg( "%c", ( nEndExtraBytes % 10 ) + '0' );
		}
	}

	return 0;
}




