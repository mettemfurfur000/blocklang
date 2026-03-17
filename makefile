CFLAGS += -O0 -Wall -Wpedantic -fanalyzer -g -no-pie
LDFLAGS += -lm -g

CC := /c/msys64/mingw64/bin/gcc.exe
CXX := /c/msys64/mingw64/bin/g++.exe

LDFLAGS += -LC:/msys64/mingw64/lib -lmingw32 -lws2_32
LDFLAGS += -lcjson

# sources := $(shell cd src;echo *.c)
sources_c := $(shell cd src;find . -name '*.c')
sources_cpp := $(shell cd src;find . -name '*.cpp')
sources += $(sources_c) $(sources_cpp)

objects_c := $(patsubst %.c,obj/%.o,$(sources_c))
objects_cpp := $(patsubst %.cpp,obj/%.o,$(sources_cpp))
objects := $(objects_c) $(objects_cpp)
headers := $(shell cd include;echo *.h)

all: assembler singleblock blocklang test

obj/main_%.o : mains/%.c
	$(CC) $(CFLAGS) -c $^ -o $@

obj/%.o : src/%.c
	$(CC) $(CFLAGS) -c $^ -o $@

obj/%.o : src/%.cpp
	$(CXX) -std=c++17 $(CFLAGS) -c $^ -o $@

test: $(objects) obj/main_test.o
	$(CC) ${CFLAGS} -o build/test_app $^ $(LDFLAGS)

assembler: $(objects) obj/main_assembler.o
	$(CC) ${CFLAGS} -o build/basm $^ $(LDFLAGS)
	cp -r programs build/

singleblock: $(objects) obj/main_singleblock.o
	$(CC) ${CFLAGS} -o build/block $^ $(LDFLAGS)

blocklang: $(objects) obj/main_blocklang.o
	$(CC) ${CFLAGS} -o build/blocklang $^ $(LDFLAGS)

codegen_test: $(objects) obj/main_codegen_test.o
	$(CC) ${CFLAGS} -o build/codegen_test $^ $(LDFLAGS)

clean:
	rm -rf build/*
	rm -rf obj/*
