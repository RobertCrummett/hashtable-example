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
const uint64_t fnv_offset_basis_64 = 14695981039346656037;

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
	printf("hash of \"%s\" => 0x%zx\n", str, fnv1a_64((uint8_t *)str, strlen(str)));
}

//
// Dynamic array stores file contents
//
typedef struct {
	size_t count;
	size_t capacity;
	char *content;
} array_t;

typedef struct {
	size_t count;
	char *content;
} view_t;

typedef struct {
	view_t key;
	int value;
} keyval_t;

typedef struct {
	size_t count;
	size_t capacity;
	keyval_t *content;
} table_t;

int view_comp(view_t a, view_t b) {
	size_t m = a.count;
	size_t n = b.count;
	
	if (m != n)
		return 0;

	for (size_t i = 0; i < n; i++)
		if (a.content[i] != b.content[i])
			return 0;

	return 1;
}

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
				array.capacity = 8;
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

	const double table_load_factor = 0.75;
	const size_t table_scale_factor = 2;

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

				//
				// If the tables current load factor is
				// greater than some threshold `table_load_factor`,
				// the table needs to be reallocated and rehashed
				// before the enxt insertion.
				//
				printf("Table Load Factor: %lf\n", ((double)table.count) / table.capacity);
				if (table_load_factor < ((double)table.count) / table.capacity) {
					size_t new_capacity = table_scale_factor * table.capacity;
					keyval_t *new_content = calloc(new_capacity, sizeof *new_content);
					if (new_content == NULL) {
						fprintf(stderr, "Failed to allocate new space during the table rehashing\n");
						free(array.content);
						free(table.content);
						return 1;
					}

					//
					// Linearly step through table and rehash each
					// key value pair. Then place the rehashed key
					// value pair into the new table space.
					//
					for (size_t j = 0; j < table.capacity; j++) {
						keyval_t bucket = table.content[j];
						//
						// Test if the key exists
						//
						if (bucket.key.content != NULL) {
							uint32_t hash = fnv1a_32(bucket.key.content, bucket.key.count);
							uint32_t index = hash % new_capacity;

							new_content[index] = bucket;
						}
					}

					//
					// Remove the old table contents and replace
					// the with the rehashed, expanded contents
					//
					free(table.content);

					table.capacity = new_capacity;
					table.content = new_content;
				}

				// size_t str_size = array.content + i - str_view;
				uint32_t hash = fnv1a_32(view.content, view.count);

				uint32_t index = hash % table.capacity;

				// printf("%.*s => %u\n", (int)(view.count), view.content, index);

				//
				// If the key does not exist, add it to
				// the table and move on
				//
				if (table.content[index].key.content == NULL) {
					table.count++;
					table.content[index].key = view;
					table.content[index].value = 1;
					printf("Inserted!\n");
				//
				// Otherwise, either the key is a duplicate
				// or the hash has collided. Report this.
				//
				} else {
					if (view_comp(table.content[index].key, view))
						printf("The value \"%.*s\" was already in the table [%.*s].\n", (int)(view.count), view.content, (int)(table.content[index].key.count), table.content[index].key.content);
					else
						printf("The hash collided!\n");
				}
				
				view.count = 0;
				break;
			//
			// Letters
			//
			default:
				if (view.count == 0)
					view.content = array.content + i;
				view.count++;
				break;
		}
		
			
	}

	free(array.content);
	free(table.content);

	return 0;
}
