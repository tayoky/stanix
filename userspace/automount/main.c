#include <module/part.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <stdio.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

struct fs_type {
	uint64_t gpt_uuid[2];
	uint8_t  mbr_uuid;
	char *name;
	char *mount_type;
};

#define arraylen(array) sizeof(array)/sizeof(*array)
#define FS(n,m,mbr,gpt1,gpt2) {.name = n, .mount_type = m, .mbr_uuid = mbr, .gpt_uuid = {gpt1,gpt2}}

struct fs_type fs_types[] = {
	FS("EFI system","fat",0x00,0x024DEE4133E711D3L,0x9D690008C781F39FL),
	FS("FAT16"     ,"fat",0x01,0,0),
	FS("FAT32"     ,"fat",0x0b,0,0),
	FS("FAT32"     ,"fat",0x0c,0,0),
};

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
			fail:
			fprintf(stderr,"\rtry to mount device %s%c : [fail]\n",prefix,letter);
			fprintf(stderr,"%s\n",strerror(errno));
			ret = 1;
			continue;
		}



		//TODO : mount the loaded partions
		for(int i = 0;;i++){
			sprintf(path,"%s%c%d",prefix,letter,i);
			int fd = open(path,O_WRONLY);
			if(fd < 0)break;
			
			struct part_info info;
			if(ioctl(fd,I_PART_GET_INFO,&info) < 0){
				goto fail;
			}
			close(fd);
			struct fs_type *fs = NULL;
			if(info.type == PART_TYPE_MBR){
				for(size_t i=0; i<arraylen(fs_types); i++){
					if(info.mbr.type == fs_types[i].mbr_uuid){
						fs = &fs_types[i];
						break;
					}
				}
			} else {
				for(size_t i=0; i<arraylen(fs_types); i++){
					if(!memcmp(&info.gpt.type,&fs_types[i].gpt_uuid,sizeof(info.gpt.type))){
						fs = &fs_types[i];
						break;
					}
				}
			}
			if(!fs){
				fprintf(stderr,"\rtry to mount device %s%c : [fail]\n",prefix,letter);
				if(info.type == PART_TYPE_MBR){
					fprintf(stderr,"unknow fs type %#x (mbr)\n",info.mbr.type);
				} else {
					fprintf(stderr,"unknow fs type %llx-%llx (gpt)\n",info.gpt.type[0],info.gpt.type[1]);
				}
				ret = 1;
				goto cont;
			}
		}


		fprintf(stderr,"\rtry to mount device %s%c : [ok]\n",prefix,letter);
		cont:
		continue;
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