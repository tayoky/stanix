#include <stdio.h>
#include <twm.h>

int main() {
	twm_init(NULL);
	twm_window_t window = twm_create_window("test window", 250, 250);
	printf("created window with id %d", window);
	for(;;);
}