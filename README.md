# Unicast-Routing
This is my course project submitted for **CS-498 Cloud Networking** course at UIUC.

In this project, I implemented the traditional shortest-path routing using Dijkstra's algorithm, with the link state (LS) protocols.

# Author
Bahman Sheikh

# Programming Language
- C

## Routing Environment
A virtual network will be emulated using the iptables in Linux. It acts as a packet filter and firewall that examines and directs traffic based on port, protocol and other criteria.
The nodes will all run on the same machine. There will be a made-up topology applied to them in the following manner:
- For each node, a virtual interface (eth0:1, eth0:2, etc) will be created and given an IP address.
- A node with ID n gets address 10.1.1.n. IDs 0-255 inclusive are valid.
- The developed code will be given its ID on the command line, and when binding a socket to receive UDP packets, it should specify the corresponding IP address (rather than INADDR_ANY / NULL).
- iptables rules will be applied to restrict which of these addresses can talk to which others. 10.1.1.30 and 10.1.1.0 can talk to each other if and only if they are neighbors in the made-up topology.
- The topology's connectivity is defined in a file that gets read by the script that creates the environment. The link costs are defined in files that tell nodes what their initial link costs to other nodes are, if they are in fact neighbors.
- A node's only way to determine who its neighbors are is to see who it can directly communicate with. Nodes do not get to see the aforementioned connectivity file.

## Manager:
While running,the nodes will receive instructions and updated information from a manager program. The interaction with the manager is very simple: it sends messages in individual UDP packets to the nodes on UDP port 7777, and they do not reply in any way.
The manager's packets have one of two meanings: “send a data packet into the network”, or “your direct link to node X now has cost N.” Their formats are:

“Send a data packet”:

"send"<4 ASCII bytes>destID<net order 2 bytes>message<some ASCII message (shorter than 100 bytes)>

Example: destination Id is 4 and message is “hello”. The manager command will be:
“send4hello”

where 4 occupies 2 bytes and is in the network order. Note that there is no space delimiter among "send", destID and the message body.

“New cost to your neighbor”:

"cost"<4 ASCII bytes>destID<net order 2 bytes>newCost<net order 4 bytes>

Example: the manager informs node 2 that the cost to node 5 is set to 33. The command received by node 2 will be:
“cost533”

where 5 occupies 2 bytes and 33 occupies 4 bytes. Both of them are in the network order. 

## Nodes:
the node should run like:
./ls_router nodeid initialcostsfile logfile

Examples:
./ls_router 5 node5costs logout5.txt

When originating, forwarding, or receiving a data packet, the node should log the event to its log file. The sender of a packet should also log when it learns that the packet was undeliverable. The format of the logging is described in the next section. Note: even if the node has nothing to write to the log file, the log file should still be created.

The link costs that the node receives from the input file and the manager don't tell the node whether those links actually currently exist, just what they would cost if they did. The node therefore needs to constantly monitor which other nodes it is able to send/receive UDP packets directly to/from. 

That concludes the description of how the nodes need to do I/O, interact with the manager program, and stay aware of the topology. Now, for what they should actually accomplish:
- Using LS, maintain a correct forwarding table.
- Forward any data packets that come along according to the forwarding table.
- React to changes in the topology (changes in cost and/or connectivity).
- The nodes should converge within 5 seconds of the most recent change.

Partition: the network might become partitioned, and the protocols should react correctly: when a node is asked to originate a packet towards a destination it does not know a path for, it should drop the packet, and rather than log a send event, log an unreachable event 

## File Formats
The nodes take the “initial costs file” as input, and write their output to a log file. The locations of both of these files are specified on the command line, as described earlier. 

Initial costs file format:
<nodeid> <nodecost>
<nodeid> <nodecost>
Example initial costs file:
5 23453245
2 1
3 23
19 1919
200 23555
  
In this example, if this file was given to node 6, then the link between nodes 5 and 6 has cost 23453245 – as long as that link is up in the physical topology.

If you don't find an entry for a node, default to cost 1. We will only use positive costs – never 0 or negative. We will not try to break your program with malformed inputs. Once again, just because this file contains a line for node n, it does NOT imply that the node will be neighbors with n.

### Log file: (See our provided code for sprintf()s that generate these lines correctly.)

Example log file:
forward packet dest [nodeid] nexthop [nodeid] message [text text]
sending packet dest [nodeid] nexthop [nodeid] message [text text]
receive packet message [text text text]
unreachable dest [nodeid]
…
In this example, the node forwarded a message bound for node 56, received a message for itself, originated packets for nodes 11 and 12 (but realized it couldn't reach 12), then forwarded another two packets.
forward packet dest 56 nexthop 11 message Message1
receive packet message Message2!
sending packet dest 11 nexthop 11 message hello there!
unreachable dest 12
forward packet dest 23 nexthop 11 message Message4
forward packet dest 56 nexthop 11 message Message5
Our tests will have data packets be sent relatively far apart, so don't worry about ordering.

## Extra Notes:
To set an initial topology, run perl make_topology.pl thetopofile (an example topology file is provided). Note that you need to replace all the "eth0" in the make_topology.pl file to your VM's network name (e.g., enp0s3) to make the iptables work. This will set the connectivity between nodes. To give them their initial costs, provide each with an initialcosts file (examples of these are provided as well. See the spec document for more details on all file formats). You can just give them all an empty file, if you want all links to default to cost 1.

- To make the nodes send messages, or change link costs while the programs are running, use manager_send.
- To bring links up and down while running:

e.g. to bring the link between nodes 1 and 2 up:

sudo iptables -I OUTPUT -s 10.1.1.1 -d 10.1.1.2 -j ACCEPT ; sudo iptables -I OUTPUT -s 10.1.1.2 -d 10.1.1.1 -j ACCEPT

To take that link down:

sudo iptables -D OUTPUT -s 10.1.1.1 -d 10.1.1.2 -j ACCEPT ; sudo iptables -D OUTPUT -s 10.1.1.2 -d 10.1.1.1 -j ACCEPT

log file format:
sprintf(logLine, "sending packet dest %d nexthop %d message %s\n", dest, nexthop, message);
sprintf(logLine, "forward packet dest %d nexthop %d message %s\n", dest, nexthop, message);
sprintf(logLine, "receive packet message %s\n", message);
sprintf(logLine, "unreachable dest %d\n", dest);
... and then fwrite(logLine, 1, strlen(logLine), theLogFile);


