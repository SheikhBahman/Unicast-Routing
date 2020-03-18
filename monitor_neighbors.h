#define BufferSize 1000
#define MSGTRY 100
#define TRY_aLSA 1
#define heartBeatMSG 15
#define wordNodeSize 100
#define maxNeighbors 30

typedef struct LSA {
	long cost;	
	int LSAAnnounced;
	int LSADownAnnounced;
	int neighbor;	
	int sequenceNumber;	
	int up;
	struct LSA *next;
} LSA;

typedef struct dijkstraRoute {
	long cost;
	int neighborID;
	int destID;
} dijkstraRoute;

typedef struct LSATicket {	
	int comeFrom;
	struct LSA *aLSA;
	struct LSATicket *next;
} LSATicket;

typedef struct allLSATickets {
	int LSAUpdated;
	LSATicket *lsaTicket;
} allLSATickets;

int globalMyID = 0;
//last time you heard from each node. TODO: you will want to monitor this
//in order to realize when a neighbor has gotten cut off from you.
struct timeval globalLastHeartbeat[256];

//our all-purpose UDP socket, to be bound to 10.1.1.globalMyID, port 7777
int globalSocketUDP;
//pre-filled for sending to 10.1.1.0 - 255, port 7777
struct sockaddr_in globalNodeAddrs[256];

allLSATickets *allLSA;
LSATicket *neighborsLSA;
int *myWorld;
int *myWorldSelectNeighbor;
int myWorldSize = 0;
dijkstraRoute *wordList;
int myNeighborIDs[maxNeighbors];
int myNeighborSize = 0;


void hackyBroadcast(const char* buf, int length)
{
	int i;
	for (i = 0; i<256; i++)
		if (i != globalMyID) {
			//(although with a real broadcast you would also get the packet yourself)
			sendto(globalSocketUDP, buf, length, 0,
				(struct sockaddr*)&globalNodeAddrs[i], sizeof(globalNodeAddrs[i]));
		}

}

void hackyBroadcastJustNeighbors(const char* buf, int length)
{
	for (int i = 0; i< myNeighborSize; i++)
		sendto(globalSocketUDP, buf, length, 0,
		(struct sockaddr*)&globalNodeAddrs[myNeighborIDs[i]], sizeof(globalNodeAddrs[i]));
}

void hackyBroadcastToSpecificNeighbors(const char* buf, int length, int ID)
{
	sendto(globalSocketUDP, buf, length, 0, (struct sockaddr*)&globalNodeAddrs[ID], sizeof(globalNodeAddrs[ID]));
}


void announceToSpecificNeighbors(int ID)
{
	LSATicket *lsaTicket;
	LSA *ticket;

	char *buffer = malloc(sizeof(char) * BufferSize);

	for (int i = 0; i < TRY_aLSA; ++i){
		lsaTicket = allLSA->lsaTicket;
		while (lsaTicket) {
			if (lsaTicket->aLSA) {
				ticket = lsaTicket->aLSA;
				while (ticket) {
					sprintf(buffer, "LSA%d,%d,%d,%d,%ld", lsaTicket->comeFrom, ticket->neighbor, ticket->sequenceNumber, ticket->up, ticket->cost);
					hackyBroadcastToSpecificNeighbors(buffer, strlen(buffer), ID);
					//nanosleep(&sleepFor, 0);
					ticket = ticket->next;
				}
			}
			lsaTicket = lsaTicket->next;
		}
		//nanosleep(&sleepFor2, 0);
	}
}


void updateMyWordList(int *sizeWordList, long cost, int neighborID, int destinationID) {
	
	for (int i = 0; i < *sizeWordList; ++i ){
		if (wordList[i].destID == destinationID) {
			if (cost < wordList[i].cost || (cost == wordList[i].cost && neighborID < wordList[i].neighborID)) {
				wordList[i].cost = cost;
				wordList[i].neighborID = neighborID;
			}
			return;
		}
	}
	wordList[*sizeWordList].destID = destinationID;
	wordList[*sizeWordList].cost = cost;
	wordList[*sizeWordList].neighborID = neighborID;
	++(*sizeWordList);
}

