# Skeleton Computer networks, Leiden University

SDL2_CONFIG ?= sdl2-config
CC          ?= gcc
CFLAGS       = -Icommon -std=gnu11 -Wall -Wextra -Wpedantic -g $(shell $(SDL2_CONFIG) --cflags)
LDFLAGS      = -lm -pthread $(shell $(SDL2_CONFIG) --libs)

BINDIR  = bin

COMMON_OBJ = \
			asp.o \
			wave.o \
			util.o

CLIENT_OBJ = \
			player.o \
			client.o

SERVER_OBJ = \
			server.o

TARGETS = \
			client		\
			server 		\
			wav_player

COMMON_TARGETS = $(addprefix common/, $(COMMON_OBJ))
CLIENT_TARGETS = $(addprefix client/, $(CLIENT_OBJ))
SERVER_TARGETS = $(addprefix server/, $(SERVER_OBJ))

all: 		$(TARGETS)

client: 	$(COMMON_TARGETS) $(CLIENT_TARGETS)
			@mkdir -p $(BINDIR)
			$(CC) $(CFLAGS) -o $(BINDIR)/$@ $^ $(LDFLAGS)

server: 	$(COMMON_TARGETS) $(SERVER_TARGETS)
			@mkdir -p $(BINDIR)
			$(CC) $(CFLAGS) -o $(BINDIR)/$@ $^ $(LDFLAGS)

wav_player:	common/wave.o client/player.o client/wav_player.o
			@mkdir -p $(BINDIR)
			$(CC) $(CFLAGS) -o $(BINDIR)/$@ $^ $(LDFLAGS)

%.o: 		%.c
			$(CC) $(CFLAGS) -c -o $@ $<

clean:
			rm -f $(addprefix $(BINDIR)/,$(TARGETS)) $(COMMON_TARGETS) $(SERVER_TARGETS) $(CLIENT_TARGETS) client/wav_player.o

.PHONY: 	clean
