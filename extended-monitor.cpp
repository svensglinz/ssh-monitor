#include <mariadb/mysql.h>
#include <systemd/sd-journal.h>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <cstring>
#include <thread>
#include <mutex>
#include <regex>

/*
 *
 *
 *
 *
 *
 use ip sets in the future to manage
 Yes, you can remove IPs from an ipset as well. Here's how to do it:
Remove an IP from an ipset

To remove an IP from the blocked_ips set, use the following command:

bash

ipset del blocked_ips 192.168.1.1

Example Workflow

    Add an IP:

    bash

ipset add blocked_ips 192.168.1.1

Remove an IP:

bash

ipset del blocked_ips 192.168.1.1

Check the contents of the ipset:

bash

    ipset list blocked_ips

Summary

Using ipset allows you to efficiently manage multiple IP addresses without creating a large number of individual iptables rules. You can easily add or remove IPs from the set as needed.

*/

// also make a shell script to install the library from github that automatically sets up the database and everythign (assuming all libraries are installed)
// global variables
// also set up library commands like log-monitor show-blocked -d sshd --> shows table with the blocked ip addresses
// log-monitor show-logins -d sshd -l 100 (shows the last 100 ip addresses that tried to login)
// set up a configuration file for environment variables etc...

// also need to define a log handler to write messages to a log file
struct AttemptInfo {
    int count; // Number of failed attempts
    std::chrono::steady_clock::time_point lastAttempt; // first attempt timestamp
    AttemptInfo() : count(0), lastAttempt(std::chrono::steady_clock::now()) {}

    // Parameterized constructor
    AttemptInfo(int c, std::chrono::steady_clock::time_point t) : count(c), lastAttempt(t) {}
};

std::unordered_map<std::string, AttemptInfo> failed_attempts;
std::mutex mtx;

//****************************************************************************************
// configuration variables
//****************************************************************************************
const int MAX_ATTEMPT = 3;
const std::chrono::minutes WINDOW_TIME(5);
const char* DATABASE = "login_monitor";
const char* USER = "sveng";
const char* HOST = "localhost";
const char* PASSWORD = "lajolla98";
std::regex LOG_REGEX(R"(MESSAGE=(\S+) password for .* from (\S+))");
const std::chrono::minutes BLOCK_TIME(30);
const std::string NEVER_BLOCK = "192.168.1.1"; // should accept a list of ip addresses that are never blocked no matter how many wrong passwords are entered!

// connect to database instance
MYSQL* connect_db(const char* host, const char* user, const char* password, const char* database) {
    MYSQL* conn = mysql_init(nullptr);
    if (!conn) {
        throw std::runtime_error("mysql_init() failed");
    }

    if (!mysql_real_connect(conn, host, user, password, database, 0, nullptr, 0)) {
        mysql_close(conn);
        throw std::runtime_error("mysql_real_connect() failed");
    }
    return conn;
}

MYSQL* conn = connect_db(HOST, USER, PASSWORD, DATABASE);

// update database for log attempts
// and checks if login attempt was successful or not
void log_attempt_db(MYSQL* conn, const std::string& ip, const bool success, bool is_blocked) {

    // log attempt into database but also into internal hash table that makes blocking easier
    const std::string last_success = success? "NOW()" : "NULL";

    // Construct the SQL query
    std::string query;
    if (success)
        query = "INSERT INTO log_table (ip_address, attempt_count, last_attempt, last_success, is_blocked) "
                        "VALUES ('" + ip + "', 1, NOW(), " + last_success + ", " + std::to_string(is_blocked) + ") "
                        "ON DUPLICATE KEY UPDATE "
                        "attempt_count = attempt_count + 1, "
                        "last_attempt = NOW(), "
                        "last_success = NOW()" + ", "
                        "is_blocked = " + std::to_string(is_blocked);
    else
        query = "INSERT INTO log_table (ip_address, attempt_count, last_attempt, last_success, is_blocked) "
                        "VALUES ('" + ip + "', 1, NOW(), " + last_success + ", " + std::to_string(is_blocked) + ") "
                        "ON DUPLICATE KEY UPDATE "
                        "attempt_count = attempt_count + 1, "
                        "last_attempt = NOW(), "
                        "last_success = last_success" + ", "
                        "is_blocked = " + std::to_string(is_blocked);

    if (mysql_query(conn, query.c_str())) {
        std::cerr << "INSERT failed: " << mysql_error(conn) << std::endl;
    }
}

