#include <pthread.h>
#include <semaphore.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <limits.h>
#include <stdbool.h>


#define handle_error_en(en, msg)				\
  do { errno = en; perror(msg); exit(EXIT_FAILURE); } while (0)

#define handle_error(msg)				\
  do { perror(msg); exit(EXIT_FAILURE); } while (0)

long sum = 0;
long odd = 0;
long min = INT_MAX;
long max = INT_MIN;
struct Queue* queue;
pthread_mutex_t mutexSum, mutexMin, mutexMax, mutexOdd, mutexList;
pthread_cond_t threadCond;
bool done = false;

struct thread_info {
  pthread_t thread_id;
  int       thread_num;
};

struct QNode {
  int time;
  struct QNode* next;
};
 
struct Queue {
  struct QNode *front, *rear;
};
 
struct QNode* newNode(int time)
{
  struct QNode* itemList = (struct QNode*)malloc(sizeof(struct QNode));
  itemList->time = time;
  itemList->next = NULL;
  return itemList;
}
 
void createQueue()
{
  queue = (struct Queue*)malloc(sizeof(struct Queue));
  queue->front = queue->rear = NULL;
}
 
void enQueue(struct QNode* itemList)
{
  if (queue->front == NULL) {
    pthread_mutex_lock(&mutexList);
    queue->front = queue->rear = itemList;
    pthread_mutex_unlock(&mutexList);
    return;
  }
  pthread_mutex_lock(&mutexList);
  queue->rear->next = itemList;
  queue->rear = itemList;
  pthread_mutex_unlock(&mutexList);
}
 
struct QNode* getItemAndRemoveItemFromQueue()
{
  if(queue->front == NULL) return NULL;
  struct QNode* item = queue->front;
  queue->front = item->next;
  if (queue->front == NULL)
    queue->rear = NULL;
  return item;
}

void update(long time)
{
  sleep(time);

  pthread_mutex_lock(&mutexSum);
  sum += time;
  pthread_mutex_unlock(&mutexSum);

  if (time % 2 == 1) {
    pthread_mutex_lock(&mutexOdd);
    odd++;
    pthread_mutex_unlock(&mutexOdd);
  }

  if (time < min) {
    pthread_mutex_lock(&mutexMin);
    min = time;
    pthread_mutex_unlock(&mutexMin);
  }
  
  if (time > max) {
    pthread_mutex_lock(&mutexMax);
    max = time;
    pthread_mutex_unlock(&mutexMax);
  }

}

static void *
thread_start(void *arg)
{
  struct thread_info *tinfo = arg;
  while (!done || queue->front != NULL)
  {
    struct QNode* item;
    pthread_mutex_lock(&mutexList);
    while(queue->front == NULL && !done) {
      pthread_cond_wait(&threadCond, &mutexList);
    }
    item = getItemAndRemoveItemFromQueue();
    pthread_mutex_unlock(&mutexList);
    if(item != NULL) {
      printf("THREAD NUMERO: %i CONSUMINDO TAREFA DE TEMPO %i\n", tinfo->thread_num, item->time);
      update(item->time);
      free(item);
    }
  }
  printf("THREAD NUMERO: %i FINALIZADA\n", tinfo->thread_num);
  return 0;
}


int main(int argc, char *argv[])
{   
  int s, tnum, opt, quantityThreads;
  char * filename;
  struct thread_info *workers;
  pthread_attr_t attr;
  void *res;

  while ((opt = getopt(argc, argv, "f:t:")) != -1) {
    switch (opt) {
    case 'f':
      filename = optarg;
      break;
    case 't':
      quantityThreads = strtoul(optarg, NULL, 0);
      break;
    }
  }
  
  createQueue();

  s = pthread_attr_init(&attr);
  if (s != 0)
    handle_error_en(s, "pthread_attr_init");

  pthread_mutex_init(&mutexMax, NULL);
  pthread_mutex_init(&mutexList, NULL);
  pthread_mutex_init(&mutexMin, NULL);
  pthread_mutex_init(&mutexOdd, NULL);
  pthread_mutex_init(&mutexSum, NULL);
  pthread_cond_init(&threadCond, NULL);

  workers = calloc(quantityThreads, sizeof(struct thread_info));
  if (workers == NULL)
    handle_error("calloc");

  for (tnum = 0; tnum < quantityThreads; tnum++) {
    workers[tnum].thread_num = tnum + 1;
    s = pthread_create(&workers[tnum].thread_id, &attr, &thread_start, &workers[tnum]);
    if (s != 0)
      handle_error_en(s, "pthread_create");
  }
  
  FILE* program = fopen(filename, "r");
  char action;
  long num;

  while (fscanf(program, "%c %ld\n", &action, &num) == 2) {
    if (action == 'p') {
      struct QNode* itemList = newNode(num);
      enQueue(itemList);
      pthread_cond_signal(&threadCond);
    } else if (action == 'e') {
      sleep(num);
    } else {
      printf("ERROR: Unrecognized action: '%c'\n", action);
      exit(EXIT_FAILURE);
    }
  }
  done = true;
  pthread_cond_broadcast(&threadCond);
  fclose(program);

  for (tnum = 0; tnum < quantityThreads; tnum++) {
    s = pthread_join(workers[tnum].thread_id, &res);
    if (s != 0)
      handle_error_en(s, "pthread_join");

    free(res);
  }
  
  printf("%ld %ld %ld %ld\n", sum, odd, min, max);

  pthread_mutex_destroy(&mutexMax);
  pthread_mutex_destroy(&mutexMin);
  pthread_mutex_destroy(&mutexList);
  pthread_mutex_destroy(&mutexOdd);
  pthread_mutex_destroy(&mutexSum);
  pthread_cond_destroy(&threadCond);

  return (EXIT_SUCCESS);
}