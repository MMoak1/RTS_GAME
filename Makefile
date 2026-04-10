CXX = g++
NVCC = nvcc
CFLAGS = -Wall -Wextra -O2 -std=c++11 -I./raylib/src -I./raylib/build/raylib/include
NVFLAGS = -O3
LDFLAGS = -L./raylib/build/raylib -lraylib -lGL -lm -lpthread -ldl -lrt -lX11

SRC_CPP = main.cpp ai.cpp
SRC_CU = game.cu
OBJ = $(SRC_CPP:.cpp=.o) $(SRC_CU:.cu=.o)
EXEC = rts_game

all: $(EXEC)

$(EXEC): $(OBJ)
	$(NVCC) -o $@ $^ $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CFLAGS) -c $< -o $@

%.o: %.cu
	$(NVCC) $(NVFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(EXEC)

.PHONY: all clean
