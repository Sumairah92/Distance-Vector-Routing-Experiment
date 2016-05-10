#include <stdio.h>      /* for printf() and fprintf() */
#include <sys/socket.h> /* for socket(), connect(), sendto(), and recvfrom() */
#include <arpa/inet.h>  /* for sockaddr_in and inet_addr() */
#include <stdlib.h>     /* for atoi() and exit() */
#include <string.h>     /* for memset() */
#include <unistd.h>     /* for close() and alarm() */
#include <errno.h>      /* for errno and EINTR */
#include <signal.h>     /* for sigaction() */

///////////////////////////////Node Parameters///////////////////////////////////////

char all_nodes[] = {'A','B','C','D','E','F','G'};
char node_name;      //name of node read from file
int port;            //port of node read from file

//////////////////////////////Table Structures///////////////////////////////////////

typedef struct{
        char node;
        int dist;
        char ip[16];
}neighbor_t;		//structure for neighbor table

typedef struct{
	char dest;
	int dist;
	char next_hop;
}route_t;		//structure for routing table

typedef struct{
	char dest;
	int dist;
}dvr;			//structure for dvr element

typedef struct{
	char sender;
	int num_of_dest;
	dvr vectors[6];
}dvr_table;		//structure for distance vector table

////////////////////////////////////////Function Prototypes/////////////////////////////////////////////////

void CatchAlarm(int ignored);

////////////////////////////////////////Main///////////////////////////////////////////////////////////////

