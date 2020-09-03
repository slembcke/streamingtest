# CFLAGS = -g -O0
CFLAGS = -O3
BLOCK_SIZE = 65536
# BLOCK_SIZE = 262144

default:

debug: streamtest
	gdb -q streamtest

test: streamtest
	gamemoderun ./streamtest

streamtest: streamtest.o tinycthread.o
	cc -o $@ -pthread $^ /usr/lib/x86_64-linux-gnu/liblz4.a

clean:
	-rm *.o streamtest

clean-data:
	-rm data01 data03 data06 data09 data12 data15 data.h

data01:
	# head -c 1024 /usr/share/dict/words > $@
	head -c $(BLOCK_SIZE) /usr/share/dict/words | lz4 -c --best --favor-decSpeed > $@

data03: data01
	cat $< $< $< $< $< $< $< $< > $@

data06: data03
	cat $< $< $< $< $< $< $< $< > $@

data09: data06
	cat $< $< $< $< $< $< $< $< > $@

data12: data09
	cat $< $< $< $< $< $< $< $< > $@

data15: data12
	cat $< $< $< $< $< $< $< $< > $@

data.h: data.h.m4 data15
	m4 -D BLOCK_SIZE=$(BLOCK_SIZE) $< > $@

streamtest.o: data.h
