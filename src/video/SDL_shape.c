/*
    SDL - Simple DirectMedia Layer
    Copyright (C) 2010 Eli Gottlieb

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

    Eli Gottlieb
    eligottlieb@gmail.com
*/
#include "SDL_config.h"

#include "SDL.h"
#include "SDL_video.h"
#include "SDL_sysvideo.h"
#include "SDL_pixels.h"
#include "SDL_surface.h"
#include "SDL_shape.h"
#include "../src/video/SDL_shape_internals.h"

SDL_Window* SDL_CreateShapedWindow(const char *title,unsigned int x,unsigned int y,unsigned int w,unsigned int h,Uint32 flags) {
	SDL_Window *result = SDL_CreateWindow(title,x,y,w,h,SDL_WINDOW_BORDERLESS | flags & !SDL_WINDOW_FULLSCREEN & !SDL_WINDOW_SHOWN);
	if(result != NULL) {
		result->shaper = result->display->device->shape_driver.CreateShaper(result);
		if(result->shaper != NULL) {
			result->shaper->usershownflag = flags & SDL_WINDOW_SHOWN;
			result->shaper->mode.mode = ShapeModeDefault;
			result->shaper->mode.parameters.binarizationCutoff = 1;
			result->shaper->hasshape = SDL_FALSE;
			return result;
		}
		else {
			SDL_DestroyWindow(result);
			return NULL;
		}
	}
	else
		return NULL;
}

SDL_bool SDL_IsShapedWindow(const SDL_Window *window) {
	if(window == NULL)
		return SDL_FALSE;
	else
		return (SDL_bool)(window->shaper != NULL);
}

/* REQUIRES that bitmap point to a w-by-h bitmap with ppb pixels-per-byte. */
void SDL_CalculateShapeBitmap(SDL_WindowShapeMode mode,SDL_Surface *shape,Uint8* bitmap,Uint8 ppb) {
	int x = 0;
	int y = 0;
	Uint8 r = 0,g = 0,b = 0,alpha = 0;
	Uint8* pixel = NULL;
	Uint32 bitmap_pixel,pixel_value = 0,mask_value = 0;
	SDL_Color key;
	if(SDL_MUSTLOCK(shape))
		SDL_LockSurface(shape);
	pixel = (Uint8*)shape->pixels;
	for(y = 0;y<shape->h;y++) {
		for(x=0;x<shape->w;x++) {
			alpha = 0;
			pixel_value = 0;
			pixel = (Uint8 *)(shape->pixels) + (y*shape->pitch) + (x*shape->format->BytesPerPixel);
			switch(shape->format->BytesPerPixel) {
				case(1):
					pixel_value = *(Uint8*)pixel;
					break;
				case(2):
					pixel_value = *(Uint16*)pixel;
					break;
				case(3):
					pixel_value = *(Uint32*)pixel & (~shape->format->Amask);
					break;
				case(4):
					pixel_value = *(Uint32*)pixel;
					break;
			}
			SDL_GetRGBA(pixel_value,shape->format,&r,&g,&b,&alpha);
			bitmap_pixel = y*shape->w + x;
			switch(mode.mode) {
				case(ShapeModeDefault):
					mask_value = (alpha >= 1 ? 1 : 0);
					break;
				case(ShapeModeBinarizeAlpha):
					mask_value = (alpha >= mode.parameters.binarizationCutoff ? 1 : 0);
					break;
				case(ShapeModeReverseBinarizeAlpha):
					mask_value = (alpha <= mode.parameters.binarizationCutoff ? 1 : 0);
					break;
				case(ShapeModeColorKey):
					key = mode.parameters.colorKey;
					mask_value = ((key.r != r && key.g != g && key.b != b) ? 1 : 0);
					break;
			}
			bitmap[bitmap_pixel / ppb] |= mask_value << (7 - ((ppb - 1) - (bitmap_pixel % ppb)));
		}
	}
	if(SDL_MUSTLOCK(shape))
		SDL_UnlockSurface(shape);
}

