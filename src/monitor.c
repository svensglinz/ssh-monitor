#include <systemd/sd-journal.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "hashmap.h"
#include <sqlite3.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include "args.h"

#define MAX_BLOCK_TIME (args->t)
#define MAX_ATTEMPT (args->n)

// global variables
static struct hashmap *login_map;
static sqlite3 *db;
static sd_journal *journal;

static pthread_mutex_t lock;

struct hash_pair {
    char *key;
    int attempts;
    time_t last_attempt;
};

void log_attempt(char *ip_addr, int success);

// static variables
const static char *create_tbl_cmd = "CREATE TABLE IF NOT EXISTS logins(addr VARCHAR(20) PRIMARY KEY, \
count INT, last_attempt DATE, last_success DATE)";

const static char *insert_query = "INSERT INTO logins (addr, count, last_attempt, last_success) \
    VALUES ('%s', 1, datetime('now'), %s) \
    ON CONFLICT(addr) DO UPDATE SET \
    count = count + 1, \
    last_attempt = datetime('now'), \
    last_success = %s;";

// custom hash function and comparator for hashmap
// djb2 http://www.cse.yorku.ca/%7Eoz/hash.html

uint64_t hash_fn(void *key) {
    const char *k = *(const char **)key;
    unsigned long hash = 5381;
    int c;
    while ((c = (int) *k++))
        hash = ((hash << 5) + hash) + c;
    return hash;
}

time_t get_cur_time() {
    time_t t;
    time(&t);
    return t;
}

int comp_fn(void *a, void *b) {
    const char *fst = ((struct hash_pair*)a)->key;
    const char *snd = ((struct hash_pair*)b)->key;
    return strncmp(fst, snd, strlen(snd)) == 0 ? 1 : 0;
}

void cleanup_fun(void *elem) {
    struct hash_pair *p = elem;
    free(p->key);
}

struct args *args;

/*
 * Environ variables:
 * DB_PATH
 */

// initialize database, journal and hashmap
void init() {
    // open database
    const char *db_path = getenv("DB_PATH");
    char path[256];
    if (db_path) {
        snprintf(path, sizeof(path), "%s/ip_logins.db", db_path);
    } else {
        fprintf(stderr, "db path not specified\n");
        exit(EXIT_FAILURE);
    }
    if (sqlite3_open(path, &db) != SQLITE_OK) {
        fprintf(stderr, "%s", "error opening database\n");
        exit(EXIT_FAILURE);
    }

    char *errmsg;
    if (sqlite3_exec(db, create_tbl_cmd, NULL, NULL, &errmsg) != SQLITE_OK) {
        fprintf(stderr, "%s", errmsg);
        exit(EXIT_FAILURE);
    }

    // initialize journal
    if (sd_journal_open(&journal, SD_JOURNAL_LOCAL_ONLY) < 0) {
        fprintf(stderr, "opening journal failed");
        exit(EXIT_FAILURE);
    }
    sd_journal_add_match(journal, "SYSLOG_IDENTIFIER=sshd", 0);
    sd_journal_seek_tail(journal);

    // initialize hash map
    struct hashmap_params params = {
        .cmp_fun = comp_fn,
        .hash_fun = hash_fn,
        .obj_size = sizeof(struct hash_pair),
        .cleanup_fun = cleanup_fun
    };

    login_map = hashmap_init(16, &params);

    // initialize block list
    char cmd_buf[100];
    snprintf(cmd_buf, sizeof(cmd_buf),
             "{ ipset list ssh_blocklist || ipset create ssh_blocklist hash:ip timeout %d; } > /dev/null 2>&1",
             MAX_BLOCK_TIME);
    if (system(cmd_buf) < 0) {
        exit(EXIT_FAILURE);
    }

    // add rule to iptables to block all incoming requests from the ip addresses in ssh_blocklist
    if (system(
        "iptables -C INPUT -m set --match-set ssh_blocklist src -j DROP || iptables -A INPUT -m set --match-set ssh_blocklist src -j DROP") < 0) {
        exit(EXIT_FAILURE);
    }
}

