PKGS = sdl2 sdl2_net

CC = gcc
CFLAGS = -ggdb -std=c99 -Wall -Wextra -Wpedantic -Wno-unused-parameter `pkg-config --cflags $(PKGS)` -mconsole
CPPFLAGS =
LDFLAGS = `pkg-config --libs $(PKGS)` -mconsole
LDLIBS =

SRC	= \
	src/client.c \
	src/data.c \
	src/main.c \
	src/SDL_net_ext.c \
	src/server.c
TARGET = bin/networking

.PHONY: all
all: $(TARGET)

$(TARGET): $(SRC:src/%.c=obj/%.o)
	@mkdir -p $(@D)
	$(CC) $^ -o $@ $(LDFLAGS) $(LDLIBS)

obj/%.o: src/%.c
	@mkdir -p $(@D)
	@mkdir -p $(@D:obj%=dep%)
	$(CC) -c $< -o $@ -MMD -MF $(@:obj/%.o=dep/%.d) $(CFLAGS) $(CPPFLAGS)

-include $(SRC:src/%.c=dep/%.d)

.PHONY: run
run: all
	./$(TARGET) -h

.PHONY: run_client
run_client: all
	./$(TARGET) -c

.PHONY: run_server
run_server: all
	./$(TARGET) -s

.PHONY: clean
clean:
	rm -rf bin obj dep
