CC = gcc
CFLAGS = -Wall -std=c99 -O2 -I.. $(shell pkg-config --cflags sdl2 SDL2_image SDL2_ttf)
LDFLAGS = -lmingw32 -lSDL2main $(shell pkg-config --libs sdl2 SDL2_image SDL2_ttf)

TARGET = MapEditor.exe
SRCS = map_editor.c clipboard.c ../cJSON.c
OBJS = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

clean:
	rm -f $(OBJS) $(TARGET)

rebuild: clean all

.PHONY: all clean rebuild