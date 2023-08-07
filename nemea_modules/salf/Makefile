CC = gcc

  # compiler flags:
CFLAGS  =-Wall -lunirec -ltrap -lnemea-common -lm

  # the build target executable:
TARGET = salf

all: $(TARGET)

$(TARGET):
	$(CC) $(TARGET).c $(CFLAGS) -o $(TARGET)

clean:
	$(RM) $(TARGET)