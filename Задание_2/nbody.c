#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <pthread.h>
#include <string.h>
#include "timer.h"

/* Гравитационная постоянная, м^3 / (кг * с^2) */
const double G = 6.67430e-11;

int n;                /* число частиц */
int thread_count;     /* число потоков */
double t_end;         /* конечный момент времени */
double dt;            /* шаг по времени */
int nsteps;           /* число шагов интегрирования */

double *m;            /* массы, длина n */
double *pos;          /* координаты, длина 3*n (x,y,z на частицу) */
double *vel;          /* скорости, длина 3*n */
double *force;        /* суммарная сила на частицу, длина 3*n */
double **lforce;      /* локальные силы по потокам, thread_count * 3*n */

int *par_start, *par_end;  /* блок частиц каждого потока (сведение и обновление) */
int *frc_start, *frc_end;  /* блок "строк" q каждого потока (этап сил) */

FILE *outfp;          /* выходной CSV-файл */

/* Многоразовый барьер на мьютексе + условной переменной.
 * Использует счётчик «поколений» (generation), чтобы поток, проснувшийся
 * поздно, не «прилип» к следующему поколению барьера (иначе — дедлок). */
pthread_mutex_t bmutex;
pthread_cond_t  bcond;
int bcount;
int bthreads;
int bgeneration;

void barrier_wait(void) {
    pthread_mutex_lock(&bmutex);
    int gen = bgeneration;
    bcount++;
    if (bcount == bthreads) {
        bgeneration++;
        bcount = 0;
        pthread_cond_broadcast(&bcond);
    } else {
        while (gen == bgeneration)
            pthread_cond_wait(&bcond, &bmutex);
    }
    pthread_mutex_unlock(&bmutex);
}

void Usage(char *prog) {
    fprintf(stderr,
        "usage: %s <nthreads> <t_end> <dt> <input_file> [output_file]\n", prog);
    exit(1);
}

/* Число пар, покрытых строками q = 0..q-1 (для балансировки этапа сил) */
static long cum_pairs(int q) {
    return (long) q * (n - 1) - (long) q * (q - 1) / 2;
}

/* Наименьшее q, при котором cum_pairs(q) >= target */
static int find_q(long target) {
    int lo = 0, hi = n;
    while (lo < hi) {
        int mid = (lo + hi) / 2;
        if (cum_pairs(mid) >= target) hi = mid;
        else lo = mid + 1;
    }
    return lo;
}

/* Чтение входного файла:
 *   первая строка: n
 *   далее n строк: масса x y z vx vy vz */
int read_input(char *fname) {
    FILE *f = fopen(fname, "r");
    if (!f) { perror("input file"); return 0; }
    if (fscanf(f, "%d", &n) != 1) { fclose(f); return 0; }

    m     = malloc(n      * sizeof(double));
    pos   = malloc(3 * n  * sizeof(double));
    vel   = malloc(3 * n  * sizeof(double));
    force = malloc(3 * n  * sizeof(double));

    for (int q = 0; q < n; q++) {
        if (fscanf(f, "%lf %lf %lf %lf %lf %lf %lf",
                   &m[q], &pos[3*q], &pos[3*q+1], &pos[3*q+2],
                   &vel[3*q], &vel[3*q+1], &vel[3*q+2]) != 7) {
            fclose(f);
            return 0;
        }
    }
    fclose(f);
    return 1;
}

/* Вывод одной строки CSV: t x1 y1 z1 x2 y2 z2 ... xn yn zn */
void write_row(double time) {
    fprintf(outfp, "%.10g", time);
    for (int q = 0; q < n; q++)
        fprintf(outfp, " %.10g %.10g %.10g", pos[3*q], pos[3*q+1], pos[3*q+2]);
    fprintf(outfp, "\n");
}

/* Диагностика: суммарная энергия и суммарный импульс */
void diagnostics(const char *label) {
    double KE = 0.0, PE = 0.0;
    double P[3] = {0.0, 0.0, 0.0};
    for (int q = 0; q < n; q++) {
        double v2 = vel[3*q]*vel[3*q] + vel[3*q+1]*vel[3*q+1]
                  + vel[3*q+2]*vel[3*q+2];
        KE += 0.5 * m[q] * v2;
        P[0] += m[q] * vel[3*q];
        P[1] += m[q] * vel[3*q+1];
        P[2] += m[q] * vel[3*q+2];
        for (int k = q + 1; k < n; k++) {
            double dx = pos[3*k]   - pos[3*q];
            double dy = pos[3*k+1] - pos[3*q+1];
            double dz = pos[3*k+2] - pos[3*q+2];
            PE -= G * m[q] * m[k] / sqrt(dx*dx + dy*dy + dz*dz);
        }
    }
    printf("%s: total energy E = %.6e   momentum P = (%.3e, %.3e, %.3e)\n",
           label, KE + PE, P[0], P[1], P[2]);
}

