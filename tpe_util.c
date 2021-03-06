
#include <arpa/inet.h>
#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <talloc.h>

#include <Ecore_Data.h>

#include "tpe_util.h"

#include "galaxie.h"
#include "server.h" /* for the htonll */
#include "objects.h" /* Only for types */
#include "tpe_orders.h" /* Only for types */

/* Parser functions */
static int parse_header(void **data, void **end, va_list *ap);

static int extract_int(const char **src);
static char *extract_string(const char **src, int *len);
static int64_t extract_long(const char **src);
static void * extract_struct(const char **buf, struct parseitem *item, int count);


/**
 * tpe_util_string_extract
 *
 * Extracts a string (in byte swapped format), and returns a pointer to first
 * byte after
 */
char *
tpe_util_string_extract(const char *src, int *lenp, const char **endp){
	int len;
	char *buf;
	len = ntohl(*(int *)src);
	/* This assert needs to go */
	assert(len < 2000);
	if (endp) *endp = src + len + 4;
	if (lenp) *lenp = len;
	buf = malloc(len + 1);
	strncpy(buf,src + 4, len);
	buf[len] = 0;
	return buf;
}


/*
 * Raw dump of the packet to standard out
 * Warning: May be big
 *
 */
char *
tpe_util_dump_packet(void *pdata){
	char *p;

	p = pdata;
	printf("%c%c%c%c %d %d %d\n",
			p[0],p[1],p[2],p[3],
			ntohl(*(int *)(p + 4)),
			ntohl(*(int *)(p + 8)),
			ntohl(*(int *)(p + 12)));

	return NULL;
}


/**
 * tpe_util_parse_packet: Parses a packet into the specified data pointers.
 *
 * FIXME: Need a more general way of parsing complex structures (lists)
 *
 * Format string:
 *  -: Nothing - skip an int
 *  H: TP03/TP04 header
 *  	Protocol magic  (int)
 *  	SeqID   (int)
 *  	Message Type (int)
 *  	end	(pointer)
 *  s: A string - will be malloced into pointer
 *  i: int - 32 bit int
 *  l: long long int - 64 bit int
 *  a: Array of ints
 *  r: Array of generic references
 *  O: Array of Oids
 *  S: Array of ships
 *  R: Array of Planet resources 
 *  B: Array of build resouts { int resource id, #required }
 *  Q: Array of order description args
 *  6: Arg type 6: An array of { int, string, int }
 *  p: Save a pointer to the current offset
 */
