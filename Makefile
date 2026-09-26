CC      = gcc
CFLAGS  = -O3 -march=native -fwrapv -Wall -Ilib
LDFLAGS = -lm -lpthread

CORE_SRCS = lib/generator.c lib/layers.c lib/biomenoise.c lib/biomes.c lib/noise.c lib/terrainnoise.c
CORE_OBJS = $(CORE_SRCS:.c=.o)

all: measure

measure: measure.c $(CORE_OBJS)
	$(CC) $(CFLAGS) measure.c $(CORE_OBJS) -o measure $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f measure measure.exe $(CORE_OBJS)

.PHONY: all clean