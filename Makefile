test: test.c json.h json.c json_write.c json_parser.c
	gcc -o test -Wall test.c json.c json_write.c json_parser.c

.PHONY: clean

clean:
	rm -f *.o test