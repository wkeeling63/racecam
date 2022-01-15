#include <stdio.h>


#include "interface/mmal/mmal.h"

#include "racecamQueue.h"
#include "racecamLogger.h" 

QUEUE_STATE *alloc_queue(void)
{
   log_debug("%s in file: %s(%d)", __func__,  __FILE__, __LINE__);   
   QUEUE_STATE *queue=NULL;
   queue = malloc(sizeof(QUEUE_STATE));

   if (queue)
      {
      queue->length=0;
      queue->head = NULL;
      queue->tail = NULL;

       if (pthread_mutex_init(&queue->mutex, NULL))
         {
         log_error("Failed to initilized mutex");
         return NULL;
         }
      if (pthread_cond_init(&queue->cond, NULL))
         {
         log_error("Failed to initilized wait condition");
         return NULL;
         }
      return queue;
      }
   else
      {
      log_error("faild to allocate queue!");
      return NULL;
      }
}

int queue_frame(QUEUE_STATE *queue, char *data, int size, int type, int64_t pts, int32_t flag)
{
   log_debug("%s in file: %s(%d)", __func__,  __FILE__, __LINE__);
   queue_frame_s *frame = malloc(size+sizeof(queue_frame_s));
   if (!frame)
      {
      log_error("Unable to allocate frame");
      return -1;
      }
   memcpy(&frame->data, data, size);
   frame->flag = flag;
   frame->size = size;
   frame->type = type;
   frame->pts = pts;
   frame->next_packet = NULL;
//   log_debug("frame inited");

   pthread_mutex_lock(&queue->mutex);
      
   if (queue->head)
      {
//      log_debug("frame has head");
      queue_frame_s *head_frame = queue->head;
      head_frame->next_packet = frame;
      queue->head = frame;
      }
   else
      {
//      log_debug("frame has no head");
      queue->head = frame;
      queue->tail = frame;
      }
//   log_debug("frame head of queue");
   queue->length++;
   pthread_cond_signal(&queue->cond);
   pthread_mutex_unlock(&queue->mutex);

//   log_debug("%s Done %s in file:(%d)", __func__,  __FILE__, __LINE__);
   return 0;
}

int queue_end(QUEUE_STATE *queue)
{
   log_debug("%s in file: %s(%d)", __func__,  __FILE__, __LINE__);
   queue_frame_s *frame = malloc(sizeof(queue_frame_s));
   if (!frame)
      {
      log_error("Unable to allocate frame");
      return -1;
      }
   frame->data = 0;
   frame->flag = 0;
   frame->size = 0;
   frame->type = END_OF_QUEUE;
   frame->pts = 0;
   frame->next_packet = NULL;

   pthread_mutex_lock(&queue->mutex);
   
   if (queue->head)
      {
      queue_frame_s *head_frame = queue->head;
      head_frame->next_packet = frame;
      queue->head = frame;
      }
   else
      {
      queue->head = frame;
      queue->tail = frame;
      }

   queue->length++;
   pthread_cond_signal(&queue->cond);
   pthread_mutex_unlock(&queue->mutex);
   
   return 0;
}

queue_frame_s *get_tail(QUEUE_STATE *queue)
{
   log_debug("%s in file: %s(%d)", __func__,  __FILE__, __LINE__);

   pthread_mutex_lock(&queue->mutex);
   queue_frame_s *tail = queue->tail;

   pthread_mutex_unlock(&queue->mutex);
   return tail; 
}

void free_frame(QUEUE_STATE *queue)
{
   log_debug("%s in file: %s(%d)", __func__,  __FILE__, __LINE__);
   int status=0;

   pthread_mutex_lock(&queue->mutex);
   if (queue->tail == NULL) goto done;
   queue_frame_s *saved_ptr = queue->tail;
   queue->tail = saved_ptr->next_packet;
   if (queue->head == saved_ptr) 
      {
      queue->head=NULL;
      }
   if (saved_ptr)
      {
      free(saved_ptr);
      saved_ptr = NULL;
      }
   else
      {
      log_error("non NULL");
      }
   queue->length--;

done:  
   pthread_mutex_unlock(&queue->mutex);
}
queue_frame_s *unqueue_frame(QUEUE_STATE *queue)
{
  log_debug("%s in file: %s(%d)", __func__,  __FILE__, __LINE__);
   int status=0;

   pthread_mutex_lock(&queue->mutex);
   if (queue->tail == NULL) goto done;
   queue_frame_s *saved_ptr = queue->tail;
   queue->tail = saved_ptr->next_packet;
   if (queue->head == saved_ptr) 
      {
      queue->head=NULL;
      }
   queue->length--;

   pthread_mutex_unlock(&queue->mutex);
   return saved_ptr;
done:  
   pthread_mutex_unlock(&queue->mutex);
   return NULL;
}


int free_queue(QUEUE_STATE *queue)
{
   log_debug("%s in file: %s(%d)", __func__,  __FILE__, __LINE__);
   if (queue->length != 0) 
      {
      log_warning("Queue not empty when freed!");
      queue_frame_s *frame = queue->tail;
      while (frame) 
         {
         queue_frame_s *saved_frame = frame;
         frame = frame->next_packet;
         if (saved_frame)
            {
            free(saved_frame);
            saved_frame = NULL;
            }
         else
            {
            log_error("non null free");
            return -1;
            }
         }
      }

   if (pthread_mutex_destroy(&queue->mutex)) log_error("failed to destory mutex ");
   if (pthread_cond_destroy(&queue->cond)) log_error("failed to destory wait condition");
   free(queue);
   return 0;
   
}

int queue_length(QUEUE_STATE *queue)
{
  log_debug("%s in file: %s(%d)", __func__,  __FILE__, __LINE__);

   pthread_mutex_lock(&queue->mutex);
   int length=queue->length;

   pthread_mutex_unlock(&queue->mutex);
   return length; 
}

void empty_wait(QUEUE_STATE *queue)
{
   log_debug("%s in file: %s(%d)", __func__,  __FILE__, __LINE__);

   pthread_mutex_lock(&queue->mutex);
   while (!queue->length)
      {
      pthread_cond_wait(&queue->cond, &queue->mutex);
      }

   pthread_mutex_unlock(&queue->mutex);
}
