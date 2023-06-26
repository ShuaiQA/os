#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>

#define NBUCKET 5
#define NKEYS 100000

struct entry {
  int key;
  int value;
  struct entry *next;
};

// 一共有5个entry指针数组
struct entry *table[NBUCKET];
pthread_mutex_t lock[NBUCKET]; // 每一个桶的添加需要上锁

void initlock() {
  for (int i = 0; i < NBUCKET; i++) {
    pthread_mutex_init(&lock[i], NULL);
  }
}

int keys[NKEYS];
int nthread = 1;

double now() {
  struct timeval tv;
  gettimeofday(&tv, 0);
  return tv.tv_sec + tv.tv_usec / 1000000.0;
}

// 将相关的线程插入到桶的队头
static void insert(int key, int value, struct entry **p, struct entry *n) {
  struct entry *e = malloc(sizeof(struct entry));
  e->key = key;
  e->value = value;
  e->next = n;
  *p = e;
}

static void put(int key, int value) {
  int i = key % NBUCKET; // 查看当前的key应该放到哪一个桶中

  // is the key already present?
  struct entry *e = 0;
  // 查看当前的key是否存在,如果存在修改是由哪一个线程产生的value
  for (e = table[i]; e != 0; e = e->next) {
    if (e->key == key)
      break;
  }
  if (e) { // 当前的桶里的key是别的线程放入的,注意此时可能会导致覆盖操作
    // update the existing key.
    e->value = value;
  } else {
    // the new is new.
    pthread_mutex_lock(&lock[i]);
    insert(key, value, &table[i], table[i]);
    pthread_mutex_unlock(&lock[i]);
  }
}

static struct entry *get(int key) {
  int i = key % NBUCKET;

  struct entry *e = 0;
  for (e = table[i]; e != 0; e = e->next) {
    if (e->key == key)
      break;
  }

  return e;
}

static void *put_thread(void *xa) {
  int n = (int)(long)xa;   // thread number
  int b = NKEYS / nthread; // 每一个线程的平均put

  for (int i = 0; i < b; i++) {
    // 每一个线程负责桶中一部分区间的数据(但是桶中的数据可能是一致的)
    put(keys[b * n + i], n);
  }

  return NULL;
}

static void *get_thread(void *xa) {
  int n = (int)(long)xa; // thread number
  int missing = 0;

  for (int i = 0; i < NKEYS; i++) {
    struct entry *e = get(keys[i]);
    if (e == 0)
      missing++;
  }
  printf("%d: %d keys missing\n", n, missing);
  return NULL;
}

int main(int argc, char *argv[]) {
  pthread_t *tha; // 指向一组线程数组
  void *value;
  double t1, t0;

  if (argc < 2) {
    fprintf(stderr, "Usage: %s nthreads\n", argv[0]);
    exit(-1);
  }
  initlock();
  nthread = atoi(argv[1]);
  tha = malloc(sizeof(pthread_t) * nthread);
  srandom(0);
  assert(NKEYS % nthread == 0);
  for (int i = 0; i < NKEYS; i++) { // 生产100000个随机数放到keys数组中
    keys[i] = random();
  }

  // first the puts
  t0 = now();
  for (int i = 0; i < nthread; i++) {
    assert(pthread_create(&tha[i], NULL, put_thread, (void *)(long)i) == 0);
  }
  for (int i = 0; i < nthread; i++) {
    assert(pthread_join(tha[i], &value) == 0);
  }
  t1 = now();

  printf("%d puts, %.3f seconds, %.0f puts/second\n", NKEYS, t1 - t0,
         NKEYS / (t1 - t0));

  // now the gets
  t0 = now();
  for (int i = 0; i < nthread; i++) {
    assert(pthread_create(&tha[i], NULL, get_thread, (void *)(long)i) == 0);
  }
  for (int i = 0; i < nthread; i++) {
    assert(pthread_join(tha[i], &value) == 0);
  }
  t1 = now();

  printf("%d gets, %.3f seconds, %.0f gets/second\n", NKEYS * nthread, t1 - t0,
         (NKEYS * nthread) / (t1 - t0));
  return 0;
}
