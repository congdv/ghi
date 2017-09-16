CC=cc
FLAGS=-Wall -Wextra -pedantic 
STD=-std=c99
DBUG= -g

.PHONY: ghi debug clean
ghi: ghi.c
	$(CC) $(FLAGS) ghi.c unicode.c -o ghi $(STD) $(DBUG)
debug: ghi.c
	$(CC) $(FLAGS) ghi.c unicode.c -o ghi $(STD) $(DBUG)
clean:
	rm -rf ghi
