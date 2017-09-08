CC=cc
FLAGS=-Wall -Wextra -pedantic 
STD=-std=c99
DBUG= -g

ghi: ghi.c
	$(CC) $(FLAGS) ghi.c -o ghi $(STD)
debug: ghi.c
	$(CC) $(FLAGS) ghi.c -o ghi $(STD) $(DBUG)
clean:
	rm -rf ghi
