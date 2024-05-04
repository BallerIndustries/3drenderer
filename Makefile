build:
	gcc -Wall -std=c99 ./src/*.c -lSDL2 -lm -o renderer -I include -L lib

run:
	./renderer

clean:
	rm renderer
