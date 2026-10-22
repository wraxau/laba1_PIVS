/* параллельный поиск точек множества Мандельброта (pthreads)
 * запуск: ./mandelbrot <thread_count> <n_points>
 * результат: mandelbrot.csv  */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include "timer.h"

/* прямоугольник, в котором целиком лежит множество Мандельброта */
#define RE_MIN (-2.0)
#define RE_MAX 0.5
#define IM_MIN (-1.25)
#define IM_MAX 1.25

/* если за MAX_ITER итераций |z| < 2, точка считается принадлежащей множеству */
#define MAX_ITER 1000

#define OUTPUT_FILE "mandelbrot.csv"

typedef struct {
   double re;
   double im;
} point_t;

/* общие для всех потоков данные, tested[r] — сколько точек проверил поток r */
int      thread_count;
long     n_points;
point_t* points;
long*    tested;

void     Usage(char* prog_name);
double   Rand_unit(uint64_t* state_p);
int      In_mandelbrot(double cr, double ci);
void*    Thread_work(void* rank);
void     Write_csv(void);

int main(int argc, char* argv[]) {
   long thread;
   pthread_t* thread_handles;
   double start, finish;

   if (argc != 3) Usage(argv[0]);
   thread_count = strtol(argv[1], NULL, 10);
   n_points     = strtol(argv[2], NULL, 10);
   if (thread_count < 1 || n_points < 0) Usage(argv[0]);

   thread_handles = malloc(thread_count*sizeof(pthread_t));
   points         = malloc(n_points*sizeof(point_t));
   tested         = malloc(thread_count*sizeof(long));
   if (thread_handles == NULL || points == NULL || tested == NULL) {
      fprintf(stderr, "Не удалось выделить память\n");
      return 1;
   }

   GET_TIME(start);
   for (thread = 0; thread < thread_count; thread++) {
      int err = pthread_create(&thread_handles[thread], NULL,
            Thread_work, (void*) thread);
      if (err != 0) {
         fprintf(stderr, "pthread_create: ошибка %d\n", err);
         return 1;
      }
   }
   for (thread = 0; thread < thread_count; thread++)
      pthread_join(thread_handles[thread], NULL);
   GET_TIME(finish);

   long total_tested = 0;
   for (thread = 0; thread < thread_count; thread++)
      total_tested += tested[thread];

   Write_csv();

   printf("потоков = %d, точек = %ld, проверено = %ld\n",
         thread_count, n_points, total_tested);
   printf("время работы = %e с\n", finish - start);

   free(thread_handles);
   free(points);
   free(tested);
   return 0;
}

void Usage(char* prog_name) {
   fprintf(stderr, "запуск: %s <число_потоков> <число_точек>\n", prog_name);
   exit(1);
}

double Rand_unit(uint64_t* state_p) {
   uint64_t x = *state_p;
   x ^= x >> 12;
   x ^= x << 25;
   x ^= x >> 27;
   *state_p = x;
   return ((x * 0x2545F4914F6CDD1DULL) >> 11) * (1.0 / 9007199254740992.0);
}

int In_mandelbrot(double cr, double ci) {
   double zr = 0.0, zi = 0.0;
   for (int n = 0; n < MAX_ITER; n++) {
      double zr2 = zr*zr;
      double zi2 = zi*zi;
      if (zr2 + zi2 >= 4.0)
         return 0;
      zi = 2.0*zr*zi + ci;
      zr = zr2 - zi2 + cr;
   }
   return 1;
}

void* Thread_work(void* rank) {
   long my_rank = (long) rank;
   long quotient  = n_points / thread_count;
   long remainder = n_points % thread_count;
   long my_count, my_first_i, my_last_i;
   if (my_rank < remainder) {
      my_count   = quotient + 1;
      my_first_i = my_rank*my_count;
   } else {
      my_count   = quotient;
      my_first_i = my_rank*my_count + remainder;
   }
   my_last_i = my_first_i + my_count - 1;

   uint64_t state = 0x9E3779B97F4A7C15ULL*(uint64_t)(my_rank + 1);
   long my_tested = 0;
   long i = my_first_i;

   while (i <= my_last_i) {
      double cr = RE_MIN + (RE_MAX - RE_MIN)*Rand_unit(&state);
      double ci = IM_MIN + (IM_MAX - IM_MIN)*Rand_unit(&state);
      my_tested++;
      if (In_mandelbrot(cr, ci)) {
         points[i].re = cr;
         points[i].im = ci;
         i++;
      }
   }

   tested[my_rank] = my_tested;
   return NULL;
}

/* запись результата */
void Write_csv(void) {
   FILE* f = fopen(OUTPUT_FILE, "w");
   if (f == NULL) {
      perror(OUTPUT_FILE);
      exit(1);
   }
   fprintf(f, "re,im\n");
   for (long i = 0; i < n_points; i++)
      fprintf(f, "%.17g,%.17g\n", points[i].re, points[i].im);
   fclose(f);
}
