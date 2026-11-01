CC = gcc
CFLAGS = -Wall -std=c99 -O2 -I.. $(shell pkg-config --cflags sdl2 SDL2_image SDL2_ttf)
LDFLAGS = -lmingw32 -lSDL2main $(shell pkg-config --libs sdl2 SDL2_image SDL2_ttf)

TARGET = Database.exe
SRCS = DatabaseEditor.c items_editor.c actors_editor.c classes_editor.c formatter.c ../cJSON.c enemies_editor.c system_editor.c
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