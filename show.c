#include <stdio.h>
#include "buddy.h"

int main(int argc, char * argv[]) {
	(void)argc;
	(void)argv;
	
	buddyInit(330);
	buddyDump();
	
	int * array = (int *)buddyMalloc(5 * sizeof(int));
	array[0] = 0xBB;
	array[1] = 0xCC;
	array[2] = 0xDD;
	array[3] = 0xEE;
	array[4] = 0xFF;
	printf("-- after malloc ---\n");
	buddyDump();
	printf("-- free --\n");
	buddyFree(array);
	buddyDump();
	
	
	buddyDestroy();
	return 0;
}