int
tpe_util_parse_packet(void *pdata, void *end, char *format, ...){
	int parsed;
	va_list ap;
	int rv = 0;
	void *endtmp = NULL;
	char *tformat;

	tformat = format;
	parsed = 0;
	if (format == NULL){
		printf("You must pass a format string to %s\n",__FUNCTION__);
		return -1;
	}

	if (end == NULL && *format != 'H')
		printf("tpe_util_parse_packet called without end: %s\n",format);
	
	if (end == NULL)
		end = &endtmp;

	va_start(ap,format);
	
	while (*format){
		if (end && pdata > end){
			printf("**** Serious Error occurred ****\n");
			printf("Overflow of the end of the message buffer\n");
			printf("Format was %s\n",tformat);
			printf("Current arg is %s\n",format);
			exit(1);
			return -1;
		}

		switch (*format){
			case '-':{
				int *idata;
				format ++;
				parsed ++;
				idata = pdata;
				idata ++;
				pdata = idata;
				break;
			}
			case 'H':{
				format ++;
				parsed ++;
				rv = parse_header(&pdata, &end, &ap);
				break;
			}
			case 'i':{ /* Single 32 bit int */
				 int val;
				 int *idata;
				 int *dest;

				 dest = va_arg(ap,int *);	

				 idata = pdata;
				 val = ntohl(*idata);
				 idata ++;

				 if (dest) *dest = val;

				 pdata = idata;
				 format ++;
				 parsed ++;
				 break;
			 }
			case 'l':{ /* Single 64 bit int */
				 int64_t val;
				 int64_t *idata;
				 int64_t *dest;

				 dest = va_arg(ap,int64_t *);	

				 idata = pdata;
				 val = *idata;
				 val = ntohll(val);
				 idata ++;

				 if (dest) *dest = val;

				 pdata = idata;
				 format ++;
				 parsed ++;
				 break;
			 }
			case 's':{ /* string */
				char **dest;
				char *sval;

				dest = va_arg(ap, char **);

				sval = tpe_util_string_extract(pdata, 
					NULL, (void *)&pdata);
				if (dest)
					*dest = sval;
				else
					free(sval);

				format ++;
				parsed ++;
				break;
			}
			case 'a':{ /* Array of ints */
				int **adest;
				int *cdest;
				int *idata;
				int len;
				int i;

				idata = pdata;
				len = ntohl(*idata);
				idata ++;
			
				cdest = va_arg(ap, int *);
				adest = va_arg(ap, int **);

				if (cdest) *cdest = len;

				if (adest){
					*adest = realloc(*adest, (len + 1)* sizeof(int));
					for (i = 0 ; i < len ; i ++){
						(*adest)[i] = ntohl(*idata ++);
					}
				} else {
					idata += len;
				}

				pdata = (char *)idata;
				format ++;
				parsed ++;
				break;
			}
			case 'O':{
				struct ObjectSeqID **adest;
				int len;
				int *idata;
				int64_t tmp;
				int *cdest;
				int i;
				
				idata = pdata;
				len = ntohl(*idata);
				idata++;

				cdest = va_arg(ap, int *);
				adest = va_arg(ap, struct ObjectSeqID **);

				if (cdest) *cdest = len;
	
				*adest = realloc(*adest, (len+1)*
						sizeof(struct ObjectSeqID));
				
				for (i = 0 ; i < len ; i ++){
					(*adest)[i].oid = ntohl(*idata ++);
					memcpy(&tmp,idata,8);
					(*adest)[i].updated = ntohll(tmp);
					idata += 2;
				}

				pdata = (char *)idata;
				format ++;
				parsed ++;
				break;

			}
			case 'p':{
				int **adest;
				format ++;
				adest = va_arg(ap, int **);
				if (adest != NULL)
					*adest = pdata;
				else	
					printf("Why request a p and not use it\n");
				break;
			}


			/* Array of order arguments */
			case 'Q':{
				struct order_arg **adest;
				int len;
				int *idata;
				int *cdest;
				int i;
				
				idata = pdata;
				len = ntohl(*idata);
				idata++;

				cdest = va_arg(ap, int *);
				adest = va_arg(ap, struct order_arg **);

				if (cdest) *cdest = len;
	
				*adest = realloc(*adest, (len+1)*
						sizeof(struct order_arg));

				for (i = 0 ; i < len ; i ++){
					(*adest)[i].name = 
						tpe_util_string_extract(
								(void *)idata, 
								NULL, 
								(void *)&idata);
					(*adest)[i].arg_type = ntohl(*idata ++);
					(*adest)[i].description = 
						tpe_util_string_extract(
								(void *)idata, 
								NULL, 
								(void *)&idata);
				}

				pdata = (char *)idata;
				format ++;
				parsed ++;
				break;
			}
			/* Array of order arguments */
			case 'r':{
				struct reference **adest;
				int len;
				int *idata;
				int *cdest;
				int i;
				
				idata = pdata;
				len = ntohl(*idata);
				idata++;

				cdest = va_arg(ap, int *);
				adest = va_arg(ap, struct reference **);

				if (cdest) *cdest = len;
	
				*adest = realloc(*adest, (len+1)*
						sizeof(struct reference));

				for (i = 0 ; i < len ; i ++){
					(*adest)[i].type = ntohl(*idata);
					idata ++;
					(*adest)[i].value = ntohl(*idata);
					idata ++;
				}

				pdata = (char *)idata;
				format ++;
				parsed ++;
				break;
			}

			/* Arg type '6'
			 * 	An array returned when querying orders */
			case '6':{
				struct arg_type6 **adest;
				int len;
				int *idata;
				int *cdest;
				int i;
				
				idata = pdata;
				len = ntohl(*idata);
				idata++;

				cdest = va_arg(ap, int *);
				adest = va_arg(ap, struct arg_type6 **);

				if (cdest) *cdest = len;
	
				*adest = realloc(*adest, (len+1)*
						sizeof(struct arg_type6));

				for (i = 0 ; i < len ; i ++){
					(*adest)[i].id = ntohl(*idata ++);
					(*adest)[i].name = 
						tpe_util_string_extract(
								(void*)idata, 
								NULL, 
								(void *)&idata);
					(*adest)[i].max = ntohl(*idata ++);
				}

				pdata = (char *)idata;
				format ++;
				parsed ++;
				break;
			}
			/* Arg type '6'
			 * 	An array returned when querying orders */
			case 'B':{
				struct build_resources **adest;
				int len;
				int *idata;
				int *cdest;
				int i;
				
				idata = pdata;
				len = ntohl(*idata);
				idata++;

				cdest = va_arg(ap, int *);
				adest = va_arg(ap, struct build_resources **);

				if (cdest) *cdest = len;
	
				*adest = realloc(*adest, (len+1)*
						sizeof(struct build_resources));

				for (i = 0 ; i < len ; i ++){
					(*adest)[i].rid = ntohl(*idata ++);
					(*adest)[i].cost = ntohl(*idata ++);
				}

				pdata = (char *)idata;
				format ++;
				parsed ++;
				break;
			}
			/* Array of resources */
			case 'R':{
				struct planet_resource **adest;
				int len;
				int *idata;
				int *cdest;
				int i;
				
				idata = pdata;
				len = ntohl(*idata);
				idata++;

				cdest = va_arg(ap, int *);
				adest = va_arg(ap, struct planet_resource **);

				if (cdest) *cdest = len;
	
				*adest = realloc(*adest, (len+1)*
						sizeof(struct planet_resource));
				
				for (i = 0 ; i < len ; i ++){
					(*adest)[i].rid = ntohl(*idata ++);
					(*adest)[i].surface = ntohl(*idata ++);
					(*adest)[i].minable = ntohl(*idata ++);
					(*adest)[i].inaccessable = 
							ntohl(*idata ++);
				}

				pdata = (char *)idata;
				format ++;
				parsed ++;
				break;
			}

			/* Array of ships */
			case 'S':{
				struct fleet_ship **adest;
				int len;
				int *idata;
				int *cdest;
				int i;
				
				idata = pdata;
				len = ntohl(*idata);
				idata++;

				cdest = va_arg(ap, int *);
				adest = va_arg(ap, struct fleet_ship **);

				if (cdest) *cdest = len;
	
				*adest = realloc(*adest, (len+1)*
						sizeof(struct fleet_ship));
				
				for (i = 0 ; i < len ; i ++){
					(*adest)[i].design = ntohl(*idata ++);
					(*adest)[i].count = ntohl(*idata ++);
				}

				pdata = (char *)idata;
				format ++;
				parsed ++;
				break;
			}
			default: 
				printf("Unhandled code %c\n",*format);
				return parsed;
		}
		if (rv != 0){
			--format;
			printf("Error handling request for '%c' (%s)\n",
				*format,format);
			return -1;
		}

	}


	return parsed;
}

