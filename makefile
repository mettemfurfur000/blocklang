CFLAGS += -O0 -Wall -Wpedantic -g #-pg -no-pie
LDFLAGS += -lm -g# -pg

LDFLAGS += -LC:/msys64/mingw64/lib -lmingw32 -lws2_32

# sources := $(shell cd src;echo *.c)
sources_c := $(shell cd src;find . -name '*.c')
sources_cpp := $(shell cd src;find . -name '*.cpp')
sources += $(sources_c) $(sources_cpp)

objects_c := $(patsubst %.c,obj/%.o,$(sources_c))
objects_cpp := $(patsubst %.cpp,obj/%.o,$(sources_cpp))
objects := $(objects_c) $(objects_cpp)
headers := $(shell cd include;echo *.h)

obj/%.o : src/%.c
	gcc $(CFLAGS) -c $^ -o $@

obj/%.o : src/%.cpp
	g++ -std=c++17 $(CFLAGS) -c $^ -o $@

all: $(objects)
	gcc ${CFLAGS} -o build/app $(objects) $(LDFLAGS)
	cp -r programs/* build/

clean:
	rm build/*
	rm obj/*
