#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#include <pwd.h>
#include <grp.h>
#include <sys/wait.h>
#include <sys/types.h>

/* R E P L */

void change_dir(char **argv, int argc){
	char *path = NULL;
	if(argc==1){
		path = getenv("HOME");
		if(path==NULL){
			fprintf(stderr, "cd: variabila env HOME nu e setata.");
			return;
		}
	}else if(argc ==2){
		//argumentul 2 este calea unde va merge cd-ul
		path = argv[1];
	}else {
		fprintf(stderr, "prea multe argumente");
		return;
	}
	
	if(chdir(path) != 0){
		fprintf(stderr, "cd: %s: %s\n", path, strerror(errno));
		//chdir schimba directorul in functie de cale; daca returneaza eroare atunci o afisam
	}
}

void func_pwd(int argc, char **argv){

	if(argc>1){
		fprintf(stderr, "pwd ar trebui sa aiba un singur argument\n");
		return;
	}
	
	char *buf = NULL;
	size_t lungime = 1024;
	buf = (char*)malloc(lungime);
	if(!buf){
		perror("nu s-a reusit alocarea\n");
		return;
	}
	
	// verificam daca spatiul alocat este sau nu suficient pentru a memora calea completa a directorului curent
	
	while(getcwd(buf, lungime) == NULL){
		if(errno == ERANGE){
			// dublam lungimea
			lungime *= 2;
			char *new_buf = realloc(buf, lungime);
			if(!new_buf){
				perror("nu s-a reusit alocarea (realloc).\n");
				free(buf);
				return;
			}
			buf = new_buf;
		} else {
			perror("eroare la getcwd\n");
			free(buf);
			return;
		}
	}
	
	// getcwd reuseste, in buf avem calea
	printf("Director curent: %s\n", buf);
	free(buf);
}

void func_ls(const char *path, int all, int long_format){
		
		// deschidem directorul curent - adica path care va fi "."
		DIR *dir = opendir(path);
		if(dir==NULL){
			perror("Directorul nu a putut fi deschis");
			return;
		}
		
		// definim un pointer pentru structura dirent - stocheaza informatiile pentru fiecare fisier sau director gasit
		
		struct dirent *intrare;
		
		
		//parcurgem toate intrarile
		while((intrare = readdir(dir)) != NULL) {
		
			// sarim peste fisierele ascunse
			if(!all && intrare->d_name[0] == '.'){
				continue;
			}
			
			// daca avem formatul lung activ atunci afisam detaliile extinse ale fisierelor
			
			if(long_format){
				
				// structura standard - o folosim pt a stoca informatii despre fisier
				struct stat statbuf;
				
				// calea completa
				char path_complet[1024];
				
				// construieste calea completa
				snprintf(path_complet, sizeof(path_complet), "%s/%s", path, intrare->d_name);
				
				// obtinem inf despre fisier si le stocam in statbuf, eroare daca esueaza
				if(stat(path_complet, &statbuf) == -1){
					perror("Eroare stats fisier");
					continue;
				}
				
				// verifica fiecare permisiune si stocheaza in array-ul perm
				
				char perm[11];
				perm[0] = (S_ISDIR(statbuf.st_mode)) ? 'd' : '-';
				perm[1] = (statbuf.st_mode & S_IRUSR) ? 'r' : '-';
				perm[2] = (statbuf.st_mode & S_IWUSR) ? 'w' : '-';
				perm[3] = (statbuf.st_mode & S_IXUSR) ? 'x' : '-';
				perm[4] = (statbuf.st_mode & S_IRGRP) ? 'r' : '-';
				perm[5] = (statbuf.st_mode & S_IWGRP) ? 'w' : '-';
				perm[6] = (statbuf.st_mode & S_IXGRP) ? 'x' : '-';
				perm[7] = (statbuf.st_mode & S_IROTH) ? 'r' : '-';
				perm[8] = (statbuf.st_mode & S_IWOTH) ? 'w' : '-';
				perm[9] = (statbuf.st_mode & S_IXOTH) ? 'x' : '-';
				perm[10] = '\0';  // se termina in NULL

				long dimensiune = statbuf.st_size;
				char date[20];	
				struct tm *tm_inf = localtime(&statbuf.st_mtime);
				strftime(date, sizeof(date), "%Y-%m-%d %H:%M", tm_inf);
				char *nume = intrare->d_name;
				
				
				// printam informatiile pentru fiecare fisier anume: permisiunile, dimensiunea, data ultimei modificari si numele
				printf("%10.10s %5ld %s %s\n", perm, dimensiune, date, nume);
			} else {
				printf("%s\n", intrare->d_name);
			}
		}
		closedir(dir);
}

// executa comanda - substituie system()

char *execute_command(char *command) {
    int pipefd[2];
    pid_t pid;
    char *output = malloc(1024);  
    if (!output) {
        perror("Alocarea memoriei esuata");
        return NULL;
    }

    if (pipe(pipefd) == -1) {
        perror("Pipe esuat");
        free(output);
        return NULL;
    }

    pid = fork();
    if (pid == -1) {
        perror("Fork esuat");
        close(pipefd[0]);
        close(pipefd[1]);
        free(output);
        return NULL;
    }

    if (pid == 0) {
        close(pipefd[0]); 
        dup2(pipefd[1], STDOUT_FILENO); 
        close(pipefd[1]);

        execlp("/bin/sh", "sh", "-c", command, (char *) NULL); 
        perror("Exec esuat");
        exit(EXIT_FAILURE);
    } else {
        close(pipefd[1]); 
        wait(NULL); 

        read(pipefd[0], output, 1024);
        close(pipefd[0]);
        output[1023] = '\0';  

        char *p = output;
        while (*p) {  
            if (*p == '\n') *p = ' ';
            p++;
        }
    }

    return output;
}