LSATicket *findLSATicket(LSATicket *myNeighbors, int ID) {
	LSATicket *lsaTicket = myNeighbors;
	while (lsaTicket) {
		if (lsaTicket->comeFrom == ID)return lsaTicket;
		lsaTicket = lsaTicket->next;
	}
	return 0;
}

void dijkstrasAlgorithm(int wanted) {

	if (allLSA->LSAUpdated == MSGTRY){

		int nodeReached[100];
		int sizeNodeReached = 0;
		int sizeWordList = 0;
		myWorldSize = 0;		

		// Immidiate neighbors
		struct LSA *aLSA = neighborsLSA->aLSA;
		while (aLSA) {
			updateMyWordList(&sizeWordList, aLSA->cost, aLSA->neighbor, aLSA->neighbor);
			aLSA = aLSA->next;
		}

		while (sizeWordList > 0) {
			int minimumIndex;
			long minimumCost = 10000000;
			for (int i = 0; i < sizeWordList; ++i){
				if (wordList[i].cost < minimumCost){
					minimumCost = wordList[i].cost;
					minimumIndex = i;
				}
				else if (wordList[i].cost == minimumCost) {
					if (wordList[i].destID < wordList[minimumIndex].destID) {
						minimumIndex = i;
					}
				}
			}

			
			int destID = wordList[minimumIndex].destID;
			int neighborID = wordList[minimumIndex].neighborID;
			long minCost = wordList[minimumIndex].cost;

			myWorld[myWorldSize] = destID;
			myWorldSelectNeighbor[myWorldSize] = wordList[minimumIndex].neighborID;
			++myWorldSize;
			nodeReached[sizeNodeReached] = destID;
			++sizeNodeReached;

			wordList[minimumIndex].destID = wordList[sizeWordList - 1].destID;
			wordList[minimumIndex].neighborID = wordList[sizeWordList - 1].neighborID;
			wordList[minimumIndex].cost = wordList[sizeWordList - 1].cost;
			--sizeWordList;

			if (wanted == destID) return;

			LSATicket *lsaTicket = findLSATicket(allLSA->lsaTicket, destID);
			if (!lsaTicket) continue;
			LSA *aLSA = lsaTicket->aLSA;
			while (aLSA) {
				if (aLSA->up && aLSA->neighbor != globalMyID)  {
					for (int i = 0; i < sizeNodeReached; ++i)
						if (nodeReached[i] == aLSA->neighbor)
							goto next;
					updateMyWordList(&sizeWordList, aLSA->cost + minCost, neighborID, aLSA->neighbor);
				}
				next:
				aLSA = aLSA->next;
			}
		}
		allLSA->LSAUpdated = 0;
	}
}

