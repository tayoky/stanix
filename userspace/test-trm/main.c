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


	trm_timings_t timings = {
		.hdisplay = 320,
		.vdisplay = 200,
	};

	trm_card_t *card = trm_get_ressources(fd);
	printf("card '%s', driver '%s'", card->name, card->driver);
	printf("found %zd planes, %zd crtcs and %zd connectors\n", card->planes_count, card->crtcs_count, card->connectors_count);
	printf("has %zdKb of vram\n", card->vram_size / 1024);
	puts("=== PLANES ===");
	for (size_t i=0; i<card->planes_count; i++) {
		trm_plane_t *plane = &card->planes[i];
		if (plane->type != TRM_PLANE_PRIMARY) continue;
		plane->dest_x = 0;
		plane->dest_y = 0;
		plane->dest_w = timings.hdisplay;
		plane->dest_h = timings.vdisplay;
		plane->src_x = 0;
		plane->src_y = 0;
		plane->src_w = timings.hdisplay;
		plane->src_h = timings.vdisplay;
	}

	trm_fb_t *fb = trm_allocate_framebuffer(fd, 320, 200, TRM_C8);
	printf("framebuffer at fd %d\n", fb->fd);

	trm_mode_t mode = {
		.crtcs_count = 1,
		.crtcs = card->crtcs,
		.planes_count = 1,
		.planes = card->planes,
	};
	card->crtcs[0].timings = &timings;
	card->planes[0].fb_id = fb->id;
	if (trm_commit_mode(fd, &mode) < 0){
		perror("commit mode");
	}

	return 0;	
}