/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2013 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/

/**
 *  \file SDL_vertex.h
 *  
 *  Header file for SDL_vertex definition and management functions.
 */

#ifndef _SDL_vertex_h
#define _SDL_vertex_h

#include "SDL_pixels.h"

#include "begin_code.h"
/* Set up for C function definitions, even when using C++ */
#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

typedef struct
{
    float x;
    float y;
} SDL_Vector2f;

/**
 *  \brief  The structure that defines a vertex
 *
 */
typedef struct
{
    SDL_Vector2f position;
    SDL_Color color;
    SDL_Vector2f tex_coord;
} SDL_Vertex;

/* Ends C function definitions when using C++ */
#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif
#include "close_code.h"

#endif /* _SDL_vertex_h */

/* vi: set ts=4 sw=4 expandtab: */
