# We must compile in 32-bit mode to avoid generating 64-bit addresses

OBJECTS=machine.o loader.o stubs.o stub_glue.o map.o symtab.o hardware.o dyld_cache.o


armulator: $(OBJECTS)
	gcc -g -m32 -Wall $(OBJECTS) -o $@ -L/opt/local/lib 

%.o:	%.c
	gcc -Wall -g -m32 -c $< -o $@ -I/opt/local/include


stub_glue.c: stubs.c
	@echo "Rebuilding stub glue"
	@echo '#include "stub_glue.h"\n' > $@
	@grep "^__stub" $< | sed -e "s@__stub \([^()]*\).*@extern uint32_t \1();@g" >> $@
	@echo 'BEGIN_STUBS' >> $@
	@grep "^__stub" $< | sed -e "s@__stub \([^()]*\).*@STUB(\1);@g" >> $@
	@echo 'END_STUBS\n' >> $@


clean:
	rm -f stub_glue.c
	rm -f *.o
	rm -f armulator
