CC = gcc
CFLAGS = -Wall -std=c99 -O2 -I. -I..   # если cJSON.h в родительской папке
LDFLAGS = -lSDL2 -lSDL2_image -lSDL2_ttf -lmingw32 -mwindows

SOURCES = map_event_editor.c map.c utils.c events.c ../cJSON.c
OBJECTS = $(SOURCES:.c=.o)
EXECUTABLE = map_event_editor.exe

all: $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(EXECUTABLE)