/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - Graphics_1.3.h                                          *
 *   Mupen64Plus homepage: http://code.google.com/p/mupen64plus/           *
 *   Copyright (C) 2002 Zilmar                                             *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.          *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/***********************************************************************************

Notes:
------

Setting the approprate bits in the MI_INTR_REG and calling CheckInterrupts which 
are both passed to the DLL in InitiateGFX will generate an Interrupt from with in 
the plugin.

The Setting of the RSP flags and generating an SP interrupt  should not be done in
the plugin

**********************************************************************************/
#ifndef _GFX_H_INCLUDED__
#define _GFX_H_INCLUDED__

#if defined(__cplusplus)
extern "C" {
#endif

/* Plugin types */
#define PLUGIN_TYPE_GFX             2
	
#define CALL

/******************************************************************
  Function: CaptureScreen
  Purpose:  This function dumps the current frame to a file
  input:    pointer to the directory to save the file to
  output:   none
*******************************************************************/ 
EXPORT void CALL CaptureScreen ( char * Directory );

/******************************************************************
  Function: ChangeWindow
  Purpose:  to change the window between fullscreen and window 
            mode. If the window was in fullscreen this should 
            change the screen to window mode and vice vesa.
  input:    none
  output:   none
*******************************************************************/ 
EXPORT void CALL ChangeWindow (void);

/******************************************************************
  Function: CloseDLL
  Purpose:  This function is called when the emulator is closing
            down allowing the dll to de-initialise.
  input:    none
  output:   none
*******************************************************************/ 
EXPORT void CALL CloseDLL (void);

/******************************************************************
  Function: DllAbout
  Purpose:  This function is optional function that is provided
            to give further information about the DLL.
  input:    a handle to the window that calls this function
  output:   none
*******************************************************************/ 
EXPORT void CALL DllAbout ( HWND hParent );

/******************************************************************
  Function: DllConfig
  Purpose:  This function is optional function that is provided
            to allow the user to configure the dll
  input:    a handle to the window that calls this function
  output:   none
*******************************************************************/ 
EXPORT void CALL DllConfig ( HWND hParent );

/******************************************************************
  Function: DllTest
  Purpose:  This function is optional function that is provided
            to allow the user to test the dll
  input:    a handle to the window that calls this function
  output:   none
*******************************************************************/ 
EXPORT void CALL DllTest ( HWND hParent );

/******************************************************************
  Function: DrawScreen
  Purpose:  This function is called when the emulator receives a
            WM_PAINT message. This allows the gfx to fit in when
            it is being used in the desktop.
  input:    none
  output:   none
*******************************************************************/ 
EXPORT void CALL DrawScreen (void);

/******************************************************************
  Function: InitiateGFX
  Purpose:  This function is called when the DLL is started to give
            information from the emulator that the n64 graphics
            uses. This is not called from the emulation thread.
  Input:    Gfx_Info is passed to this function which is defined
            above.
  Output:   TRUE on success
            FALSE on failure to initialise
             
  ** note on interrupts **:
  To generate an interrupt set the appropriate bit in MI_INTR_REG
  and then call the function CheckInterrupts to tell the emulator
  that there is a waiting interrupt.
*******************************************************************/ 
EXPORT BOOL CALL InitiateGFX (GFX_INFO Gfx_Info);

/******************************************************************
  Function: MoveScreen
  Purpose:  This function is called in response to the emulator
            receiving a WM_MOVE passing the xpos and ypos passed
            from that message.
  input:    xpos - the x-coordinate of the upper-left corner of the
            client area of the window.
            ypos - y-coordinate of the upper-left corner of the
            client area of the window. 
  output:   none
*******************************************************************/ 
EXPORT void CALL MoveScreen (int xpos, int ypos);

/******************************************************************
  Function: ProcessDList
  Purpose:  This function is called when there is a Dlist to be
            processed. (High level GFX list)
  input:    none
  output:   none
*******************************************************************/ 
EXPORT void CALL ProcessDList(void);

/******************************************************************
  Function: ProcessRDPList
  Purpose:  This function is called when there is a Dlist to be
            processed. (Low level GFX list)
  input:    none
  output:   none
*******************************************************************/ 
EXPORT void CALL ProcessRDPList(void);

/******************************************************************
  Function: RomClosed
  Purpose:  This function is called when a rom is closed.
  input:    none
  output:   none
*******************************************************************/ 
EXPORT void CALL RomClosed (void);

/******************************************************************
  Function: RomOpen
  Purpose:  This function is called when a rom is open. (from the 
            emulation thread)
  input:    none
  output:   none
*******************************************************************/ 
EXPORT void CALL RomOpen (void);

/******************************************************************
  Function: ShowCFB
  Purpose:  Useally once Dlists are started being displayed, cfb is
            ignored. This function tells the dll to start displaying
            them again.
  input:    none
  output:   none
*******************************************************************/ 
EXPORT void CALL ShowCFB (void);

/******************************************************************
  Function: UpdateScreen
  Purpose:  This function is called in response to a vsync of the
            screen were the VI bit in MI_INTR_REG has already been
            set
  input:    none
  output:   none
*******************************************************************/ 
EXPORT void CALL UpdateScreen (void);

/******************************************************************
  Function: ViStatusChanged
  Purpose:  This function is called to notify the dll that the
            ViStatus registers value has been changed.
  input:    none
  output:   none
*******************************************************************/ 
EXPORT void CALL ViStatusChanged (void);

/******************************************************************
  Function: ViWidthChanged
  Purpose:  This function is called to notify the dll that the
            ViWidth registers value has been changed.
  input:    none
  output:   none
*******************************************************************/ 
EXPORT void CALL ViWidthChanged (void);

/******************************************************************
  Function: ReadScreen
  Purpose:  Capture the current screen
  Input:    none
  Output:   dest - 24-bit RGB data
            width - width of image
            height - height of image
 ******************************************************************/
EXPORT void CALL ReadScreen (void **dest, int *width, int *height);

/******************************************************************
   NOTE: THIS HAS BEEN ADDED FOR MUPEN64PLUS AND IS NOT PART OF THE
         ORIGINAL SPEC
  Function: SetConfigDir
  Purpose:  To pass the location where config files should be read/
            written to.
  input:    path to config directory
  output:   none
*******************************************************************/
EXPORT void CALL SetConfigDir( char *configDir );

/******************************************************************
   NOTE: THIS HAS BEEN ADDED FOR MUPEN64PLUS AND IS NOT PART OF THE
         ORIGINAL SPEC
  Function: SetRenderingCallback
  Purpose:  Allows emulator to register a callback function that will
            be called by the graphics plugin just before the the
            frame buffers are swapped.
            This was added as a way for the emulator to draw emulator-
            specific things to the screen, e.g. On-screen display.
  input:    pointer to callback function. The function expects
            to receive the current window width and height.
  output:   none
*******************************************************************/
EXPORT void CALL SetRenderingCallback(void (*callback)());

#if defined(__cplusplus)
}
#endif

#endif