void *thread_work(void *arg) {
    long t = (long) arg;
    double *lf = lforce[t];
    int q, k, dim, u;

    for (int step = 1; step <= nsteps; step++) {
        /* 1) обнуляем локальные силы данного потока */
        memset(lf, 0, 3 * n * sizeof(double));

        /* 2) этап сил: пары (q,k), q < k, q — из блока данного потока.
         *    Сила между q и k вычисляется один раз и применяется к обоим
         *    телам (закон Ньютона 3-го: F_qk = -F_kq). Это вдвое сокращает
         *    число вычислений. */
        for (q = frc_start[t]; q < frc_end[t]; q++) {
            double mq   = m[q];
            double px   = pos[3*q];
            double py   = pos[3*q+1];
            double pz   = pos[3*q+2];
            for (k = q + 1; k < n; k++) {
                double dx = pos[3*k]   - px;
                double dy = pos[3*k+1] - py;
                double dz = pos[3*k+2] - pz;
                double d2 = dx*dx + dy*dy + dz*dz;
                double inv3 = 1.0 / (d2 * sqrt(d2));   /* 1/r^3 */
                double f = G * mq * m[k] * inv3;
                double fx = f * dx, fy = f * dy, fz = f * dz;
                lf[3*q]   += fx;  lf[3*q+1]   += fy;  lf[3*q+2]   += fz;
                lf[3*k]   -= fx;  lf[3*k+1]   -= fy;  lf[3*k+2]   -= fz;
            }
        }

        barrier_wait();   /* все локальные силы готовы */

        /* 3) сведение локальных сил и интегрирование (метод Эйлера) */
        for (q = par_start[t]; q < par_end[t]; q++) {
            double mq = m[q];
            for (dim = 0; dim < 3; dim++) {
                double F = 0.0;
                for (u = 0; u < thread_count; u++)
                    F += lforce[u][3*q + dim];
                force[3*q + dim] = F;
                double old_v = vel[3*q + dim];
                pos[3*q + dim]  += old_v * dt;            /* x^n = x^(n-1) + v^(n-1)*dt */
                vel[3*q + dim]  = old_v + (F / mq) * dt;  /* v^n = v^(n-1) + (F/m)*dt */
            }
        }

        barrier_wait();   /* позиции обновлены */

        /* 4) вывод строки выполняет нулевой поток */
        if (t == 0) write_row(step * dt);
    }
    return NULL;
}

int main(int argc, char **argv) {
    if (argc < 5 || argc > 6) Usage(argv[0]);
    thread_count = atoi(argv[1]);
    t_end = atof(argv[2]);
    dt    = atof(argv[3]);
    char *infile  = argv[4];
    char *outfile = (argc == 6) ? argv[5] : "trajectories.csv";

    if (thread_count < 1 || dt <= 0.0 || t_end <= 0.0) Usage(argv[0]);
    if (!read_input(infile)) return 1;

    nsteps = (int) ceil(t_end / dt);
    if (nsteps < 1) nsteps = 1;

    /* блоки частиц для сведения/обновления */
    par_start = malloc(thread_count * sizeof(int));
    par_end   = malloc(thread_count * sizeof(int));
    for (int t = 0; t < thread_count; t++) {
        par_start[t] = (int)((long) t * n / thread_count);
        par_end[t]   = (int)((long)(t + 1) * n / thread_count);
    }

    /* блоки строк q для этапа сил (равное число пар на поток) */
    frc_start = malloc(thread_count * sizeof(int));
    frc_end   = malloc(thread_count * sizeof(int));
    long total_pairs = (long) n * (n - 1) / 2;
    for (int t = 0; t < thread_count; t++) {
        frc_start[t] = find_q(total_pairs * t / thread_count);
        frc_end[t]   = find_q(total_pairs * (t + 1) / thread_count);
    }

    lforce = malloc(thread_count * sizeof(double *));
    for (int t = 0; t < thread_count; t++)
        lforce[t] = calloc(3 * n, sizeof(double));

    outfp = fopen(outfile, "w");
    if (!outfp) { perror("output file"); return 1; }

    write_row(0.0);          /* начальное состояние (t = 0) */

    printf("n=%d threads=%d steps=%d dt=%.6g t_end=%.6g out=%s\n",
           n, thread_count, nsteps, dt, t_end, outfile);
    diagnostics("initial");

    pthread_mutex_init(&bmutex, NULL);
    pthread_cond_init(&bcond, NULL);
    bthreads = thread_count;
    bcount   = 0;
    bgeneration = 0;

    pthread_t *th = malloc(thread_count * sizeof(pthread_t));
    double start, finish;
    GET_TIME(start);
    for (long t = 0; t < thread_count; t++)
        pthread_create(&th[t], NULL, thread_work, (void *) t);
    for (int t = 0; t < thread_count; t++)
        pthread_join(th[t], NULL);
    GET_TIME(finish);

    diagnostics("final");
    printf("Elapsed time = %e s\n", finish - start);

    fclose(outfp);
    pthread_cond_destroy(&bcond);
    pthread_mutex_destroy(&bmutex);
    for (int t = 0; t < thread_count; t++) free(lforce[t]);
    free(lforce); free(frc_start); free(frc_end);
    free(par_start); free(par_end); free(th);
    free(force); free(vel); free(pos); free(m);
    return 0;
}