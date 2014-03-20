LDFLAGS = -luv -ljson -ljson-c

EXEC = marionette

build: 
	$(CC) -o $(EXEC) marionette.h marionette.c request.c json_parser.c commons.c spawn_process.c $(LDFLAGS)

clean:
	rm -Rf *.o *.gch $(EXEC)

run:
	$(MAKE) build
	./$(EXEC)
