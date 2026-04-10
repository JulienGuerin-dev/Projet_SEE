CC = gcc
CFLAGS = -Wall -Wextra

all: ultrason_control

ultrason_control: ultrason_control.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f ultrason_control
