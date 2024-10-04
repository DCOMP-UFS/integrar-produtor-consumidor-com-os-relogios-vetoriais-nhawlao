// mpicc -o integracao integracao.c -lpthread
// mpirun -n 3 ./integracao 

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <mpi.h>

#define THREAD_NUM 3
#define BUFFER_SIZE 10
#define TEMPO_MAX 8

typedef struct {
    int clock[THREAD_NUM];
} VectorClock;

VectorClock buffer_entrada[BUFFER_SIZE];
VectorClock buffer_saida[BUFFER_SIZE];
int buffer_entrada_count = 0;
int buffer_saida_count = 0;

pthread_mutex_t mutex_entrada, mutex_saida;
pthread_cond_t can_produce_entrada, can_consume_entrada;
pthread_cond_t can_produce_saida, can_consume_saida;

void print_vector_clock(VectorClock *vc, int rank) {
    printf("Processo %d - Relógio: [", rank);
    for (int i = 0; i < THREAD_NUM; i++) {
        printf("%d", vc->clock[i]);
        if (i < THREAD_NUM - 1) printf(", ");
    }
    printf("]\n");
}

void Event(int id, VectorClock *vc) {
    vc->clock[id]++;
}

void Send(VectorClock *clock, int rank, int dest) {
    printf("Processo %d: Enviando relógio para o processo %d\n", rank, dest);
    MPI_Send(clock->clock, THREAD_NUM, MPI_INT, dest, 0, MPI_COMM_WORLD);
    printf("Processo %d: Relógio enviado para o processo %d\n", rank, dest);
}

void Receive(VectorClock *clock, int rank, int source) {
    int new_clock[THREAD_NUM];
    printf("Processo %d: Aguardando mensagem do processo %d\n", rank, source);
    MPI_Recv(new_clock, THREAD_NUM, MPI_INT, source, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    
    for (int i = 0; i < THREAD_NUM; i++) {
        if (clock->clock[i] < new_clock[i]) {
            clock->clock[i] = new_clock[i];
        }
    }
    Event(rank, clock);
    printf("Processo %d: Mensagem recebida do processo %d e relógio atualizado\n", rank, source);
    print_vector_clock(clock, rank);
}

void *thread_entrada(void *arg) {
    int rank = *(int *)arg;
    VectorClock vc = {{0}};
    int source = (rank == 0) ? THREAD_NUM - 1 : rank - 1;
    
    while (1) {
        Receive(&vc, rank, source);

        pthread_mutex_lock(&mutex_entrada);
        while (buffer_entrada_count == BUFFER_SIZE) {
            printf("Processo %d: Thread Entrada - Fila cheia, aguardando...\n", rank);
            pthread_cond_wait(&can_produce_entrada, &mutex_entrada);
        }

        buffer_entrada[buffer_entrada_count++] = vc;
        printf("Processo %d: Thread Entrada - Mensagem colocada na fila de entrada\n", rank);
        pthread_cond_signal(&can_consume_entrada);
        pthread_mutex_unlock(&mutex_entrada);
    }
}

void *thread_relogios(void *arg) {
    int rank = *(int *)arg;
    VectorClock vc = {{0}};
    
    while (1) {
        pthread_mutex_lock(&mutex_entrada);
        while (buffer_entrada_count == 0) {
            printf("Processo %d: Thread Relógios - Fila de entrada vazia, aguardando...\n", rank);
            pthread_cond_wait(&can_consume_entrada, &mutex_entrada);
        }

        vc = buffer_entrada[--buffer_entrada_count];
        printf("Processo %d: Thread Relógios - Consumiu da fila de entrada\n", rank);
        pthread_cond_signal(&can_produce_entrada);
        pthread_mutex_unlock(&mutex_entrada);

        Event(rank, &vc);
        printf("Processo %d: Thread Relógios - Relógio atualizado\n", rank);
        print_vector_clock(&vc, rank);

        pthread_mutex_lock(&mutex_saida);
        while (buffer_saida_count == BUFFER_SIZE) {
            printf("Processo %d: Thread Relógios - Fila de saída cheia, aguardando...\n", rank);
            pthread_cond_wait(&can_produce_saida, &mutex_saida);
        }

        buffer_saida[buffer_saida_count++] = vc;
        printf("Processo %d: Thread Relógios - Colocou na fila de saída\n", rank);
        pthread_cond_signal(&can_consume_saida);
        pthread_mutex_unlock(&mutex_saida);

        sleep(1); 
    }
}

void *thread_saida(void *arg) {
    int rank = *(int *)arg;
    VectorClock vc = {{0}};
    int destination = (rank + 1) % THREAD_NUM;

    while (1) {
        pthread_mutex_lock(&mutex_saida);
        while (buffer_saida_count == 0) {
            printf("Processo %d: Thread Saída - Fila de saída vazia, aguardando...\n", rank);
            pthread_cond_wait(&can_consume_saida, &mutex_saida);
        }

        vc = buffer_saida[--buffer_saida_count];
        printf("Processo %d: Thread Saída - Consumiu da fila de saída\n", rank);
        pthread_cond_signal(&can_produce_saida);
        pthread_mutex_unlock(&mutex_saida);

        Send(&vc, rank, destination);
        printf("Processo %d: Thread Saída - Mensagem enviada para o processo %d\n", rank, destination);
        print_vector_clock(&vc, rank);

        sleep(1); 
    }
}

int main(int argc, char** argv) {
    int rank, size;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (size != THREAD_NUM) {
        if (rank == 0) {
            printf("Este programa deve ser executado com exatamente %d processos.\n", THREAD_NUM);
        }
        MPI_Finalize();
        return 1;
    }

    pthread_t entrada_thread, relogios_thread, saida_thread;
    pthread_mutex_init(&mutex_entrada, NULL);
    pthread_mutex_init(&mutex_saida, NULL);
    pthread_cond_init(&can_produce_entrada, NULL);
    pthread_cond_init(&can_consume_entrada, NULL);
    pthread_cond_init(&can_produce_saida, NULL);
    pthread_cond_init(&can_consume_saida, NULL);

    pthread_create(&entrada_thread, NULL, thread_entrada, &rank);
    pthread_create(&relogios_thread, NULL, thread_relogios, &rank);
    pthread_create(&saida_thread, NULL, thread_saida, &rank);

    if (rank == 0) {
        VectorClock initial_vc = {{1, 0, 0}};
        Send(&initial_vc, rank, (rank + 1) % THREAD_NUM);
        printf("Processo 0: Enviou mensagem inicial\n");
    }

    pthread_join(entrada_thread, NULL);
    pthread_join(relogios_thread, NULL);
    pthread_join(saida_thread, NULL);

    pthread_mutex_destroy(&mutex_entrada);
    pthread_mutex_destroy(&mutex_saida);
    pthread_cond_destroy(&can_produce_entrada);
    pthread_cond_destroy(&can_consume_entrada);
    pthread_cond_destroy(&can_produce_saida);
    pthread_cond_destroy(&can_consume_saida);

    MPI_Finalize();
    return 0;
}
