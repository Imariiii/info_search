CXX = g++
CXXFLAGS = -O3 -std=c++17 -Wall -Iinclude -I../common/include

COMMON_SRC = ../common/src
TARGET = search_engine
OBJS = main.o index.o tokenizer.o stemmer.o search_engine.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(TARGET).out $(OBJS)

main.o: main.cpp
	$(CXX) $(CXXFLAGS) -c main.cpp -o main.o

index.o: src/index.cpp include/index.h
	$(CXX) $(CXXFLAGS) -c src/index.cpp -o index.o

search_engine.o: src/search_engine.cpp include/search_engine.h
	$(CXX) $(CXXFLAGS) -c src/search_engine.cpp -o search_engine.o

tokenizer.o: $(COMMON_SRC)/tokenizer.cpp
	$(CXX) $(CXXFLAGS) -c $(COMMON_SRC)/tokenizer.cpp -o tokenizer.o

stemmer.o: $(COMMON_SRC)/stemmer.cpp
	$(CXX) $(CXXFLAGS) -c $(COMMON_SRC)/stemmer.cpp -o stemmer.o

clean:
	rm -f *.o search_engine
