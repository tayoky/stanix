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
	struct gpt_guid gpt_uuid;
	uint8_t  mbr_uuid;
	char *name;
	char *mount_type;
};

#define arraylen(array) sizeof(array)/sizeof(*array)
#define GUID(...) {__VA_ARGS__}
#define FS(n,m,mbr,gpt) {.name = n, .mount_type = m, .mbr_uuid = mbr, .gpt_uuid = gpt}
#define DEV_PATH "/dev/"
#define MNT_PATH "/mnt/"

struct fs_type fs_types[] = {
	FS("EFI system","fat",0x00,GUID(0xC12A7328,0xF81F,0x11D2,0xBA4B,{0x00,0xA0,0xC9,0x3E,0xC9,0x3B})),
	FS("FAT16"     ,"fat",0x01,GUID(0)),
	FS("FAT32"     ,"fat",0x0b,GUID(0)),
	FS("FAT32"     ,"fat",0x0c,GUID(0)),
};

int ret = 0;

void check(const char *prefix){
	char path[PATH_MAX];
	for(char letter = 'a'; letter <= 'z'; letter++){
		sprintf(path,DEV_PATH"%s%c",prefix,letter);
		FILE *f = fopen(path,"r");
		if(!f)continue;
		fclose(f);

		//is it already mounted ?
		sprintf(path,DEV_PATH"%s%c0",prefix,letter);
		f = fopen(path,"r");
		if(f){
			//already mounted, ignore
			fclose(f);
			continue;
		}

		fprintf(stderr,"try to mount device "DEV_PATH"%s%c : ...",prefix,letter);

		//we can find partitons
		sprintf(path,DEV_PATH"%s%c",prefix,letter);
		if(mount(path,path,"part",0,NULL) < 0){
			fail:
			fprintf(stderr,"\rtry to mount device "DEV_PATH"%s%c : [fail]\n",prefix,letter);
			fprintf(stderr,"%s\n",strerror(errno));
			ret = 1;
			continue;
		}



		//TODO : mount the loaded partions
		for(int i = 0;;i++){
			sprintf(path,DEV_PATH"%s%c%d",prefix,letter,i);
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
				fprintf(stderr,"\rtry to mount device "DEV_PATH"%s%c : [fail]\n",prefix,letter);
				if(info.type == PART_TYPE_MBR){
					fprintf(stderr,"unknow fs type %#x (mbr)\n",info.mbr.type);
				} else {
					fprintf(stderr,"unknow fs type %08x-%04hx-%04hx-%04hx-",info.gpt.type.e1,info.gpt.type.e2,info.gpt.type.e3,info.gpt.type.e4);
					for(int i=0; i<6; i++){
						printf("%02hhx",info.gpt.type.e5[i]);
					}
					fprintf(stderr," (gpt)\n");
				}
				ret = 1;
				goto cont;
			}
			char target[PATH_MAX];
			sprintf(target,MNT_PATH"%s%c%d",prefix,letter,i);
			if (mkdir(target, 0777) < 0) {
				goto fail;
			}
			if(mount(path,target,fs->mount_type,0,NULL) < 0){
				goto fail;
			}
		}


		fprintf(stderr,"\rtry to mount device "DEV_PATH"%s%c : [ok]\n",prefix,letter);
		cont:
		continue;
	}
}

int main(){
	if(getuid() != 0){
		fprintf(stderr,"automount : not run as root\n");
		return 1;
	}
	check("hd");
	check("cdrom");
	check("nvme");

	return ret;
}