int main(int argc, char *argv[]){

if(argc !=2){				//check for main arguments
	printf("usage: %s config file",argv[0]);
	return(0);
	}

///////////////////////////////////////Allocating memory to table structures////////////////////////////

neighbor_t *n = (neighbor_t*)malloc(sizeof(neighbor_t)*10);
route_t *r = (route_t*)malloc(sizeof(route_t)*10);
dvr_table *d = (dvr_table*)malloc(sizeof(dvr_table));

/////////////////////////////////////Call initial table populating functions///////////////////////////

load_neighbor_table(n,argv[1]);
load_initial_routing_table(n,r);
load_dvrs(r,d);

/////////////////////////////////////////socket declarations//////////////////////////////////////////////

int sock,sock2;               /* Socket descriptors */
struct sockaddr_in recvAddr; /* for binding address */
struct sockaddr_in SendAddr; /*To-send to address*/
unsigned short ServPort;     /*port */
struct sockaddr_in fromAddr; /*incoming addresses*/
unsigned int fromAddrLen;
struct sigaction action;
int rc;
ServPort = 5555;  /*chose a port > 1024*/

if ((sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0){
    perror("Error creating Socket");
}

if ((sock2 = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0){
        perror("Error creating Socket");
}

action.sa_handler = CatchAlarm;
if(sigfillset(&action.sa_mask) < 0){
    perror(" An error with signal handler");
}
action.sa_flags = 0;
if (sigaction(SIGALRM, &action, 0) < 0)
  perror("sigaction() failed for SIGALRM");

memset(&recvAddr, 0, sizeof(recvAddr));
recvAddr.sin_family = AF_INET;
recvAddr.sin_addr.s_addr = htonl(INADDR_ANY);  /* for all incoming interfaces */
recvAddr.sin_port = htons(ServPort); 

memset(&SendAddr, 0, sizeof(SendAddr));
SendAddr.sin_family = AF_INET;
//SendAddr.sin_addr.s_addr = inet_addr(servIP); /*will set this later according to neighbor table*/
SendAddr.sin_port = htons(ServPort);

if(bind(sock, (struct sockaddr *) &recvAddr, sizeof(recvAddr))< 0){
        perror("Bind failed");
}

dvr_table dr;
int x = 0;
int check = 0; /*this will tell if routing table was updated or not*/
int tries = 0;
while(1){
	check = 0;
	fromAddrLen = sizeof(fromAddr);
	alarm(25); /*set the alarm*/
	if((rc = recvfrom(sock, &dr, sizeof(dvr_table),0 ,(struct sockaddr *) &fromAddr, &fromAddrLen)) < 0){
		if(errno == EINTR){	//will be true if interrupt received is from alarm
			x = 0;
			printf("\nSending periodic dvr to neighbors...\n");
			while(n[x].node!='\0'){ //get ip addresses of neighbors 
				SendAddr.sin_addr.s_addr = inet_addr(n[x].ip);
				if(sendto(sock,d, sizeof(dvr_table),0,(struct sockaddr *) &SendAddr, sizeof(SendAddr)) != sizeof(dvr_table)){
					printf("Error in sending");
				}
				x++;
			}		
		}
		else{
				printf("Error in receiving");
				return(0);
		}
	}
	else{
	//	alarm(0);
		printf("\ndvr received from %c\n",dr.sender);
		check = update_routing_table(r,&dr);
		if(check == 1){		//if routing table was changed, update dvrs and send
			load_dvrs(r,d);
			x = 0;
			printf("\nSending updated dvrs\n");
			while(n[x].node!='\0'){  
				SendAddr.sin_addr.s_addr = inet_addr(n[x].ip);
				if(sendto(sock,d, sizeof(dvr_table),0,(struct sockaddr *) &SendAddr, sizeof(SendAddr)) != sizeof(dvr_table)){
					printf("Error in sending");
				}
				x++;
			}
		}
		else{
			tries++;
			if(tries > 6){
				x = 0;
			printf("\nSending periodic dvr to neighbors...\n");
			while(n[x].node!='\0'){ //get ip addresses of neighbors 
				SendAddr.sin_addr.s_addr = inet_addr(n[x].ip);
				if(sendto(sock,d, sizeof(dvr_table),0,(struct sockaddr *) &SendAddr, sizeof(SendAddr)) != sizeof(dvr_table)){
					printf("Error in sending");
				}
				x++;
			}
				tries = 0;
			}

		}	
	}
}

free(n);
free(r);		/*free memory*/
free(d);
close(sock);
close(sock2);		/*close sockets*/
return(0);
}

//////////////////////////////////////////////////////Function definitions////////////////////////////////////////

void CatchAlarm(int ignored){

}

///////////////////////////////////Function to load neighbor table from file/////////////////////////
load_neighbor_table(neighbor_t *n, char *c){
int x,i;
FILE *fh;
fh = fopen(c,"r");

if(fh == NULL){
        perror("Error opening file");
        return 1;
        }
else {
x = 0;
        fscanf(fh, "%c\n%d\n", &node_name, &port);
        while(!feof(fh)){
                fscanf(fh, "%c %d %s\n",&n[x].node,&n[x].dist,n[x].ip);
                x++;
        }
/*printf("Neighbor Table\n");
printf("%c\n%d\n",node_name,port);
i = 0;
while(n[i].node!='\0'){  
      printf("%c %d %s\n",n[i].node,n[i].dist,n[i].ip);
        i++;
        }
*/

fclose(fh);
}
}

/////////////////////////////////function to load intial routing table//////////////////////////
load_initial_routing_table(neighbor_t *n, route_t *r){
int i,j,x,t;
j = 0;
t = 0;
for(i = 0; i < 7; i++)
{

	if(node_name == all_nodes[i]){		//check to not add own node to routing table
		continue;
	}
	else{
		t = 0;
		r[j].dest = all_nodes[i];
		x = 0;
		while(n[x].node!='\0'){
			if(n[x].node==r[j].dest){
				r[j].dist = n[x].dist;
				r[j].next_hop = n[x].node;
				t = 1;
				break;
			}
			x++;
		}
		if(t == 0){			//will be zero for the nodes not in routing table
			r[j].dist = -1 ;
			r[j].next_hop = '-';
		}
		j++;

	}
}
printf("\n----------------------\n");
printf("initial routing table");
j=0;
printf("\n");
while(r[j].dest!='\0'){
 printf("%c %d %c\n",r[j].dest,r[j].dist,r[j].next_hop);
        j++;
        }
printf("----------------------\n");
}

/////////////////////////////////function to load distance vectors from routing table/////////
load_dvrs(route_t *r,dvr_table *d){
int i,j;
d->sender = node_name;
i = 0;
while(r[i].dest!='\0'){
	d->vectors[i].dest = r[i].dest;
	d->vectors[i].dist = r[i].dist;
	i++;
}
d->num_of_dest = i;	
/*
printf("Distance vectors\n");
printf("from: %c num of dest:%d\n",d->sender, d->num_of_dest);
j=0;
for(j = 0; j < i; j++){
printf("%c %d\n",d->vectors[j].dest,d->vectors[j].dist);
}
*/
}

///////////////////////////////function to update routing table/////////////////////////
int update_routing_table(route_t *r,dvr_table *dr){
int i,j,cost,ck;
ck = 0;                     //flag to check if table was updated
for(i = 0; i < 6; i++){
	if(dr->sender == r[i].dest){
		cost = r[i].dist;	//get cost to the sender of the dvr received
	}
}
for(i = 0; i < dr->num_of_dest; i++){
	if(dr->vectors[i].dest == node_name){ //if its cost to the node itself, continue
		continue;
	}
	else{
		for(j = 0; j < 6; j++){
			if(dr->vectors[i].dest == r[j].dest){ //check for each dest
				if(r[j].next_hop == dr->sender && r[j].dist!= dr->vectors[i].dist + cost){
					 r[j].dist = dr->vectors[i].dist + cost;
                                         r[j].next_hop = dr->sender; 
                                         ck = 1;
				}/*this clause checks for dynamic link changes only*/
	
				if(dr->vectors[i].dist == -1){ //if -1 is received continue
					continue;
				}
				else if(r[j].dist > (dr->vectors[i].dist + cost)){ //if current>recv+cost
					r[j].dist = dr->vectors[i].dist + cost;
					r[j].next_hop = dr->sender; 
					ck = 1;
				}
				else if(r[j].dist == -1 && dr->vectors[i].dist != -1){//if i have infinity and received has something other than infinity, update
					 r[j].dist = dr->vectors[i].dist + cost;
                                         r[j].next_hop = dr->sender;
					 ck = 1; 	
				}
				else{
				}				
			}
		}
	}
}
if(ck == 1){//if routing table was changed, print

printf("----------------------\n");
printf("updated routing table\n");
for(j = 0; j < 6; j++){
 printf("%c %d %c\n",r[j].dest,r[j].dist,r[j].next_hop);
        }
printf("----------------------\n");
}
return ck;
}