void block_ip(const std::string& ip) {
    std::string query = "SELECT is_blocked FROM log_table WHERE ip_address = '" + ip + "'";
    MYSQL_RES* result = mysql_store_result(conn);

    bool blocked = false;
    MYSQL_ROW row = mysql_fetch_row(result);
    if (row) {
        blocked = (std::string(row[0]) == "1");
    }

    mysql_free_result(result);

    if (!blocked) {
        const std::string command = "sudo iptables -A INPUT -s " + ip + " -j DROP";
        system(command.c_str());
    }

    // register ip as blocked in the second database
}

void log_attempt(std::string& ip, std::string& status) {
    auto now = std::chrono::steady_clock::now();
    bool success = status == "Accepted";

    if (status == "Failed") {
        if (failed_attempts.find(ip) == failed_attempts.end() || now - failed_attempts[ip].lastAttempt > WINDOW_TIME) {
            failed_attempts[ip] = AttemptInfo(1, now);
        } else {
            failed_attempts[ip].count++;
        }
    }
       if (failed_attempts[ip].count >= MAX_ATTEMPT){
            block_ip(ip);
            log_attempt_db(conn, ip, success, true);
        } else {
            log_attempt_db(conn, ip, success, false);
        }
}

void check_for_event(sd_journal* journal) {
        const void *data;
        size_t length;
        std::smatch match;
    std::string ip;
    std::string status;
        if (sd_journal_get_data(journal, "MESSAGE", &data, &length) >= 0) {
            std::string message(static_cast<const char*>(data), length);
            std::cout << message << std::endl;
            // process message according to regex
            if (std::regex_search(message, match, LOG_REGEX)) {
                ip = match[2];
                status = match[1];
                log_attempt(ip, status);
                std::cout << failed_attempts[ip].count << std::endl;
            }
            // check if ip should be blocked
            if (failed_attempts[ip].count >= MAX_ATTEMPT) {
                //log_blocked(ip);
                block_ip(ip);
            }
        }
    }

void monitor_logs(MYSQL* conn) {
    sd_journal* journal = nullptr;
    int ret = sd_journal_open(&journal, SD_JOURNAL_LOCAL_ONLY);
    if (ret < 0) {
        throw std::runtime_error("Failed to open journal");
    }
    sd_journal_add_match(journal, "SYSLOG_IDENTIFIER=sshd", 0);
    sd_journal_seek_tail(journal);

    while (true) {
        sd_journal_wait(journal, -1);
        const void *data;
        size_t length;
        while (sd_journal_next(journal) > 0) {
            check_for_event(journal);
        }
    }
}

// check every 30 minutes if some ip addresses should be unblocked --> worst time until unblock = 1 hour
// at startup, load still locked ip addresses as indicated in the database into the locked-ip datastructure!
    /*
void unblock_ip() {
    while (true) {
        std::this_thread::sleep_for(std::chrono::minutes(30));
        std::lock_guard<std::mutex> lock(mtx);

        auto now = std::chrono::steady_clock::now();
        for (auto it = failed_attempts.begin(); it != failed_attempts.end();) {
            if (now - it->second > BLOCK_TIME) {
                std::cout << "unblocking ip" << it->first << std::endl;
                it = failed_attempts.erase(it);
            } else {
                ++it;
            }
        }
    }
}
*/
int main() {
    MYSQL* conn = nullptr;

        try {
            conn = connect_db("localhost", "sveng", "lajolla98", "login_monitor");
            monitor_logs(conn);
        } catch (const std::runtime_error& e) {
            std::cerr << e.what() << std::endl;
        }


    monitor_logs(conn);


    if (conn) {
        mysql_close(conn);
    }
    return 0;
}
