#If you use threads, add -pthread here.
COMPILERFLAGS = -g -Wall -Wextra -Wno-sign-compare

#Any libraries you might need linked in.
#LINKLIBS = -lm

# C++ objects
CPP_SERVEROBJECTS = obj/http_server.o
CPP_CLIENTOBJECTS = obj/http_client.o

#Every rule listed here as .PHONY is "phony": when you say you want that rule satisfied,
#Make knows not to bother checking whether the file exists, it just runs the recipes regardless.
#(Usually used for rules whose targets are conceptual, rather than real files, such as 'clean'.
#If you DIDNT mark clean phony, then if there is a file named 'clean' in your directory, running
#`make clean` would do nothing!!!)
.PHONY: all clean

#The first rule in the Makefile is the default (the one chosen by plain `make`).
#Since 'all' is first in this file, both `make all` and `make` do the same thing.
#(`make obj server client talker listener` would also have the same effect).
all : obj http_server http_client

# C++ targets
http_server: $(CPP_SERVEROBJECTS)
	$(CXX) $(COMPILERFLAGS) $^ -o $@ $(LINKLIBS)

http_client: $(CPP_CLIENTOBJECTS)
	$(CXX) $(COMPILERFLAGS) $^ -o $@ $(LINKLIBS)

#RM is a built-in variable that defaults to "rm -f".
clean:
	$(RM) obj/*.o http_server http_client output

# C++ object rule
obj/%.o: src/%.cpp
	$(CXX) $(COMPILERFLAGS) -c -o $@ $<

obj:
	mkdir -p obj