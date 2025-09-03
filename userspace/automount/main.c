#include <stdio.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <errno.h>
#include <sys/mount.h>

int ret = 0;

void check(const char *prefix){
	char path[PATH_MAX];
	for(char letter = 'a'; letter <= 'z'; letter++){
		sprintf(path,"%s%c",prefix,letter);
		FILE *f = fopen(path,"r");
		if(!f)continue;
		fclose(f);

		//is it already mounted ?
		sprintf(path,"%s%c0",prefix,letter);
		f = fopen(path,"r");
		if(f){
			//already mounted, ignore
			fclose(f);
			continue;
		}

		fprintf(stderr,"try to mount device %s%c : ...",prefix,letter);

		//we can find partitons
		sprintf(path,"%s%c",prefix,letter);
		if(mount(path,path,"part",0,NULL) < 0){
			fprintf(stderr,"\rtry to mount device %s%c : [fail]\n",prefix,letter);
			fprintf(stderr,"%s\n",strerror(errno));
			ret = 1;
			continue;
		}


		fprintf(stderr,"\rtry to mount device %s%c : [ok]\n",prefix,letter);


		//TODO : mount tha loaded partions
	}
}

int main(int argc,char **argv){
	if(getuid() != 0){
		fprintf(stderr,"automount : not run as root\n");
		return 1;
	}
	check("/dev/hd");
	check("/dev/cdrom");
	check("/dev/nvme");

	return ret;
}