/**
 * Intenral function to parse a 'H' argument
 */
static int
parse_header(void **data, void **end, va_list *ap){
	int *idata;
	int *protodest,*seqdest,*typedest;
	void **enddest;
	int proto,seq,type,len;

	idata = *data;

	/* Not enough data for message */
	if (*end && (idata + 4) >= (int *)end){
		printf("Error: Buffer too short for header\n");
		return -1;
	}

	/* First pull everything out of the packet */
	proto = ntohl(*idata);
	idata ++;
	seq = ntohl(*idata); 
	idata ++;
	type = ntohl(*idata);
	idata ++;
	len = ntohl(*idata);
	idata ++;
	
	/* Now store it back if necessary */
	protodest = va_arg(*ap, int *);
	seqdest = va_arg(*ap, int *);
	typedest = va_arg(*ap, int *);
	enddest = va_arg(*ap, void **);
	if (protodest){
		/* FIXME: Handle tp03 and 4 correctly */
		*protodest = 3;
	}
	if (seqdest) *seqdest = seq;
	/* FIXME: Should convert to msgtype */
	if (typedest) *typedest = type;
	/* Ends of things */
	if (enddest) *enddest = ((char *)*data) + len + 16;
	if (end && *end == NULL) *end = ((char *)*data) + len + 16;

	*data = idata;

	return 0;
}

