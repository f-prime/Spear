#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "hashmap.h"
#include "indexer.h"
#include "server.h"
#include "tests.h"
#include "version.h"

int main(int argc, char *argv[]) {
    if(argc == 2) {
        if(!strcmp(argv[1], "test")) {
            run_tests();
        } else if(!strcmp(argv[1], "version")) {
            printf("%s\n", VERSION);
        }
        return 0;
    }

    struct config config;
    config_parse(NULL, &config);

    return start_server(&config);
}
