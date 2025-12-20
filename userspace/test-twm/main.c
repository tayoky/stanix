#include <stdio.h>
#include <twm.h>

int main() {
	twm_init(NULL);
	twm_create_window("test window", 250, 250);
	for(;;);
}