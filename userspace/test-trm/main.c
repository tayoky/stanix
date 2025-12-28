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
	printf("has %zdKb of vram\n", card->vram_size / 1024);
	puts("=== PLANES ===");
	for (size_t i=0; i<card->planes_count; i++) {
		trm_plane_t *plane = &card->planes[i];
		printf("plane (%d)\n", plane->id);
	}

	trm_fb_t *fb = trm_allocate_framebuffer(fd, 320, 200, TRM_C8);
	printf("framebuffer at fd %d\n", fb->fd);

	trm_timings_t timings = {
		.hdisplay = 320,
		.vdisplay = 200,
	};
	trm_mode_t mode = {
		.crtcs_count = 1,
		.crtcs = card->crtcs,
		.planes_count = 1,
		.planes = card->planes,
	};
	card->crtcs[0].timings = &timings;
	card->planes[0].fb_id = fb->id;
	trm_commit_mode(fd, &mode);

	return 0;	
}