/* Parses a generic array.
 * Possible member types are:
 * 	i 	Int
 * 	l 	Long long (64 bit)
 * 	s	String (expexts two params:
 * 			length (uint);
 * 			pointer to store buffer
 */
void *
tpe_util_parse_array(void *bufv, void *end, size_t size, char *format, ...){
	char *p;
	char *buf;
	int i,len;
	va_list ap;
	char *dest;
	char *destbuf;
	ptrdiff_t off,off2;
	uint32_t slen;
	assert(bufv); assert(end); assert(format); 
	assert(size);

	assert(*format == '[');

	buf = bufv;
	len = *(int *)buf; // FIXME: NTHOL
	buf += 4;

	destbuf = calloc(len,size);
	dest = destbuf;

	for (i = 0 ; i < len ; i ++){
		dest = destbuf + i * size;
		va_start(ap,format);
		for (p = format + 1 ; *p != ']' ; p ++){
			off = va_arg(ap, ptrdiff_t);
			switch (*p){
			case 'i':
				memcpy(dest+off, buf, sizeof(uint32_t));
				buf += 4;
				break;
			case 'l':
				memcpy(dest+off, buf, sizeof(uint64_t));
				buf += 8;
				break;
			case 's':
				memcpy(dest+off, buf, sizeof(uint32_t));
				buf += 4;
				off2 = va_arg(ap,ptrdiff_t);
				slen = *(int*)(dest+off);
				if (slen){
					*((char**)(dest+off2)) = malloc(slen+1);
					memcpy(*(char **)(dest+off2),buf,slen);
					(*(char **)(dest+off2))[slen] = 0;
				} else {
					*((char**)(dest+off2)) = NULL;
				}
				buf += slen;
			}
		}
		va_end(ap);
	}

	return destbuf;
}




struct tut_i {
	int i;
};
struct tut_il {
	int i;
	uint64_t l;
};
struct tut_li {
	uint64_t l;
	int i;
};

struct tut_s {
	uint32_t len;
	char *buf;
};

struct tut_iisi {
	uint32_t i2,i3;
	uint32_t len;
	char *buf;
	uint32_t i4;
};

