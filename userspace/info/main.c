#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/block.h>
#include <libinput.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <trm.h>

void help(void){
	puts("info DEVICE");
	puts("show information about a device");
}

const char *trm_plane_type(uint32_t type) {
	switch (type) {
	case TRM_PLANE_PRIMARY:
		return "primary plane";
	case TRM_PLANE_CURSOR:
		return "cursor plane";
	case TRM_PLANE_OVERLAY:
		return "overlay plane";
	default :
		return "unknow plane type";
	}
}

const char *byte_amount(size_t amount) {
	static char *suffix[] = {
		"b",
		"Kb",
		"Mb",
		"Gb",
		"Tb",
		"Pb",
		NULL,
	};
	int i = 0;
	while(amount >= 1024 && suffix[i + 1]) {
		i++;
		amount /= 1024;
	}
	static char buf[32];
	sprintf(buf, "%zu%s", amount, suffix[i]);
	return buf;
}

int main(int argc,char **argv){
	if(argc != 2){
		fprintf(stderr,"not enought argument\n");
		return 1;
	}
	if(!strcmp(argv[1],"--help")){
		help();
		return 0;
	}
	int fd = open(argv[1],O_RDONLY);
	if(fd < 0){
		perror(argv[1]);
		return 1;
	}
	printf("%s :\n",argv[1]);
	struct stat st;
	if (fstat(fd, &st) >= 0) {
		printf("device     : %u, %u\n", major(st.st_rdev), minor(st.st_rdev));
	}

	// try to get model
	char model[256];
	if(ioctl(fd, I_MODEL, model) >= 0){
		printf("model      : %s\n", model);
	}

	// try to get block size
	size_t size;
	if(ioctl(fd, I_BLOCK_GET_SIZE, &size) >= 0){
		printf("size       : %s\n", byte_amount(size));
	}


	// try to get input info
	struct input_info input_info;
	if(libinput_get_info(fd, &input_info) >= 0){
		printf("class      : %s\n", libinput_subclass_string(input_info.if_class, input_info.if_subclass));
	}

	// try to print trm info
	trm_card_t *card = trm_get_resources(fd);
	if (card) {
		printf("card       : %s\n", card->name);
		printf("driver     : %s\n", card->driver);
		printf("vram       : %s\n", byte_amount(card->vram_size));
		printf("planes     : %lu\n", card->planes_count);
		printf("crtcs      : %lu\n", card->crtcs_count);
		printf("connectors : %lu\n", card->connectors_count);
		for (size_t i=0; i<card->planes_count; i++) {
			trm_plane_t *plane = &card->planes[i];
			printf("plane(%u) : %s\n", plane->id, trm_plane_type(plane->type));
		}
	}
	return 0;
}
