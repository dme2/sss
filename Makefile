CC=g++
LIBS= -lm
LDFLAGS= --std=c++20

all: simple_sound_system

simple_sound_system:
	$(CC) $(LDFLAGS) $(LIBS) src/main.cc -o $@ -ggdb -g -framework CoreAudio -framework AudioUnit -framework CoreFoundation

#sss_threads.o: src/sss_threads.cc
#	$(CC) $(LDFLAGS) $(LIBS) src/sss_threads.cc  -fext-numeric-literals -c

test:
	$(CC) $(LDFLAGS) $(LIBS) tests/file_test.cc -o file_io -ggdb -g
	$(CC) $(LDFLAGS) $(LIBS) tests/input_test.cc -o input

clean:
	rm simple_sound_system
clean-test:
	rm file_io input

.PHONY: all clean
