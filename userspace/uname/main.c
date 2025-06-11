#include <sys/mount.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef __x86_64__
const char *arch = "x86_64";
#elif defined(__i386__)
const char *arch = "i386";
#elif defined(__aarch64__)
const char *arch = "aarch64";
#else
const char *arch = "unknow-arch"
#endif

#ifdef __linux__
const char *kernel = "linux";
const char *os = "linux/GNU";
#elif defined(__stanix__)
const char *kernel = "stanix";
const char *os = "stanix";
#elif defined(__unix__)
const char *kernel = "unix";
const char *os = "unix";
#else
const char *kernel = "unknow-kernel";
const char *os = "unknow-os";
#endif

void help(){
	printf("uname [OPTION]\n");
	printf("-a/--all              : print all information in the following order\n");
	printf("-s/--kernel-name      : print kernel name\n");
	printf("-m--machine           : print machine hardware name\n");
	printf("-o/--operating-system : print operating system name\n");
}

int main(int argc,char **argv){
	int show_kernel_name = 0;
	int show_arch = 0;
	int show_os = 0;
	for(int i=1; i<argc; i++){
		if(argv[i][0] != '-'){
			fprintf(stderr,"uname : unknow operand %s\n",argv[i]);
			return 1;
		}
		if(argv[i][1] == '-'){
			if(!strcmp("--all",argv[i])){
				show_kernel_name = 1;
				show_arch = 1;
				show_os = 1;
			} else if(!strcmp("--kernel-name",argv[i])){
				show_kernel_name = 1;
			} else if(!strcmp("--machine",argv[i])){
				show_arch = 1;
			} else if(!strcmp("--operating-system",argv[i])){
				show_os = 1;
			} else {
				fprintf(stderr,"invalid option : %s\n",argv[i]);
				return 1;
			}
		} else {
			for(int j=1; argv[i][j]; j++){
				switch(argv[i][j]){
				case 'a':
					show_kernel_name = 1;
					show_arch = 1;
					show_os = 1;
					break;
				case 's':
					show_kernel_name = 1;
					break;
				case 'm':
					show_arch = 1;
					break;
				case 'o':
					show_os = 1;
					break;
				default:
					fprintf(stderr,"uname : invalid option -%c\n",argv[i][j]);
					return 1;
				}
			}
		}
	}

	//by default show kernel name only
	if(!(show_kernel_name || show_arch || show_os)){
		show_kernel_name = 1;
	}

	if(show_kernel_name) printf("%s ",kernel);
	if(show_arch) printf("%s ",arch);
	if(show_os) printf("%s ",os);
	putchar('\n');
	return 0;
}