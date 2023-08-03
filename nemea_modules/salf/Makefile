CC = gcc

  # compiler flags:
  #  -g    adds debugging information to the executable file
  #  -Wall turns on most, but not all, compiler warnings
CFLAGS  = -g -Wall -lunirec -ltrap -lnemea-common

  # the build target executable:
TARGET = salf

all: $(TARGET)

$(TARGET):
	$(CC) $(TARGET).c $(CFLAGS) -o $(TARGET)

clean:
	$(RM) $(TARGET)