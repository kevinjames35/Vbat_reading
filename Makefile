TESTCFLAGS=-g -Wall -Werror
TESTLINKER=-lcunit
TESTSOURCES=Vbat_reading.c
TESTBINARY=Vbat_reading

.FORCE: test
all:
	make test

clean:
	rm -rf *.o $(TESTBINARY)

test:
	$(CC) $(TESTCFLAGS) -o $(TESTBINARY) $(TESTSOURCES) -lpthread -lm