void updateAllLSATickets(int comeFrom, int neighbor, int sequenceNumber, long cost, int up) {
	LSA *aLSA;
	LSA *aLSAPrev = 0;
	LSATicket *lsaTicketPrev = 0;
	allLSA->LSAUpdated = MSGTRY;
	LSATicket *lsaTicket = allLSA->lsaTicket;
	while (lsaTicket) {
		if (lsaTicket->comeFrom == comeFrom) {
			aLSA = lsaTicket->aLSA;
			while (aLSA) {
				if (aLSA->neighbor == neighbor) {
					if (sequenceNumber > aLSA->sequenceNumber ) {
						aLSA->cost = cost;
						aLSA->LSAAnnounced = 0;
						aLSA->LSADownAnnounced = 0;
						aLSA->sequenceNumber = sequenceNumber;
						aLSA->up = up;						
					}
					return;
				}
				aLSAPrev = aLSA;
				aLSA = aLSA->next;
			}
			aLSA = malloc(sizeof(LSA));
			aLSA->cost = cost;
			aLSA->LSAAnnounced = 0;
			aLSA->LSADownAnnounced = 0;
			aLSA->neighbor = neighbor;
			aLSA->sequenceNumber = sequenceNumber;			
			aLSA->up = up;
			aLSA->next = 0;			
			if (!lsaTicket->aLSA) lsaTicket->aLSA = aLSA;
			if (aLSAPrev) aLSAPrev->next = aLSA;
			return;
		}
		lsaTicketPrev = lsaTicket;
		lsaTicket = lsaTicket->next;
	}

	lsaTicket = malloc(sizeof(LSATicket));
	lsaTicket->comeFrom = comeFrom;
	lsaTicket->next = 0;	
	lsaTicket->aLSA = malloc(sizeof(LSA));
	lsaTicket->aLSA->cost = cost;
	lsaTicket->aLSA->neighbor = neighbor;
	lsaTicket->aLSA->LSAAnnounced = 0;	
	lsaTicket->aLSA->up = 1;	
	lsaTicket->aLSA->LSADownAnnounced = 0;
	lsaTicket->aLSA->sequenceNumber = sequenceNumber;
	lsaTicket->aLSA->next = 0;	
	if (!allLSA->lsaTicket) allLSA->lsaTicket = lsaTicket;
	if (lsaTicketPrev) lsaTicketPrev->next = lsaTicket;
}

void updateNeighborsLSA(int comeFrom, int sequenceNumber, long cost, int up) {
	LSA *prev = 0;	
	LSA *nLSA = neighborsLSA->aLSA;
	while (nLSA) {
		if (nLSA->neighbor == comeFrom) {
			if (sequenceNumber < 0) {
				nLSA->cost = cost;
				nLSA->LSAAnnounced = 0;
				nLSA->LSADownAnnounced = 0;
				nLSA->sequenceNumber++;				
				allLSA->LSAUpdated = MSGTRY;
			}
			else if (sequenceNumber > nLSA->sequenceNumber) {
				nLSA->cost = cost;
				nLSA->LSAAnnounced = 0;
				nLSA->LSADownAnnounced = 0;
				nLSA->sequenceNumber = sequenceNumber;
				nLSA->up = up;				
				allLSA->LSAUpdated = MSGTRY;
			}
			return;
		}
		prev = nLSA;
		nLSA = nLSA->next;
	}

	nLSA = malloc(sizeof(LSA));
	if (sequenceNumber >= 0) nLSA->sequenceNumber = sequenceNumber;
	else nLSA->sequenceNumber = 0;
	nLSA->neighbor = comeFrom;
	myNeighborIDs[myNeighborSize] = comeFrom;
	++myNeighborSize;
	nLSA->cost = cost;
	nLSA->next = 0;
	nLSA->up = up;
	nLSA->LSAAnnounced = 0;
	nLSA->LSADownAnnounced = 0;
	if (prev) prev->next = nLSA;
	if (!neighborsLSA->aLSA) neighborsLSA->aLSA = nLSA;
	allLSA->LSAUpdated = MSGTRY;
	announceToSpecificNeighbors(nLSA->neighbor);
}

void* heartBeat(void* unusedParam){
	struct timespec sleepFor;
	sleepFor.tv_sec = 0;
	sleepFor.tv_nsec = 300*1000*1000;
	char heartBeat[heartBeatMSG];
	sprintf(heartBeat, "H%d", globalMyID);
	int sizeHeartBeat = strlen(heartBeat);
	while (1) {
		// I am UP (heartBeat)
		hackyBroadcast(heartBeat, sizeHeartBeat);
		nanosleep(&sleepFor, 0);
	}

}

