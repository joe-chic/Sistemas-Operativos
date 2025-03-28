#include "fcntl.h"
#include "stdlib.h"
#include "ctype.h"
#include "errno.h"
#include "unistd.h"
#include "stdio.h"
#include "semaphore.h"
#include "dirent.h"
#include "string.h"
#include "signal.h"
#include "sys/types.h"
#include "sys/ipc.h"
#include "sys/msg.h"
#include "sys/wait.h"
#include "cjson/cJSON.h"
#include "sys/stat.h"

#define INITIAL_CAPACITY 10 //Reserve at least 10 files.

/*

   This program will use mmap() for shared memory between child processes.

*/

int signalReceived, extractionFinished;
sem_t *file_lock;
int numberChild, file_count, file_capacity=INITIAL_CAPACITY;

// CORROBORAR LA EXISTENCIA DE UN ARCHIVO. 
int dirExists(const char* path){
	struct stat st;
	return (stat(path, &st) == 0 && S_ISDIR(st.st_mode));
}

// VERIFICAR LA EXISTENCIA DE CADA MODULO USADO EN dataExtraction.py
int pythonModuleExists(const char* module){
	char command[126];
	snprintf(command, sizeof(command),
			"bash -c 'source venv/bin/activate && venv/bin/python3 -c \"import %s\" 2>/dev/null'", module);
	return system(command) == 0;
}

// CREACION DE UN AMBIENTE VIRTUAL SI NO EXISTE PREVIAMENTE
void venvCreate()
{
	if(!dirExists("venv"))
	{
		printf("Ambiente virtual no existente. Creando 'venv'...\n");
		if(system("python3 -m venv venv") != 0){
			fprintf(stderr, "Fallo al crear el ambiente 'venv', ABORTANDO.\n");
			exit(1);
		}
		
		printf("Ambiente virtual 'venv' ha sido creado exitosamente.\n");
	} else{
		printf("Ambiente virtual 'venv' ha sido identificado.\n");
	}
}


// FUNCION PARA INCREMENTAR EL NUMERO DE ARCHIVOS QUE PUEDE SOPORTAR EL ARREGLO.
void addFiles(const char* filename, char ** file_list, int *file_status){
	if(file_count >= file_capacity){
		file_capacity*=2;
		file_list = realloc(file_list, file_capacity*sizeof(char *)); // SEND AS PARAMETER THE FILELIST
		file_status = realloc(file_status, file_capacity*sizeof(int));
		if(!file_list || !file_status){
			perror("La reasignacion de memoria ha fallado.\n");
			printf("Valor Errno: %d\n", errno);
			printf("Descripcion de Errno: %s\n", strerror(errno));
			exit(1);
		}
	}
	
	file_list[file_count] = strdup(filename);
	file_status[file_count] = 0;
	file_count++; //Increase the number of files.
}

// FUNCION PARA CARGAR LOS ARCHIVOS DEL DIRECTORIO EN LOS ARREGLOS.
void loadFiles(const char* directory, char** file_list, int* file_status){
	DIR *dir = opendir(directory);
	if (!dir){
		perror("Error al abrir el directorio.\n");
		printf("Valor Errno: %d\n", errno);
		printf("Descripcion de Errno: %s\n", strerror(errno));
		exit(1);
	}

	struct dirent *entry; //EXPLAIN THIS
	while((entry  = readdir(dir)) != NULL){
		if(strstr(entry->d_name, ".txt")){
			addFiles(entry->d_name, file_list, file_status);
		}
	}

	closedir(dir);
}

// HANDLER SIGNAL PARA ASIGNAR EL NUMERO DE HIJOS.
void handlerUSR1(int sig){
	signalReceived = 1;
}

