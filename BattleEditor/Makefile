CC = gcc
CFLAGS = -Wall -std=c99 -O2 -I.. -IC:/msys64/ucrt64/include/SDL2 -DHWY_SHARED_DEFINE -DLIBDEFLATE_DLL -DAVIF_DLL -IC:/msys64/ucrt64/include/rav1e -IC:/msys64/ucrt64/include/svt-av1 -DEB_DLL -DRTC_BUILD=0 -IC:/msys64/ucrt64/include/webp -Dmain=SDL_main -IC:/msys64/ucrt64/include/freetype2 -IC:/msys64/ucrt64/include/libpng16 -IC:/msys64/ucrt64/include/harfbuzz -IC:/msys64/ucrt64/include/glib-2.0 -IC:/msys64/ucrt64/lib/glib-2.0/include
LDFLAGS = -LC:/msys64/ucrt64/lib -lmingw32 -lSDL2main -lSDL2 -lSDL2_image -lSDL2_ttf -Wl,--no-undefined -lm -ldinput8 -ldxguid -ldxerr8 -luser32 -lgdi32 -lwinmm -limm32 -lole32 -loleaut32 -lshell32 -lversion -luuid -static-libgcc

OBJS = battle_editor.o editor.o ../cJSON.o

all: BattleEditor.exe

BattleEditor.exe: $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

clean:
	rm -f *.o BattleEditor.exe