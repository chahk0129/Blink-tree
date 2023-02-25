.PHONY: all test clean
CXX = g++ -g -O3 -std=c++17 -march=native
CFLAGS =  -Wno-invalid-offsetof -Wno-deprecated-declarations -fno-builtin-malloc -fno-builtin-calloc -fno-builtin-realloc -fno-builtin-free -faligned-new
LDLIBS = -lpthread -latomic 

all: test

test: test.cpp timestamp.cpp range.cpp
	$(CXX) $(CFLAGS) -o bin/test test.cpp $(LDLIBS) 
	$(CXX) $(CFLAGS) -o bin/timestamp timestamp.cpp $(LDLIBS)  
	$(CXX) $(CFLAGS) -o bin/range range.cpp $(LDLIBS) 

clean:
	rm bin/*
