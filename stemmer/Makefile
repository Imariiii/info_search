CXX = g++
CXXFLAGS = -O3 -std=c++17 -Wall -I../common/include

COMMON_SRC = ../common/src
TARGET = stemmer
OBJS = main.o stemmer.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(TARGET).out $(OBJS)

main.o: main.cpp
	$(CXX) $(CXXFLAGS) -c main.cpp -o main.o

stemmer.o: $(COMMON_SRC)/stemmer.cpp
	$(CXX) $(CXXFLAGS) -c $(COMMON_SRC)/stemmer.cpp -o stemmer.o

clean:
	rm -f $(OBJS) $(TARGET)
