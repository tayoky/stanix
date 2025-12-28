#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <libtrm/func.h>
#include <trm.h>

int main(int argc,char **argv){
	if(argc < 2){
		printf("usage : test-trm VIDEO\n");
		return 1;
	}
	int fd = open(argv[1], O_RDWR);
	if (fd < 0) {
		perror(argv[1]);
		return 1;
	}

	trm_card_t *card = trm_get_ressources(fd);
	printf("card '%s', driver '%s'", card->name, card->driver);
	printf("found %zd planes, %zd crtcs and %zd connectors\n", card->planes_count, card->crtcs_count, card->connectors_count);
	puts("=== PLANES ===");
	for (size_t i=0; i<card->planes_count; i++) {
		trm_plane_t *plane = &card->planes[i];
		printf("plane (%d)\n", plane->id);
	}

	return 0;	
}