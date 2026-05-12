CC := gcc
CFLAGS := -Wall -std=c99 -O2
CFLAGS += -IC:/msys64/ucrt64/include/SDL2
CFLAGS += -I..
CFLAGS += -Dmain=SDL_main

LDFLAGS := -LC:/msys64/ucrt64/lib
LDFLAGS += -lmingw32 -lSDL2main -lSDL2 -lSDL2_ttf -lSDL2_image -mwindows

TARGET := BattleScenes.exe
SRC := battle_scenes.c ../cJSON.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(TARGET)

.PHONY: all clean