SDL_ShapeTree* RecursivelyCalculateShapeTree(SDL_WindowShapeMode mode,SDL_Surface* mask,SDL_bool invert,SDL_Rect dimensions) {
	int x = 0,y = 0;
	Uint8* pixel = NULL;
	Uint32 pixel_value = 0;
	Uint8 r = 0,g = 0,b = 0,a = 0;
	SDL_bool pixel_transparent = SDL_FALSE;
	int last_transparent = -1;
	SDL_Color key;
	SDL_ShapeTree* result = (SDL_ShapeTree*)SDL_malloc(sizeof(SDL_ShapeTree));
	SDL_Rect next = {0,0,0,0};
	for(y=dimensions.y;y<dimensions.h;y++)
		for(x=dimensions.x;x<dimensions.w;x++) {
			pixel_value = 0;
			pixel = (Uint8 *)(mask->pixels) + (y*mask->pitch) + (x*mask->format->BytesPerPixel);
			switch(mask->format->BytesPerPixel) {
				case(1):
					pixel_value = *(Uint8*)pixel;
					break;
				case(2):
					pixel_value = *(Uint16*)pixel;
					break;
				case(3):
					pixel_value = *(Uint32*)pixel & (~mask->format->Amask);
					break;
				case(4):
					pixel_value = *(Uint32*)pixel;
					break;
			}
			SDL_GetRGBA(pixel_value,mask->format,&r,&g,&b,&a);
			switch(mode.mode) {
				case(ShapeModeDefault):
					pixel_transparent = (SDL_bool)(a >= 1 ? !invert : invert);
					break;
				case(ShapeModeBinarizeAlpha):
					pixel_transparent = (SDL_bool)(a >= mode.parameters.binarizationCutoff ? !invert : invert);
					break;
				case(ShapeModeReverseBinarizeAlpha):
					pixel_transparent = (SDL_bool)(a <= mode.parameters.binarizationCutoff ? !invert : invert);
					break;
				case(ShapeModeColorKey):
					key = mode.parameters.colorKey;
					pixel_transparent = (SDL_bool)((key.r == r && key.g == g && key.b == b) ? !invert : invert);
					break;
			}
			if(last_transparent == -1) {
				last_transparent = pixel_transparent;
				break;
			}
			if(last_transparent != pixel_transparent) {
				result->kind = QuadShape;
				//These will stay the same.
				next.w = dimensions.w / 2;
				next.h = dimensions.h / 2;
				//These will change from recursion to recursion.
				next.x = dimensions.x;
				next.y = dimensions.y;
				result->data.children.upleft = (struct SDL_ShapeTree *)RecursivelyCalculateShapeTree(mode,mask,invert,next);
				next.x = dimensions.w / 2 + 1;
				//Unneeded: next.y = dimensions.y;
				result->data.children.upright = (struct SDL_ShapeTree *)RecursivelyCalculateShapeTree(mode,mask,invert,next);
				next.x = dimensions.x;
				next.y = dimensions.h / 2 + 1;
				result->data.children.downleft = (struct SDL_ShapeTree *)RecursivelyCalculateShapeTree(mode,mask,invert,next);
				next.x = dimensions.w / 2 + 1;
				//Unneeded: next.y = dimensions.h / 2 + 1;
				result->data.children.downright = (struct SDL_ShapeTree *)RecursivelyCalculateShapeTree(mode,mask,invert,next);
				return result;
			}
		}
	//If we never recursed, all the pixels in this quadrant have the same "value".
	result->kind = (last_transparent == SDL_FALSE ? OpaqueShape : TransparentShape);
	result->data.shape = dimensions;
	return result;
}

SDL_ShapeTree* SDL_CalculateShapeTree(SDL_WindowShapeMode mode,SDL_Surface* shape,SDL_bool invert) {
	SDL_Rect dimensions = {0,0,shape->w,shape->h};
	SDL_ShapeTree* result = NULL;
	if(SDL_MUSTLOCK(shape))
		SDL_LockSurface(shape);
	result = RecursivelyCalculateShapeTree(mode,shape,invert,dimensions);
	if(SDL_MUSTLOCK(shape))
		SDL_UnlockSurface(shape);
	return result;
}

void SDL_TraverseShapeTree(SDL_ShapeTree *tree,void(*function)(SDL_ShapeTree*,void*),void* closure) {
	if(tree->kind == QuadShape) {
		SDL_TraverseShapeTree((SDL_ShapeTree *)tree->data.children.upleft,function,closure);
		SDL_TraverseShapeTree((SDL_ShapeTree *)tree->data.children.upright,function,closure);
		SDL_TraverseShapeTree((SDL_ShapeTree *)tree->data.children.downleft,function,closure);
		SDL_TraverseShapeTree((SDL_ShapeTree *)tree->data.children.downright,function,closure);
	}
	else
		function(tree,closure);
}

void SDL_FreeShapeTree(SDL_ShapeTree** shapeTree) {
	if((*shapeTree)->kind == QuadShape) {
		SDL_FreeShapeTree((SDL_ShapeTree **)&(*shapeTree)->data.children.upleft);
		SDL_FreeShapeTree((SDL_ShapeTree **)&(*shapeTree)->data.children.upright);
		SDL_FreeShapeTree((SDL_ShapeTree **)&(*shapeTree)->data.children.downleft);
		SDL_FreeShapeTree((SDL_ShapeTree **)&(*shapeTree)->data.children.downright);
	}
	SDL_free(*shapeTree);
	*shapeTree = NULL;
}

int SDL_SetWindowShape(SDL_Window *window,SDL_Surface *shape,SDL_WindowShapeMode *shapeMode) {
	int result;
	if(window == NULL || !SDL_IsShapedWindow(window))
		//The window given was not a shapeable window.
		return SDL_NONSHAPEABLE_WINDOW;
	if(shape == NULL)
		//Invalid shape argument.
		return SDL_INVALID_SHAPE_ARGUMENT;
	
	if(shapeMode != NULL)
		window->shaper->mode = *shapeMode;
	//TODO: Platform-specific implementations of SetWindowShape.  X11 is finished.  Win32 is finished.  Debugging is in progress on both.
	result = window->display->device->shape_driver.SetWindowShape(window->shaper,shape,shapeMode);
	window->shaper->hasshape = SDL_TRUE;
	if((window->shaper->usershownflag & SDL_WINDOW_SHOWN) == SDL_WINDOW_SHOWN) {
		SDL_ShowWindow(window);
		window->shaper->usershownflag &= !SDL_WINDOW_SHOWN;
	}
	return result;
}

SDL_bool SDL_WindowHasAShape(SDL_Window *window) {
	if (window == NULL && !SDL_IsShapedWindow(window))
		return SDL_FALSE;
	return window->shaper->hasshape;
}

int SDL_GetShapedWindowMode(SDL_Window *window,SDL_WindowShapeMode *shapeMode) {
	if(window != NULL && SDL_IsShapedWindow(window)) {
		if(shapeMode == NULL) {
			if(SDL_WindowHasAShape(window))
				//The window given has a shape.
				return 0;
			else
				//The window given is shapeable but lacks a shape.
				return SDL_WINDOW_LACKS_SHAPE;
		}
		else {
			*shapeMode = window->shaper->mode;
			return 0;
		}
	}
	else
		//The window given is not a valid shapeable window.
		return SDL_NONSHAPEABLE_WINDOW;
}