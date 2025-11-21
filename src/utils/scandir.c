#include "scandir.h"

#ifdef _WIN32
int scandir(const char *dirp, struct dirent ***namelist, int (*filter)(const struct dirent *),
            int (*compar)(const struct dirent **, const struct dirent **))
{
	DIR *dir = opendir(dirp);
	if (!dir)
		return -1;

	struct dirent *entry;
	struct dirent **list = NULL;
	size_t count = 0;

	while ((entry = readdir(dir)) != NULL) {
		if (filter && !filter(entry))
			continue;

		struct dirent *copy = malloc(sizeof(*entry));
		if (!copy) {
			// free already allocated entries
			for (size_t i = 0; i < count; i++)
				free(list[i]);
			free(list);
			closedir(dir);
			return -1;
		}
		memcpy(copy, entry, sizeof(*entry));

		struct dirent **tmp = realloc(list, (count + 1) * sizeof(struct dirent *));
		if (!tmp) {
			free(copy);
			for (size_t i = 0; i < count; i++)
				free(list[i]);
			free(list);
			closedir(dir);
			return -1;
		}
		list = tmp;
		list[count++] = copy;
	}
	closedir(dir);

	if (compar) {
		qsort(list, count, sizeof(struct dirent *), (int (*)(const void *, const void *))compar);
	}

	*namelist = list;
	return (int)count;
}

int alphasort(const struct dirent **a, const struct dirent **b) { return strcoll((*a)->d_name, (*b)->d_name); }
#endif