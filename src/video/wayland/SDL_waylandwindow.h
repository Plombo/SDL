#include "SDL_config.h"

#ifndef _SDL_waylandwindow_h
#define _SDL_waylandwindow_h

#include "../SDL_sysvideo.h"

#include "SDL_waylandvideo.h"

typedef struct
{
	struct wl_surface	*surface;
	struct wl_buffer	*buffer[2];

	EGLImageKHR		image[2];
	GLuint			rbo[2];
	uint32_t		fb_id[2];
	uint32_t		current;

    EGLContext context;
    
} SDL_WaylandWindow;

extern int Wayland_CreateWindow(_THIS, SDL_Window * window);

#endif /* _SDL_waylandwindow_h */

/* vi: set ts=4 sw=4 expandtab: */