// FUNCION PARA PROCESAR EL ARCHIVO ASIGNADO AL HIJO.
char* processText(char *text)
{
	//printf("The received text was the following: %s\n", text);
	//printf("Size of text: %ld\n", strlen(text));
	
	size_t bufferSize = strlen("bash -c 'source venv/bin/activate && python3 dataExtraction.py ./textFiles/\"") + strlen(text) + 3;
	//printf("The bufferSize is the following: %ld\n", bufferSize);
	char* bufferText = (char *)malloc(bufferSize);
	memset(bufferText, 0, bufferSize); //Llenar el arreglo con puros null bytes.
	
	if(bufferText == NULL){
		perror("Error al asignar una memoria en processText().\n");
		printf("Valor Errno: %d\n", errno);
		printf("Descripcion de Errno: %s\n", strerror(errno));
		return NULL;
	}
	
	strcat(bufferText, "bash -c 'source venv/bin/activate && python3 dataExtraction.py ./textFiles/\"");
	strcat(bufferText, text);
	strcat(bufferText, "\"'");
	
	//printf("The final command that will be executed is: %s\n", bufferText);

	FILE *fp = popen(bufferText,"r");

	if(fp == NULL){
		perror("Error al ejecutar el script.");
		printf("Valor Errno: %d\n", errno);
		printf("Descripcion de Errno: %s\n", strerror(errno));
		pclose(fp);
		return NULL;
	}

	size_t outputSize = 1024;
    	size_t currentLength = 0;
    	char *output = (char *)malloc(outputSize);
    	
	if (output == NULL) {
        	perror("Error al asignar memoria para la salida.");
       		printf("Valor Errno: %d\n", errno);
		printf("Descripcion de Errno: %s\n", strerror(errno));
		pclose(fp);
        	return NULL;
    	}
    	output[0] = '\0'; // Initialize the string to be empty

    // Read and dynamically resize as needed
	char temp[256];  // Temporary buffer for reading chunks
	while (fgets(temp, sizeof(temp), fp) != NULL) {
		size_t tempLength = strlen(temp);
	
	if(currentLength + tempLength + 1 > outputSize) {
	 // Reallocate with double the size
	 outputSize *= 2;
	 output = (char *)realloc(output, outputSize);
	 
	 if (output == NULL) {
		perror("Error al reasignar memoria cuando el archivo es leido.\n");
		printf("Valor Errno: %d\n", errno);
		printf("Descripcion de Errno: %s\n", strerror(errno));
		pclose(fp);
		return NULL;
	    }
	}
	
	strcpy(output + currentLength, temp);  // Append the chunk
	currentLength += tempLength;
    }

    pclose(fp);
    free(bufferText);
    return output;  // Return the final output string
}

int file_exists(const char* filename){
	FILE *file = fopen(filename, "r");
	if(file){
		fclose(file);
		return 1;
	}
	return 0;
}

void writeHeaders(){
	if(!file_exists("output.csv")){
		FILE *file = fopen("output.csv", "w");
		if(file){
			fprintf(file,"nombre_archivo, tipo_dato, dato\n");
			fclose(file);
		} else{
			perror("Error al crear el output.csv.\n");
			exit(EXIT_FAILURE);
		}
	}
}

// FUNCION PARA ESCRIBIR EN UN ARCHIVO CSV DE MANERA SEGURA
void writeCSV(char* output, sem_t *file_lock, char* filename) {
    // Verificar que la salida no esté vacía
	if (output == NULL || strlen(output) == 0) {
        	fprintf(stderr, "Error: La salida JSON está vacía.\n");
        	return;
	}	
	
	cJSON *json = cJSON_Parse(output);
	if (!json) {
		printf("Error parsing JSON\n");
		return;
	}
	
	sem_wait(file_lock);
	
	writeHeaders();
	FILE *file = fopen("output.csv", "a");
	
	if (!file) {
		perror("Error opening file");
		cJSON_Delete(json);
		return;
	}

	// Helper function to write array data
	void write_array_to_csv(const char *field_name, cJSON *array) {
	if (cJSON_IsArray(array)) {
		cJSON *item;
		cJSON_ArrayForEach(item, array) {
			if (cJSON_IsString(item)) {
			    fprintf(file, "%s,%s,%s\n", filename,field_name, item->valuestring);
			}
		    }
		}
	}

	    // Process all expected arrays
	write_array_to_csv("nombre", cJSON_GetObjectItem(json, "nombres"));
	write_array_to_csv("palabra_clave", cJSON_GetObjectItem(json, "palabras_clave"));
	write_array_to_csv("direccion", cJSON_GetObjectItem(json, "direcciones"));
	write_array_to_csv("fecha_de_nacimiento", cJSON_GetObjectItem(json, "fechas_de_nacimiento"));
	write_array_to_csv("fecha", cJSON_GetObjectItem(json, "fechas"));

	fclose(file);
	cJSON_Delete(json);	
	sem_post(file_lock);
}

