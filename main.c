//
// http://www.isthe.com/chongo/tech/comp/fnv/index.html#FNV-param
//
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

//
// The hash function of choice is the FNV-1a alterate
// algorithm for 32 and 64 bit hashes.
//
// Homepage:
// http://www.isthe.com/chongo/tech/comp/fnv/index.html
//
// Note: FNV-1a uses the same recommended offsets and
// prime factors for 32 and 64 bit hashes as the FNV-1
// algorithm.
//

const uint32_t fnv_prime_32 = 16777619;
const uint32_t fnv_offset_basis_32 = 2166136261;

uint32_t fnv1a_32(const uint8_t *data, size_t length) {
	uint32_t h = fnv_offset_basis_32;

	// For each octet of data to be hashed...
	for (size_t i = 0; i < length; i++)
		h = (h ^ data[i]) * fnv_prime_32;

	return h;
}

const uint64_t fnv_prime_64 = 1099511628211;
const uint64_t fnv_offset_basis_64 = 14695981039346656037ULL;

uint64_t fnv1a_64(const uint8_t *data, size_t length) {
	uint64_t h = fnv_offset_basis_64;

	// For each octet of data to be hashed...
	for (size_t i = 0; i < length; i++)
		h = (h ^ data[i]) * fnv_prime_64;

	return h;
}

//
// What if I need a general length hash (e.g., 24 bits)?
//
// Take a larger hash and xor fold the excess bits
// into the desired range (e.g., 24 bits)
//
// For example, say we want a 24-bit hash:
//
//   const uint32_t mask_24 = (1<<24) - 1;
//                  ^^^^^^^
//                  0xffffff
//
//   uint32_t hash = fnv1a_32(data, size);
//            ^^^^
//         0x5aecf734
//
//   hash = (hash >> 24) ^ (hash & mask_24);
//           ^^^^^^^^^^    
//              0x5a       ^^^^^^^^^^^^^^^^
//   ^^^^                      0xecf734
// 0xecf76e
//
// So the string "Hello, World!" hashes to
// 0xecf76e after xor-folding a 32-bit hash
// into a 24-bit window.
//

void prints32(char *str) {
	printf("hash of \"%s\" => 0x%x\n", str, fnv1a_32((uint8_t *)str, strlen(str)));
}

void prints64(char *str) {
	printf("hash of \"%s\" => 0x%llx\n", str, fnv1a_64((uint8_t *)str, strlen(str)));
}

//
// Dynamic array stores file contents
//
typedef struct {
	size_t count;
	size_t capacity;
	char *content;
} array_t;

//
// View acts like an array but does not "own" its memory.
// It just looks at it - therefore, there is no `capacity` member.
//
typedef struct {
	size_t count;
	char *content;
} view_t;

//
// Key - value data structure.
//
typedef struct {
	view_t key;
	int value;
} keyval_t;

//
// Table data structure acts like a dynamic array of key - values.
//
typedef struct {
	size_t count;
	size_t capacity;
	keyval_t *content;
} table_t;

int view_comp(view_t a, view_t b) {
	size_t m = a.count;
	size_t n = b.count;
	//
	// If the size of the views are not the same, they could not possibly be equal
	//
	if (m != n)
		return 0;
	for (size_t i = 0; i < n; i++)
		if (a.content[i] != b.content[i])
			return 0;
	return 1;
}

//
// Default table parameters recommended by Deepseek
//
const double table_load_factor = 0.7;
const size_t table_scale_factor = 2;

double table_current_load(table_t table) {
	return ((double)table.count) / ((double)table.capacity);
}

int table_needs_to_expand(table_t table) {
	return (table_current_load(table) > table_load_factor) ? 1 : 0;
}

void table_insert(table_t *table, keyval_t bucket) {
	//
	// Compute the 32-bit hash of the bucket's key to get
	// an index into the table content
	//
	uint32_t hash = fnv1a_32((const uint8_t *)bucket.key.content, bucket.key.count);
	uint32_t index = hash % table->capacity;
	//
	// Insert the bucket into the table
	//
	while (1) {
		//
		// Check if the bucket is space is available
		//
		if (table->content[index].key.content == NULL) {
			table->content[index] = bucket;
			table->count++;
			break;
		} else {
			if (view_comp(table->content[index].key, bucket.key))
				//
				// The key is a duplicate. No need to do anything.
				//
				break;
			else
				//
				// The hash collided.
				// Collision resolution is currently accomplished via linear probing
				//
				index = (index + 1) % table->capacity; 
		}
	}
}

int *table_get(table_t *table, view_t key) {
	//
	// Compute the 32-bit hash of the key to get an index into the table
	//
	uint32_t hash = fnv1a_32((const uint8_t *)key.content, key.count);
	uint32_t index = hash % table->capacity;
	//
	// Search for the value in the table
	//
	while (1) {
		//
		// If the value indexed is NULL at any point, then the value
		// does not exist in the table. Return NULL in this case.
		//
		if (table->content[index].key.content == NULL) {
			return NULL;
		} else {
			if (view_comp(table->content[index].key, key))
				//
				// The key exists and we have found it. Return the value.
				//
				return &table->content[index].value;
			else
				//
				// The key may exist, but this is not the correct value.
				// The hash has collided. Continue search by linearly probing.
				//
				index = (index + 1) % table->capacity; 
		}
	}
}

