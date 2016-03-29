#!/bin/sh

SDL_DIR="SDL2-2.0.4/x86_64-w64-mingw32"
SDL_INC="$SDL_DIR/include/SDL2"
SDL_LIB="$SDL_DIR/lib/libSDL2.a"

gcc -m32 -std=gnu99 -o PitchBlack \
	-I$SDL_INC $SDL_LIB \
	main.c

