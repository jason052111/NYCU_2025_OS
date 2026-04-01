.PHONY: all clean

CC      = riscv64-linux-gnu-gcc
CFLAGS  = -Wall -Wextra -O2
LDFLAGS = -pthread -static

TARGET  = sched_demo_314551147
SRCS    = sched_demo_314551147.c
OBJS    = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJS)

