CXX = g++
CXXFLAGS = -O3 -std=c++17 -Wall -I../common/include

COMMON_SRC = ../common/src
TARGET = tokenizer
OBJS = main.o tokenizer.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(TARGET).out $(OBJS)

main.o: main.cpp
	$(CXX) $(CXXFLAGS) -c main.cpp -o main.o

tokenizer.o: $(COMMON_SRC)/tokenizer.cpp
	$(CXX) $(CXXFLAGS) -c $(COMMON_SRC)/tokenizer.cpp -o tokenizer.o

clean:
	rm -f $(OBJS) $(TARGET)