int 
tpe_util_test(void){
	int tutidatai[] = { 4, 27, 33, 33, 22 };
	int tutidataii[] = { 2, 27, 33, 33, 22 };
	int tutidataiiii[] = { 1, 27, 33, 33, 22 };
	int tutildata[] = { 2, 42, 0xa5a5a5a5, 0x5a5a5a5a,
				37, 0xb7b7b7b7, 0x7b7b7b7b };
	int tutsdata[] = { 3,   4, 0x6873614e,
				4, 0x61616161,
				5, 0x69766c45, 0x73};
	int tutiisidata[] = { 3,
				99, 112, 4, 0x44434241, 0xdeadbeef,
				1, 2, 4, 0x505421, 0xfeed,
				4, 5, 9, 0x44434241, 0x48474645,
					0xadbeef49, 0xde };
	struct tut_i *tut_i;
	struct tut_il *tut_il;
	struct tut_li *tut_li;
	struct tut_s *tut_s;
	struct tut_iisi *tut_iisi;
	ptrdiff_t loff,ioff,soff,ioff2,ioff3,ioff4;
	int i;

	tut_i = tpe_util_parse_array(tutidatai ,tutidatai + 5, 
				sizeof(struct tut_i), "[i]", 0);
	for (i = 0 ; i < 4 ; i ++){
		printf("%d -> %d [%d/%p] (%s)\n",tutidatai[i + 1],tut_i[i].i,
				((int *)(tut_i))[i],&((int *)(tut_i))[i],
				(tutidatai[i + 1] == tut_i[i].i) ?"Good": "Bad");
	}


	tut_i = tpe_util_parse_array(tutidataii,tutidataii + 5, 
				sizeof(int) * 2, "[ii]", 0,4);
	for (i = 0 ; i < 4 ; i ++){
		printf("%d -> %d [%d/%p] (%s)\n",tutidatai[i + 1],tut_i[i].i,
				((int *)(tut_i))[i],&((int *)(tut_i))[i],
				(tutidatai[i + 1] == tut_i[i].i) ?"Good": "Bad");
	}
	tut_i = tpe_util_parse_array(tutidataiiii,tutidataiiii + 5, 
				sizeof(int) * 4,"[iiii]",0,4,8,12);
	for (i = 0 ; i < 4 ; i ++){
		printf("%d -> %d [%d/%p] (%s)\n",tutidatai[i + 1],tut_i[i].i,
				((int *)(tut_i))[i],&((int *)(tut_i))[i],
				(tutidatai[i + 1] == tut_i[i].i) ?"Good": "Bad");
	}

	ioff = (char *)&tut_il->i - (char *)tut_il;
	loff = (char *)&tut_il->l - (char *)tut_il;
	tut_il = tpe_util_parse_array(tutildata, tutildata + sizeof(tutildata),
			sizeof(struct tut_il),"[il]", ioff,loff);
	for (i = 0 ; i < 2 ; i ++){
		printf("%d -> %d && %llx -> %llx\n",
				tutildata[i * 3 + 1],tut_il[i].i,
				*((uint64_t*)(tutildata + (i * 3 + 2))),
				tut_il[i].l);
		//printf("%d -> %d [%d/%p] (%s)\n",tutidatai[i + 1],tut_i[i].i,
		//		((int *)(tut_i))[i],&((int *)(tut_i))[i],
		//		(tutidatai[i + 1] == tut_i[i].i) ?"Good": "Bad");
	}

	ioff = (char *)&tut_li->i - (char *)tut_li;
	loff = (char *)&tut_li->l - (char *)tut_li;
	tut_li = tpe_util_parse_array(tutildata, tutildata + sizeof(tutildata),
			sizeof(struct tut_li),"[il]", ioff,loff);
	for (i = 0 ; i < 2 ; i ++){
		printf("%d -> %d && %llx -> %llx\n",
				tutildata[i * 3 + 1],tut_li[i].i,
				*((uint64_t*)(tutildata + (i * 3 + 2))),
				tut_li[i].l);
	}

	ioff = (char *)&tut_s->len - (char*)tut_s;
	soff = (char *)&tut_s->buf - (char*)tut_s;
	tut_s = tpe_util_parse_array(tutsdata, tutsdata + sizeof(tutsdata), 
			sizeof(struct tut_s),"[s]", ioff, soff);
	for (i = 0 ; i < 3 ; i ++){
		printf("[%d]%s -> [%d]%s\n",
				0,"",
				tut_s[i].len,tut_s[i].buf);
	}


	ioff = (char *)&tut_iisi->len - (char*)tut_iisi;
	soff = (char *)&tut_iisi->buf - (char*)tut_iisi;
	ioff2 = (char *)&tut_iisi->i2 - (char*)tut_iisi;
	ioff3 = (char *)&tut_iisi->i3 - (char*)tut_iisi;
	ioff4 = (char *)&tut_iisi->i4 - (char*)tut_iisi;
	tut_iisi = tpe_util_parse_array(tutiisidata, 
			tutiisidata + sizeof(tutiisidata), 
			sizeof(struct tut_iisi),
			"[iisi]", ioff2, ioff3, ioff, soff, ioff4); 
	for (i = 0 ; i < 3 ; i ++){
		printf("[%d]%s -> [%d]%s\n",
				0,"",
				tut_iisi[i].len,tut_iisi[i].buf);
	}

	/* Parse a small amount of data into a large structure */
	ioff = (char *)&tut_iisi->len - (char*)tut_iisi;
	soff = (char *)&tut_iisi->buf - (char*)tut_iisi;
	tut_iisi = tpe_util_parse_array(tutsdata, tutsdata + sizeof(tutsdata), 
			sizeof(struct tut_iisi), "[s]", ioff, soff);
	for (i = 0 ; i < 3 ; i ++){
		printf("[%d]%s -> [%d]%s\n",
				0,"",
				tut_iisi[i].len,tut_iisi[i].buf);
	}





	exit(0);
	return 0;
}

