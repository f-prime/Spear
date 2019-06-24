#include "stdlib.h"
#include "stdio.h"
#include "serializer.h"

void sdump(hashmap *hmap) {
    // Write size of hashmap to file. (# keys)

    FILE *dump = fopen("fist.db", "wb");

    int num_indices = 0;

    for(int i = 0; i < HMAP_SIZE; i++) {
        // Get number of indices have values
        hashmap on = hmap[i];
        if(on.length > 0)
            num_indices++;
    }

    fwrite(&num_indices, sizeof(num_indices), 1, dump);
    // Iterate through hashmap and write key and array of values to file

    for(int i = 0; i < HMAP_SIZE; i++) {
        hashmap on = hmap[i];
        if(on.length > 0) {
            for(int key = 0; key < on.length; key++) {
                keyval object = on.maps[key];
                int length = object.key.length;
                // Writes key length and key name to db file
                fwrite(&length, sizeof(length), 1, dump);
                fwrite(dtext(object.key), object.key.length, 1, dump);

                int num_values = object.values.length;
                // Writes number of values associated with key to db file
                fwrite(&num_values, sizeof(num_values), 1, dump);
                for(int value = 0; value < object.values.length; value++) {
                    // Writes value to db file
                    dstring value_on = object.values.values[value];
                    int val_length = value_on.length;
                    fwrite(&val_length, sizeof(val_length), 1, dump);
                    fwrite(dtext(value_on), value_on.length, 1, dump);
                }
            }
        }
    }
    fclose(dump);
}

hashmap *sload() {
    hashmap *hmap = hcreate();
    FILE *db;
    if((db = fopen("fist.db", "rb"))) {
        int num_keys;
        fread(&num_keys, sizeof(num_keys), 1, db);
        for(int i = 0; i < num_keys; i++) {
            int key_size;
            fread(&key_size, sizeof(key_size), 1, db);
            char key[key_size + 1];
            key[key_size] = 0;
            fread(key, key_size, 1, db);
            int num_vals;
            fread(&num_vals, sizeof(num_vals), 1, db);
            //printf("%d %d %s %d\n", num_keys, key_size, key, num_vals);
            for(int j = 0; j < num_vals; j++) {
                int val_size;
                fread(&val_size, sizeof(val_size), 1, db);
                char value[val_size + 1];
                value[val_size] = 0;
                fread(value, val_size, 1, db);
                //printf("%d %s\n", val_size, value);
                hmap = hset(hmap, dcreate(key), dcreate(value));
            }
        }

        printf("Database file has been loaded. Previous state restored.\n");
	fclose(db);
    } else {
        printf("No previous state found. Creating new database file.\n");
    }

    return hmap;
}
