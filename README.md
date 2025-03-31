# SSH-Block Service: ssh-monitor

This service monitors SSH password login attempts and automatically blocks IP addresses
that exceed a specified number of failed attempts.
**It is designed to work with Linux distributions that use journald for system logging.**

## Features

* **Real-time Monitoring:** Tracks SSH login attempts from system logs.
* **Automatic Blocking:** Blocks offending IP addresses using `ipset` and `iptables`.
* **Database Logging:** Stores login attempts in an SQLite3 database.
* **Configurable Thresholds:** Easily adjust the number of allowed failed attempts.
* **Easy Installation and Uninstallation:** Provided install and uninstall scripts.

## Installation

1.  **Clone the Repository:**

    ```bash
    git clone git@github.com:svensglinz/ssh-monitor.git
    cd ./ssh-monitor
    ```
    
2.  **Run the Installation Script:**

    ```bash
    sudo chmod +x ./install.sh
    sudo ./install.sh
    ```

    The install script will:

    * Check and install required dependencies (`sqlite3`, `ipset`, `iptables`, `less`, `make`).
    * Compile the `ssh-monitor` binary.
    * Create directories for the database and logs (`/var/lib/ssh-monitor` and `/var/log/ssh-monitor`).
    * Install the `ssh-monitor` binary and a wrapper script to `/usr/local/bin`.
    * Create an `uninstall.sh` script for easy removal.

### User Configurable Variables

The following variables can be configured at the beginning of the `install.sh` script:

* `BIN_PATH`: The directory where the `ssh-monitor` binary and script will be installed (default: `/usr/local/bin`).
* `DB_PATH`: The directory where the SQLite3 database will be stored (default: `/var/lib/ssh-monitor`).
* `LOG_PATH`: The directory where the log files will be stored (default: `/var/log/ssh-monitor`).
* `APP_NAME`: The name of the application (default: `ssh-monitor`).

## Usage

After installation, you can use the `ssh-monitor` command with the following options:

* `ssh-monitor`: Starts the `ssh-monitor` service (must be run as root).
* `ssh-monitor attempts`: Shows the last 6 login attempts from the database.
* `ssh-monitor blocked`: Lists all currently blocked IP addresses.
* `ssh-monitor log`: Views the `ssh-monitor` log file.

**Example:**

```bash
sudo ssh-monitor # start the service
ssh-monitor attempts # view login attempts
```

## Uninstallation
To uninstall the ssh-monitor service, run
```bash
sudo ./uninstall.sh
```