#include <sys/wait.h>
#include <sys/mount.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sched.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>

#define STACKSIZE (1024 * 1024)
#define SANDBOX_ROOT "/var/lib/danbo"
#define SANDBOX_VAR_RUN  "/var/run/danbo"
#define LAYER_ROOT SANDBOX_ROOT "/root"

typedef struct layer_list {
	char *name;
	struct layer_list *next;
} layer_list_t;

struct layer_data {
	uint32_t options;
	layer_list_t *layers;
	char name[65];
};

layer_list_t * get_layer_list() {
	FILE *f = fopen(SANDBOX_ROOT "/layers", "r");
	if (f == NULL) {
		return NULL;
	}

	layer_list_t *root = NULL;
	layer_list_t *n = root;
	char *line = NULL;
	size_t len = 0;
	size_t read = 0;
	while ((read = getline(&line, &len, f)) != -1) {
		layer_list_t *nn = malloc(sizeof(layer_list_t));
		line[read - 1] = 0;
		nn->name = line;
		nn->next = NULL;
		if (root == NULL) {
			root = nn;
		} else {
			n->next = nn;
		}
		n = nn;
		line = NULL;
		len = 0;
	}

	fclose(f);

	return root;
}

layer_list_t * add_layer(layer_list_t *n, const char *name) {
	layer_list_t *nn = malloc(sizeof(layer_list_t));
	nn->name = malloc(strlen(name) + 1);
	strcpy(nn->name, name);
	nn->next = NULL;

	if (n == NULL) {
		return nn;
	} else {
		layer_list_t *i = n;
		while (i->next != NULL) {
			i = i->next;
		}
		i->next = nn;
		return n;
	}
}

void print_layers(layer_list_t *n) {
	printf("base");
	while (n != NULL) {
		printf(" -> %s", n->name);
		n = n->next;
	}
	printf("\n");
}

char * build_aufs_options(layer_list_t *n) {
	if (n == NULL) return "";

	char *ret = malloc(4096);

	char *nextstr = build_aufs_options(n->next);
	sprintf(ret, "%s:" SANDBOX_ROOT "/layers/%s", nextstr, n->name);
	if (nextstr[0] != 0) {
		free(nextstr);
	}

	return ret;
}

static int layer_child(void *arg) {
	int ret;
	char data_path[256];
	char mount_options[4096];
	struct layer_data *data = (struct layer_data *) arg;

	sprintf(data_path, SANDBOX_ROOT "/layers/%s", data->name);
	mkdir(data_path, 0755);

	char *aufs_layers = build_aufs_options(data->layers);
	sprintf(mount_options, "br%s:" SANDBOX_ROOT "/base", aufs_layers);
	free(aufs_layers);
	ret = mount("aufs", LAYER_ROOT, "aufs", 0, mount_options);
	if (ret == -1) {
		perror("mounting aufs");
		exit(1);
	}

	ret = mount(NULL, LAYER_ROOT, NULL, MS_UNBINDABLE, NULL);
	if (ret == -1) {
		perror("making root unbindable");
		exit(1);
	}

	mkdir(LAYER_ROOT SANDBOX_ROOT, 0755);

	ret = mount(SANDBOX_ROOT, LAYER_ROOT SANDBOX_ROOT, NULL, MS_BIND | MS_REC, NULL);
	if (ret == -1) {
		perror("bind mounting " SANDBOX_ROOT);
		exit(1);
	}

	ret = chroot(LAYER_ROOT);
	if (ret == -1) {
		perror("chroot");
		exit(1);
	}
	chdir("/");

	ret = mount("proc", "/proc", "proc", 0, "");
	if (ret == -1) {
		perror("mounting proc");
		exit(1);
	}

	ret = mount("sysfs", "/sys", "sysfs", 0, "");
	if (ret == -1) {
		perror("mounting sysfs");
		exit(1);
	}

	ret = mount("devtmpfs", "/dev", "devtmpfs", 0, "");
	if (ret == -1) {
		perror("mounting devtmpfs");
		exit(1);
	}

	ret = mount("devpts", "/dev/pts", "devpts", 0, "");
	if (ret == -1) {
		perror("mounting devpts");
		exit(1);
	}

	mkdir(SANDBOX_VAR_RUN, 0700);

	FILE *f = fopen(SANDBOX_VAR_RUN "/layers", "w");
	fchmod(fileno(f), 0400);
	layer_list_t *node = data->layers;
	do {
		fprintf(f, "%s\n", node->name);
		node = node->next;
	} while (node != NULL);
	fclose(f);

	if (execl("/bin/sh", "sh", NULL) == -1) {
		perror("execl");
		exit(1);
	}
}

void generate_random_id(char id[]) {
	int i;

	for (i = 0; i < 8; i++) {
		id[i] = 97 + random() % 26;
	}

	id[8] = 0;
}

int main(int argc, char *argv[]) {
	pid_t pid = 0;
	char *stack = malloc(STACKSIZE);
	char *stackTop = stack + STACKSIZE;
	char id[9];

	int fd = open("/dev/urandom", O_RDONLY);
	uint32_t seed;
	read(fd, &seed, 4);
	close(fd);
	srandom(seed);

	/* I'm playing pretty fast and loose with this on the assumption that
	 * it will be trashed when the process exits. */
	struct layer_data *data = (struct layer_data *) malloc(sizeof(struct layer_data));
	data->options = 0;
	generate_random_id(data->name);
	data->layers = get_layer_list();

	data->layers = add_layer(data->layers, data->name);

	print_layers(data->layers);

	pid = clone(layer_child, stackTop, CLONE_NEWNS | CLONE_NEWPID, (void *) data);
	if (pid == -1) {
		perror("clone");
		exit(1);
	}

	int status;
	do {
		if (waitpid(pid, &status, __WALL) == -1) {
			perror("waitpid");
			exit(1);
		}

		if (WIFEXITED(status) && WEXITSTATUS(status) > 0) {
			fprintf(stderr, "Child exited %d\n", WEXITSTATUS(status));
		} else if (WIFSIGNALED(status)) {
			fprintf(stderr, "Child killed by signal %d\n", WTERMSIG(status));
		}
	} while (!WIFEXITED(status) && !WIFSIGNALED(status));

	layer_list_t *n = data->layers;
	layer_list_t *np;
	while (n->next != NULL) {
		np = n;
		n = n->next;
	}
	fprintf(stderr, "Exited layer %s\n", n->name);
	if (np == NULL) {
		printf("No more layers\n");
	} else {
		/* "pop" the last layer and print layers */
		np->next = NULL;
		print_layers(data->layers);
	}

	return 0;
}
