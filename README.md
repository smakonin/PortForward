# PortForward
## Copyright (C) 2008 Stephen Makonin

PFWD is a minimal port forwarding utility that uses Epoll and Threading to forward requests from multiple clients on to a protected/hidden server.

Connections are queued up through the use of Epoll. When a client sends data/requests a thread is created to deal with the send and receive request. Using the threads will free up the main loop from having to wait for lengthy communication delays which would in turn cause response performance hits.

## How to Use

PFWD will forward connection to a specified server. To use PFWD you must specify 3 configuration options: the listening port, the forwarding server hostname or IP and the forwarding server port. For example:

```pfwd 8080 www.xyz.com 80```
```pfwd 8080 192.168.1.1 80```

* ```8080``` is the listening port that clients will connect to.
* ```www.xyz.com``` and ```192.168.1.1``` are the forwarding server hostname/IP.
* ```80``` is the forwarding server port.

In these 2 examples PFWD is forwarding HTTP web server requests.