// in loop-ul din main se trimite comanda la procesare: o parseaza, extrage din paranteze si trimite la executie


char *parse_and_execute(char *input) {
    
    printf("%s\n", input);
    
    if(!input) return NULL;
    
    char *result = strdup(input);
    if(!result) return NULL;
    
    char *cursor = result;
    char *end_cursor;
    int depth = 0;
    
    while(*cursor){
    	if(*cursor == '('){
    		if(depth == 0){
    			end_cursor = cursor + 1;
    			
    			while(*end_cursor && (depth > 0 || *end_cursor != ')')){
    				if(*end_cursor == '(') depth++;
    				else if (*end_cursor == ')') depth--;
    				end_cursor++;
    			}
    			if(depth != 0 || *end_cursor != ')'){
    				fprintf(stderr, "Eroare de parantezare\n");
    				return NULL;
    			}
    			
    			char *sub_command = strndup(cursor+1, end_cursor - cursor - 1);
    			char *sub_command_result = execute_command(sub_command);
    			free(sub_command);
    			
    			
    			size_t prefix_len = cursor - result;
    			size_t suffix_len = strlen(end_cursor+1);
    			char *new_result = malloc(prefix_len + strlen(sub_command_result) + suffix_len + 1);
    			strncpy(new_result, result, prefix_len);
    			strcpy(new_result + prefix_len, sub_command_result);
    			strcpy(new_result + prefix_len + strlen(sub_command_result), end_cursor + 1);
    
    			free(result);
    			free(sub_command_result);
    			
    			return parse_and_execute(new_result);
    		} else {
    			depth++;
    		}
    	}else if(*cursor == ')'){
    		depth--;
    	}
    	cursor++;
    }
    //free(input);
    return result; // daca nu gasim subcomenzi imbricate in paranteze returnam comanda originala
    	
}

/*
void execute_line(char *line) {
    int status = parse_and_execute(line);
    if (status != 0) {
        fprintf(stderr, "Eroare la executarea comenzii: %s\n", line);
    }
}
*/

void run_script(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Failed to open file");
        return;
    }

    char *line = NULL;
    size_t len = 0;
    ssize_t read;

    while ((read = getline(&line, &len, file)) != -1) {
        parse_and_execute(line);
    }

    free(line);
    fclose(file);
}

void handle_command(char **argv, int argc){

	
	if (strncmp(argv[0], "./", 2) == 0) {
        	run_script(argv[0]);  
    	}else if (strcmp(argv[0], "cd") == 0) {
        	change_dir(argv, argc);
    	} else if (strcmp(argv[0], "pwd") == 0) {
        	func_pwd(argc, argv);
    	} else if (strcmp(argv[0], "ls") == 0) {
        	int all = 0, format_lung = 0;
        	char *path = ".";
        	for (int i = 1; i < argc; i++) {
            		if (strcmp(argv[i], "-a") == 0) {
                		all = 1;
            		} else if (strcmp(argv[i], "-l") == 0) {
                		format_lung = 1;
            		}
        	}
        func_ls(path, all, format_lung);
    	}else{
    		// construieste comanda
        	char command[1024] = "";
        	strcpy(command, argv[0]);
        	for (int i = 1; i < argc; i++) {
            		strcat(command, " ");
            		strcat(command, argv[i]);
        	}
        	// proceseaza ( parseaza in functie de paranteze ) apoi executa
        	
        	char *output = parse_and_execute(command);
        	if (output) {
            		printf("%s\n", output);
            		free(output);
        	}
        	
    	}
    	
}

int main(void){

	char *cmd = NULL;
	size_t n = 0;
	ssize_t len = 0;
	
	while(1){
		printf("$ ");
		
		//nr de caractere pt a putea face error handling: daca este negativ inseamna ca nu a citit, apoi scoatem caracterul \n 
		len = getline(&cmd, &n, stdin);
		if(len == -1){
			if(feof(stdin)){
				printf("exit\n");
				break;
			}
			perror("getline esuat");
			continue;
		}
		
		// daca ultimul caracter este eol il scoatem pentru a nu fi interpretat drept argument sau caracter al argumentului
		
		if(cmd[len-1] == '\n')
			cmd[len-1] = '\0';
		
		
		int argc = 0;
		char **argv = NULL;
		char *token = strtok(cmd, " ");
		
		while(token!=NULL){
			argv = realloc(argv, sizeof(char *) * (argc+1));
			if(argv==NULL){
				perror("Alocarea memoriei a esuat.");
				free(cmd);
				return -1;
			}
			
			argv[argc++] = token;
			token = strtok(NULL, " ");
		}
		
		
		// termin lista de argumente cu NULL
		argv = realloc(argv, sizeof(char *) * (argc + 1));
		argv[argc] = NULL;
		
		// ls, cd si pwd sunt apelate inter, iar celelalte extern, in functie de parantezele imbricate
		handle_command(argv, argc);
		
		
		free(argv);
	}
	
	free(cmd);
	
	
	
	
	return 0;

}
