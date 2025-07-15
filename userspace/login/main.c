#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <pwd.h>

int main(int argc,char **argv){
	if(geteuid() != 0){
		fprintf(stderr,"login must be run as root !\n");
		return 1;
	}
	
	for (int i = 1; i < argc; i++){
		if(!strcmp(argv[i],"--setup-stdin-from-stdout")){
			dup2(STDOUT_FILENO,STDIN_FILENO);
		}
	}
	dup2(STDOUT_FILENO,STDERR_FILENO);
	//skip login on -f
	struct passwd *pwd;
	if(argc >= 2 && !strcmp(argv[1],"-f")){
		pwd = getpwuid(getuid());
		if(!pwd){
			fprintf(stderr,"login : can't skip login\n");
			return 1;
		}
	} else {
		for(;;){
			printf("username : ");
			fflush(stdout);
			char username[256];
			fgets(username,sizeof(username),stdin);
			if(strchr(username,'\n'))*strrchr(username,'\n') = '\0';
			pwd = getpwnam(username);
			if(pwd && !strcmp(pwd->pw_passwd,"")){
				//no password
				break;
			}
			printf("password : ");
			fflush(stdout);
			char password[256];
			fgets(password,sizeof(password),stdin);
			if(strrchr(password,'\n'))*strrchr(password,'\n') = '\0';
			if(!pwd || strcmp(pwd->pw_passwd,password)){
				fprintf(stderr,"wrong username or password\n");
				sleep(2);
			} else {
				break;
			}
		}
	}
	//setup an env
	char logname[256];
	char home[256];
	char shell[256];
	snprintf(logname,sizeof(logname),"LOGNAME=%s",pwd->pw_name);
	snprintf(home   ,sizeof(home)   ,"HOME=%s"   ,pwd->pw_dir);
	snprintf(shell  ,sizeof(shell)  ,"SHELL=%s"  ,pwd->pw_shell);
	putenv(logname);
	putenv(home);
	putenv(shell);
	putenv("PATH=/bin;/usr/bin");

	setegid(pwd->pw_gid);
	seteuid(pwd->pw_uid);

	chdir(pwd->pw_dir);

	//clear screen and print /motd
	system("clear");
	system("cat /etc/motd");

	const char *arg[] = {
		pwd->pw_shell,
		NULL
	};
	execv(pwd->pw_shell,arg);
	perror(pwd->pw_shell);
	
	return 1;
}
