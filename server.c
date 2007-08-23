/**
 * Messaging for the system
 * Controls the connection to the server, and the messages and interactions
 * going on.
 *
 * Decodes and encodes frames
 */
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <Ecore.h>
#include <Ecore_Con.h>

#include "server.h"
#include "tpe.h"
#include "tpe_event.h"

/*
 * For reference:
 *    struct _Ecore_Con_Event_Server_Data
     {
        Ecore_Con_Server *server;
        void             *data;
        int               size;
     };
 */

enum {
	HEADER_SIZE = 16
};

#define ID "GalaxiE"


static const struct msgname {
	const char *name;
	int tp03;
} msgnames[] = {
	{ "MsgOK", 			 0 },
	{ "MsgFail", 			 1 },
	{ "MsgSEQUENCE", 		 2 },
	{ "MsgConnect", 		 3 },
	{ "MsgLogin", 			 4 },	
	{ "MsgGetObjectsByID",		 5 },
	{ "MsgUNUSED1", 		 6 },
	{ "MsgObject", 			 7 },
	{ "MsgGetOrderDescription",	 8 },
	{ "MsgOrderDescription",	 9 },
	{ "MsgGetOrder", 		10 },
	{ "MsgOrder", 			11 },
	{ "MsgInsertOrder", 		12 },
	{ "MsgRemoveOrder", 		13 },
	{ "MsgGetTimeRemaining", 	14 },
	{ "MsgTimeRemaining", 		15 },
	{ "MsgGetBoards", 		16 },
	{ "MsgBoard", 			17 },
	{ "MsgMessageGet", 		18 },
	{ "MsgMessage", 		19 },
	{ "MsgPOST_MESSAGE", 		20 },
	{ "MsgREMOVE_MESSAGE", 		21 },
	{ "MsgGetResourceDescription",		22 },
	{ "MsgResourceDescription", 		23 },
	{ "MsgREDIRECT", 			24 },
	{ "MsgGetFeatures", 			25 },
	{ "MsgAvailableFeatures", 		26 },
	{ "MsgPING", 				27 },
	{ "MsgGetObjectIDs", 			28 },
	{ "MsgGET_OBJECT_IDS_BY_POSITION", 	29 },
	{ "MsgGET_OBJECT_IDS_BY_CONTAINER", 	30 },
	{ "MsgListOfObjectIDs", 		31 },
	{ "MsgGetOrderDescriptionIDs",	 	32 },
	{ "MsgOrderDescriptionIDs", 		33 },
	{ "MsgProbeOrder", 			34 },
	{ "MsgGetBoardIDs", 			35 },
	{ "MsgListOfBoards", 			36 },
	{ "MsgGetResourceDescriptionIDs",	37 },
	{ "MsgListOfResourceDescriptionsIDs",	38 },
	{ "MsgGetPlayerData",	 		39 },
	{ "MsgPlayerData", 			40 },
	{ "MsgGET_CATEGORY", 			41 },
	{ "MsgCATEGORY", 			42 },
	{ "MsgADD_CATEGORY", 			43 },
	{ "MsgREMOVE_CATEGORY", 		44 },
	{ "MsgGET_CATEGORY_IDS", 		45 },
	{ "MsgLIST_OF_CATEGORY_IDS", 		46 },
	{ "MsgGetDesign", 			47 },
	{ "MsgDesign", 				48 },
	{ "MsgADD_DESIGN", 			49 },
	{ "MsgMODIFY_DESIGN", 			50 },
	{ "MsgREMOVE_DESIGN", 			51 },
	{ "MsgGetDesignIDs", 			52 },
	{ "MsgListOfDesignIDs", 		53 },
	{ "MsgGET_COMPONENT", 			54 },
	{ "MsgCOMPONENT", 			55 },
	{ "MsgGET_COMPONENT_IDS", 		56 },
	{ "MsgLIST_OF_COMPONENT_IDS", 		57 },
	{ "MsgGET_PROPERTY", 			58 },
	{ "MsgPROPPERY", 			59 },
	{ "MsgGET_PROPERTY_IDS", 		60 },
	{ "MsgLIST_OF_PROPERTY_IDS", 		61 },
	{ "MsgCreateAccount",			62 },
};
#define N_MESSAGETYPES (sizeof(msgnames)/sizeof(msgnames[0]))

struct servers {
	struct tpe *tpe;
	struct server *servers;
};


