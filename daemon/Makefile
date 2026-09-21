# Build phoneserverd. Run on the phone inside the Arch chroot (scripts/pc/build-daemon.sh does
# that), which produces a static binary that runs on Android outside the chroot.
CC      ?= gcc
CFLAGS  ?= -O2 -Wall -Wextra
LDFLAGS ?= -static

phoneserverd: phoneserverd.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

# quick check on your own computer: full compile with warnings as errors, output discarded
check: phoneserverd.c
	$(CC) $(CFLAGS) -Werror -c -o /dev/null $<

clean:
	rm -f phoneserverd
.PHONY: check clean
