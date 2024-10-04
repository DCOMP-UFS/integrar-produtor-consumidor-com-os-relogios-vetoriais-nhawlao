[![Open in Visual Studio Code](https://classroom.github.com/assets/open-in-vscode-2e0aaae1b6195c2367325f4f02e2d04e9abb55f0b24a779b69b11b9e10269abc.svg)](https://classroom.github.com/online_ide?assignment_repo_id=16344778&assignment_repo_type=AssignmentRepo)
# Etapa 3 - Projeto PPC

## Equipe responsável:
 * Nayla Sahra Santos das Chagas - 202000024525  
 * Túlio Sousa de Gois - 202000024599
   
---

# Explicação da Integração do Sistema Produtor Consumidor com o Sistema de Relógios Vetoriais

## Solução

A nossa solução implementa um sistema distribuído de relógios vetoriais usando MPI para comunicação entre processos e pthreads para concorrência dentro de cada processo. O sistema segue um modelo produtor-consumidor com buffers intermediários.

* **Relógio Vetorial**   
    O relógio vetorial é implementado como uma estrutura `VectorClock` que contém um array de inteiros. O tamanho deste array é igual ao número de processos no sistema (definido por `THREAD_NUM`).

    ```c
    typedef struct {
        int clock[THREAD_NUM];
    } VectorClock;
    ```

* **Buffer**  
    Dois buffers compartilhados são utilizados: `buffer_entrada` e `buffer_saida`. Ambos são arrays de `VectorClock` com tamanho fixo `BUFFER_SIZE`.

    ```c
    VectorClock buffer_entrada[BUFFER_SIZE];
    VectorClock buffer_saida[BUFFER_SIZE];
    ```

* **Sincronização**    
    A sincronização é implementada usando mutexes e variáveis de condição para acessar os buffers, evitando condições de corrida:

    ```c
    pthread_mutex_t mutex_entrada, mutex_saida;
    pthread_cond_t can_produce_entrada, can_consume_entrada;
    pthread_cond_t can_produce_saida, can_consume_saida;
    ```

### Componentes Principais

1. **Thread de Entrada**
   - Recebe mensagens via MPI
   - Atualiza o relógio vetorial local
   - Coloca o relógio atualizado no buffer de entrada

2. **Thread de Relógios**
   - Consome relógios do buffer de entrada
   - Atualiza o relógio local (evento local)
   - Coloca o relógio atualizado no buffer de saída

3. **Thread de Saída**
   - Consome relógios do buffer de saída
   - Envia o relógio para o próximo processo via MPI

### Funcionamento

1. **Inicialização**
   - O programa inicia `THREAD_NUM` processos MPI
   - Cada processo cria 3 threads: entrada, relógios e saída
   - O processo 0 envia uma mensagem inicial para iniciar o fluxo

2. **Comunicação**
   - Os processos formam um anel lógico
   - Cada processo envia mensagens para o próximo e recebe do anterior

3. **Atualização do Relógio**
   - Na recepção de uma mensagem, o relógio é atualizado com o máximo entre o local e o recebido
   - Um evento local incrementa apenas o contador do processo atual

### Cenários de teste

Os cenários de teste estão implementados nas funções de cada thread. O cenário principal é o de buffer cheio/vazio, que é tratado usando as variáveis de condição.

1. **Buffer de Entrada Cheio**
   ```c
   while (buffer_entrada_count == BUFFER_SIZE) {
       printf("Processo %d: Thread Entrada - Fila cheia, aguardando...\n", rank);
       pthread_cond_wait(&can_produce_entrada, &mutex_entrada);
   }
   ```

2. **Buffer de Saída Vazio**
   ```c
   while (buffer_saida_count == 0) {
       printf("Processo %d: Thread Saída - Fila de saída vazia, aguardando...\n", rank);
       pthread_cond_wait(&can_consume_saida, &mutex_saida);
   }
   ```
# Dificuldades encontradas
- Sincronização das threads;
- Validação do resultado dos relógios vetoriais;
