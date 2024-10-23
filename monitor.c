#include <systemd/sd-journal.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include "hash_map.h"
#include <sqlite3.h>
#include <unistd.h>
#include <pthread.h>

#define MAX_BLOCK_TIME (60*60)
#define MAX_ATTEMPT 3

static hash_map *logins;
static sqlite3 *db;
static sd_journal *journal;
static pthread_mutex_t hash_table_lock = PTHREAD_MUTEX_INITIALIZER;

typedef struct ip {
    char *addr;
    int attempts;
    time_t last_attempt;
} ip;

typedef struct hash_pair {
    char *key;
    ip value;
} hash_pair;

// custom hash function and comparator for hashmap
// djb2 http://www.cse.yorku.ca/%7Eoz/hash.html
unsigned long hash_fn(const void *elem) {
    const ip *ip_elem = (ip *) elem;
    const char *ip_addr = ip_elem->addr;

    unsigned long hash = 5381;
    int c;
    while ((c = (int) *ip_addr++))
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    return hash;
}

int comp_fn(const void *a, const void *b) {
    const ip *ip_a = (ip *) a;
    const ip *ip_b = (ip *) b;
    return ip_a->addr == ip_b->addr;
}

// create table in dastabase if it does not exist yet
void create_table(sqlite3 *db) {
    const char *query =
            "CREATE TABLE IF NOT EXISTS ip_logins(ip_address TEXT PRIMARY KEY, \
        attempt_count int, last_attempt text, last_success text)";

    char *errmsg;
    if (sqlite3_exec(db, query, NULL, NULL, &errmsg) != SQLITE_OK) {
        fprintf(stderr, errmsg);
        exit(1);
    }
}

// initialize database, journal and hashmap
void init() {
    // initialize block list
    char cmd_buf[70];
    snprintf(cmd_buf, sizeof(cmd_buf), "ipset create ssh_blocklist hash:ip timeout %d > /dev/null 2>&1", MAX_BLOCK_TIME);
    system(cmd_buf);

    // add rule to iptables to block all incoming requests from the ip addresses in ssh_blocklist
    system(
        "iptables -C INPUT -m set --match-set ssh_blocklist src -j DROP || iptables -A INPUT -m set --match-set ssh_blocklist src -j DROP > /dev/null 2>&1");

    // initialize hash map
    hash_map_init(&logins, sizeof(ip), hash_fn, comp_fn, 100, 0.75f, NULL, NULL);

    // open database and create table if it does not exist yet
    if (sqlite3_open("ip_logins.db", &db) != SQLITE_OK) {
        fprintf(stderr, "opening database ip_logins.db failed");
        exit(1);
    }
    create_table(db);

    // initialize journal
    if (sd_journal_open(&journal, SD_JOURNAL_LOCAL_ONLY) < 0) {
        fprintf(stderr, "opening journal failed");
        exit(1);
    }
    sd_journal_add_match(journal, "SYSLOG_IDENTIFIER=sshd", 0);
    sd_journal_seek_tail(journal);
}

// adds an ip to the block list and print info to stdout
void block_ip(const char *ip_addr) {
    static char buff[70];
    snprintf(buff, sizeof(buff), "ipset add ssh_blocklist %s > /dev/null 2>&1", ip_addr);
    system(buff);

    const time_t cur_time = time(NULL);
    char time_buf[26];
    strftime(time_buf, 26, "%Y:%m:%d %H:%M:%S", localtime(&cur_time));
    printf("[%s] Blocked: %s\n", time_buf, ip_addr);
}

// log attempt into database
void log_attempt(char *ip_addr, const int success) {
    static char *insert_query =
            "INSERT INTO ip_logins (ip_address, attempt_count, last_attempt, last_success) \
    VALUES ('%s', 1, strftime('%%Y-%%m-%%d %%H:%%M:%%S', 'now'), %s) \
    ON CONFLICT(ip_address) DO UPDATE SET \
    attempt_count = attempt_count + 1, \
    last_attempt = strftime('%%Y-%%m-%%d %%H:%%M:%%S', 'now'), \
    last_success = %s;";

    char *last_success = success ? "strftime('%Y-%m-%d %H:%M:%S', 'now')" : "NULL";
    char *last_success_conflict = strncmp("NULL", last_success, strlen("NULL"))
                                      ? last_success
                                      : "last_success";
    char query_buf[450];
    snprintf(query_buf, sizeof(query_buf), insert_query, ip_addr, last_success, last_success_conflict);

    // record attempt in database
    char *err_msg;
    if (sqlite3_exec(db, query_buf, NULL, NULL, &err_msg) != SQLITE_OK) {
        fprintf(stderr, err_msg);
        exit(1);
    }

    // add attempt to hash map
    hash_pair *res = NULL;

    pthread_mutex_lock(&hash_table_lock);
    if (!((res = hash_map_find(logins, &(hash_pair){.key = ip_addr})))) {
        ip ip_elem = {.addr = ip_addr, .attempts = 1, .last_attempt = time(NULL)};
        hash_pair p = {.key = ip_addr, .value = ip_elem};
        hash_map_insert(logins, &p);
    } else {
        // unblock ip address

        if (time(NULL) - res->value.last_attempt >= MAX_BLOCK_TIME) {
            res->value.attempts = 1;
        } else {
            res->value.attempts++;
        }
        res->value.last_attempt = time(NULL);
        if (res->value.attempts >= MAX_ATTEMPT) {
            block_ip(res->value.addr);
        }
    }
    pthread_mutex_unlock(&hash_table_lock);
}

void *clean_map(void *args) {
    while (1) {
        pthread_mutex_lock(&hash_table_lock);
        // iterate through all elements in the hashmap and remove them if they have not attempted
        // to login over the last 10 minutes
        // otherwise size may blow up...
        pthread_mutex_unlock(&hash_table_lock);
        sleep(MAX_BLOCK_TIME);
    }
}

void check_for_event() {
    const void *data;
    size_t length;
    if (sd_journal_get_data(journal, "MESSAGE", &data, &length) >= 0) {
        if (strstr((char *) data, "password")) {
            // check if login was accepted or not
            const int accepted = (strstr((char *) data, "Accepted") != NULL);
            char *ip_start = strstr((char *) data, "from");

            if (!ip_start) return;

            ip_start += 5;
            const char *ip_end = strpbrk(ip_start, " \n");
            const size_t ip_len = ip_end - ip_start;
            static char ip_addr[15];
            strncpy(ip_addr, ip_start, ip_len);
            ip_addr[ip_len] = '\0';
            log_attempt(ip_addr, accepted);
        }
    }
}

void monitor_logs() {
    while (true) {
        sd_journal_wait(journal, -1);
        while (sd_journal_next(journal) > 0) {
            check_for_event(journal);
        }
    }
}

int main() {
    init();
    pthread_t cleanup_thread;
    pthread_create(&cleanup_thread, NULL, clean_map, NULL);

    monitor_logs();
    sqlite3_close(db);

    pthread_join(cleanup_thread, NULL);
    return 0;
}
