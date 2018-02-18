//############################################################################
//##                                                                        ##
//##  EXAM3D.C                                                              ##
//##                                                                        ##
//##  Example program that plays a video with a fixed function 3D pipeline. ##
//##                                                                        ##
//##  Author: Jeff Roberts                                                  ##
//##                                                                        ##
//############################################################################
//##                                                                        ##
//##  Copyright (C) RAD Game Tools, Inc.                                    ##
//##                                                                        ##
//##  For technical support, contact RAD Game Tools at 425 - 893 - 4300.    ##
//##                                                                        ##
//############################################################################


#include <stdio.h>
#include <windows.h>
#include <zmouse.h>

#include "bink.h"

#include "rad3d.h"


//
// Example globals
//

static HBINK Bink = 0;

static HRAD3D Rad_3d = 0;
static HRAD3DIMAGE Image = 0;

static U32 Maximum_texture_size = 256;
static U32 Play_fast = 0;
static U32 Show_texture_lines = 0;

//
// Statistics variables.
//

static U64 Last_timer = 0;
static U32 Frame_count;
static U32 Bink_microseconds;
static U32 Render_microseconds;


//############################################################################
//##                                                                        ##
//## Ask for a timer count in 64-bit accuracy.                              ##
//##                                                                        ##
//############################################################################

static void Start_us_count( U64* out_count )
{
  //
  // Read the fast timer.
  //

  QueryPerformanceCounter( ( LARGE_INTEGER* )out_count );
}


//############################################################################
//##                                                                        ##
//## Delta from a previous call to Start or Delta and return microseconds.  ##
//##                                                                        ##
//############################################################################

static U32 Delta_us_count( U64* last_count )
{
  static U64 frequency=1000;
  static S32 got_frequency=0;
  U64 start;

  //
  // If this is the first time called, get the high-performance timer count.
  //

  if ( !got_frequency )
  {
    got_frequency = 1;
    QueryPerformanceFrequency( ( LARGE_INTEGER* )&frequency );
  }

  start = *last_count;

  QueryPerformanceCounter( ( LARGE_INTEGER* )last_count );

  return(  ( U32 ) ( ( ( *last_count - start ) * ( U64 ) 1000000 ) / frequency ) );
}


//############################################################################
//##                                                                        ##
//## Simple macros using the above two functions for perfoming timings.     ##
//##                                                                        ##
//############################################################################

#define Start_timer() { U64 __timer; Start_us_count( &__timer );
#define End_and_start_next_timer( count ) count += Delta_us_count( &__timer );
#define End_timer( count ) End_and_start_next_timer( count ) }


//############################################################################
//##                                                                        ##
//## Setup the array to convert from 3D image formats to Bink formats.      ##
//##                                                                        ##
//############################################################################

static U32 Bink_surface_type [ RAD3DSURFACECOUNT ];

static void Setup_surface_array( void )
{
  Bink_surface_type[ RAD3DSURFACE24 ] = BINKSURFACE24;
  Bink_surface_type[ RAD3DSURFACE24R ] = BINKSURFACE24R;
  Bink_surface_type[ RAD3DSURFACE32 ] = BINKSURFACE32;
  Bink_surface_type[ RAD3DSURFACE32R ] = BINKSURFACE32R;
  Bink_surface_type[ RAD3DSURFACE32A ] = BINKSURFACE32A;
  Bink_surface_type[ RAD3DSURFACE32RA ] = BINKSURFACE32RA;
  Bink_surface_type[ RAD3DSURFACE555 ] = BINKSURFACE555;
  Bink_surface_type[ RAD3DSURFACE565 ] = BINKSURFACE565;
  Bink_surface_type[ RAD3DSURFACE5551 ] = BINKSURFACE5551;
  Bink_surface_type[ RAD3DSURFACE4444 ] = BINKSURFACE4444;
}


//############################################################################
//##                                                                        ##
//## Advance a Bink file by one frame into a 3D image buffer.               ##
//##                                                                        ##
//############################################################################

