CXX = g++-10
CXXFLAGS = -std=c++2a -Wall -Wextra -O3
LDFLAGS = -pthread
SRC = $(wildcard *.cpp)
OBJ = $(SRC:.cpp=.o)
TARGET = fuzzy-search-server

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CXX) -o $@ $^ $(LDFLAGS)

%.o: %.cpp
	$(CXX) -c $< $(CXXFLAGS)

cleano:
	rm -f $(OBJ)

clean:
	rm -f $(OBJ) $(TARGET)

.PHONY: all clean cleano