void* announceToNeighbors(void* unusedParam)
{
	LSATicket *lsaTicket;
	LSA *ticket;
	struct timespec sleepFor;
	sleepFor.tv_sec = 0;
	sleepFor.tv_nsec = 10*1000*1000;

	struct timespec sleepFor2;
	sleepFor2.tv_sec = 0;
	sleepFor2.tv_nsec = 100 * 1000 * 1000;

	char *buffer = malloc(sizeof(char) * BufferSize);

	while (1) {	
		lsaTicket = allLSA->lsaTicket;
		while (lsaTicket) {
			if (lsaTicket->aLSA) {
				ticket = lsaTicket->aLSA;
				while (ticket) {
					if (!ticket->LSADownAnnounced){					
						if (ticket->LSAAnnounced < TRY_aLSA){
							sprintf(buffer, "LSA%d,%d,%d,%d,%ld", lsaTicket->comeFrom, ticket->neighbor, ticket->sequenceNumber, ticket->up, ticket->cost);
							//hackyBroadcast(buffer, strlen(buffer));
							hackyBroadcastJustNeighbors(buffer, strlen(buffer));
							nanosleep(&sleepFor, 0);
							ticket->LSAAnnounced++;
						}
						else {
							//ticket->LSADownAnnounced = 1;
						}
					}
					ticket = ticket->next;
				}
			}
			lsaTicket = lsaTicket->next;
		}	
		nanosleep(&sleepFor2, 0);
	}
}

void* checkNeighbors(void* unusedParam){
	struct timeval current;
	while (1) {
		gettimeofday(&current, 0);
		LSA *aLSA = neighborsLSA->aLSA;
		while (aLSA) {
			if ((current.tv_sec - globalLastHeartbeat[aLSA->neighbor].tv_sec) * 1000000 + 
				(current.tv_usec - globalLastHeartbeat[aLSA->neighbor].tv_usec) > 610000) {
				if (aLSA->up) { 
					allLSA->LSAUpdated = MSGTRY;
					aLSA->up = 0;
					aLSA->LSAAnnounced = 0;
					aLSA->LSADownAnnounced = 0;
					aLSA->sequenceNumber++;					
					dijkstrasAlgorithm(-1);
				}
			}
			else { 
				if (!aLSA->up) {
					aLSA->up = 1;
					aLSA->sequenceNumber++;
					allLSA->LSAUpdated = MSGTRY;
					aLSA->LSAAnnounced = 0;
					aLSA->LSADownAnnounced = 0;
					announceToSpecificNeighbors(aLSA->neighbor);
					dijkstrasAlgorithm(-1);
				}
			}
			aLSA = aLSA->next;
		}
	}
}

