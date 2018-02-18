//############################################################################
//##                                                                        ##
//##  EXAMTEX.CPP                                                           ##
//##                                                                        ##
//##  Example of playing a Bink movie using pixel shaders with threads.     ##
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
#include <d3d9.h>   

#include "bink.h"

#include "binktextures.h"


//
// Example globals
//

static HBINK Bink = 0;
static BINKTEXTURESET texture_set = { 0 };

static F32 x_scale = 1.0f;
static F32 y_scale = 1.0f;

static LPDIRECT3D9 d3d = 0;
static LPDIRECT3DDEVICE9 d3d_device = 0;

static U32 num_cpus = 0;
static U32 extra_thread_index;

//
// Statistics variables.
//

static U64 Timer_freq = 0;
static U64 Last_timer = 0;
static U32 Frame_count;
static U64 Render_time = 0;


//############################################################################
//##                                                                        ##
//## Simple utility functions                                               ##
//##                                                                        ##
//############################################################################

static U32 get_num_cpus( void )
{
  SYSTEM_INFO info;

  info.dwNumberOfProcessors = 1;

  GetSystemInfo( &info );
  if ( info.dwNumberOfProcessors )
    return info.dwNumberOfProcessors;

  return 1;
}

static U64 get_time( void )
{
  U64 ret;
  
  //
  // Read the fast timer.
  //

  QueryPerformanceCounter( ( LARGE_INTEGER* ) &ret );

  return ret;
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

void Show_error( LPCSTR text, LPCSTR title )
{
  MessageBox( 0, text, title, MB_OK | MB_ICONSTOP );
}


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
  d3d_pp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
  
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

  if ( Bink )
  {
    x_scale = (F32)width / (F32)Bink->Width;
    y_scale = (F32)height / (F32)Bink->Height;
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
  U64 cur;

  cur = get_time();
  
  if ( Last_timer == 0 )
  {
    Last_timer = cur;
  }
  else
  {
    cur -= Last_timer; // get delta

    sprintf( buffer,
             "Frame rate: %3.1f   D3D9 Rendering: %2.1f%%",
             ( ( F32 ) Frame_count * (F32) Timer_freq ) / ( F32 ) cur,
             ( ( F32 ) Render_time * 100.0F ) / ( F32 ) cur );

    Last_timer += cur; // add delta

    SetWindowText( window, buffer );
  }

  Frame_count = 0;
  Render_time = 0;
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
      DestroyWindow( window );
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


//############################################################################
//##                                                                        ##
//## Show_frame - shows the next Bink frame.                                ##
//##                                                                        ##
//############################################################################

static void Show_frame( HBINK Bink, BINKTEXTURESET * texture_set )
{
  Render_time -= get_time();
  
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
                      texture_set,
                      Bink->Width, Bink->Height,
                      0, 0,
                      x_scale, y_scale,
                      1.0f, 0 );

  //
  // End the rendering.
  //

  d3d_device->EndScene();
  d3d_device->Present( 0, 0, 0, 0 );

  Render_time += get_time();
}


//############################################################################
//##                                                                        ##
//## Locks the texture and starts decompressing.                            ##
//##                                                                        ##
//############################################################################

static void start_next_frame( HBINK Bink, BINKTEXTURESET * texture_set )
{
  Render_time -= get_time();

  //
  // Lock the textures.
  //

  Lock_Bink_textures( texture_set );

  //
  // Register our locked texture pointers with Bink
  //

  BinkRegisterFrameBuffers( Bink, &texture_set->bink_buffers );

  Render_time += get_time();

  //
  // start the first frame in the background immediately
  //
  
  BinkDoFrameAsync( Bink, 0, extra_thread_index ); 
}



//############################################################################
//##                                                                        ##
//## Checks and advanced the Bink video if necessary.                       ##
//##                                                                        ##
//############################################################################

static S32 Check_bink( HBINK Bink, BINKTEXTURESET * texture_set )
{
  S32 new_frame = 0;
    
  //
  // Is it time for a new Bink frame?
  //

//  if ( ! BinkWait( Bink ) )
  {
    //
    // is the previous frame done yet (wait for a ms)
    //   note that this logic assumes you already 
    //   have a frame decompressing the first time 
    //   this function is called
    //

    if ( BinkDoFrameAsyncWait( Bink, 1000 ) )
    {
      Render_time -= get_time();

      //
      // Unlock the textures.
      //

      Unlock_Bink_textures( d3d_device, texture_set, Bink );

      Render_time += get_time();

      //
      // we have finished a frame, so set a flag to draw it
      //
      
      new_frame = 1;
    
      //
      // Keep playing the movie.
      //

      BinkNextFrame( Bink );

      //
      // do we need to skip a frame?
      //

      if ( BinkShouldSkip( Bink ) )
      {
        BinkDoFrameAsync( Bink, 0, extra_thread_index );
        BinkDoFrameAsyncWait( Bink, -1 );
        BinkNextFrame( Bink );
      }

      //
      // start decompressing the next frame
      //

      start_next_frame( Bink, texture_set );
     }
  }

  return( new_frame );
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
  // init the timer
  //
  
  QueryPerformanceFrequency( ( LARGE_INTEGER* ) &Timer_freq );

  //
  // get the number of cpus and create our threads
  // 

  num_cpus = get_num_cpus();

  // we use one background thread regardless, and if we have
  //   at least two CPUs, we'll use another background one

  BinkStartAsyncThread( 0, 0 );

  if ( num_cpus <= 1 )
  {
    extra_thread_index = 0;
  }
  else
  {
    BinkStartAsyncThread( 1, 0 );
    extra_thread_index = 1;
  }

  
  //
  // Tell Bink to use DirectSound (must be before BinkOpen)!
  //

  BinkSoundUseDirectSound( 0 );

  //
  // Try to open the Bink file.
  //

  Bink = BinkOpen( cmd_line, BINKSNDTRACK | BINKNOFRAMEBUFFERS );
  if ( !Bink )
  {
    Show_error( BinkGetError( ), "Error opening file..." );
    DestroyWindow( window );
    return( 2 );
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
  // Init d3d, device and viewport
  //

  if ( !Init_d3d( window, Bink->Width, Bink->Height ) )
  {
    Show_error( "Error initializing D3D.", "D3D" );
    DestroyWindow( window );
    BinkClose( Bink );
    return( 3 );
  }

  //
  // Create the Bink shaders to use
  //

  if ( Create_Bink_shaders( d3d_device ) )
  {
    //
    // Get the frame buffers info, so we know what size/type of textures to create
    //

    BinkGetFrameBuffersInfo( Bink, &texture_set.bink_buffers );

    //
    // Try to create textures for Bink to use.
    //

    if ( Create_Bink_textures( d3d_device, &texture_set ) )
    {
      //
      // start the first frame decoding in the background
      //

      start_next_frame( Bink, &texture_set );
      
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
          S32 new_frame;
          
          //
          // check the Bink playback
          //

          new_frame = Check_bink( Bink, &texture_set );

          //
          // do we have a new frame?
          //

          if ( new_frame )
          {
            //
            // Yup, draw it
            //

            Show_frame( Bink, &texture_set );

            //
            // Are we at the end of the movie?
            //

            if ( Bink->FrameNum == Bink->Frames )
            {
              break;
            }
          }
          else
          {
            //
            // Nope, give the rest of the system a chance to run
            //

            Sleep( 1 );
          }
        }
      }


      //
      // wait until background is done
      //
      
      BinkDoFrameAsyncWait( Bink, -1 );

      //
      // Free textures.
      //

      Free_Bink_textures( d3d_device, &texture_set );
    }

    Free_Bink_shaders( );
  }

  //
  // Close the Bink file.
  //

  BinkClose( Bink );
  Bink = 0;

  //
  // stop the background threads
  //
  
  BinkRequestStopAsyncThread( 0 );
  if ( num_cpus > 1 )
    BinkRequestStopAsyncThread( 1 );

  BinkWaitStopAsyncThread( 0 );
  if ( num_cpus > 1 )
    BinkWaitStopAsyncThread( 1 );


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

