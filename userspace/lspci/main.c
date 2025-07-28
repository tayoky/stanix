#include <sys/mount.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <dirent.h>
//we are including kernel header in userspace program XD
#include <module/pci.h>

void help(){
	printf("lspci [OPTIOn]\n");
	printf("list pci devices\n");
}

uint16_t read_word(FILE *f,size_t offset){
	fseek(f,offset,SEEK_SET);
	uint16_t ret;
	fread(&ret,sizeof(ret),1,f);
	return ret;
}

void print_pci(const char *name){
	char *full = malloc(strlen("/dev/pci/") + strlen(name) + 1);
	sprintf(full,"/dev/pci/%s",name);
	FILE *f = fopen(full,"r");
	free(full);
	if(!f){
		perror(name);
		exit(1);
	}
	printf("%s ",name);
	uint8_t base_class = read_word(f,PCI_CONFIG_BASE_CLASS);
	uint8_t sub_class = read_word(f,PCI_CONFIG_SUB_CLASS);
	printf("class : %x  subclass : %x ",base_class,sub_class);
	putchar('\n');
	fclose(f);
}

int main(int argc,char **argv){
	if(argc == 2 && !strcmp(argv[1],"--help")){
		help();
		return 0;
	}
	DIR *pci = opendir("/dev/pci");
	if(!pci){
		perror("/dev/pci");
		return 1;
	}

	for(;;){
		struct dirent *ent = readdir(pci);
		if(!ent)break;
		if(ent->d_name[0] == '.')continue;
		print_pci(ent->d_name);
	}

	closedir(pci);
	return 0;
}