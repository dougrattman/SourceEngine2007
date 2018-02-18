//############################################################################
//##                                                                        ##
//##  EXAMTEX.CPP                                                           ##
//##                                                                        ##
//##  Example of playing a Bink movie using pixel shaders.                  ##
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
#define D3D_OVERLOADS
#include <windows.h>
#include <d3d9.h>   // change to your 9.0 or up path
#include <zmouse.h>

#include "bink.h"

#include "binktextures.h"


//
// If your Bink video has premultiplied alpha (which is recommended),
//   then set this define to 1, otherwise set it to zero.
//

#define MY_BINK_ALPHA_IS_PREMULTIPLIED 1


//
// Example globals
//

static HBINK Back_Bink = 0;
static HBINK Alpha_Bink = 0;
static BINKTEXTURESET Back_texture_set = { 0 };
static BINKTEXTURESET Alpha_texture_set = { 0 };

static F32 x_scale = 1.0f;
static F32 y_scale = 1.0f;
static F32 x_offset = 0.0f;
static F32 y_offset = 0.0f;
static F32 Alpha_level = 1.0f;

static LPDIRECT3D9 d3d = 0;
static LPDIRECT3DDEVICE9 d3d_device = 0;

//
// Statistics variables.
//

static U64 Last_timer = 0;
static U32 Frame_count;
static U32 Bink_microseconds;
static U32 Render_microseconds;


//############################################################################
//##                                                                        ##
//## Call this function to actually open Direct3D.                          ##
//##                                                                        ##
//############################################################################

static int Init_d3d( HWND window, U32 width, U32 height )
{
  d3d = Direct3DCreate9( D3D_SDK_VERSION );

  if( ! d3d )
  {
    return( 0 );
  }

  D3DPRESENT_PARAMETERS d3d_pp = {0};
  d3d_pp.hDeviceWindow = window;
  d3d_pp.Windowed = TRUE;
  d3d_pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
  d3d_pp.BackBufferFormat = D3DFMT_UNKNOWN;
  d3d_pp.BackBufferWidth = width;
  d3d_pp.BackBufferHeight = height;

  if ( FAILED( d3d->CreateDevice( D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, window,
                                  D3DCREATE_HARDWARE_VERTEXPROCESSING,
                                  &d3d_pp, &d3d_device ) ) )
  { 
    if ( FAILED( d3d->CreateDevice( D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, window,
                                    D3DCREATE_SOFTWARE_VERTEXPROCESSING,
                                    &d3d_pp, &d3d_device ) ) )
    {
      d3d->Release();
      return( 0 );
    }
  }

  //
  // Set up the viewport
  //

  D3DVIEWPORT9 vp = { 0, 0, width, height, 0.0f, 1.0f };

  d3d_device->SetViewport( &vp );

  //
  // And return success.
  //

  return( 1 );
}


//############################################################################
//##                                                                        ##
//## Call this function to close Direct3D.                                  ##
//##                                                                        ##
//############################################################################

static void Close_d3d( void )
{
  if ( d3d_device )
  {
    d3d_device->Release( );
    d3d_device = 0;
  }

  if ( d3d )
  {
    d3d->Release( );
    d3d = 0;
  }
}


//############################################################################
//##                                                                        ##
//## Resize the Direct3D viewport.                                          ##
//##                                                                        ##
//############################################################################

static void Resize_d3d( HWND window, U32 width, U32 height )
{
  //
  // Make sure there is at least a pixel.
  //

  if ( width < 1 )
  {
    width = 1;
  }

  if ( height < 1 )
  {
    height = 1;
  }

  //
  // Adjust x and y scale for new size
  //

  if ( Back_Bink )
  {
//############################################################################
//##                                                                        ##
//## Calc_window_values - calculates the extra width and height to add to   ##
//##   the windows's size so that the video fits.                           ##
//##                                                                        ##
//############################################################################
    x_scale = (F32)width / (F32)Back_Bink->Width;
    y_scale = (F32)height / (F32)Back_Bink->Height;
  }
  else
  {
    x_scale = 1.0f;
    y_scale = 1.0f;
  }

  //
  // Create the viewport
  //

  D3DVIEWPORT9 vp = { 0, 0, width, height, 0.0f, 1.0f };

  d3d_device->SetViewport( &vp );
}


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
//## Show_frame - shows the next Bink frame.                                ##
//##                                                                        ##
//############################################################################

