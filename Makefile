hp:
	@echo " Compile hp_main ...";
	rm -rf data.db
	gcc -g -I ./include/ -L ./lib/ -Wl,-rpath,./lib/ ./examples/hp_main.c ./src/heap_file.c -lbf -o ./build/runner -O2

bf:
	@echo " Compile bf_main ...";
	rm -rf data.db
	gcc -g -I ./include/ -L ./lib/ -Wl,-rpath,./lib/ ./examples/bf_main.c -lbf -o ./build/runner -O2