static void Decompress_frame( HBINK bink, HRAD3DIMAGE image, S32 copy_all, S32 never_skip )
{
  void* pixels;
  U32 pixel_pitch;
  U32 pixel_format;
  U32 src_x, src_y, src_w, src_h;

  Start_timer( );

  //
  // Decompress the Bink frame.
  //

  BinkDoFrame( bink );

  //
  // if we are falling behind, decompress an extra frame to catch up
  //
              
  if ( !never_skip )
  {
    while ( BinkShouldSkip( bink ) )
    {
      BinkNextFrame( bink );
      BinkDoFrame( bink );
    }
  }
 
  //
  // if more than 75% of the image has changed, then tell d3d to discard
  //   the previous pixels and then update everything
  //

  if ( !copy_all )
  {
    if ( bink->FrameChangePercent >= 75 )
      copy_all = 1;
    }

  End_and_start_next_timer( Bink_microseconds );

  //
  // Lock the 3D image so that we can copy the decompressed frame into it.
  //

  while ( Lock_RAD_3D_image( image, &pixels, &pixel_pitch, &pixel_format,
                             &src_x, &src_y, &src_w, &src_h, copy_all ) )
  {
    //
    // Copy the decompressed frame into the 3D image.
    //

    BinkCopyToBufferRect( bink,
                          pixels,
                          pixel_pitch,
                          bink->Height,
                          0,0,
                          src_x, src_y, src_w, src_h,
                          Bink_surface_type[ pixel_format ] | BINKNOSKIP |
                          ( ( copy_all ) ? BINKCOPYALL : 0 ) );

    //
    // Unlock the 3D image.
    //

    Unlock_RAD_3D_image( image );
  }

  End_and_start_next_timer( Render_microseconds );

  //
  // Keep playing the movie.
  //

  BinkNextFrame( bink );

  End_timer( Bink_microseconds );
}


//############################################################################
//##                                                                        ##
//## Show_frame - advances to the next Bink frame.                          ##
//##                                                                        ##
//############################################################################

static void Show_frame( void )
{
  //
  // Keep track of how many frames we've played.
  //

  ++Frame_count;

  Start_timer( );

  //
  // Begin a 3D frame.
  //

  Start_RAD_3D_frame ( Rad_3d );

  //
  // Draw the image on the screen.
  //

  Blit_RAD_3D_image( Image, 0, 0, 1.0F, 1.0F, 1.0F, 0 );

  //
  // Draw the texture lines, if requested.
  //

  if ( Show_texture_lines )
    Draw_lines_RAD_3D_image( Image, 0, 0, 1.0F, 1.0F );

  //
  // End a 3D frame.
  //

  End_RAD_3D_frame ( Rad_3d );

  End_timer( Render_microseconds );
}


//############################################################################
//##                                                                        ##
//## Allocate the two image handles that we'll use to show the videos.      ##
//##                                                                        ##
//############################################################################

static S32 Allocate_3D_images( void )
{
  HRAD3DIMAGE New_image;

  //
  // Try to open a 3D image for the Bink.
  //

  New_image = Open_RAD_3D_image( Rad_3d,
                                 Bink->Width,
                                 Bink->Height,
                                 0,
                                 Maximum_texture_size );

  //
  // Did it open?
  //

  if ( New_image )
  {
    //
    // Yup, it opened! Close the old image (if there was one) and replace it.
    //

    if ( Image )
      Close_RAD_3D_image( Image );

    Image = New_image;

    //
    // Advance the Bink to fill the texture.
    //

    Decompress_frame( Bink, Image, 1, 1 );

    return( 1 );
  }

  //
  // Free the image if one was opened.
  //

  if ( New_image )
    Close_RAD_3D_image( New_image );

  return( 0 );
}


static void Clear_to_black( HWND window )
{
  PAINTSTRUCT ps;
  HDC dc;

  //
  // Get the repaint DC and then fill the window with black.
  //

  dc = BeginPaint( window, &ps );

  PatBlt( dc, 0, 0, 4096, 4096, BLACKNESS );

  EndPaint( window, &ps );
}


//############################################################################
//##                                                                        ##
//## Calc_window_values - calculates the extra width and height to add to   ##
//##   the windows's size so that the video fits.                           ##
//##                                                                        ##
//############################################################################

static void Calc_window_values( HWND window,
                                S32* out_extra_width,
                                S32* out_extra_height )
{
  RECT r, c;

  //
  // Get the current window rect (in screen space).
  //

  GetWindowRect( window, &r );

  //
  // Get the client rectangle of the window.
  //

  GetClientRect( window, &c );

  *out_extra_width = ( r.right - r.left ) - ( c.right - c.left );
  *out_extra_height = ( r.bottom - r.top ) - ( c.bottom - c.top );
}


//############################################################################
//##                                                                        ##
//## WindowProc - the main window message procedure.                        ##
//##                                                                        ##
//############################################################################

void Copy_one_filename( char* out_dest, char* out_src )
{
  char* s = out_src;
  char* d = out_dest;
  int quoted = 0;

  //
  // Skip initial whitespace.
  //

  while ( ( *s ) && ( ( *s ) <= ' ' ) )
    ++s;

  //
  // Start the file copy.
  //

  while ( *s )
  {
    if ( ( *s ) == '"' )
    {
      //
      // If we get a quote, turn on or off quoted mode.
      //

      quoted = !quoted;
    }
    else if ( ( ( *s ) <= ' ' ) && ( !quoted ) )
    {
      //
      // If we get a space or other low character, assume white space and exit
      //   the loop.
      //

      ++s;
      break;
    }
    else
    {
      //
      // Otherwise, just copy the character.
      //

      *d++ = *s;
    }

    ++s;
  }

  *d = 0;

  //
  // Skip middle whitespace.
  //

  while ( ( *s ) && ( ( *s ) <= ' ' ) )
    ++s;

  //
  // Copy up the end of the source string (to remove the input).
  //

  strcpy( out_src, s );

}