void listenForNeighbors(FILE *logFile)
{
	char fromAddr[100];
	struct sockaddr_in theirAddr;
	socklen_t theirAddrLen;
	unsigned char recvBuf[BufferSize];
	int bytesRecvd;		

	LSATicket *lsaTicket;
	LSA *ticket;
	struct timespec sleepFor;
	sleepFor.tv_sec = 0;
	sleepFor.tv_nsec = 10 * 1000 * 1000;

	struct timespec sleepFor2;
	sleepFor2.tv_sec = 0;
	sleepFor2.tv_nsec = 100 * 1000 * 1000;

	char *buffer = malloc(sizeof(char) * BufferSize);
	
	while (1)
	{
		theirAddrLen = sizeof(theirAddr);
		if ((bytesRecvd = recvfrom(globalSocketUDP, recvBuf, BufferSize, 0,
			(struct sockaddr*)&theirAddr, &theirAddrLen)) == -1)
		{
			perror("connectivity listener: recvfrom failed");
			exit(1);
		}

		recvBuf[bytesRecvd] = '\0';

		inet_ntop(AF_INET, &theirAddr.sin_addr, fromAddr, 100);

		short int heardFrom = -1;
		if (strstr(fromAddr, "10.1.1."))
		{
			heardFrom = atoi(
				strchr(strchr(strchr(fromAddr, '.') + 1, '.') + 1, '.') + 1);
			//TODO: this node can consider heardFrom to be directly connected to it; do any such logic now.			
			gettimeofday(&globalLastHeartbeat[heardFrom], 0);	
			updateNeighborsLSA(heardFrom, 0, 1, 1);	
			dijkstrasAlgorithm(-1);
		}

		struct timeval current;

		gettimeofday(&current, 0);
		LSA *aLSA = neighborsLSA->aLSA;
		while (aLSA) {
			if ((current.tv_sec - globalLastHeartbeat[aLSA->neighbor].tv_sec) * 1000000 +
				(current.tv_usec - globalLastHeartbeat[aLSA->neighbor].tv_usec) > 610000) {
				if (aLSA->up) {
					aLSA->up = 0;
					aLSA->sequenceNumber++;
					allLSA->LSAUpdated = MSGTRY;
					aLSA->LSAAnnounced = 0;
					aLSA->LSADownAnnounced = 0;
					dijkstrasAlgorithm(-1);
				}
			}
			else {
				if (!aLSA->up) {
					aLSA->up = 1;
					aLSA->sequenceNumber++;
					allLSA->LSAUpdated = MSGTRY;
					aLSA->LSAAnnounced = 0;
					aLSA->LSADownAnnounced = 0;
					announceToSpecificNeighbors(aLSA->neighbor);
					dijkstrasAlgorithm(-1);
				}
			}
			aLSA = aLSA->next;
		}

		
		lsaTicket = allLSA->lsaTicket;
		while (lsaTicket) {
			if (lsaTicket->aLSA) {
				ticket = lsaTicket->aLSA;
				while (ticket) {
					if (!ticket->LSADownAnnounced){
						if (ticket->LSAAnnounced < TRY_aLSA){
							sprintf(buffer, "LSA%d,%d,%d,%d,%ld", lsaTicket->comeFrom, ticket->neighbor, ticket->sequenceNumber, ticket->up, ticket->cost);
							//hackyBroadcast(buffer, strlen(buffer));
							hackyBroadcastJustNeighbors(buffer, strlen(buffer));
							nanosleep(&sleepFor, 0);
							ticket->LSAAnnounced++;
						}
						else {
							//ticket->LSADownAnnounced = 1;
						}
					}
					ticket = ticket->next;
				}
			}
			lsaTicket = lsaTicket->next;
		}
		//nanosleep(&sleepFor2, 0);
	

		//Is it a packet from the manager? (see mp2 specification for more details)
		//send format: 'send'<4 ASCII bytes>, destID<net order 2 byte signed>, <some ASCII message>
		if (!strncmp(recvBuf, "send", 4)) {
			//TODO send the requested message to the requested destination node
			int destination = (recvBuf[4] << 8) + (recvBuf[5] & 0xff);		

			if (destination == globalMyID) {
				fprintf(logFile, "receive packet message %s\n", recvBuf + 6);fflush(logFile);
			}
			else {
				//Update if changed
				dijkstrasAlgorithm(destination);
				int selectedNeighbor = -1;
				for (int i = 0; i < myWorldSize; ++i)
					if (myWorld[i] == destination)
						selectedNeighbor = myWorldSelectNeighbor[i];
				if (selectedNeighbor >= 0) {					
					recvBuf[0] = 'p';
					sendto(globalSocketUDP, recvBuf, bytesRecvd, 0, (struct sockaddr*)&globalNodeAddrs[selectedNeighbor], sizeof(globalNodeAddrs[selectedNeighbor]));
					fprintf(logFile, "sending packet dest %d nexthop %d message %s\n", destination, selectedNeighbor, recvBuf + 6); fflush(logFile);

				}
				else {
					fprintf(logFile, "unreachable dest %d\n", destination); fflush(logFile);
				}
			}
		}
		else if (!strncmp(recvBuf, "LSA", 3)) {
			int comeFrom, neighbor, sequenceNumber, up;
			long cost;
			sscanf(recvBuf, "LSA%d,%d,%d,%d,%ld", &comeFrom, &neighbor, &sequenceNumber, &up, &cost);
			if (neighbor == globalMyID)
				updateNeighborsLSA(comeFrom, sequenceNumber, cost, 1);
			else if (comeFrom != globalMyID)
				updateAllLSATickets(comeFrom, neighbor, sequenceNumber, cost, up);
			dijkstrasAlgorithm(-1);
		}
		else if (!strncmp(recvBuf, "pend", 4)) {

			int destination = (recvBuf[4] << 8) + (recvBuf[5] & 0xff);
			
			if (destination == globalMyID) {
				fprintf(logFile, "receive packet message %s\n", recvBuf + 6);fflush(logFile);
			}
			else {
				//Update if changed
				dijkstrasAlgorithm(destination);
				int selectedNeighbor = -1;
				for (int i = 0; i < myWorldSize; ++i)
					if (myWorld[i] == destination)
						selectedNeighbor = myWorldSelectNeighbor[i];
				if (selectedNeighbor >= 0) {
					sendto(globalSocketUDP, recvBuf, bytesRecvd, 0,	(struct sockaddr*)&globalNodeAddrs[selectedNeighbor],sizeof(globalNodeAddrs[selectedNeighbor]));
					fprintf(logFile, "forward packet dest %d nexthop %d message %s\n", destination, selectedNeighbor, recvBuf + 6); fflush(logFile);
				}
				else {
					fprintf(logFile, "unreachable dest %d\n", destination); fflush(logFile);
				}
			}
		}
		else if (!strncmp(recvBuf, "cost", 4)) {
			//'cost'<4 ASCII bytes>, destID<net order 2 byte signed> newCost<net order 4 byte signed>
			//TODO record the cost change (remember, the link might currently be down! in that case,
			//this is the new cost you should treat it as having once it comes back up.)
			// ...
			int destination = (recvBuf[4] << 8) + (recvBuf[5] & 0xff);
			long cost = (recvBuf[6] << 24) + (recvBuf[7] << 16) + (recvBuf[8] << 8) + (recvBuf[9] & 0xff);
			updateNeighborsLSA(destination, -1, cost, 0);			
			dijkstrasAlgorithm(-1);
		}		
	}
	//(should never reach here)
	close(globalSocketUDP);
}

