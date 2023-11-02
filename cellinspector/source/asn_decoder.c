#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include "cellinspector/headers/asn_decoder.h"
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

struct _ASNDecoder
{
    char * log_path;
    char * name;
    FILE * file;
    Node * first;
    Node * last;
    pthread_mutex_t queue_mux;
    pthread_mutex_t file_mux;
};


/***************************/
/* Message queue functions */
/***************************/
void queue_push(ASNDecoder * decoder, Node * node)
{
    pthread_mutex_lock(&(decoder->queue_mux));
    /* The queue is empty */
    if(decoder->last == NULL) {
        decoder->first = node;
        decoder->last = node;
    }
    else { 
        decoder->last->next = node;
        decoder->last = node;
    }
    pthread_mutex_unlock(&(decoder->queue_mux));
    return;
}

Node * queue_pop(ASNDecoder * decoder)
{
    Node * node = NULL;
    pthread_mutex_lock(&(decoder->queue_mux));
    /* The queue is emty */
    if(decoder->first == NULL)
        goto exit;
    /* Only one message in the queue */
    if(decoder->first == decoder->last) {
        node = decoder->first;
        decoder->first = NULL;
        decoder->last = NULL;
        goto exit;
    }
    /* Multiple messages in the queue */
    node = decoder->first;
    decoder->first = node->next;
exit:
    pthread_mutex_unlock(&(decoder->queue_mux));
    return node;
}

int open_sib_log(ASNDecoder * decoder)
{
    char path[1024];

    /* Assemble new path */
    bzero(path, 1024);
    sprintf(path, "%s/%s_sibs.dump", decoder->log_path, decoder->name);

    printf("Openning %s...\n", path);

    if((decoder->file = fopen(path, "w")) == NULL) {
        decoder->file = NULL;
        printf("Error openning SIB log file %s (%d): %s\n", path, errno, strerror(errno));
        return 1;
    }

    return 0;
}


/****************************************************/
/* Private functions used internally in this module */
/****************************************************/
void * asn_processor(void * args)
{
    Node * node;
    ASNDecoder * decoder;

    decoder = (ASNDecoder *) args;

    if(open_sib_log(decoder)) {
        return NULL;
    }

    /* Loop that polls the message queue */
    while(1) {
        /* Get node from the queue */
        if((node = queue_pop(decoder)) == NULL) {
            usleep(MSG_QUEUE_POLL_TIME); /* Sleep 10 ms */
            continue; /* Continue if queue is empty */
        }

        pthread_mutex_lock(&(decoder->file_mux));

        if(decoder->file == NULL)
            return NULL;

        fprintf(decoder->file, "\nTTI (%d):\n", node->tti);
        switch (node->type) {
            case MIB_4G:
#ifdef ENABLE_ASN4G
                mib_decode_4g(decoder->file, node->payload, node->len);
                /* Free Node and payload */
                free(node->payload);
                free(node);
#else
                fprintf(decoder->file, "4G MIB message cannot be decoded: libasn4g is not installed\n");
#endif
                break;
            case SIB_4G:
#ifdef ENABLE_ASN4G
                sib_decode_4g(decoder->file, node->payload, node->len);
                /* Free Node and payload */
                free(node->payload);
                free(node);
#else
                fprintf(decoder->file, "4G SIB message cannot be decoded: libasn4g is not installed\n");
#endif
                break;
            default:
                printf("Invalid ASN1 payload type (%d)\n", node->type);
                break;
        }
        fflush(decoder->file);
        pthread_mutex_unlock(&(decoder->file_mux));
    }

    return NULL;
}


/********************/
/* Public functions */
/********************/

ASNDecoder * init_asn_decoder(char * path, char * name)
{
    pthread_t processor;
    ASNDecoder * decoder;
    char cmd[1024];

    decoder = (ASNDecoder *) calloc(1, sizeof(ASNDecoder));
    if(decoder == NULL)
        return NULL;

    decoder->file = NULL;
    decoder->first = NULL;
    decoder->last = NULL;
    decoder->name = name;
    decoder->log_path = path;
    pthread_mutex_init(&(decoder->queue_mux), NULL);
    pthread_mutex_init(&(decoder->file_mux), NULL);

    /* Create folders */
    sprintf(cmd, "mkdir -p %s/", decoder->log_path);
    system(cmd);

    if(pthread_create(&processor, NULL, &asn_processor, (void *) decoder) > 0) {
        printf("Error creating SIB processing thread\n");
        return NULL;
    }

    return decoder;
}

void terminate_asn_decoder(ASNDecoder * decoder)
{
    pthread_mutex_lock(&(decoder->file_mux));
    fclose(decoder->file);
    decoder->file = NULL;
    pthread_mutex_unlock(&(decoder->file_mux));
}

int push_asn_payload(ASNDecoder * decoder, uint8_t * payload, int len, PayloadType type, uint32_t tti)
{
    Node * node;

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
    queue_push(decoder, node);
    return 0;
}
