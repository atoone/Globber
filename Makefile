# Define the target binary
TARGET = globber

# Define the C++ source files
SOURCES = globber.cpp

# Define the compiler
CC = g++

# Define compiler flags
CFLAGS = -std=c++17 -Wall

# Define the object files
OBJECTS = $(SOURCES:.cpp=.o)

# Define the default target
all: $(TARGET)

# Compile the source files into object files
%.o: %.cpp
	$(CC) $(CFLAGS) -c $< -o $@

# Link the object files into the target binary
$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) -o $@

# Clean up object files and the target binary
clean:
	rm -f $(OBJECTS) $(TARGET)

.PHONY: all clean