void check_for_event() {
    const void *data;
    size_t length;
    if (sd_journal_get_data(journal, "MESSAGE", &data, &length) >= 0) {
        // extract IP address if login attempt via password
        if (strstr((char *) data, "password")) {
            // check if login was accepted or not
            int accepted = strstr((char *) data, "Accepted") != NULL;
            const char *ip_start = strstr((char *) data, "from");

            // extract IP addr
            ip_start += 5;
            const char *ip_end = strpbrk(ip_start, " \n");
            const size_t ip_len = ip_end - ip_start;
            char ip_addr[ip_len + 1];
            strncpy(ip_addr, ip_start, ip_len);
            ip_addr[ip_len] = '\0';
            log_attempt(ip_addr, accepted);
        }
    }
}

// adds an ip to the block list and print info to stdout
void block_ip(const char *ip_addr) {
    static char buff[70];
    snprintf(buff, sizeof(buff), "ipset add ssh_blocklist %s", ip_addr);
    if (system(buff) < 0) {
        exit(EXIT_FAILURE);
    }

    // print log message
    time_t cur_time;
    time(&cur_time);

    char time_buf[26];
    strftime(time_buf, 26, "%Y/%m/%d %H:%M:%S", localtime(&cur_time));
    printf("[%s] Blocked: %s\n", time_buf, ip_addr);
}

// log attempt into database
void log_attempt(char *ip_addr, const int success) {
    const char *last_success = success ? "datetime('now')" : "last_success";
    const char *cur_success = success ? "datetime('now')" : "NULL";

    char query_buf[450];
    snprintf(query_buf, sizeof(query_buf), insert_query, ip_addr, cur_success, last_success);

    // record attempt in database
    char *err_msg;
    if (sqlite3_exec(db, query_buf, NULL, NULL, &err_msg) != SQLITE_OK) {
        fprintf(stderr, "%s", "error inserting log into database\n");
        fprintf(stderr, "%s\n", err_msg);
        exit(EXIT_FAILURE);
    }

    // if attempt was successful and is in map, remove
    pthread_mutex_lock(&lock);
    if (success) {
        hashmap_remove(login_map, &ip_addr);
        return;
    }

    // if attempt was unsuccessful, add to map
    printf("logging attempt by %s\n", ip_addr);

    struct hash_pair *e = hashmap_get(login_map, &ip_addr);
    time_t cur_time = get_cur_time();
    if (e != NULL) {
        e->attempts++;
        // entry has not been removed by cleaning thread but should be reset
        if (e->attempts >= MAX_ATTEMPT &&  cur_time - e->last_attempt >= MAX_BLOCK_TIME) {
            e->attempts = 1;
            e->last_attempt = cur_time;
        } else if (e->attempts >= MAX_ATTEMPT){
            block_ip(ip_addr);
        }
    } else {
        // first attempt
        size_t len = strlen(ip_addr);
        char * const ip = malloc(sizeof(char) * (len + 1));
        strncpy(ip, ip_addr, len + 1);
        struct hash_pair p = {.key = ip, .attempts = 1, .last_attempt = get_cur_time()};
        hashmap_insert(login_map, &p);
    }
    pthread_mutex_unlock(&lock);
}

void *clean_map() {
    while (1) {
        pthread_mutex_lock(&lock);
        time_t cur_time = get_cur_time();
        size_t len = login_map->len;
        struct hash_node *cur;

        for (size_t i = 0; i < len; i++) {
            cur = login_map->elems[i];
            while (cur != NULL) {
                struct hash_pair *p = (struct hash_pair *) cur->elem;
                if (cur_time - p->last_attempt >= MAX_BLOCK_TIME) {
                    cur = cur->next;
                    hashmap_remove(login_map, &p->key);
                } else {
                    cur = cur->next;
                }
            }
        }
        pthread_mutex_unlock(&lock);
        sleep(MAX_BLOCK_TIME);
    }
}

int main(int argc, char **argv) {
    pthread_mutex_init(&lock, NULL);
    args = parse_args(argc, argv);
    init();
    // start cleanup thread
    pthread_t cleanup_thread;
    pthread_create(&cleanup_thread, NULL, clean_map, NULL);

    // monitor threads
    while (1) {
        sd_journal_wait(journal, -1);
        while (sd_journal_next(journal) > 0) {
            check_for_event(journal);
        }
    }
}
