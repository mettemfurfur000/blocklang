CFLAGS += -O0 -Wall -Wpedantic -fanalyzer -g -pg -no-pie
LDFLAGS += -lm -g -pg

LDFLAGS += -LC:/msys64/mingw64/lib -lmingw32 -lws2_32

# sources := $(shell cd src;echo *.c)
sources_c := $(shell cd src;find . -name '*.c')
sources_cpp := $(shell cd src;find . -name '*.cpp')
sources += $(sources_c) $(sources_cpp)

objects_c := $(patsubst %.c,obj/%.o,$(sources_c))
objects_cpp := $(patsubst %.cpp,obj/%.o,$(sources_cpp))
objects := $(objects_c) $(objects_cpp)
headers := $(shell cd include;echo *.h)

all: assembler singleblock blocklang

obj/main_%.o : mains/%.c
	gcc $(CFLAGS) -c $^ -o $@

obj/%.o : src/%.c
	gcc $(CFLAGS) -c $^ -o $@

obj/%.o : src/%.cpp
	g++ -std=c++17 $(CFLAGS) -c $^ -o $@

test: $(objects) obj/main_test.o
	gcc ${CFLAGS} -o build/test_app $^ $(LDFLAGS)

assembler: $(objects) obj/main_assembler.o
	gcc ${CFLAGS} -o build/basm $^ $(LDFLAGS)
	cp -r example_blocklang/* build/
	cp -r asm_programs/* build/

singleblock: $(objects) obj/main_singleblock.o
	cc ${CFLAGS} -o build/block $^ $(LDFLAGS)

blocklang: $(objects) obj/main_blocklang.o
	gcc ${CFLAGS} -o build/blocklang $^ $(LDFLAGS)

clean:
	rm build/*
	rm obj/*