void initialization(FILE *costsFile) {
	char *newLine;
	size_t size;
	neighborsLSA = malloc(sizeof(LSATicket));
	neighborsLSA->comeFrom = globalMyID;
	LSA *aLSAPrev = 0;
	while (getline(&newLine, &size, costsFile) > 0) {
		int neighbor;
		long cost;
		sscanf(newLine, "%d %ld\n", &neighbor, &cost);
		LSA *aLSA = malloc(sizeof(LSA));
		aLSA->neighbor = neighbor;
		myNeighborIDs[myNeighborSize] = neighbor;
		++myNeighborSize;
		aLSA->sequenceNumber = 1;
		aLSA->LSAAnnounced = 0;
		aLSA->LSADownAnnounced = 0;
		aLSA->cost = cost;
		aLSA->up = 0;
		aLSA->next = 0;		
		if (!neighborsLSA->aLSA) neighborsLSA->aLSA = aLSA;
		if (aLSAPrev) aLSAPrev->next = aLSA;
		aLSAPrev = aLSA;
		aLSA = aLSA->next;
	}
	wordList = malloc(wordNodeSize * sizeof(dijkstraRoute));
	myWorld = malloc(wordNodeSize * sizeof(int));
	myWorldSelectNeighbor = malloc(wordNodeSize * sizeof(int));
	allLSA = malloc(sizeof(allLSATickets));
	allLSA->lsaTicket = neighborsLSA;
	allLSA->LSAUpdated = MSGTRY;
	dijkstrasAlgorithm(-1);
}

