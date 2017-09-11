#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <dirent.h>

#define MAX_PATH_LEN 256
#define MAX_NAME_LEN 128
#define MAX_HISTORY_NUM 100

#define EXEC 1
#define EXIT 2
#define CD 3
#define HISTORY 4

struct shell {
	char current_path[MAX_PATH_LEN];
	int command_type;
	char **input_args;
	int num_of_args;
	char *output;
} current_shell;

struct history_node {
	struct history_node *next;
	struct history_node *prev;
	char *history_string;
};

struct history_link {	
	struct history_node *head;
	struct history_node *tail;
	int num_of_history;
} current_history;

void delete_node_from_head(void)
{
	struct history_node *node;
	node = current_history.head->next;
	node->next->prev = current_history.head;
	current_history.head->next = node->next;
	free(node);
	current_history.num_of_history--;
}

int add_node(char *str)
{
	struct history_node *node;
	if (current_history.num_of_history == 100) {
		delete_node_from_head();
	}
	if (!(node = malloc(sizeof(struct history_node))))
		return -1;
	if (!(node->history_string = malloc(sizeof(str))))
		return -1;
	strcpy(node->history_string, str);
	node->prev = current_history.tail->prev;
	node->next = current_history.tail;
	node->prev->next = node;
	current_history.tail->prev = node;
	current_history.num_of_history++;
	return 0;
}

char *input_string(FILE *fp, size_t size)
{
	char *str;
	int ch;
	size_t len = 0;
	if (!(str = malloc(sizeof(char) * size)))
		return str;
	while (EOF != (ch = fgetc(fp)) && ch != '\n') {
		str[len++] = ch;
		if (len == size) {
			str = realloc(str, sizeof(char) * (size += 16));
			if (!str)
				return str;
		}
	}
	str[len++] = '\0';
	str = realloc(str, sizeof(char) * len);
	if (add_node(str) == -1) {
		free(str);
		str = NULL;
		return str;
	}
	return str;
}

char** parse_input(char *input, int *num_of_input_args)
{
	int index = 0;
	int max_subs = 5; 
	char **subs;
	if (!(subs = malloc(sizeof(char *) * max_subs)))
		return subs;
	char *str;
	str = strtok(input, " ");
	while (str) {
		subs[index] = malloc(sizeof(str));
		strcpy(subs[index++], str);
		if (index == max_subs) {
			max_subs += 5;
			subs = realloc(subs, sizeof(char *) * max_subs); 
			if (!subs)
				return subs;
		}
		str = strtok(NULL, " ");
	}
	*num_of_input_args = index;
	return subs;
}

int decide_command_type(char *str)
{
	if (strcmp(str, "exit") == 0)
		return EXIT;
	else if (strcmp(str, "cd") == 0)
		return CD;
	else if (strcmp(str, "history") == 0)
		return HISTORY;
	else
		return EXEC;
}

char *execute(char **subs, int *start_index, int *success)
{
	char *output;
	output = malloc(32 * sizeof(char));
	char path[MAX_PATH_LEN];
	pid_t wpid;
	sprintf(path, "%s/%s", current_shell.current_path, subs[*start_index]);
	if (access(path, F_OK) != -1) {
		if (access(path, X_OK) == -1) {
			strcpy(output, "file not executable!");
			*success = 0;
			return output;
		} 
	} else {
		strcpy(output, "file not exist!");
		*success = 0;
		return output;
	}
	int pid = fork();
	if (pid == 0) {
		if (execl(path, subs[*start_index], NULL) == -1)
			printf("execute failed!\n");
		exit(0);
	}
	while ((wpid = wait(NULL)) > 0); //child program ends
	*start_index += 1;
	*success = 1;
	strcpy(output, "success");
	return output;
}

char *change_dir(char **subs, int *start_index, int *success)
{
	char *output;
	DIR *dir = opendir(subs[*start_index]);
	if (dir) {
		strcpy(current_shell.current_path, subs[*start_index]);
		closedir(dir);	
		*success = 1;
	} else if (access(subs[*start_index], F_OK) != -1) {
		output = malloc(32 * sizeof(char));
		strcpy(output, "parameter is a file, not a directory!");
		*success = 0;
	} else {
		output = malloc(32 * sizeof(char));
		strcpy(output, "directory not exist!");
		*success = 0;
	}
	*start_index += 1;
	return output;
}

int initialize_history(void)
{
	current_history.num_of_history = 0;
	if (!(current_history.head = malloc(sizeof(struct history_node))))
		return -1;
	if (!(current_history.tail = malloc(sizeof(struct history_node))))
		return -1;
	current_history.head->next = current_history.tail;
	current_history.tail->prev = current_history.head;
	return 0;
}

void history_all(void)
{
	struct history_node *node = current_history.head->next;
	int index = 0;
	while (index < current_history.num_of_history) {
		printf("%i %s\n", index++, node->history_string);
		node = node->next;
	}
}

void history_clear(void)
{
	struct history_node *node = current_history.head->next;
	while (node != current_history.tail) {
		if (node->history_string)
			free(node->history_string);
		node = node->next;
		free(node->prev);
	}
	current_history.num_of_history = 0;
	current_history.head->next = current_history.tail;
	current_history.tail->prev = current_history.head;
}

char *history(char **subs, int num_of_input_args, int *start_index, int *success)
{
	*start_index += 1;
	if (num_of_input_args <= *start_index)
		history_all();
	else if (strcmp(subs[*start_index], "|") == 0)
		history_all();
	else if (strcmp(subs[*start_index], "-c") == 0)
		history_clear();
	
	return NULL;
}

int main()
{
	char *current_input;
	char **subs;
	int num_of_input_args;
	char *output;
	int success;
	int i;
	if (!getcwd(current_shell.current_path, MAX_PATH_LEN * sizeof(char))) {
		printf("something wrong with getcwd(), bash exits\n");
		exit(0);
	}
	if (initialize_history() == -1)
		goto out_of_memory;
	while (1) {
		printf("$");
		if (!(current_input = input_string(stdin, 64)))
			goto out_of_memory;
		subs = parse_input(current_input, &num_of_input_args);
		int index = 0;
		while (index < num_of_input_args) {
			current_shell.command_type = decide_command_type(subs[index]);
			switch (current_shell.command_type) {
			case EXEC:
				printf("exec\n");
				output = execute(subs, &index, &success);
				//printf("output = %s\n", output);
				break;
			case EXIT:
				printf("exit\n");
				goto graceful_exit;
			case CD:
				printf("cd\n");
				index++;
				output = change_dir(subs, &index, &success);
				//printf("output = %s\n", output);
				break;
			case HISTORY:
				printf("history\n");
				output = history(subs, num_of_input_args, &index, &success);
				break;
			}
			break;
		}
/* 
 * From now, it's for cleaning the momory we alloc for each shell cycle 
 */
		for (i = 0; i < num_of_input_args; i++) {
			free(subs[i]);
		}
		free(subs);
		if (output)
			free(output);
		if (current_input)
			free(current_input);
	}





out_of_memory:
	printf("out_of_memory\n");

graceful_exit:
	for (i = 0; i < num_of_input_args; i++) {
		free(subs[i]);
	}
	free(subs);
	if (output)
		free(output);
	if (current_input)
		free(current_input);


}
