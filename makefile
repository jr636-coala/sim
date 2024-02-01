LIBS = -lSDL2 -lSDL2main
CXXFLAGS += -O2 -std=c++20 -g $(LIBS) -I/opt/homebrew/include -Iinclude -L/opt/homebrew/lib

all: main

main: main.cpp gridrenderer.hpp boltzmann.hpp
	$(CXX) main.cpp -o main $(CXXFLAGS)

clean:
	rm -drf main main.dSYM