// FUNCION PRINCIPAL
int main()
{
	// VERIFICAR QUE venv ESTE VIGENTE.
	venvCreate();	

	// VERIFICAR QUE LA LISTA DE MODULOS ESTEN INSTALADOS.
	char *modules[] = {"spacy", "regex", "json", "sys", "yake"};
	int numModules = sizeof(modules)/sizeof(modules[0]);
	char missingModules[1024] = ""; 
	int allInstalled = 1;

	// REVISION DE CADA MODULO
	
	for(int i=0; i<numModules; i++){
		if(!pythonModuleExists(modules[i])){
			printf("Modulo %s no esta instalado.\n", modules[i]);
			strcat(missingModules, modules[i]);
			strcat(missingModules, " ");
			allInstalled = 0;
		} else{
			printf("Modulo %s esta instalado.\n", modules[i]);
		}
	}

	if(!allInstalled){
		printf("Instalando modulos faltantes: \n");
		char moduleText[2064];
	       	strcpy(moduleText, "bash -c 'source venv/bin/activate && venv/bin/pip install ");
		strcat(moduleText, missingModules);
		strcat(moduleText, "'");

		//printf("The full command: %s\n", moduleText);

		if(system(moduleText) != 0){
			fprintf(stderr,"Hubo un fallo al instalar todos los modulos.\n");
			exit(1);
		}

		printf("Todos los modulos han sido instalados con exito.\n");
	}

	// VERIfICAR LA PRESENCIA DEL MODELO PARA PROCESAMIENTO DE PALABRAS.
	int check = system("venv/bin/python3 -c \"import spacy; spacy.load('en_core_web_md')\"");
	
	if(check){
		printf("El modelo en_core_web_md no fue encontrado. INSTALANDO.\n");
		int install = system("venv/bin/python3 -m spacy download en_core_web_md");

		if(!install){
			printf("La instalacion del modelo en_core_web_md fue completada.\n");	
		} else{
			printf("La instalacion del modelo en_core_web_md ha fallado.\n");
			exit(1);
		}
	} else{
		printf("El modelo en_core_web_md ya esta instalado\n");
	}

	char answer[10];
	signal(SIGUSR1, handlerUSR1);		
	
	// VERIFICAR LA INSTALACION DE ARCHIVOS .txt.
	while(1){
		printf("Descargar archivos de texto (y/n)?\n");
		scanf("%s",answer);
		
		for(char *ptr = answer; *ptr; ptr++){
			*ptr = tolower(*ptr);
		}

		if(*answer == 'y'){
			char command[] = "wget -r -np -nd -A \"*.txt\" -P ./english_text_files https://ubiquitous.udem.edu/~raulms/Suecia/Museum/english_text_files/";
			
			int status = system(command);

			if(status == -1){
				printf("Error al ejecutar el comando wget.\n");
			} else{
				printf("Descarga realizada exitosamente.\n");
				break;
			}

		} else if (*answer == 'n'){
			break;
		} else{
			printf("Solo inserta n para no, y para si. Intente otra vez.\n");
		}
	}

	// IF NOT, DOWNLOAD ALL FILES INTO A DIRECTORY.
	
	// LOOP FOR WAITING FOR SIGNAL TO CREATE N CHILD PROCESSES.
	while(!signalReceived)
	{
		printf("Esperando por la senial USR1...\n");
		sleep(7);	
	}

	printf("Inserte la cantidad de forks a crear: ");
	scanf("%d",&numberChild);

	// Initialize a status array with malloc().
	// Use messages to update which file is available between child and parent process.
	// After a child has selected a file, it will process and write the result in a CSV.
	// This process will continue until all files have been selected, then, all process will end.
	
	// INITIALIZE ARRAY OF FILES WITH MMAP():
	char **file_list = malloc(INITIAL_CAPACITY*sizeof(char*));
	int *file_status = malloc(INITIAL_CAPACITY*sizeof(int));
	
	if(!file_list || !file_status){
		perror("Error al establecer memoria para el arreglo archivos.");
		printf("Valor Errno: %d\n", errno);
		printf("Descripcion de Errno: %s\n", strerror(errno));
		exit(1);
	}

	// INITIALIZE SEMAPHORE.
	file_lock = sem_open("/csv_lock", O_CREAT, 0644, 1);
	if(file_lock == SEM_FAILED){
		perror("sem_open() ha fallado.\n");
		exit(1);
	}

	const char* directory = "./textFiles";	
	loadFiles(directory, file_list, file_status);		

	for(int i = 0; i<file_count; i++)
		printf("The name of identified files are the following: %s\n", file_list[i]);
	
	//CREATE A UNIQUE MESSAGE QUEUE AND ITS ID
	key_t key = ftok("./README.md", 65);
	if(key == -1){
		perror("Hubo un error al generar la llave.\n");
		exit(1);
	}

	int msgid = msgget(key, 0666 | IPC_CREAT);
	if(msgid == -1){
		perror("mssget() ha fallado.\n");
		printf("Valor Errno: %d\n", errno);
		printf("Descripcion de Errno: %s\n", strerror(errno));	
		exit(1);
	}
	
	// if flag remains 1 until the end of the loop, then that means that there are no more available files. 
	int flag; 
	
	// MAKE CHILD PROCESSES:
	for(int i=0; i<numberChild; i++)
	{
		pid_t pid = fork();
		
		if(pid == -1){
			perror("fork");
			return 1;
		} else if(pid == 0){
			printf("Child process %d (PID: %d) will begin operations.",i+1, getpid());
			char filename[] = "";
			
			// LOOP THROUGH THE FILE ARRAY, UNTIL THERE ARE NO MORE UNHANDLED FILES.
			int flag = 1;
			for(int i=0; i<file_count;i+)
			{
				if( == 0)
					flag = 0;



				if(i==file_count-1 && flag) break;
				if(i==file_count-1) flag=1;
			}

			//LIBERAR MEMORIA

			printf("(PID: %d) No more files to process. Terminating...\n", getpid());
			// CHILD PROCRESS FINISHED:
			exit(0);

		} else if(i==numberChild-1){
			printf("Parent process (PID: %d) created child (PID: %d)\n", getpid(), pid);
			// MAX_TEXT_SIZE to be defined?	
			// HANDLE MESSAGES BETWEEN PARENT AND CHILD
		
			msgChild = malloc(sizeof(struct message));
			msgParent = malloc(sizeof(struct message));

			while(continueListening){
				ssize_t received = msgrcv(msgid, msgChild, sizeof(struct message) - sizeof(long), 1, IPC_NOWAIT);
				
				if(received == -1){
					continue;
				} else{

					// Check if the file is available.
					//printf("The parent has received a request from child no. %d\n", msgChild->pid);
					if(file_status[msgChild->index] == 0){ // Si el archivo esta disponible, notificar.
						file_status[msgChild->index] = 1;
						msgParent->mtype=msgChild->pid;
						msgParent->pid=0;
						msgParent->index=0;
						
						if(msgsnd(msgid, msgParent, sizeof(struct message) - sizeof(long), 0) == -1){
							perror("msgsnd() ha falldo en padre.\n");
							printf("Valor Errno: %d\n", errno);
							printf("Descripcion de Errno: %s\n", strerror(errno));
							exit(1);
						}

					} else{ // Si el archivo no esta disponible de acuerdo al padre, notificar hijo.
						msgParent->mtype=msgChild->pid;
						msgParent->pid=0;
						msgParent->index=1;

						if(msgsnd(msgid, msgParent, sizeof(struct message) - sizeof(long), 0) == -1){
							perror("msgsnd() ha falldo en padre.\n");
							printf("Valor Errno: %d\n", errno);
							printf("Descripcion de Errno: %s\n", strerror(errno));
							exit(1);
						}			
					}
				}

				/*
				printf("parent: ");
				for(int i=0; i<file_count; i++)
				{
					printf("%d ", file_status[i]);
				}
				printf("\n");
				*/

				received = msgrcv(msgid, msgChild, sizeof(struct message) - sizeof(long), 2, IPC_NOWAIT);
				if(msgChild->mtype==2){
					//printf("GOT MESSAGE FROM THIS CHILD: %d. ITS TYPE: %ld.\n", msgChild->pid, msgChild->mtype);
					child_counter--;
				}

				if(child_counter == 0){
					continueListening=0;
					printf("All children have finished. Parent terminating...\n");
				}
			}

			// LIBERAR MEMORIA
			free(msgChild);
			free(msgParent);
		
		} else{
			printf("Parent process (PID: %d) created child (PID: %d)\n", getpid(), pid);	
		}
	}
	// TERMINATE CHILD PROCESSES:
	for(int i=0; i<numberChild; i++)
		wait(NULL);
	

	// FREE MEMORY
	for(int i=0; i<file_count; i++){
		free(file_list[i]);
	}

	free(file_list);
	free(file_status);

	// CLOSE FILE PROGRAM:
	return 0;
}