static void Handle_character( HWND window,
                              WPARAM ch )
{
  switch ( ch )
  {
    //
    // Toggle the "play-fast" flag.
    //

    case 'f': case 'F':
      Play_fast = !Play_fast;
      break;

    //
    // Advance the one frame.
    //

    case 'b': case 'B':
      Decompress_frame( Bink, Image, 0, 1 );
      break;

    //
    // Draw lines.
    //

    case 'l': case 'L':
      Show_texture_lines = !Show_texture_lines;
      break;

    //
    // Handle shrinking or enlarging the texture sizes.
    //

    case '+':
      Maximum_texture_size *= 2;
      if ( !Allocate_3D_images( ) )
        Maximum_texture_size /= 2;
      break;

    case '-':
      if ( Maximum_texture_size >= 32 )
      {
        Maximum_texture_size /= 2;

        if ( !Allocate_3D_images( ) )
          Maximum_texture_size *= 2;
      }
      break;

    //
    // Close the application.
    //

    case 27:
      DestroyWindow( window );
      break;
  }
}


//############################################################################
//##                                                                        ##
//## Update the playback statistics.                                        ##
//##                                                                        ##
//############################################################################

static void Update_statistics( HWND window )
{
  char buffer[256];
  U32 delta;

  if ( Last_timer == 0 )
  {
    Start_us_count( &Last_timer );
  }
  else
  {
    delta = Delta_us_count ( &Last_timer );

    sprintf( buffer,
             "Frame rate: %3.1f   Bink: %2.1f%%   %s Rendering: %2.1f%%",
             ( ( F32 ) Frame_count * 1000000.0F ) / ( F32 ) delta,
             ( ( F32 ) Bink_microseconds * 100.0F ) / ( F32 ) delta,
             Describe_RAD_3D( ),
             ( ( F32 ) Render_microseconds * 100.0F ) / ( F32 ) delta );

    SetWindowText( window, buffer );
  }

  Frame_count = 0;
  Bink_microseconds = 0;
  Render_microseconds = 0;
}


//############################################################################
//##                                                                        ##
//## WindowProc - the main window message procedure.                        ##
//##                                                                        ##
//############################################################################

LRESULT WINAPI WindowProc( HWND   window,
                           UINT   message,
                           WPARAM wparam,
                           LPARAM lparam )
{

  switch( message )
  {
    //
    // Just close the window if the user hits a key.
    //

    case WM_CHAR:
      Handle_character( window, wparam );
      break;

    //
    // Pause/resume the video when the focus changes.
    //

    case WM_KILLFOCUS:
      BinkPause( Bink, 1 );
      break;

    case WM_SETFOCUS:
      BinkPause( Bink, 0 );
      break;

    //
    // Handle the window paint messages.
    //

    case WM_PAINT:
      Clear_to_black( window );
      return( 0 );

    case WM_ERASEBKGND:
      return( 1 );

    //
    // Handle the window being sized.
    //

    case WM_SIZE:
      if ( Rad_3d )
      {
        RECT r;
        GetClientRect( window, &r);

        Resize_RAD_3D( Rad_3d, r.right - r.left, r.bottom - r.top );
      }
      break;


    //
    // Use a timer to display periodic playback statistics.
    //

    case WM_TIMER:
      Update_statistics( window );
      break;

    //
    // Post the quit message.
    //

    case WM_DESTROY:
      PostQuitMessage( 0 );
      return( 0 );
  }

  //
  // Call the OS default window procedure.
  //

  return( DefWindowProc( window, message, wparam, lparam ) );
}


//############################################################################
//##                                                                        ##
//## Build_window_handle - creates a window class and window handle.        ##
//##                                                                        ##
//############################################################################

