#include "server.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include "bst.h"
#include "config.h"
#include "dstring.h"
#include "hashmap.h"
#include "indexer.h"
#include "serializer.h"
#include "server.h"
#include "utils.h"
#include "version.h"

#define READ_MAX 1024

#define BYE "Bye\n"
#define INDEXED "Text has been indexed\n"
#define INVALID_COMMAND "Invalid command\n"
#define NOT_FOUND "[]\n"
#define TOO_FEW_ARGUMENTS "Too few arguments\n"

typedef int (*command_handler_t)(struct config *config, hashmap *hm, int fd, dstringa params);

static const int YES = 1;

static int dirty = 0;
static volatile int running = 1;
static volatile int should_save = 0;
static struct bst_node *command_tree;

struct connection_info
{
    dstring last_command;
};

static int do_delete(struct config *config, hashmap *hm, int fd, dstringa params) {
    send(fd, "Not implemented\n", 16, 0);
    return 0;
}

static int do_exit(struct config *config, hashmap *hm, int fd, dstringa params) {
    send(fd, "Bye\n", 4, 0);
    return 1;
}

static int do_index(struct config *config, hashmap *hm, int fd, dstringa params) {
    if(params.length < 3) {
        send(fd, TOO_FEW_ARGUMENTS, strlen(TOO_FEW_ARGUMENTS), 0);
        return 0;
    }
    dstring document = params.values[1];
    dstring text = djoin(drange(params, 2, params.length), ' ');
    dstringa index = indexer(text, config->max_phrase_length);
    printf("INDEX SIZE: %d\n", index.length);
    for(int i = 0; i < index.length; i++) {
        dstring on = index.values[i];
        hm = hset(hm, on, document);
    }
    dirty = 1;
    send(fd, INDEXED, strlen(INDEXED), 0);
    return 0;
}

static int do_search(struct config *config, hashmap *hm, int fd, dstringa params) {
    if(params.length < 2) {
        send(fd, TOO_FEW_ARGUMENTS, strlen(TOO_FEW_ARGUMENTS), 0);
        return 0;
    }
    dstring text = djoin(drange(params, 1, params.length), ' ');
    dstringa value = hget(hm, text);
    if(value.length == 0) {
        send(fd, NOT_FOUND, strlen(NOT_FOUND), 0);
        return 0;
    }
    dstring output = dcreate("[");
    for(int i = 0; i < value.length; i++) {
        dstring on = value.values[i];
        output = dappendc(output, '"');
        output = dappendd(output, on);
        output = dappendc(output, '"');
        if(i != value.length - 1) {
            output = dappendc(output, ',');
        }
    }
    output = dappendc(output, ']');
    output = dappendc(output, '\n');
    send(fd, dtext(output), output.length, 0);
    return 0;
}

static int do_version(struct config *config, hashmap *hm, int fd, dstringa params) {
    dstring output = dcreate(VERSION);
    output = dappendc(output, '\n');
    send(fd, dtext(output), output.length, 0);
    return 0;
}

static int process_command(struct config *config, hashmap *hm, int fd, dstring req) {
    dstringa commands;
    dstring trimmed;
    command_handler_t handler;

    trimmed = dtrim(req);
    commands = dsplit(trimmed, ' ');
    printf("%d '%s'\n", req.length, dtext(trimmed));

    handler = (command_handler_t)bst_search(command_tree, dtext(commands.values[0]));
    if(!handler) {
        send(fd, INVALID_COMMAND, strlen(INVALID_COMMAND), 0);
        return 0;
    }

    return handler(config, hm, fd, commands);
}

static void sighandler_alarm(int signum) {
    should_save = 1;
}

static void sighandler_int(int signum) {
    running = 0;
}

static void install_sighandlers(struct config *config) {
    struct sigaction sa_int;
    sa_int.sa_flags = 0;
    sa_int.sa_handler = sighandler_int;
    sigemptyset(&(sa_int.sa_mask));
    sigaddset(&(sa_int.sa_mask), SIGINT);
    sigaction(SIGINT, &sa_int, NULL);

    if(config->save_period > 0) {
        struct sigaction sa_alrm;
        sa_alrm.sa_flags = 0;
        sa_alrm.sa_handler = sighandler_alarm;
        sigemptyset(&(sa_alrm.sa_mask));
        sigaddset(&(sa_alrm.sa_mask), SIGALRM);
        sigaction(SIGALRM, &sa_alrm, NULL);
        alarm(config->save_period);
    }
}

