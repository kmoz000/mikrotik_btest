# Mikrotik bandwith test protocol description
## about
This is attempt to create opensource version of the btest tool. Currently it is possible to run bandwith-test only from another mikrotik devices or from the windows closed source client. 

Protocol itsef seems to be not complicated, so it should not be easy to create 3rd party client for it.

## Protocol description
There is no official protocol description, so everything was obtained using WireShark tool and RouterOS 6 running in the virtualbox, which was connecting to the real device. For now i am inspecting only TCP, later will try to do UDP if TCP works. Please keep in mind that this data is guessed on the captures, so possibly is not accurate or wrong. 

```
> bserver: hello
01:00:00:00 
> client: Start test cmd, depending on the client settings:
01:01:01:00:00:80:00:00:00:00:00:00:00:00:00:00 (transmit)
01:01:00:00:00:80:00:00:00:00:00:00:00:00:00:00 (transmit, random data)
00:01:01:00:dc:05:00:00:00:00:00:00:00:00:00:00 (transmit, UDP)
01:02:01:00:00:80:00:00:00:00:00:00:00:00:00:00 (receive)
01:02:00:00:00:80:00:00:00:00:00:00:00:00:00:00 (receive, random data)
00:02:01:00:dc:05:00:00:00:00:00:00:00:00:00:00 (receive, UDP, remote-udp-tx-size: default (1500))
00:02:01:00:d0:07:00:00:00:00:00:00:00:00:00:00 (receive, UDP, remote-udp-tx-size: 2000)
00:02:01:00:00:fa:00:00:00:00:00:00:00:00:00:00 (receive, UDP, remote-udp-tx-size: 64000)
01:02:01:00:00:80:00:00:01:00:00:00:00:00:00:00 (receive, remote-tx-speed=1)
01:02:01:00:00:80:00:00:ff:ff:ff:ff:00:00:00:00 (receive, remote-tx-speed=4294967295)
01:02:01:00:00:80:00:00:00:00:00:00:01:00:00:00 (receive, local-tx-speed=1)
01:02:01:00:00:80:00:00:00:00:00:00:ff:ff:ff:ff (receive, local-tx-speed=4294967295)
01:02:01:0a:00:80:00:00:00:00:00:00:00:00:00:00 (receive, tcp-connection-count=10)
01:03:01:00:00:80:00:00:00:00:00:00:00:00:00:00 (both)
01:03:00:00:00:80:00:00:00:00:00:00:00:00:00:00 (both, random data)
00:03:01:00:dc:05:00:00:00:00:00:00:00:00:00:00 (both, UDP)
> bserver: Start test confirm (auth is disabled on server):
01:00:00:00 (tcp-connection-count=1 on a client)
01:bc:04:00 (tcp-connection-count>1 on a client)
> bserver: auth requested, with 16 challenge bytes (3 random packets provided)
02:00:00:00:90:67:3f:0f:5c:c7:4e:17:a0:e0:9e:1c:b9:ee:3b:0c
02:00:00:00:17:e7:ee:84:83:cc:15:53:e8:fa:9c:0d:ad:ac:b8:e1
02:00:00:00:ee:d1:19:b9:d3:f2:df:6d:04:46:da:25:55:44:49:81
> client: auth reply (3 random packets, userid test)
90:75:1f:b0:2a:f7:25:51:46:25:71:c3:16:ce:cc:2b:74:65:73:74:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00
a0:1b:fa:27:78:55:08:71:93:09:70:86:15:30:84:ac:74:65:73:74:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00
b4:f2:9e:06:5e:74:da:89:65:c9:be:94:4d:bf:8f:20:74:65:73:74:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00
> bserver: packet data follows
00:00:00:00........
```
1. Server always starts with "hello" (01:00:00:00) command after establishing TCP connection to port 2000. If UDP protocl is specified in the client TCP connection is still established.
2. Client sends 16 bytes command started with 01 (TCP) or 00 (UDP) command to the server. Some of the bytes guess:
 - **cmd[0]**: protocol, 01: TCP, 00: UDP
 - **cmd[1]**: direction, 01 - transmit, 02 - receive, 03 - both
 - **cmd[2]**: use random data, 00 - use random, 01: use \00 character
 - **cmd[3]**: tcp-connection-count, 0 if tcp-connection-count=1, number if more
 - **cmd[4:5]**: remote-udp-tx-size (dc:05) on UDP, 00:80 on TCP, UINT16 - Little Endian
 - **cmd[6:7]**: client buffer size. Maximum value for TCP is 65535 (FF:FF).
 - **cmd[8:11]**: remote-tx-speed, 0: unlimimted, 1-4294967295: value, UINT32 - Little Endian
 - **cmd[12:15]**: local-tx-speed, 0: unlimimted, 1-4294967295: value, UINT32 - Little Endian
3. If server authentication is disabled it sends 01:00:00:00 and starts to transmit/recieve data. 
If auth is enabled and **btest server versions is < 6.43** server sends 20bytes reply started with 02:00:00:00 in the beginning and random bytes (challenge) in the [4:15] range.
Customer sends back 48 bytes reply containing user name (unencrypted) and 16 bytes hash of the password with challenge. Hashing alghoritm is double md5, hashed by challenge, see "authentication" section. 
If **btest server versions is >= 6.43** server sends **03:00:00:00** reply and new authentication method is used, based on the EC-SRP5. Exact implementation details are not yet known and method is not yet supported in the OSS software. See https://github.com/haakonnessjoen/MAC-Telnet/issues/42 for the related discussion. 
4. If auth failed server sends back `00000000` (client shows "authentication failed"), if succeed - `01000000` packet and test is starting.
5. If tcp-connection-count > 1 server should reply with `01:xx:xx:00` where xx seems to be some kind of authentification data to start other connections. This number is used in other threads. 
6. From time to time (~1 per second) server or client sends 12 bytes messages started with `07`, e.g. `07:00:00:00:01:00:00:00:36:6e:03:00`. Btest client relies on this information to show "transmit" speed.  It is server-level statistic, where values are:
  - **stat[0]** is 07 (message id)
  - **stat[4-7]**  number or time from start in seconds, sends one per second, UINT32 - Little Endian
  - **stat[8-11]** number of bytes transferred per sequence, UINT32 - Little Endian

## Old authentication methoud
Sample of the challenge-response for the "test" password:
```
ad32d6f94d28161625f2f390bb895637
3c968565bc0314f281a6da1571cf7255
```

Hashing alghoritm was found with implementation of the testing server and help from the **Chupaka** on a Mikrotik forum. It is 
md5('password' + md5('password' + hash)). This is an example on the Perl
language, which using challenge from the example and returns same hash:

```perl
use Digest::MD5 qw(md5 md5_hex);

my $salt='ad32d6f94d28161625f2f390bb895637';
my $pass='test';
print md5_hex($pass.md5($pass.pack( 'H*', $salt)))."\n";
```
Script should return `3c968565bc0314f281a6da1571cf7255` and its matching captured traffic. 