int table_expand(table_t *table) {
	size_t new_capacity = table_scale_factor * table->capacity;
	//
	// Allocate memory for the new table. This storage will
	// replace the tables current contents once the values inside
	// the current storage are rehashed into this new table
	//
	// TODO:
	// This can fail. I do not like that this can fail, but
	// I do not like manually passing in auxillary memory even
	// more. Is there another alterative to these two evils?
	//
	keyval_t *new_content = calloc(new_capacity, sizeof *new_content);
	if (new_content == NULL) {
		fprintf(stderr, "Failed to allocate new space during the table rehashing\n");
		return 1;
	}
	//
	// Linearly step through table and rehash each
	// key value pair. Then place the rehashed key
	// value pair into the new table space.
	//
	for (size_t j = 0; j < table->capacity; j++) {
		keyval_t bucket = table->content[j];
		//
		// Test if the key exists
		//
		if (bucket.key.content != NULL) {
			//
			// Compute the 32-bit hash of the bucket's key to get
			// an index into the content array
			//
			uint32_t hash = fnv1a_32((const uint8_t *)bucket.key.content, bucket.key.count);
			uint32_t index = hash % new_capacity;
			//
			// Insert the bucket into the new content memory at the new index
			//
			while (1) {
				//
				// Check if the bucket is space is available
				//
				if (new_content[index].key.content == NULL) {
					new_content[index] = bucket;
					break;
				} else {
					if (view_comp(new_content[index].key, bucket.key))
						//
						// The key is a duplicate. No need to do anything.
						//
						break;
					else
						//
						// The hash collided.
						// Collision resolution is currently accomplished via linear probing
						//
						index = (index + 1) % table->capacity; 
				}
			}
		}
	}
	//
	// Remove the old table contents and replace
	// the with the rehashed, expanded contents
	//
	free(table->content);
	table->capacity = new_capacity;
	table->content = new_content;
	return 0;
}

view_t cstring_to_view(const char *string) {
	return (view_t){strlen(string), (char *)string};
}

keyval_t create_keyval_from_view(view_t key, int value) {
	return (keyval_t){key, value};
}

keyval_t create_keyval_from_cstring(const char *key, int value) {
	return (keyval_t){cstring_to_view(key), value};
}

//
// Note: to expand the argument of this macro correctly for
// printf, no outer parentesis should be placed around the
// entire expression. For instance, 
//
//             (((int)((v).count)),((v).content))
//
// expands incorrectly. Neat!
//
#define VIEW_FMT(v) ((int)((v).count)),((v).content)

int main(void) {
	const char *filepath = "share/shakespeare.txt";
	FILE *fin = fopen(filepath, "r");
	if (fin == NULL) {
		fprintf(stderr, "Failed to read %s\n", filepath);
		return 1;
	}

	array_t array = {0};

	char c = 0;
	while ((c = getc(fin)) != EOF) {
		//
		// If the number of elements in the
		// array equals or exceeds the array
		// capacity, the array must be expanded
		// before the new value can be added.
		//
		if (array.count >= array.capacity) {
			//
			// The default array size
			//
			if (array.capacity == 0)
				array.capacity = 512;
			//
			// The default array capacity
			// increase rate
			//
			else
				array.capacity *= 2;

			array.content = realloc(array.content, array.capacity * sizeof *array.content);

			if (array.content == NULL) {
				fclose(fin);
				fprintf(stderr, "reallocation of the dynamic array contents failed\n");
				return 1;
			}
		}

		array.content[array.count++] = c;
	}

	fclose(fin);

	//
	// Display dynamic array contents as single words
	//
	view_t view = {0};

	const size_t table_default_size = 256;
	table_t table = {
		.count = 0,
		.capacity = table_default_size,
		.content = NULL
	};

	table.content = calloc(table.capacity, sizeof *table.content);
	if (table.content == NULL) {
		fprintf(stderr, "Failed to allocate table\n");
		free(array.content);
		return 1;
	}

	for (size_t i = 0; i < array.count; i++) {
		char c = array.content[i];

		switch (c) {
			//
			// White space characters
			//
			case ' ':
			case '\t':
			case '\n':
				//
				// If the incomming view is NULL,
				// then there is nothing to hash
				//
				if (view.count == 0)
					break;
				//
				// Otherwise, put the view into the table
				//
				// If the tables current load factor is
				// greater than some threshold `table_load_factor`,
				// the table needs to be reallocated and rehashed
				// before the enxt insertion.
				//
				if (table_needs_to_expand(table)) {
					int ret = table_expand(&table);
					if (ret) {
						fprintf(stderr, "Failed to expand the table\n");
						free(array.content);
						free(table.content);
						return 1;
					}
				}
				//
				// Insert the new value into the table if it
				// does not already exist.
				//
				keyval_t bucket = create_keyval_from_view(view, (int)view.count);
				table_insert(&table, bucket);
				//
				// Reset the view
				//
				view.count = 0;
				view.content = NULL;
				break;
			//
			// Non white space characters
			//
			default:
				if (view.count == 0)
					view.content = array.content + i;
				view.count++;
				break;
		}
	}
	//
	// Test table indexing
	//
	view_t key = cstring_to_view("water");
	int *value = table_get(&table, key);
	if (value == NULL)
		//printf("The key \"%.*s\" does not exist in the table\n", (int)key.count, key.content);
		printf("The key \"%.*s\" does not exist in the table\n", VIEW_FMT(key));
	else
		printf("The key \"%.*s\" is associated with the value %d\n", VIEW_FMT(key), *value);
	//
	// Cleanup
	//
	free(array.content);
	free(table.content);
	return 0;
}
