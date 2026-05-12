CC      = gcc
CFLAGS  = -I/ucrt64/include/SDL2 -Wall -std=c99 -O2
LDFLAGS = -L/ucrt64/lib -lSDL2main -lSDL2 -lSDL2_image -lSDL2_ttf -lcomdlg32

text_editor.exe: text_editor.o ../cJSON.o
	$(CC) -o $@ $^ $(LDFLAGS)

text_editor.o: text_editor.c
	$(CC) $(CFLAGS) -c $< -o $@

../cJSON.o: ../cJSON.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f text_editor.exe text_editor.o ../cJSON.o