/**
 * Status structure for msg.  Keeps track of all the important info
 * for the system.
 */
struct server {
	struct server *next;
	struct servers *servers;

	/* Actual server connection */
	Ecore_Con_Server *svr;

	/* Callbacks for connect */
	conncb conncb;
	void *conndata;

	/* Sequence */
	unsigned int seq;
	struct server_cb *cbs;

	/* Header */
	unsigned int header;

	/* Buffered Data */
	struct {
		int size;
		char *data;
	} buf;
};



struct server_cb {
	struct server_cb *next,*prev;

	int seq;

	msgcb 	cb;
	void *userdata;

	int deleteme;

};


static void server_event_register(struct tpe *tpe);


static int server_receive(void *data, int type, void *edata);
static int server_con_event_server_add(void *data, int type, void *edata);
static int server_cb_add(struct server *server, int seq, msgcb cb, void *userdata);
//static void connect_logged_in(void *server, const char *type, int len, void *mdata);
//static int server_register_events(struct server *server);
//static void connect_accept(void *server, const char * type, int len,
//		void *mdata);
static void server_handle_packet(struct server *server, int seq, int type, 
			int len, void *data);

static int format_server(int32_t *buf, const char *format, va_list ap);
static void msg_free(void *udata, void *msg);

//static const uint32_t headerv4 = htonl(('T' << 24) | ('P' << 16) | (4 << 8) | 0);  
//static const uint32_t headerv3 = htonl(('T' << 24) | ('P' << 16) | ('0' << 8) | ('3'));  
struct servers *
server_init(struct tpe *tpe){
	struct servers *servers;

	if (tpe->servers) return tpe->servers;

	ecore_con_init();

	servers = calloc(1,sizeof(struct servers));
	if (servers == NULL) return NULL;
	tpe->servers = servers;
	servers->tpe = tpe;

	/* FIXME */
	//server->header = htonl(('T' << 24) | ('P' << 16) | (4 << 8) | 0);  
	//server->header = htonl(('T' << 24) | ('P' << 16) | ('0' << 8) | '3');  
	
	//server->seq = 1;
	
	/* Register events */
	server_event_register(tpe);

	ecore_event_handler_add(ECORE_CON_EVENT_SERVER_ADD, 	
			server_con_event_server_add, tpe);
	ecore_event_handler_add(ECORE_CON_EVENT_CLIENT_DATA,
			server_receive, tpe);
	ecore_event_handler_add(ECORE_CON_EVENT_SERVER_DATA,
			server_receive, tpe);

	return servers;
}

static void
server_event_register(struct tpe *tpe){
	int i,n;

	n = sizeof(msgnames)/sizeof(msgnames[0]);

	for (i = 0 ; i < n ; i ++){
		tpe_event_type_add(tpe->event, msgnames[i].name);
	}
}

/**
 * Initialise a connection to a server
 */
struct server *
server_connect(struct tpe *tpe, 
		const char *servername, int port, int usessl,
		conncb cb, void *userdata){
	Ecore_Con_Server *svr;
	struct server *server;

	server = calloc(1,sizeof(struct server));
	
	/* FIXME: ssl */
	svr = ecore_con_server_connect(ECORE_CON_REMOTE_SYSTEM,
				servername,port,server);
	
	/* Start by assuming TP 4 */
	server->header = htonl(('T' << 24) | ('P' << 16) | (4 << 8) | 0);  
	
	server->seq = 1;
	
	/* Register events */
	server->servers = tpe->servers;

	server->svr = svr;

	server->conncb = cb;
	server->conndata = userdata;

	return server;
}

/***************************************/

/* 
 * Callback to say Ecore_Con has connected
 *
 * Now we will attempt to connect
 */
static int server_con_event_server_add(void *data, int type, void *edata){
	struct server *server;
	server = data;
printf("Server add %p %p\n",server,server->conncb);

	if (server->conncb)
		server->conncb(server->conndata, NULL);
	
	server->conncb = NULL;
	server->conndata = NULL;

	//server = data;
	//server_send_strings(server, TPE_MSG_CONNECT, connect_accept,server,ID, NULL);

	return 0;
}