static HWND Build_window_handle( HINSTANCE instance,
                                 HINSTANCE previous_instance )
{
  //
  // Create the window class if this is the first instance.
  //

  if ( !previous_instance )
  {
    WNDCLASS wc;

    wc.style = 0;
    wc.lpfnWndProc = WindowProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = instance;
    wc.hIcon = LoadIcon( instance, MAKEINTRESOURCE( 101 ) );
    wc.hCursor = LoadCursor( 0, IDC_ARROW );;
    wc.hbrBackground = 0;
    wc.lpszMenuName = 0;
    wc.lpszClassName = "BinkExam";

    //
    // Try to register the class.
    //

    if ( !RegisterClass( &wc ) )
    {
      return( 0 );
    }
  }

  //
  // Return the new window with a tiny initial default size (it is resized
  //   later on).
  //

  return( CreateWindow( "BinkExam",
                        "Bink Example Player",
                        WS_CAPTION | WS_POPUP| WS_CLIPCHILDREN |
                        WS_SYSMENU | WS_MINIMIZEBOX | WS_SIZEBOX,
                        64, 64,
                        64, 64,
                        0, 0, instance,0 ) );
}


//############################################################################
//##                                                                        ##
//## WinMain - the primary function entry point                             ##
//##                                                                        ##
//############################################################################

int PASCAL WinMain( HINSTANCE instance,
                    HINSTANCE previous_instance,
                    LPSTR cmd_line,
                    int cmd_show )
{
  //
  // Win32 locals.
  //

  HWND window;
  MSG msg;
  S32 extra_width, extra_height;

  //
  // Try to create our window.
  //

  window = Build_window_handle( instance,
                                previous_instance );
  if ( !window )
  {
    MessageBox( 0,
                "Error creating window.",
                "Windows",
                MB_OK | MB_ICONSTOP );
    return( 1 );
  }

  //
  // Calculate the extra width and height for the window.
  //

  Calc_window_values( window,
                      &extra_width, &extra_height );

  //
  // Tell Bink to use DirectSound (must be before BinkOpen)!
  //

  BinkSoundUseDirectSound( 0 );

  //
  // Try to open the Bink file.
  //

  Bink = BinkOpen( cmd_line, 0 );

  if ( !Bink )
  {
    MessageBox( 0,
                BinkGetError( ),
                "Error opening file...",
                MB_OK | MB_ICONSTOP );

    DestroyWindow( window );
    return( 3 );
  }


  //
  // Size the window such that its client area exactly fits our Bink movie.
  //

  SetWindowPos( window, 0,
                0, 0,
                Bink->Width+extra_width,
                Bink->Height+extra_height,
                SWP_NOMOVE );

  //
  // Try to open the 3D API.
  //

  Rad_3d = Open_RAD_3D( window );

  if ( !Rad_3d )
  {
    MessageBox( 0,
                "Error opening 3D API.",
                "3D Error",
                MB_OK | MB_ICONSTOP );

    DestroyWindow( window );
    BinkClose( Bink );
    return( 4 );
  }

  //
  // Setup up the array that converts between surface types.
  //

  Setup_surface_array( );

  //
  // Try to open the 3D images from the Binks.
  //

  if ( !Allocate_3D_images( ) )
  {
    MessageBox( 0,
                "Error creating 3D textures.",
                "3D Error",
                MB_OK | MB_ICONSTOP );

    Close_RAD_3D( Rad_3d );
    DestroyWindow( window );
    BinkClose( Bink );
    return( 5 );
  }

  //
  // Create a timer to update the statistics.
  //

  SetTimer( window, 0, 1000, 0 );

  //
  // Now display the window and start the message loop.
  //

  ShowWindow( window, cmd_show );

  for ( ; ; )
  {
    //
    // Are there any messages to handle?
    //

    if ( PeekMessage( &msg, 0, 0, 0, PM_REMOVE ) )
    {
      //
      // Yup, handle them.
      //

      if ( msg.message == WM_QUIT )
        break;

      TranslateMessage( &msg );
      DispatchMessage( &msg );
    }
    else
    {
      //
      // Is it time for a new Bink frame?
      //

      if ( ( !BinkWait( Bink ) ) || ( Play_fast ) )
      {
        Decompress_frame( Bink, Image, 0, 0 );

        //
        // Yup, draw the next frame.
        //

        Show_frame( );
      }
      else
      {
        //
        // Nope, give the rest of the system a chance to run (1 ms).
        //

        Sleep( 1 );
      }

    }
  }

  //
  // Close the Bink file.
  //

  if ( Bink )
  {
    BinkClose( Bink );
    Bink = 0;
  }

  //
  // Close the 3D image.
  //

  if ( Image )
  {
    Close_RAD_3D_image( Image );
    Image = 0;
  }

  //
  // Close the 3D API.
  //

  if ( Rad_3d )
  {
    Close_RAD_3D( Rad_3d );
    Rad_3d = 0;
  }

  //
  // And exit.
  //

  return( 0 );
}

// some stuff for the RAD build utility
// @cdep pre $DefaultsWin3264EXE
// @cdep pre $requires( dx9rad3d.cpp )
// @cdep pre $requiresbinary(winmm.lib)
// @cdep pre $requiresbinary(binkw$platnum.lib)
// @cdep post $BuildWin3264EXE

