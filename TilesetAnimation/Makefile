# ─── Makefile для Tileset Animation Editor (MSYS2 / MinGW) ───

CC      = gcc
CFLAGS  = -Wall -O2 -std=c11
LDFLAGS = -lmingw32 -lSDL2main -lSDL2 -lSDL2_image -lSDL2_ttf -lcomdlg32 -lm

TARGET  = TilesetAnimation.exe
SOURCES = Tileset_animation.c
OBJECTS = $(SOURCES:.c=.o)

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJECTS) 2>/dev/null || true

run: $(TARGET)
	./$(TARGET)