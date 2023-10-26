SRC = $(wildcard *.c *.S)
OBJ = $(SRC:.c=.o)

main: $(OBJ)
	gcc $^ -o $@ -g

%.o: %.c
	gcc -c $< -g

clean:
	rm *.o main