static int 
server_receive(void *udata, int ecore_event_type, void *edata){
	struct servers *servers;
	struct server *server;
	Ecore_Con_Event_Server_Data *data;
	unsigned int *header;
	char *start;
	unsigned int len, type, seq, remaining;
	int magic;

	servers = udata;
	data = edata;

	for (server = servers->servers ; server ; server = server->next){
		if (server->svr == data->server)
			break;
	}

	assert(server != NULL);

	if (server->buf.size){
		start = realloc(server->buf.data, server->buf.size + data->size);
		remaining = server->buf.size + data->size;
		memcpy(start + server->buf.size, data->data, data->size);
		server->buf.data = start; /* Save it to free later */
	} else {
		start = data->data;
		remaining = data->size;
		server->buf.data = NULL; /* Just to check */
	}

	while (remaining > 16){
		header = (uint32_t *)start;
		magic = header[0];
		if (server->header != magic){
			printf("Invalid magic ;%.4s;\n",(char *)&magic);
			exit(1);
		}
		seq = ntohl(header[1]);
		type = ntohl(header[2]);
		len = ntohl(header[3]);
		if (len + 16 > remaining)
			break;
		server_handle_packet(server, seq, type, len, start);
		start += len + 16;
		remaining -= len + 16;
	}

	if (remaining){
		/* Malloc a new buffer to save it in */
		char *tmp;
		tmp = malloc(remaining);
		memcpy(tmp, start, remaining);
		free(server->buf.data);

		server->buf.data = tmp;
		server->buf.size = remaining;
	} else {
		free(server->buf.data);
		server->buf.data = NULL;
		server->buf.size = 0;
	}
	return 1;
}

static void
server_handle_packet(struct server *server, int seq, int type, 
			int len, void *data){
	struct msg *msg;
	struct server_cb *cb,*next;
	const char *event = "Unknown";
	int i;

	for (i = 0 ; i < N_MESSAGETYPES ; i ++){ 
		if (msgnames[i].tp03 == type){
			event = msgnames[i].name;
			break;
		}
	}

	assert(i != N_MESSAGETYPES);

	msg = calloc(1,sizeof(struct msg));
	msg->type = event;
	msg->tpe = server->servers->tpe;
	msg->seq= seq;
	msg->len = len;
	msg->data = malloc(len);
	memcpy(msg->data, data, len);
	msg->end = (char*)msg->data + len;
	msg->protocol = 4; /* FIXME */

	//printf("Handling Seq %d [%s]\n",seq,event);
	if (seq){
		for (cb = server->cbs ; cb ; cb = next){
			next = cb->next;
			if (cb->seq == seq){
				if (cb->cb)
					cb->cb(cb->userdata, msg);
				cb->deleteme = 1;
			} 
		}

		/* Now clean things up without callbacks */
		for (cb = server->cbs ; cb ; cb = next){
			next = cb->next;
			if (cb->deleteme){
				if (cb->prev)
					cb->prev->next = cb->next;
				else
					server->cbs = cb->next;
				if (cb->next)
					cb->next->prev = cb->prev;
				free(cb);
			} 
		}

	}


	tpe_event_send(server->servers->tpe->event, event, 
			msg, msg_free, NULL);
}	

static void
msg_free(void *udata, void *msg){
	free(msg);
}


int
server_send(struct server *server, const char *servertype, 
		msgcb cb, void *userdata,
		void *data, int len){
	unsigned int *buf;	
	int type = -1;
	int i;

	if (server == NULL) exit(1);
	if (len > 100000) exit(1);
	if (data == NULL && len != 0) exit(1);

	for (i = 0 ; i < N_MESSAGETYPES ; i ++){
		if (strcmp(servertype,msgnames[i].name) == 0){
			type = msgnames[i].tp03;
			break;
		}
	}

	if (type == -1){
		printf("Message type %s not found\n",servertype);
		exit(1);
	}

	buf = malloc(len + HEADER_SIZE);
	if (buf == NULL) exit(1);

	server->seq ++;

	buf[0] = server->header;
	buf[1] = htonl(server->seq);
	buf[2] = htonl(type);
	buf[3] = htonl(len);
	memcpy(buf + 4, data, len);

	//printf("Sending Seq %d Type %d [%s] Len: %d [%p]\n",
	//		server->seq,type,servertype, len,cb);
	ecore_con_server_send(server->svr, buf, len + HEADER_SIZE);

	free(buf);

	return server_cb_add(server, server->seq, cb, userdata);
}



