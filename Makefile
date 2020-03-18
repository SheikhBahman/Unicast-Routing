
ls: main.c monitor_neighbors.h 
	gcc -pthread -std=gnu11 -o ls_router main.c

.PHONY: clean
clean:
	rm *.o vec_router ls_router
