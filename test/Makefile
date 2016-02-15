
export CFLAGS += -std=c99 -Wall -Wextra

test: test.o
	$(CC) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -I.. -c $< -o $@

.PHONY: clean
clean:
	rm -f test test.o

