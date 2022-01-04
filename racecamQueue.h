#ifndef RACECAMQUEUE_H_
#define RACECAMQUEUE_H_

#include <semaphore.h>

typedef struct
{
   void *next_packet;
   int64_t pts;
   int32_t flag;
   int  size;
   short int   type;
   char data;
} queue_frame_s;

typedef struct
{
   int  length;
   pthread_mutex_t mutex;
   pthread_cond_t cond;
   queue_frame_s *head;
   queue_frame_s *tail;
} QUEUE_STATE;

enum queue_data_type {
  VIDEO_DATA,
  AUDIO_DATA,
  END_OF_QUEUE = -1};
  
QUEUE_STATE *alloc_queue(void);
int queue_frame(QUEUE_STATE *queue, char *data, int size, int type, int64_t pts, int32_t flag);
int queue_end(QUEUE_STATE *queue);
queue_frame_s *get_tail(QUEUE_STATE *queue);
void free_frame(QUEUE_STATE *queue);
int free_queue(QUEUE_STATE *queue);
queue_frame_s *unqueue_frame(QUEUE_STATE *queue);
int queue_length(QUEUE_STATE *queue);
void empty_wait(QUEUE_STATE *queue);

#endif /* RACECAMQUEUE_H_ */
