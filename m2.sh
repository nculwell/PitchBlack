#!/bin/sh

SDL_PKGS="SDL2 SDL2_image SDL2_mixer"
SDL_CFLAGS=$(pkg-config --static --cflags $SDL_PKGS)
SDL_LIBS=$(pkg-config --static --libs $SDL_PKGS)

echo "CFLAGS: $SDL_CFLAGS"
echo "LIBS: $SDL_LIBS"
gcc -std=gnu99 -Wall -Werror -o PitchBlack $SDL_CFLAGS main.c $SDL_LIBS

