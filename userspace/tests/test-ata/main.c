#include <stdio.h>
#include <ctype.h>

int main(int argc,char **argv){
	if(argc != 2){
		fprintf(stderr,"usage : test-ata DEVICE\n");
		return 1;
	}

	FILE *dev = fopen(argv[1],"r");
	if(!dev)return 1;
	unsigned char buf[512];
	fread(buf,sizeof(buf),1,dev);
	fclose(dev);

	for(size_t i=0; i<512;i+=16){
		for(size_t j=i; j<i+16; j++){
			printf("%2x ",buf[j]);
		}
		for(size_t j=i; j<i+16; j++){
			if(isprint(buf[j])){
				putchar(buf[j]);
			} else {
				putchar('.');
			}
		}
		putchar('\n');
	}
	return 0;
}