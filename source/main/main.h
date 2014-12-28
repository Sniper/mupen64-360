/***************************************************************************
                          main.h  -  description
                             -------------------
    begin                : Mon 23 Mai 2005
    copyright            : (C) 2005 by jogibear
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef __MAIN_H__
#define __MAIN_H__

#define USE_THREADED_AUDIO

#define MUPEN_DIR "uda0:/mupen64-360/"

#ifndef PATH_MAX
#  define PATH_MAX 1024
#endif

#include "api/m64p_types.h"

/* globals */
extern m64p_handle g_CoreConfig;

extern int g_MemHasBeenBSwapped;
extern int g_EmulatorRunning;

extern m64p_frame_callback g_FrameCallback;

const char* get_savestatepath(void);
const char* get_savesrampath(void);
void main_message(m64p_msg_level level, unsigned int osd_corner, const char *format, ...);

void new_frame(void);
void new_vi(void);
extern int use_framelimit;
extern int use_expansion;
extern int pad_mode;
extern char g_WorkingDir[];

extern char a_mode;
extern char b_mode;
extern char x_mode;
extern char y_mode;
extern char lt_mode;
extern char rt_mode;
extern char lb_mode;
extern char rb_mode;

#endif // __MAIN_H__
