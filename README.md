# SSH Login Monitor 

### Description: 

I like to have access to my raspberry pi via ssh via password, but get paranoid that someday,
somebody may figure out my password.

To mitigate any brute force attacks or just to be able to see who attempted to login, this ...
scans the systemd ssh logfiles and scans for any attempts to login via password.

Any attempt is logged in an sqlite database and can be displayed by running `ssh-monitor attempts`

If a specific IP attempts to login 3 times or more within a 10 minute window, the IP address is blocked
for 10 minutes, using `iptables`

### Install: 
```bash
git clone XXXX

# Compile and link files
gcc ./hash-map.c ./monitor.c -lm -lsystemd -lsqlite3 -o monitor

# make file executable
sudo chmod +x ./log-monitor.sh

# run application in the background
nohup sudo ./monitor > /dev/null &
```


