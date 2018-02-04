//	Copyright 2018 Rumesh Sudhaharan
//
//	This program is free software: you can redistribute it and/or modify
//	it under the terms of the GNU General Public License as published by
//	the Free Software Foundation, either version 3 of the License, or
//	(at your option) any later version.
//
//	This program is distributed in the hope that it will be useful,
//	but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//	GNU General Public License for more details.
//
//	You should have received a copy of the GNU General Public License
//	along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include <stdio.h>
#include <getopt.h>
#include <cjson/cJSON.h>
#include <string.h>
#include <stdlib.h>
#include <curl/curl.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/stat.h>

char app_folder[44];
char header_string[49] = "Access-Token: ";
char url_string[74] = "https://stream.pushbullet.com/streaming/";
char history_filename[51];
char token_filename[50];
char token[100];

pthread_t stream_tid;
pthread_t user_input_tid;

void clear_buffer(void) {
	int a;
	do {
		a = getchar();
	} while (a != '\n' && a != EOF);
}

int setup(void) {
	char s_token[37];

	printf("Please enter your access token. It can be found at https://www.pushbullet.com/#settings/account\n");
	fgets(s_token, 35, stdin);
	strcat(s_token, " ");
	FILE *s_token_file = fopen(token_filename, "w");
	fprintf(s_token_file, "%s", s_token);
	fclose(s_token_file);
	chmod(token_filename, 0600);
}

int base_var_init(void) {
	strcpy(app_folder, getenv("HOME"));
	strcat(app_folder, "/codes/Pushbullet-test/.pb-cli/");

	strcpy(token_filename, app_folder);
	strcat(token_filename, "token");

	strcpy(history_filename, app_folder);
	strcat(history_filename, "history");
}

int var_init(void) {
	struct stat dir_check;
	if (stat(app_folder, &dir_check) == 0 && S_ISDIR(dir_check.st_mode)) {
	} else {
		printf("Please run pb-cli -s to setup your account details\n");
		mkdir(app_folder, 0755);
		exit(2);
	}

	if (access(token_filename, F_OK) == -1) {
		printf("Account details not available. Please run pb-cli -s to setup your account details.\n");
		exit(1);
	}

	FILE *token_file = fopen(token_filename, "r");
	fscanf(token_file, "%[^\n]", token);
	fclose(token_file);

	token[strlen(token)-1] = '\0';

	strcat(header_string, token);
	strcat(url_string, token);
}

size_t post_cb(char *data, size_t size, size_t nmemb) {
	return size * nmemb;
}

int post(char *text) {
	char *post_data;
	char *post_data_prefix = "type=note&body=";
	size_t length = strlen(text) + strlen(post_data_prefix);

	post_data = malloc(length);
	strcpy(post_data, post_data_prefix);
	strcat(post_data, text);

	CURL *post_handle = curl_easy_init();

	struct curl_slist *access_headers = NULL;
	access_headers = curl_slist_append(access_headers, header_string);

	curl_easy_setopt(post_handle,CURLOPT_HTTPHEADER, access_headers);
	curl_easy_setopt(post_handle, CURLOPT_POSTFIELDS, post_data);
	curl_easy_setopt(post_handle, CURLOPT_URL, "https://api.pushbullet.com/v2/pushes");
	curl_easy_setopt(post_handle, CURLOPT_POST, 1);
	curl_easy_setopt(post_handle, CURLOPT_WRITEFUNCTION, post_cb);

	curl_easy_perform(post_handle);
	curl_easy_cleanup(post_handle);
	free(post_data);

	return 0;
}

size_t get_cb(char *data, size_t size, size_t nmemb) {
	cJSON *jsonstring = cJSON_Parse(data);
	const cJSON *body = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(jsonstring, "pushes")->child, "body");

	char last_message[100];
	FILE *history_g = fopen(history_filename, "r");
	fgets(last_message, 100, history_g);
	fclose(history_g);

	if (strcmp(last_message, body->valuestring) != 0) {
		printf("\r<--\t\t%s\n", body->valuestring);
	}

	return size * nmemb;
}