int
server_send_strings(struct server *server, const char *servertype,
		msgcb cb, void *userdata,
		...){
	va_list ap;
	int total,len,lens,pos,rv;
	const char *str;
	int nstrs = 0;
	char *buf;

	for (va_start(ap,userdata), total = 0 ; 
			(str = va_arg(ap,const char *)); ){
		total += strlen(str);
		nstrs ++;
	}
	va_end(ap);

	if (total == 0) return server_send(server, servertype, cb, userdata, NULL, 0);
	
	total += nstrs * 4;
	buf = malloc(total);


	pos = 0;
	for (va_start(ap,userdata), len = 0 ; 
			(str = va_arg(ap,const char *)); ){
		len =strlen(str);
		lens = htonl(len);
		memcpy(buf + pos, &lens, 4);
		pos += 4;

		memcpy(buf + pos, str, len);
		pos += len;
	}
	va_end(ap);

	rv = server_send(server, servertype, cb, userdata, buf, total);
	free(buf);
	return rv;
}

/* 
 *
 * Format options:
 *  V : Dump the raw packet before it is sent (no args)
 *  i : Integer (one arg: int32_t)
 *  l : Long integer (one arg: uint64_t)
 *  0 : A zero (no arg)
 *  s : String (one arg: char *)
 *  r : Raw data (pre-formatted, two args, len (ints) data pointer 
 */
int 
server_send_format(struct server *server, const char *type,
		msgcb cb, void *userdata,
		const char *format, ...){
	va_list ap;
	int32_t *buf;
	int len,rv;

	buf = NULL;
	va_start(ap, format);
	len = format_server(buf, format, ap);
	va_end(ap);

	if (len > 0){
		buf = malloc(sizeof(int32_t) * len);
		va_start(ap, format);
		format_server(buf, format, ap);
		va_end(ap);
	}

	rv = server_send(server, type, cb, userdata, buf, len*sizeof(int32_t));

	if (buf) free(buf);

	return rv;
}

static int
format_server(int32_t *buf, const char *format, va_list ap){
	int32_t val;
	int len,padlen;
	int64_t val64;
	char *str;
	int pos;
	int dump = 0;

	pos = 0;

	while (*format){
		switch (*format){
		case '0':
			if (buf)
				buf[pos] = 0;
			pos ++;
			break;
		case 'i':
			val = va_arg(ap, int32_t);
			if (buf)
				buf[pos] = htonl(val);
			pos ++;
			break;
		case 'l':
			val64 = va_arg(ap, int64_t);
			val64 = htonll(val64);
			if (buf)
				memcpy(buf + pos,&val64,sizeof(int64_t));
			pos += 2;
			break;
		case 's':
			str = va_arg(ap, char *);
			if (str == NULL || (len = strlen(str)) == 0){
				if (buf) buf[pos] = 0;
				pos ++;
				break;
			}


			if (len % 4)
				padlen = len + (4 - len % 4);
			else
				padlen = len;

			if (buf)
				buf[pos] = htonl(padlen);
			pos ++;
			if (buf)
				strncpy((char*)(buf + pos),str,padlen);
			pos += padlen / 4;
			break;
		case 'r':
			len = va_arg(ap, int);
			str = va_arg(ap, void *);
			if (buf)
				memcpy(buf + pos, str, sizeof(int32_t) * len);
			pos += len;
			break;
		case 'V':
			dump ++;
			break;
		default:
			fprintf(stderr,"unknown format character %c\n",
					*format);
			exit(1);
		}
		format ++;
	}

	if (buf && dump){
		int i;
		for (i = 0 ; i < pos ; i ++){
			printf("%08x ",ntohl(buf[i]));
			if (i % 8 == 7) printf("\n");
		}
		printf("\n");
	}

	return pos;
}


static int
server_cb_add(struct server *server, int seq, msgcb cb, void *userdata){
	struct server_cb *tmcb,*tmp;

	tmcb = calloc(1,sizeof(struct server_cb));
	tmcb->seq = seq;
	tmcb->cb = cb;
	tmcb->userdata = userdata;
	tmcb->next = NULL;
	tmcb->prev = NULL;
	
	if (server->cbs == NULL)
		server->cbs = tmcb;
	else {
		for (tmp = server->cbs ; tmp->next ; tmp = tmp->next)
			;
		tmp->next = tmcb;
		tmcb->prev = tmp;
	}

	return 0;
}




