CC = gcc
SRC = constructor.c
TARGET = Constructor.exe
CFLAGS = -Wall -std=c99 -O2 -IC:/msys64/ucrt64/include/SDL2
LDFLAGS = -LC:/msys64/ucrt64/lib -lmingw32 -lSDL2main -lSDL2 -lSDL2_image -lSDL2_ttf -lfreetype -lstdc++ -mwindows

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) -o $@ $< $(CFLAGS) $(LDFLAGS)

clean:
	rm -f $(TARGET)