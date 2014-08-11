test: test.c json.h json.c json_write.c json_parser.c json_misc.c
	gcc -o test -Wall test.c json.c json_write.c json_parser.c json_misc.c

.PHONY: clean

clean:
	rm -f *.o test