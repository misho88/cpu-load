#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <time.h>

struct stat {
	char name[8];
	long user, nice, system, idle;
};

struct stats {
	struct stat * data;
	ssize_t size, capacity;
};

struct load {
	char name[8];
	double load;
};

struct loads {
	struct load * data;
	ssize_t size, capacity;
};

struct stat
get_stat(FILE * file)
{
	struct stat stat = { 0 };
	int rv = fscanf(file, "%7s", stat.name);
	if (rv == EOF || errno) {
		perror("fscanf()");
		exit(errno ? -errno : 1);
	}
	if (strncmp(stat.name, "cpu", 3) != 0) {
		return (struct stat){ 0 };
	}
	rv = fscanf(file, "%ld %ld %ld %ld %*[^\n]", &stat.user, &stat.nice, &stat.system, &stat.idle);
	if (rv == EOF || errno) {
		perror("fscanf()");
		exit(errno ? -errno : 1);
	}
	
	return stat;
}

struct stats
get_stats(struct stats stats)
{
	FILE * file = fopen("/proc/stat", "r");
	if (file == NULL) {
		fprintf(stderr, "/proc/stat not accessible\n");
		exit(1);
	}

	for (stats.size = 0; ; stats.size++) {
		struct stat stat = get_stat(file);
		if (stat.name[0] == '\0') break;

		if (stats.capacity <= stats.size) {
			stats.capacity = stats.size + 1;
			stats.data = realloc(stats.data, stats.capacity * sizeof(struct stat));
			if (stats.data == NULL || errno) {
				perror("realloc()");
				exit(errno ? -errno : 1);
			}
		}
		stats.data[stats.size] = stat;
	}

	fclose(file);
	return stats;
}

struct load
get_load(struct stat a, struct stat b)
{
	long a_total = a.user + a.nice + a.system;
	long b_total = b.user + b.nice + b.system;
	long a_total_diff = b_total - a_total;
	long a_idle_diff = b.idle - a.idle;
	
	struct load load = { 0 };
	strncpy(load.name, a.name, 8);
	load.load = a_total_diff ? (double)a_total_diff / (a_total_diff + a_idle_diff) : 0.0;
	return load;
}

struct loads
get_loads(struct stats as, struct stats bs, struct loads loads)
{
	if (loads.capacity < as.size) {
		loads.capacity = as.size;
		loads.data = realloc(loads.data, loads.capacity * sizeof(struct load));
		if (loads.data == NULL || errno) {
			perror("realloc()");
			exit(errno ? -errno : 1);
		}
	}
	for (loads.size = 0; loads.size < as.size; loads.size++) {
		loads.data[loads.size] = get_load(as.data[loads.size], bs.data[loads.size]);
	}
	return loads;
}

void swap(struct stats * a, struct stats * b) { struct stats t = *a; *a = *b; *b = t; }

double
get_time()
{
	struct timespec spec;
	int rv = timespec_get(&spec, TIME_UTC);
	if (rv == 0 || errno) {
		perror("clock_gettime()");
		exit(errno ? -errno : 1);
	}
	return spec.tv_sec + 1e-9 * spec.tv_nsec;
}

int
main(int argc, char ** argv)
{
	if (argc > 3) {
		fprintf(stderr, "usage: %s [period in seconds] [repeat count]", argv[0]);
		exit(1);
	}

	long repeat = -1;
	if (argc > 2) {
		char * endptr;
		repeat = strtol(argv[2], &endptr, 10);
		if (argv[2][0] == '\0' || endptr[0] != '\0' || repeat < 1) {
			fprintf(stderr, "%s: repeat count: '%s' is not a positive integer\n", argv[0], argv[2]);
			exit(1);
		}
	}

	double period = 1.0;
	if (argc > 1) {
		char * endptr;
		period = strtod(argv[1], &endptr);
		if (argv[1][0] == '\0' || endptr[0] != '\0' || period <= 0.0) {
			fprintf(stderr, "%s: repeat count: '%s' is not a positive number\n", argv[0], argv[1]);
			exit(1);
		}
	}
	

	struct stats as = { 0 }, bs = { 0 };
	struct loads loads = { 0 };

	as = get_stats(as);
	double start = get_time();
	for (long i = 1; repeat < 0 || i <= repeat; i++) {
		double delay = start + i * period - get_time();
		if (delay > 0)
			usleep((useconds_t)(delay * 1000000));
		else
			start = get_time();
		bs = get_stats(bs);
		loads = get_loads(as, bs, loads);
		swap(&as, &bs);

		for (ssize_t j = 0; j < loads.size; j++)
			printf("%8.6lf ", loads.data[j].load);
		fputc('\n', stdout);
		fflush(stdout);
	}
}