int start_server(struct config *config) {
    char buf[READ_MAX];
    struct sockaddr_in client_addr;
    struct connection_info *connection_infos;
    int dtablesize;
    int fd_max;
    hashmap *hm;
    fd_set master_fds;
    fd_set copy_fds;
    int rc = 0;
    struct sockaddr_in server_addr;
    int server_fd;

    command_tree = NULL;
    // not a self balancing tree, be mindful of the order
    bst_insert(&command_tree, "INDEX", do_index);
    bst_insert(&command_tree, "EXIT", do_exit);
    bst_insert(&command_tree, "SEARCH", do_search);
    bst_insert(&command_tree, "DELETE", do_delete);
    bst_insert(&command_tree, "VERSION", do_version);

    dtablesize = getdtablesize();
    connection_infos = calloc(dtablesize, sizeof(struct connection_info));

    install_sighandlers(config);

    hm = sload(); // Loads database file if it exists, otherwise returns an empty hash map

    FD_ZERO(&copy_fds);
    FD_ZERO(&master_fds);
    memset(buf, 0, READ_MAX);
    memset(&client_addr, 0, sizeof(struct sockaddr_in));
    memset(&server_addr, 0, sizeof(struct sockaddr_in));

    if((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        rc = -1;
        goto exit;
    }

    if(setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &YES, sizeof(int)) == -1) {
        perror("setsockopt");
        rc = -1;
        goto exit;
    }

    // TODO: respect host parameter
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    if(!inet_aton(dtext(config->host), &server_addr.sin_addr)) {
        perror("inet_aton");
        rc = -1;
        goto exit;
    }
    server_addr.sin_port = htons(config->port);
    if(bind(server_fd, (struct sockaddr *)&server_addr, sizeof(struct sockaddr_in)) == -1) {
        perror("bind");
        rc = -1;
        goto exit;
    }

    if(listen(server_fd, config->so_backlog) == -1) {
        perror("listen");
        rc = -1;
        goto exit;
    }

    printf("Fist started at %s:%d\n", inet_ntoa(server_addr.sin_addr), config->port);

    FD_SET(server_fd, &master_fds);
    fd_max = server_fd;

    while(running) {
        int i;

        if(should_save) {
            should_save = 0;
            if(dirty) {
                dirty = 0;
                // puts("Saving db...");
                sdump(hm);
            }
            alarm(config->save_period);
        }

        copy_fds = master_fds;

        if(select(fd_max + 1, &copy_fds, NULL, NULL, NULL) == -1) {
            if(errno != EINTR)
                perror("select");
            continue;
        }

        for(i = 0; i <= fd_max; i++) {
            if(FD_ISSET(i, &copy_fds)) {
                if(i == server_fd) {
                    socklen_t addrlen = sizeof(struct sockaddr_in);
                    int new_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addrlen);
                    if(new_fd == -1) {
                        perror("accept");
                        continue;
                    }
                    FD_SET(new_fd, &master_fds);
                    fd_max = MAX(new_fd, fd_max);
                    connection_infos[new_fd].last_command = dempty();
                } else {
                    // -1 to preserve final NULL
                    int nbytes = recv(i, buf, READ_MAX - 1, 0);
                    buf[nbytes] = '\0';
                    if(nbytes <= 0) {
                        if(nbytes < 0) {
                            perror("recv");
                        }
                        close(i);
                        FD_CLR(i, &master_fds);
                        dfree(connection_infos[i].last_command);
                    } else {
                        struct connection_info *this = &connection_infos[i];
                        int found_bs_r = 0;
                        for(int j = 0; j < nbytes; j++) {
                            char on = buf[j];
                            this->last_command = dappendc(this->last_command, on);
                            if(on == '\r') {
                                found_bs_r = 1;
                            } else if(on == '\n' && found_bs_r) {
                                int should_close =
                                    process_command(config, hm, i, this->last_command);
                                this->last_command = dempty();
                                if(should_close) {
                                    close(i);
                                    FD_CLR(i, &master_fds);
                                    dfree(connection_infos[i].last_command);
                                    break;
                                }
                            } else {
                                found_bs_r = 0;
                            }
                        }
                    }
                }
            }
        }
    }
    sdump(hm);
exit:
    bst_free(command_tree);
    free(connection_infos);
    puts("Exiting cleanly...");
    return rc;
}
