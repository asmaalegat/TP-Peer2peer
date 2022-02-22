#include <stdio.h> /* for printf() and fprintf() */
#include <sys/socket.h> /* for socket(), connect(), send(), and recv() */
#include <arpa/inet.h> /* for sockaddr_in and inet_addr() */
#include <stdlib.h> /* for atoi() */
#include <string.h> /* for memset() */
#include <unistd.h> /* for close() */
#include <signal.h> /* For sigaction() */
#include <sys/file.h>
#include <fcntl.h>
#include <math.h>
#include <pthread.h>    /* POSIX Threads */
#include "head_vars.h"
#include "headfun.h"

int recvport; /* Listening Port */
void ReceivePart(void *ptr);
void WriteToClosedSocket(int signalType);

int main(int argc, char *argv[])
{
	pthread_t thread1; /* Thread to handle the listening port */
	pthread_t receivepacketthread[MAXCLIENTS]; /* Threads to handle multiple peers */
	struct ThreadArgs *threadArgs; /* Thread Arguments for the Listening Port thread */
	int sock,option,len_file,packet_count,last_packet,no_of_files,i,j,k=0,buf_len,no_of_peers=0;
	int register_break = 1; /* To abort the register file routine */
	struct sockaddr_in ServAddr; /* Server address */
	unsigned short ServPort;	/* Server port */
	char *servIP,*return_string;	/* Server IP address (dotted quad) */
	FILE *fp; /* File Pointer to access files */
	int choose_file,contact_port,ports[MAXCLIENTS];
	int probpeer,noprobpeer;
	struct RegisterMsg msg,file_loc_reply;
	struct FileListReply file_list;

	if(argc != 4)
	{
		fprintf(stderr, "Usage: %s <Server IP> <Server Port> <Receive Port>\n",argv[0]);
		exit(1);
	}

	servIP = argv[1] ;	/* First arg server IP address (dotted quad) */
	ServPort = atoi(argv[2]); /* Server Port */
	recvport = atoi(argv[3]); /* Receive Port */

	/* Construct the server address structure */
	memset(&ServAddr, 0, sizeof(ServAddr)); /* Zero out structure */
	ServAddr.sin_family = AF_INET; /* Internet address family */
	ServAddr.sin_addr.s_addr = inet_addr(servIP); /* Server IP address */
	ServAddr.sin_port = htons(ServPort); /* Server port */

	/* Thread to handle the listening; It runs forever */
	pthread_create (&thread1, NULL, (void *) &ReceivePart, NULL);
	printf("\n\n\n=================UNIVERISTE DE VERSAILLES=================\n\n=================Projet Programmation IP==================\n\n   Présenté par :\n\n                 => Hadj Ahmed BESSEDIK  \n\n                 => Fadlellah BELHASSAINI \n\n                 => Rami CHAABANE\n\n=================ANNEE UNIVERSITAIRE 2016-2017============\n\n");

	while(1)
	{

	printf("\nEntrez votre option :\n1. Partager des fichiers avec le serveur \n2. Demander la liste des fichiers \n3. telecharger un fichier\n4. quitter\n\n\n");
	scanf("%d",&option);

	/* Create a reliable, stream socket using TCP */
	if ((sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
		DieWithError("socket () failed") ;

	switch(option)
	{
	case 1:
	{
		if (connect(sock, (struct sockaddr *) &ServAddr, sizeof(ServAddr)) < 0)
			DieWithError("connect () failed") ;
		/* Establish the connection to the server */
		register_break = 1;
		MsgSendNum(sock,1,sizeof(int));
		printf("Entrer le nombre de fichiers a partager :");
		scanf("%d",&no_of_files);
		/* Send the number of files to be registered */
		MsgSendNum(sock,no_of_files,sizeof(int));
		for(i=0;i<no_of_files;i++)
		{
			while(1)
			{
				printf("Entrer le nom de fichier %d : ",i+1);
				scanf("%s",msg.filename);

				if((fp = fopen(msg.filename,"r")) != NULL)
					break;
				/* Error Handling when inputting filename */
				printf("Fichier introuvable! Essayez à nouveau\n");
				printf("Enterr 0 pour annuler l'operation ou Autre chose pour continuer : \n");
				scanf("%d",&register_break);
				if(register_break == 0)
					break;
			}
			/* Make or break for the registering */
			MsgSendNum(sock,register_break,sizeof(int));
			if(register_break == 0)
				break;
			msg.filesize = ComputeFileSize(fp);
			msg.ipaddress = htonl(INADDR_ANY);
			msg.portnum = htons(recvport);
			/* Register the file with the server */
			RegisterClient(sock,msg);
			if(MsgReceiveNum(sock))
				printf("Enregistré avec succès\n");
			fclose(fp);
		}
		close(sock);
		break;
	}
	case 2:
	{
		/* File List Request Command */
		memset(&file_list, 0, sizeof(file_list));
		if (connect(sock, (struct sockaddr *) &ServAddr, sizeof(ServAddr)) < 0)
			DieWithError("connect () failed") ;
		MsgSendNum(sock,2,sizeof(int));
		/* Receive the number of files registered to the server */
		no_of_files=MsgReceiveNum(sock);
		printf("Il y a totalement %d fichiers dans le serveur\n",no_of_files);
		/* Get the filenames of all the files registered to the server */
		for(i=0;i<no_of_files;i++)
		{
			buf_len = MsgReceiveNum(sock);
			if ((recv(sock, &file_list, buf_len, 0)) < 0)
				DieWithError("recv() failed") ;
			/* Print the list of files along with the file size*/
			printf("%d. %s\t%d\n",i+1,file_list.filename,file_list.filesize);

		}
		close(sock);
		break;

	}
	case 3:
	{
		/* File Location Request Command */
		if (connect(sock, (struct sockaddr *) &ServAddr, sizeof(ServAddr)) < 0)
			DieWithError("connect () failed") ;
		MsgSendNum(sock,3,sizeof(int));
		no_of_files=MsgReceiveNum(sock);
		/* User chooses a file by inputting a number */
		printf("Which file do you want? : ");
		scanf("%d",&choose_file);
		/* Error handling if the user input is not proper */
		if(choose_file > no_of_files || choose_file < 1)
		{
			printf("Ne joue pas avec moi, je suis plus intelligent que toi!\n");
			break;
		}
		MsgSendNum(sock,choose_file,sizeof(int));
		/* Receive the number of peers having the same file */
		no_of_peers = MsgReceiveNum(sock);
		/* Get the port numbers of the peers */
		for(i=0;i<no_of_peers;i++)
		{
			if ((recv(sock, &file_loc_reply, sizeof(file_loc_reply), 0)) < 0)
				DieWithError("recv() failed");
			ports[i] = ntohs(file_loc_reply.portnum);
		}
		close(sock);
		/* Create a thread to connect to all the peers and download parts of the file from each */
		for(i=0;i<no_of_peers;i++)
		{
			contact_port = ports[i];
			if((threadArgs = (struct ThreadArgs *)malloc(sizeof(struct ThreadArgs))) == NULL)
				DieWithError("malloc() failed");
			threadArgs -> contact_port = contact_port;
			threadArgs -> filename = malloc(32);
			threadArgs -> filename = file_loc_reply.filename;
			threadArgs -> len_file = file_loc_reply.filesize;
			threadArgs -> peer_no = i;
			threadArgs -> no_of_peers = no_of_peers;
			if (pthread_create(&receivepacketthread[i], NULL, ReceivePacketFromPeer, (void *)threadArgs) != 0)
				DieWithError("pthread_create() failed");
		}
		/* Wait for all the threads to finish downloading */
		for(i=0;i<no_of_peers;i++)
		{
			pthread_join(receivepacketthread[i],NULL);
		}
		/* Fault Tolerance */
		if(peerleft == 1) /* If some peer has left in the middle */
		{
			peerleft = 0;
			/* If the number of peers = 1, Can't help it, exit */
			if(no_of_peers == 1)
			{
				/* Create a reliable, stream socket using TCP */
				if ((sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
					DieWithError("socket () failed") ;

				if (connect(sock, (struct sockaddr *) &ServAddr, sizeof(ServAddr)) < 0)
					DieWithError("connect () failed") ;
				/* Establish the connection to the server */

				/* Send Leave request for the misbehaving peer */
				UnregisterPeer(sock,ports[0]);
				printf("Peer est parti à mi-chemin. Réessayez plus tard.\n");
				break;
			}
			/* If there are more than one peers, download the file parts from working peers */
			/* The Download is not simultaneous anymore */
			else
			{
				for(i=0;i<no_of_peers;i++)
				{
					if(peererror[i] != 1)
						noprobpeer = i;
					else
						probpeer = i;
				}
				for(i=0;i<no_of_peers && peererror[i]==1;i++)
				{
					contact_port = ports[noprobpeer];
					printf("Peer %d a échoué, Démarrage de la retransmission pour la partie de fichier %d de peer %d", i+1, noprobpeer+1, i+1);
					sleep(1);
					if((threadArgs = (struct ThreadArgs *)malloc(sizeof(struct ThreadArgs))) == NULL)
						DieWithError("malloc() failed");
					threadArgs -> contact_port = contact_port;
					threadArgs -> filename = malloc(32);
					threadArgs -> filename = file_loc_reply.filename;
					threadArgs -> len_file = file_loc_reply.filesize;
					threadArgs -> peer_no = probpeer;
					threadArgs -> no_of_peers = no_of_peers;
					if (pthread_create(&receivepacketthread[probpeer], NULL, ReceivePacketFromPeer, (void *)threadArgs) != 0)
						DieWithError("pthread_create() failed");
					pthread_join(receivepacketthread[probpeer],NULL);

					/* Create a reliable, stream socket using TCP */
					if ((sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
						DieWithError("socket () failed") ;

					if (connect(sock, (struct sockaddr *) &ServAddr, sizeof(ServAddr)) < 0)
						DieWithError("connect () failed") ;
					/* Establish the connection to the server */

					UnregisterPeer(sock,ports[probpeer]);
				}
			}
		}
		printf("Téléchargement terminé!\n");
		/* Merging all the received parts and removing intermediate files */
		JoinFiles(no_of_peers,file_loc_reply.filename);


		/* Register the file which we just received */

		/* Create a reliable, stream socket using TCP */
		if ((sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
			DieWithError("socket () failed") ;

		if (connect(sock, (struct sockaddr *) &ServAddr, sizeof(ServAddr)) < 0)
			DieWithError("connect () failed") ;
		/* Establish the connection to the server */
		MsgSendNum(sock,1,sizeof(int));
		MsgSendNum(sock,1,sizeof(int));
		MsgSendNum(sock,1,sizeof(int));
		strcpy(msg.filename,file_loc_reply.filename);
		msg.filesize = file_loc_reply.filesize;
		msg.ipaddress = htonl(INADDR_ANY);
		msg.portnum = htons(recvport);
		RegisterClient(sock,msg);
		if(MsgReceiveNum(sock))
			printf("Enregistré avec succès\n");
		close(sock);


		break;

	}
	case 4:
	{
		/* Leave Request Command */
		/* Identify yourself using the port number */
		if (connect(sock, (struct sockaddr *) &ServAddr, sizeof(ServAddr)) < 0)
			DieWithError("connect () failed") ;
		MsgSendNum(sock,4,sizeof(int));
		MsgSendNum(sock,recvport,sizeof(int));
		close(sock);
		printf("tros long, au revoir!\n");
		exit(0);
		break;
	}

	default:
	{
		/* Error handling */
		printf("Mauvais choix, essayez de nouveau!");
		close(sock);
		break;
	}
	}

	}


	exit(0);
}

/* Function which handles the listening port to service the peers */
void ReceivePart(void *ptr)
{
	int rec_sock,clntsock,clntlen,msg_id,buf_len,packet_no,start_packet;
	char *return_string;
	int packet_count=0;
	FILE *fp;
	struct sockaddr_in clntaddr; /* Client address */
	struct sockaddr_in rec_sock_addr;
	struct sigaction handler;

	/* Set WriteToClosedSocket() as handler function */
	handler.sa_handler = WriteToClosedSocket;
	/* Create mask that masks all signals */
	if(sigfillset(&handler.sa_mask)<0)
		DieWithError("sigfillset() failed");
	/* No flags */
	handler.sa_flags = 0;

	/* Set signal handling for interrupt signals */
	if(sigaction(SIGPIPE,&handler,0) < 0)
		DieWithError("sigaction() failed");

	/* Construct local address structure */
	memset(&rec_sock_addr, 0, sizeof(rec_sock_addr)); /* Zero out structure */
	rec_sock_addr.sin_family = AF_INET; /* Internet address family */
	rec_sock_addr.sin_addr.s_addr = htonl(INADDR_ANY); /* Any incoming interface */
	rec_sock_addr.sin_port = htons(recvport); /* Local port */


	/* Create a reliable, stream socket using TCP for Reception */
	if ((rec_sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
		DieWithError("socket () failed") ;


	/* Bind to the local address */
	if (bind(rec_sock, (struct sockaddr *)&rec_sock_addr, sizeof(rec_sock_addr)) < 0)
		DieWithError ( "bind () failed");

		/* Mark the socket so it will listen for incoming connections */
	if (listen(rec_sock, MAXPENDING) < 0)
		DieWithError("listen() failed") ;

	for (;;) /* Run forever */
	{
		/* Set the size of the in-out parameter */
		clntlen = sizeof(clntaddr);
		/* Wait for a client to connect */
		if ((clntsock = accept(rec_sock, (struct sockaddr *) &clntaddr,
		&clntlen)) < 0)
			DieWithError("accept() failed");
		/* clntSock is connected to a client! */
		msg_id=MsgReceiveNum(clntsock);
		return_string = malloc(RCVBUFSIZE*sizeof(char));
		/* Get the name of the file the peer wants */
		MsgReceiveString(clntsock,return_string);
		printf("Demande de fichier : %s\n",return_string);
		/* Open the file */
		fp = fopen(return_string,"rb");
		/* Get the starting packet number and number of packets to be sent */
		start_packet = MsgReceiveNum(clntsock);
		packet_count = MsgReceiveNum(clntsock);
		packet_no = start_packet;
		printf("Transfert %d Paquets de %s\n",packet_count,return_string);
		while(packet_count)
		{
			/* Send the packet number requested for */
			packet_no=MsgReceiveNum(clntsock);
			FileSend(fp,clntsock,packet_no);
			/* If write to closed socket happens, break */
			if(socketerrorflag == 1)
				break;
			packet_no++;
			packet_count--;
		}
		/* If write to closed socket happens, clear flag and break */
		if(socketerrorflag == 1)
		{
			socketerrorflag = 0;
			break;
		}
		printf("Transfert Terminé\n");
		printf("Entrez votre option :\n1. Partager des fichiers avec le serveur \n2. Demander la liste des fichiers \n3. telecharger un fichier\n4. quitter\n");
		fclose(fp);
	}

	close(rec_sock);
}

void WriteToClosedSocket(int signalType)
{
	/* Fault Tolerance to handle SIGPIPE signal */
	printf("Interruption reçue. Arret du programme.\n");
	socketerrorflag = 1;
}