int get(void) {
	CURL *get_handle = curl_easy_init();
	struct curl_slist *access_headers = NULL;
	access_headers = curl_slist_append(access_headers, header_string);

	curl_easy_setopt(get_handle,CURLOPT_HTTPHEADER, access_headers);
	curl_easy_setopt(get_handle, CURLOPT_URL, "https://api.pushbullet.com/v2/pushes?limit=1");
	curl_easy_setopt(get_handle, CURLOPT_WRITEFUNCTION, get_cb);

	curl_easy_perform(get_handle);
	curl_easy_cleanup(get_handle);

	return 0;
}

size_t get_stream_cb(char *data, size_t size, size_t nmemb) {
	cJSON *jsonstring = cJSON_Parse(data);
	char *type_value = jsonstring->child->valuestring;

	if (strcmp(type_value, "tickle") == 0) {
		char *subtype_value = jsonstring->child->next->valuestring;

		if (strcmp(subtype_value, "push") == 0) {
			get();
		}
	}
	return size * nmemb;
}

void *get_stream() {
	char *user_response;
	CURL *get_stream_handle = curl_easy_init();

	curl_easy_setopt(get_stream_handle, CURLOPT_URL, url_string);
	curl_easy_setopt(get_stream_handle, CURLOPT_WRITEFUNCTION, get_stream_cb);

	curl_easy_perform(get_stream_handle);
	curl_easy_cleanup(get_stream_handle);
}

void *user_input() {
	char user_string[100];
	fgets(user_string, 100, stdin);

	if (strcmp(user_string, "quit\n") == 0) {
		exit(0);
	} else if (strcmp(user_string, "\n") == 0) {
		printf("-->");
		user_input();
	} else {
		FILE *history_u = fopen(history_filename, "w");
		fprintf(history_u, "%s", user_string);
		fclose(history_u);

		post(user_string);
	}

	printf("-->");
	user_input();
}

int main(int argc, char *argv[]) {
	base_var_init();

	if (argc == 1) {
		printf("Please use a valid option.\nrun pb-cli -h to see available options\n");
		return 0;
	}

	curl_global_init(CURL_GLOBAL_ALL);

	int opt;
	while ((opt = getopt(argc,argv,"hvp:is")) != -1) {
		switch (opt) {
			case 'h':
				printf("pb-cli v1.0\nCopyright 2018 Rumesh Sudhaharan\n");
				printf("Options:\n");
				printf("\t-h:\tPrint this help screen.\n");
				printf("\t-v:\tPrint version number.\n");
				printf("\t-p:\tsend message to your pushbullet account.\n");
				printf("\t-i:\tGet notified of incoming messages to your pushbullet account.\n");
				printf("\t-s:\tSetup account details.\n");
				break;
			case 'v':
				printf("v1.0\n");
				break;
			case 'p':
				var_init();
				post(optarg);
				break;
			case 'i':
				var_init();
				printf("-->");
				pthread_create(&stream_tid, NULL, get_stream, NULL);
				pthread_create(&user_input_tid, NULL, user_input, NULL);

				pthread_join(stream_tid, NULL);
				pthread_join(user_input_tid, NULL);
				break;
			case 's':
					if (access(token_filename, F_OK) == -1) {
						setup();
					} else {
						printf("Account details already present.\nWould you like to enter new account details?[y/N]\n(this will overwrite the other account and you will have to run this program again to access the previous account)\n");
						char response[1];
						fgets(response, 2, stdin);
						if (strcasecmp(response, "y") == 0) {
							clear_buffer();
							setup();
						} else {
							printf("No changes were made\n");
						}
					}
				break;
			default:
				printf("Please use a valid option.\nrun pb-cli -h to see available options\n");
				break;
		}
	}

	curl_global_cleanup();
	return 0;
}