/**
 * 
 * The memory is allocted without a parent if necessary.  It is recommended
 * you save a pointer to the parent.
 *
 * @param buf Buffer to parse
 * @param Description of the structures
 * @param Size of dest structure
 * @param End pointer of buffer to parse
 */
void *
parse_block(const char *buf, struct parseitem *items, 
		void *data, /* Where to put it or NULL */
		const char *type,
		size_t size,
		char **end){
	int i, j, count;

	assert(buf); assert(items);
	assert(data || size);

	if (buf == NULL) return NULL;
	if (data == NULL || size < 1) return NULL;

	/* FIXME: Should check that size > largest offset in items */

	if (data == NULL)
		data = talloc_named_const(NULL,size,type);

	for (i = 0 ; items[i].type != PARSETYPE_END ; i ++){
		/* FIXME: Should calculate the dest here -
		 * 	if array, also need to malloc if necessary
		 */
		if (items[i].type & PARSETYPE_ARRAYOF)
			count = extract_int(&buf);
		else
			count = 1;
		for (j = 0 ; j < count ; j ++){
			void *addr = (char*)data + items[i].off;
			switch (items[i].type & ~PARSETYPE_ARRAYOF){
			case PARSETYPE_INT:
				*(int*)addr = extract_int(&buf);
				break;
			case PARSETYPE_LONG:
				*(int64_t*)addr = extract_long(&buf);
				break;
			case PARSETYPE_STRING:
				*(char**)addr = extract_string(&buf, NULL);
				if (*(char**)addr)
					talloc_steal(data, *(char**)addr);
				break;
			case PARSETYPE_STRUCT:
				*(int *)((char*)data + items[i].lenoff) = count;
				*(void**)addr = extract_struct(&buf,
						items + i,
						count);
				if (*(void**)addr)
					talloc_steal(data, *(void**)addr);
				count = 1;
				break;
			}
		}
	}

	if (end) *end = (char *)buf;

	return data;
}

/** 
 * Uses parse_item to recursively parse a structure
 */
static void *
extract_struct(const char **buf, struct parseitem *item, int count){
	void *data;
	int i;

	assert(*buf); assert(item); assert(count> 0);

	if (count == 0) return NULL;
	if (item == NULL) return NULL;
	if (*buf == NULL) return NULL;

	data = talloc_array_size(NULL, item->subsize, count);
	if (!data) return NULL;
	memset(data, 0, item->subsize * count);

	for (i = 0 ; i < count ; i ++){
		parse_block(*buf,
				item->sub,
				(char*)data + item->subsize * i,
				item->subtype,
				item->subsize, 
				(char**)buf);

	}

	return data;
}

static int 
extract_int(const char **src){
	int val;
	memcpy(&val,*src,sizeof(int32_t));
	*src += 4;
	return ntohl(val);
}

static int64_t
extract_long(const char **src){
	int64_t val;
	memcpy(&val,*src,sizeof(int64_t));
	*src += 8;
	return ntohll(val);
};

static char *
extract_string(const char **src, int *lenp){
	int len;
	char *buf;

	len = extract_int(src);
	if (lenp) *lenp = len;
	if (len == 0) return NULL;
	/* FIXME: Sanity check len */
	if (len >= 5000) {
		printf("Extract string: Len: %d/%x/%c%c%c%c\n",len,len,
				len >> 24,len >> 16, len >> 8, len);
		return NULL;
	}
	assert(len < 5000);
	/* FIXME: Souce */
	buf = talloc_strndup(NULL, *src, len);
	*src += len;	

	return buf;	
}




/*
 * Returns distance between two object squared.
 *
 * FIXME: Should check for overflow.
 */
uint64_t 
tpe_util_dist_calc2(struct object *obj1, struct object *obj2){
	uint64_t x,y,z;
	int64_t llabs(int64_t);
	int64_t llrintl(long double);

	x = llabs(obj1->pos.x - obj2->pos.x);
	y = llabs(obj1->pos.y - obj2->pos.y);
	z = llabs(obj1->pos.z - obj2->pos.z);

	return llrintl(hypotl(x,hypotl(y,z)));
}
