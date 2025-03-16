#include <stdio.h>
#include <stdlib.h>

typedef struct {
	char *key;
	int value;
} item;

int cmp(const void *a, const void *b) {
	item *item_a = (item *)a;
	item *item_b = (item *)b;
	return strcmp(item_a->key, item_b->key);
}

item* binary_search(item *items, size_t size, const char *key) {
	size_t low = 0;
	size_t high = size;

	while (low < high) {
		size_t mid = low + (high - low) / 2;

		int c = strcmp(items[mid].key, key);

		if (c == 0)
			return items + mid;
		else if (c < 0)
			low = mid + 1;
		else
			high = mid;
	}
	return NULL;
}

int main(int argc, char **argv) {
	if (argc != 2)
		return 1;

	item items[] = {
		{"bar", 42}, {"bazz", 36},
		{"bob", 11}, {"buzz", 7},
		{"foo", 10}, {"jane", 100},
		{"x", 200}
	};
	size_t num_items = sizeof(items) / sizeof(item);

	item key = {argv[1], 0};
	item* found = bsearch(&key, items, num_items, sizeof(item), cmp);

	if (found == NULL)
		return 1;
	printf("bsearch: value of '%s' is %d\n", found->key, found->value);

	found = binary_search(items, num_items, argv[1]);
	if (found == NULL)
		return 1;
	printf("binary_search: value of '%s' is %d\n", found->key, found->value);

	return 0;
}
