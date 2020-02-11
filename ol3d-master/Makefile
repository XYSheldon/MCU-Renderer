# Set compiler
CC = g++

#Compile flags (-c to output object file .o)
CFLAGS = -c -Wall -Wextra -Wpedantic -std=c++2a

all: main.exe

debug: CFLAGS += -DDEBUG -g
debug: main.exe 

main.exe: main.o ol3d_core.o ol3d_vector.o ol3d_matrix.o ol3d_render.o
	$(CC) main.o ol3d_core.o ol3d_vector.o ol3d_matrix.o ol3d_render.o -o main.exe

main.o: main.cpp
	$(CC) $(CFLAGS) main.cpp

ol3d_core.o: ol3d_core.cpp
	$(CC) $(CFLAGS) ol3d_core.cpp

ol3d_vector.o: ol3d_vector.cpp
	$(CC) $(CFLAGS) ol3d_vector.cpp

ol3d_matrix.o: ol3d_matrix.cpp
	$(CC) $(CFLAGS) ol3d_matrix.cpp

ol3d_render.o: ol3d_render.cpp
	$(CC) $(CFLAGS) ol3d_render.cpp
clean:
	del *.o main.exe