static void Show_frame( )
{
  Start_timer( );

  //
  // Keep track of how many frames we've played.
  //

  ++Frame_count;

  //
  // Start the scene
  //

  d3d_device->BeginScene();

  //
  // Clear the screen.
  //

  d3d_device->Clear( 0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0,0,0), 1.0f, 0 );

  //
  // Draw the image on the screen.
  //

  Draw_Bink_textures( d3d_device,
                      &Back_texture_set,
                      Back_Bink->Width, Back_Bink->Height,
                      0, 0,
                      x_scale, y_scale,
                      1.0f, 0 );

  Draw_Bink_textures( d3d_device,
                      &Alpha_texture_set,
                      Alpha_Bink->Width, Alpha_Bink->Height,
                      x_offset, y_offset,
                      x_scale, y_scale,
                      Alpha_level,
                      MY_BINK_ALPHA_IS_PREMULTIPLIED );

  //
  // End the rendering.
  //

  d3d_device->EndScene();
  d3d_device->Present( 0, 0, 0, 0 );

  End_timer( Render_microseconds );
}


//############################################################################
//##                                                                        ##
//## Decompress the next frame of video.                                    ##
//##                                                                        ##
//############################################################################

static void Decompress_frame( HBINK bink, BINKTEXTURESET * texture_set )
{
  Start_timer( );

  //
  // Lock the textures.
  //

  Lock_Bink_textures( texture_set );

  //
  // Register our locked texture pointers with Bink
  //

  BinkRegisterFrameBuffers( bink, &texture_set->bink_buffers );

  End_and_start_next_timer( Render_microseconds );

  //
  // Decompress a frame
  //

  BinkDoFrame( bink );

  //
  // if we are falling behind, decompress an extra frame to catch up
  //
              
  while ( BinkShouldSkip( bink ) )
  {
    BinkNextFrame( bink );
    BinkDoFrame( bink );
  }

  End_and_start_next_timer( Bink_microseconds );

  //
  // Unlock the textures.
  //

  Unlock_Bink_textures( d3d_device, texture_set, bink );

  End_and_start_next_timer( Render_microseconds );

  //
  // Keep playing the movie.
  //
  BinkNextFrame( bink );

  End_timer( Bink_microseconds );
}


//############################################################################
//##                                                                        ##
//## Clear the window to black.                                             ##
//##                                                                        ##
//############################################################################

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
    delta = Delta_us_count( &Last_timer );

    sprintf( buffer,
             "Frame rate: %3.1f   Bink: %2.1f%%   D3D9 Rendering: %2.1f%%",
             ( ( F32 ) Frame_count * 1000000.0F ) / ( F32 ) delta,
             ( ( F32 ) Bink_microseconds * 100.0F ) / ( F32 ) delta,
             ( ( F32 ) Render_microseconds * 100.0F ) / ( F32 ) delta );

    SetWindowText( window, buffer );
  }

  Frame_count = 0;
  Bink_microseconds = 0;
  Render_microseconds = 0;
}


//############################################################################
//##                                                                        ##
//## Helper routine to extract one filename at a time from a buffer.        ##
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


//############################################################################
//##                                                                        ##
//## Handle char input.                                                     ##
//##                                                                        ##
//############################################################################

