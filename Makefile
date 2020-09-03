default:

test: streamtest
	./streamtest

streamtest: streamtest.o tinycthread.o
	cc -o $@ -pthread $^

clean:
	-rm data01 data03 data06 data09 data12 data 15 data.h streamtest

data01:
	# head -c 1024 /usr/share/dict/words > $@
	head -c 65536 /usr/share/dict/words | lz4 -c --best --favor-decSpeed > $@

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
	m4 $< > $@

streamtest.o: data.h
