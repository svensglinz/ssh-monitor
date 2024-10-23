# SSH Login Monitor 

### Description: 
ssh-monitor allows you to monitor any password based ssh login attempts on a linux system that uses systemd's journal for ssh log messages. 

Each login attempt is stored in a sqlite database. If any IP address attempts to login more than 3 times within 1 hour minutes, the IP address is blocked 
for one hour with the help of `iplist` and `iptables`.

The sqlite database can be displayed with
`ssh-monitor attempts`

The blocked IP addresses can be displayed with 
`ssh-monitor blocked`

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