static void Handle_character( HWND window,
                              WPARAM ch )
{
  const F32 alpha_nudge = 0.1f;
  const F32 offset_nudge = 8.0f;

  switch ( ch )
  {
    //
    // Increase/decrease the alpha.
    //

    case '+':
    case '=':
      Alpha_level += alpha_nudge;
      if (Alpha_level > 1.0f)
        Alpha_level = 1.0f;
      break;

    case '_':
    case '-':
      Alpha_level -= alpha_nudge;
      if (Alpha_level < 0.0f)
        Alpha_level = 0.0f;
      break;

    case '1':
      Alpha_level = 1.0f;
      break;

    case '0':
      Alpha_level = 0.0f;
      break;

    //
    // Change alpha value and alpha frame x/y offsets
    //

    case 'A':   // Left
    case 'a':   // Left
      x_offset -= offset_nudge;
      break;

    case 'D':   // Right
    case 'd':   // Right
      x_offset += offset_nudge;
      break;

    case 'W':   // Up
    case 'w':   // Up
      y_offset -= offset_nudge;
      break;

    case 'X':   // Down
    case 'x':   // Down
      y_offset += offset_nudge;
      break;

    case 'S':   // Back to starting location
    case 's':   // Back to starting location
      x_offset = 0.0f;
      y_offset = 0.0f;
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
      BinkPause( Back_Bink, 1 );
      BinkPause( Alpha_Bink, 1 );
      break;

    case WM_SETFOCUS:
      BinkPause( Back_Bink, 0 );
      BinkPause( Alpha_Bink, 0 );
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
      if ( d3d_device )
      {
        RECT r;
        GetClientRect( window, &r);

        Resize_d3d( window, r.right - r.left, r.bottom - r.top );
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


void Show_error( LPCSTR text, LPCSTR title )
{
    MessageBox( 0, text, title, MB_OK | MB_ICONSTOP );
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
  char back_name[ 256 ];
  char alpha_name[ 256 ];

  //
  // Try to create our window.
  //

  window = Build_window_handle( instance, previous_instance );
  if ( !window )
  {
    Show_error( "Error creating window.", "Windows" );
    return( 1 );
  }

  //
  // Calculate the extra width and height for the window.
  //

  Calc_window_values( window, &extra_width, &extra_height );

  //
  // Extract the filenames (copy both into background and then copy the first
  //   filename out of the background and into the foreground).
  //

  strcpy( back_name, cmd_line );
  Copy_one_filename( alpha_name, back_name );

  //
  // Tell Bink to use DirectSound (must be before BinkOpen)!
  //

  BinkSoundUseDirectSound( 0 );

  //
  // Try to open the Bink files.
  //

  Back_Bink = BinkOpen( back_name, BINKSNDTRACK | BINKNOFRAMEBUFFERS );
  if ( !Back_Bink )
  {
    Show_error( BinkGetError( ), "Error opening back file..." );
    DestroyWindow( window );
    return( 2 );
  }

  Alpha_Bink = BinkOpen( alpha_name, BINKALPHA | BINKPRELOADALL );
  if ( !Alpha_Bink )
  {
    Show_error( BinkGetError( ), "Error opening alpha file..." );
    DestroyWindow( window );
    BinkClose( Back_Bink );
    return( 3 );
  }

  //
  // Size the window such that its client area exactly fits our Bink movie.
  //

  SetWindowPos( window, 0,
                0, 0,
                Back_Bink->Width+extra_width,
                Back_Bink->Height+extra_height,
                SWP_NOMOVE );

  //
  // Init d3d, device and viewport
  //

  if ( !Init_d3d( window, Back_Bink->Width, Back_Bink->Height ) )
  {
    Show_error( "Error initializing D3D.", "D3D" );
    DestroyWindow( window );
    BinkClose( Back_Bink );
    BinkClose( Alpha_Bink );
    return( 4 );
  }

  //
  // Create the Bink shaders to use
  //

  if ( Create_Bink_shaders( d3d_device ) )
  {
    //
    // Get the frame buffers info, so we know what size/type of textures to create
    //

    BinkGetFrameBuffersInfo( Back_Bink, &Back_texture_set.bink_buffers );
    BinkGetFrameBuffersInfo( Alpha_Bink, &Alpha_texture_set.bink_buffers );

    //
    // Try to create textures for Bink to use.
    //

    if ( Create_Bink_textures( d3d_device, &Back_texture_set ) )
    {
      if ( Create_Bink_textures( d3d_device, &Alpha_texture_set ) )
      {
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
            // Is it time for a new Bink frame (we play at the speed of the background vid in this example)
            //

            if ( !BinkWait( Back_Bink ) )
            {
              Decompress_frame( Back_Bink, &Back_texture_set );

              //
              // Is it time for a new alpha frame?
              //

              if ( !BinkWait( Alpha_Bink ) )
              {
                Decompress_frame( Alpha_Bink, &Alpha_texture_set );
              }

              //
              // Draw the next frames of each movie
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
        // Free textures.
        //

        Free_Bink_textures( d3d_device, &Alpha_texture_set );
      }

      Free_Bink_textures( d3d_device, &Back_texture_set );
    }

    Free_Bink_shaders( );
  }

  //
  // Close the Bink files.
  //

  BinkClose( Back_Bink );
  BinkClose( Alpha_Bink );
  Back_Bink = 0;
  Alpha_Bink = 0;

  //
  // Close D3D.
  //

  Close_d3d( );

  //
  // And exit.
  //

  return( 0 );
}


// some stuff for the RAD build utility
// @cdep pre $DefaultsWin3264EXE
// @cdep pre $requires(binktextures.cpp)
// @cdep pre $requiresbinary(d3d9.lib)
// @cdep pre $requiresbinary(d3dx9.lib)
// @cdep pre $requiresbinary(advapi32.lib)
// @cdep pre $requiresbinary(winmm.lib)
// @cdep pre $requiresbinary(uuid.lib)
// @cdep pre $requiresbinary(binkw$platnum.lib)
// @cdep post $BuildWin3264EXE


