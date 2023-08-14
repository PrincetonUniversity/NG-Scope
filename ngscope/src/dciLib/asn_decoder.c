#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include "ngscope/hdr/dciLib/asn_decoder.h"
#ifdef ENABLE_ASN4G
#include <libasn4g.h>
#endif

#define MSG_QUEUE_POLL_TIME 10000

/*************************/
/* Structure definitions */
/*************************/
typedef struct _Node Node;
struct _Node
{
    Node * next;
    uint8_t * payload;
    int len;
    PayloadType type;
    uint32_t tti;
};

typedef struct _ASNDecoder
{
    FILE * file;
    Node * first;
    Node * last;
    pthread_mutex_t mux;
} ASNDecoder;
/* Global decoder used by the functions in this module */
ASNDecoder decoder;


/***************************/
/* Message queue functions */
/***************************/
void queue_push(Node * node)
{
    pthread_mutex_lock(&(decoder.mux));
    /* The queue is empty */
    if(decoder.last == NULL) {
        decoder.first = node;
        decoder.last = node;
    }
    else { 
        decoder.last->next = node;
        decoder.last = node;
    }
    pthread_mutex_unlock(&(decoder.mux));
    return;
}

Node * queue_pop()
{
    Node * node = NULL;
    pthread_mutex_lock(&(decoder.mux));
    /* The queue is emty */
    if(decoder.first == NULL)
        goto exit;
    /* Only one message in the queue */
    if(decoder.first == decoder.last) {
        node = decoder.first;
        decoder.first = NULL;
        decoder.last = NULL;
        goto exit;
    }
    /* Multiple messages in the queue */
    node = decoder.first;
    decoder.first = node->next;
exit:
    pthread_mutex_unlock(&(decoder.mux));
    return node;
}


/****************************************************/
/* Private functions used internally in this module */
/****************************************************/
void * asn_processor(void * args)
{
    Node * node;

    /* Loop that polls the message queue */
    while(1) {
        /* Get node from the queue */
        if((node = queue_pop()) == NULL) {
            usleep(MSG_QUEUE_POLL_TIME); /* Sleep 10 ms */
            continue; /* Continue if queue is empty */
        }
        fprintf(decoder.file, "\nTTI (%d):\n", node->tti);
        switch (node->type) {
            case MIB_4G:
#ifdef ENABLE_ASN4G
                mib_decode_4g(decoder.file, node->payload, node->len);
                /* Free Node and payload */
                free(node->payload);
                free(node);
#else
                fprintf(decoder.file, "4G MIB message cannot be decoded: libasn4g is not installed\n");
#endif
                break;
            case SIB_4G:
#ifdef ENABLE_ASN4G
                sib_decode_4g(decoder.file, node->payload, node->len);
                /* Free Node and payload */
                free(node->payload);
                free(node);
#else
                fprintf(decoder.file, "4G SIB message cannot be decoded: libasn4g is not installed\n");
#endif
                break;
            case MIB_5G:
#ifdef ENABLE_ASN5G
                mib_decode_5g(decoder.file, node->payload, node->len);
                /* Free Node and payload */
                free(node->payload);
                free(node);
#else
                fprintf(decoder.file, "5G MIB message cannot be decoded: libasn5g is not installed\n");
#endif
                break;
            case SIB_5G:
#ifdef ENABLE_ASN5G
                sib_decode_5g(decoder.file, node->payload, node->len);
                /* Free Node and payload */
                free(node->payload);
                free(node);
#else
                fprintf(decoder.file, "5G SIB message cannot be decoded: libasn5g is not installed\n");
#endif
                break;
            
            default:
                printf("Invalid ASN1 payload type (%d)\n", node->type);
                break;
        }
    }

    return NULL;
}


/********************/
/* Public functions */
/********************/

int init_asn_decoder(const char * path)
{
    pthread_t processor;

    if((decoder.file = fopen(path, "w")) == NULL) {
        decoder.file = NULL;
        printf("Error openning SIB log file %s (%d): %s\n", path, errno, strerror(errno));
        return 1;
    }

    if(pthread_create(&processor, NULL, &asn_processor, NULL) > 0) {
        printf("Error creating SIB processing thread\n");
        fclose(decoder.file);
        decoder.file = NULL;
        return 1;
    }
    return 0;
}

int push_asn_payload(uint8_t * payload, int len, PayloadType type, uint32_t tti)
{
    Node * node;

    /* If the decoder initialization has failed, this fucntion does nothing and returns error */
    if(decoder.file == NULL)
        return 1;

    /* Allocate memory for a new Node in the message queue */
    if((node = (Node *) malloc(sizeof(Node))) == NULL)
        return 1;
    /* Allocate memory for the message payload */
    if((node->payload = (uint8_t *) malloc(len)) == NULL) {
        free(node);
        return 1;
    }
    /* Fill message data */
    memcpy(node->payload, payload, len);
    node->len = len;
    node->tti = tti;
    node->type = type;
    node->next = NULL;
    /* Push message into the queue */
    queue_push(node);
    return 0;
}

void terminate_asn_decoder()
{   
    /* Lock the mutex to ensure the file is not closed durin a write */
    pthread_mutex_lock(&(decoder.mux));
    /* Close SIB log file*/
    if(decoder.file)
        fclose(decoder.file);
    pthread_mutex_unlock(&(decoder.mux));
    return;
}