CC = gcc
CFLAGS = -Wall -std=c99 -O2 -I. -I../ -IC:/msys64/ucrt64/include/SDL2 -DHWY_SHARED_DEFINE -DLIBDEFLATE_DLL -DAVIF_DLL -IC:/msys64/ucrt64/include/rav1e -IC:/msys64/ucrt64/include/svt-av1 -DEB_DLL -DRTC_BUILD=0 -IC:/msys64/ucrt64/include/webp -Dmain=SDL_main -IC:/msys64/ucrt64/include/freetype2 -IC:/msys64/ucrt64/include/libpng16 -IC:/msys64/ucrt64/include/harfbuzz -IC:/msys64/ucrt64/include/glib-2.0 -IC:/msys64/ucrt64/lib/glib-2.0/include

LFLAGS = -lmingw32 -lSDL2main -lSDL2 -lSDL2_image -lSDL2_ttf -lmingw32 -mwindows -lshell32

all: MapCreator.exe

MapCreator.exe: map_creator.o ../cJSON.o
	$(CC) -o $@ $^ $(LFLAGS)

map_creator.o: map_creator.c stb_image_write.h ../cJSON.h
	$(CC) -c -o $@ $< $(CFLAGS)

../cJSON.o: ../cJSON.c ../cJSON.h
	cd .. && $(CC) -c -o cJSON.o cJSON.c $(CFLAGS) -I.

clean:
	rm -f map_creator.o ../cJSON.o MapCreator.exe