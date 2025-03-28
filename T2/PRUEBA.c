#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cjson/cJSON.h"

void write_to_csv(const char *filename, const char *tipo_de_dato, const char *dato) {
    FILE *file = fopen("output.csv", "a");
    if (!file) {
        perror("Error opening file");
        return;
    }
    fprintf(file, "%s,%s,%s\n", filename, tipo_de_dato, dato);
    fclose(file);
}

void parse_json_and_write_to_csv(const char *json_str) {
    cJSON *json = cJSON_Parse(json_str);
    if (!json) {
        printf("Error parsing JSON\n");
        return;
    }

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
                    fprintf(file, "unknown_file,%s,%s\n", field_name, item->valuestring);
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
}

int main() {

	FILE *fp1;
	char buffer[100000];
	static char json_data[100000];
	
	fp1 = popen("venv/bin/python3 dataExtraction.py ./textFiles/'LS 0070 N_text.txt'", "r");
	if(fp1 == NULL)
	{
		perror("ERROR OPENING PIPE");
		exit(1);
	}

	while(fgets(buffer, sizeof(buffer), fp1) != NULL){
		strncat(json_data, buffer,100000 - strlen(json_data) - 1);
	}

	pclose(fp1);
	
	printf("json data:\n%s\n", json_data);

	FILE *file = fopen("output.csv", "w");
    	if (file) {
        	fprintf(file, "nombre_archivo,tipo_de_dato,dato\n");
        	fclose(file);
    	}
    
    	parse_json_and_write_to_csv(json_data);
    	printf("Data written to output.csv successfully!\n");
   	return 0;
}
