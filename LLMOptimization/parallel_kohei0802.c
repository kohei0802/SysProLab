
#include "common.h" // some common definitions

#include <unistd.h>       // for nearly everything :)
#include <stdio.h>        // for printf, sprintf, fgets
#include <stdlib.h>       // for malloc, calloc
#include <stdint.h>       // for uint8_t and uint64_t
#include <time.h>         // for time
#include <string.h>       // for memcpy and strcmp
#include <sys/resource.h> // for rusage collection

#include "model.h"// for Llama definitions -> no need to know

int pos = 0; // global position of generation
Transformer transformer; // transformer instance to be init
Tokenizer tokenizer;     // tokenizer instance to be init
Sampler sampler;         // sampler instance to be init

// YOUR CODE STARTS HERE HAJIMARI
#include <pthread.h>
#include <semaphore.h> // uncomment this line if you use semaphore
#include <stdbool.h>   // uncomment this line if you want true / false

/* Each thread has one corresponding ThreadInfo*/
typedef struct {   
    pthread_t tid;        /* ID returned by pthread_create() */
    int is_busy;  //indicate this thread should/is working on the given task
    pthread_mutex_t mutex; //protect this thread's variable
    pthread_cond_t work_cond; //signal to work 
    pthread_cond_t done_cond; //signal about the completion
    void *args;
    void (*task)(void *);
    struct timeval utime;    //time recorded by each thread before pthread_exit
    struct timeval stime;   
    int should_end; //indicate end of this thread
} ThreadInfo;

/* Store all created ThreadInfo and metadata */
typedef struct {
    ThreadInfo *threads;
    int num_threads;
    int active_threads;
    pthread_mutex_t count_mutex;  //protect this thing
    pthread_cond_t all_done; //signalled by the last worker
} ThreadPool;

ThreadPool *pool = NULL; 

/* defining the argument for mat_vec_mul_task_func */
typedef struct {
    float* out; 
    QuantizedTensor *vec;
    QuantizedTensor *mat; 
    int col;
    int start_row;
    int end_row;
} mat_mul_arg;

/* defining the argument for mat_vec_mul_task_func */
typedef struct {
    float* out;       
    float* q;           
    float* key_cache;  
    float* value_cache; 
    float* att;
    int seq_len;
    int n_heads;
    int head_size;      
    int kv_dim;
    int kv_mul;
    int start_head;
    int end_head;
} head_mul_arg;

// function executed by each thread to complete mat_vec_mul
// @note: please modify the signature to what you want
void mat_vec_mul_task_func(void *args) {
    /* Only differ from the seq's version of mat_vec_func by the index of the outer for-loop*/
    mat_mul_arg* arg = (mat_mul_arg *) args;
    float* out = arg->out;
    QuantizedTensor *vec = arg->vec;
    QuantizedTensor *mat = arg->mat; 
    int col = arg->col;
    int start_row = arg->start_row;
    int end_row = arg->end_row;

    // almost the same as seq except the range
    for (int i = start_row; i <= end_row; i++) {

        float val = 0.0f; 
        int32_t ival = 0; 
        int in = i * col;   

        for (int j = 0; j <= col - GS; j += GS) {
            for (int k = 0; k < GS; k++) {
                ival += ((int32_t) vec->q[j + k]) * ((int32_t) mat->q[in + j + k]);
            }
            val += ((float) ival) * mat->s[(in + j) / GS] * vec->s[j / GS];
            ival = 0;
        }
        out[i] = val;
    }
}

// function executed by each thread to complete multi_head_attn
// @note: please modify the signature to what you want
void multi_head_attn_task_func(void * args) {
    /* Only differ from the seq's version of mat_vec_func by the index of the outer for-loop*/
    head_mul_arg* arg = (head_mul_arg *) args;
    float* out = arg->out;       
    float* q = arg->q;           
    float* key_cache = arg->key_cache;  
    float* value_cache = arg->value_cache; 
    float* att = arg->att;
    int seq_len = arg->seq_len;
    int n_heads = arg->n_heads;
    int head_size = arg->head_size;      
    int kv_dim = arg->kv_dim;
    int kv_mul = arg->kv_mul;
    int start_head = arg->start_head;
    int end_head = arg->end_head;

    // almost the same as seq except the range
    for (int h = start_head; h <= end_head; h++) {
        float* head_q = q + h * head_size;
        float* head_att = att + h * seq_len;
        for (int t = 0; t <= pos; t++) {
            float* head_k = key_cache + t * kv_dim + (h / kv_mul) * head_size;
            float score = 0.0f;
            for (int i = 0; i < head_size; i++) {
                score += head_q[i] * head_k[i];
            }
            score /= sqrtf(head_size);
            head_att[t] = score;
        }

        softmax(head_att, pos + 1);

        float* head_out = out + h * head_size;
        memset(head_out, 0, head_size * sizeof(float));
        for (int t = 0; t <= pos; t++) {
            float* head_v = value_cache + t * kv_dim + (h / kv_mul) * head_size;
            float a = head_att[t];
            for (int i = 0; i < head_size; i++) {
                head_out[i] += a * head_v[i];
            }
        }
    }
}

/* Called by thr_func before exiting to store time info*/
void store_system_info(ThreadInfo *info) 
{
    // printf("Exiting! thread %ld\n", info->tid);
    struct rusage usage;
    if (getrusage(RUSAGE_THREAD, &usage)==0) 
    {
        info->utime = usage.ru_utime;
        info->stime = usage.ru_stime;
    }
    else {
        printf("Couldn't get usage\n");
        exit(1);
    }
}

// thread function used in pthread_create
// @note: YOU CAN NOT MODIFY this FUNCTION SIGNATURE!!!
void *thr_func(void *arg) {
    ThreadInfo *info = (ThreadInfo *) arg;
    void (*work)(void *) = NULL;

    // printf("Thread %lu is waiting for work\n",(unsigned long) pthread_self());

    while (1)
    {
        pthread_mutex_lock(&info->mutex);
        while (!info->is_busy && !info->should_end)
        {
            // printf("Thraed %lu going to wait\n", (unsigned long) pthread_self());
            pthread_cond_wait(&info->work_cond, &info->mutex);
        }
        // printf("Thread %lu got work\n", (unsigned long) pthread_self());

        if (info->should_end)
        {
            // printf("I should end!!! %ld\n", info->tid);
            pthread_mutex_unlock(&info->mutex);
            break;
        }

        //copy work address to local stack
        work = info->task; 
        pthread_mutex_unlock(&info->mutex);

        work(info->args);

        // printf("Thread %lu finished work\n", (unsigned long) pthread_self());
        pthread_mutex_lock(&info->mutex);
        info->is_busy = 0;
        pthread_cond_signal(&info->done_cond);
        pthread_mutex_unlock(&info->mutex);
    }

    store_system_info(info);
    pthread_exit(NULL);

    return NULL;
}

// function to initialize thread pool
// @note: YOU CAN NOT MODIFY this FUNCTION SIGNATURE!!!
void init_thr_pool(int num_thr) {
    pool = (ThreadPool *) malloc(sizeof(ThreadPool)); // should cast (ThreadPool *)
    if (!pool) 
        exit(1);
    pool->num_threads = num_thr;
    pool->threads = (ThreadInfo *) malloc(num_thr * sizeof(ThreadInfo));
    pool->active_threads = 0;

    pthread_mutex_init(&pool->count_mutex, NULL);
    pthread_cond_init(&pool->all_done, NULL);

    for(int i=0; i<num_thr; i++) {
        pthread_mutex_init(&pool->threads[i].mutex, NULL);
        pthread_cond_init(&pool->threads[i].work_cond, NULL);
        pthread_cond_init(&pool->threads[i].done_cond, NULL);
        pool->threads[i].is_busy = 0; //is_busy has to be 0
        pool->threads[i].task = NULL;
        pool->threads[i].args = NULL;
        pool->threads[i].should_end = 0; //should_end has to be 0
        pthread_create(&pool->threads[i].tid, NULL, thr_func, &pool->threads[i]);
    }

    return;
}

// function to close thread pool
// @note: YOU CAN NOT MODIFY this FUNCTION SIGNATURE!!!
void close_thr_pool() {

    for(int i=0; i<pool->num_threads; i++)
    {
        // printf("signalling %d\n", i);
        pool->threads[i].should_end = 1;
        pthread_cond_signal(&pool->threads[i].work_cond);
    }

    for(int i=0; i<pool->num_threads; i++)
    {
        // printf("Joining %d\n", i);
        pthread_join(pool->threads[i].tid, NULL);
    }

    for(int i=0; i<pool->num_threads; i++) 
    {
        struct timeval utime = pool->threads[i].utime;
        struct timeval stime = pool->threads[i].stime;
        printf("Thread %d has completed - user: %.4f s, system: %.4f s\n", \
        i, (utime.tv_sec+utime.tv_usec/1000000.0), (stime.tv_sec+stime.tv_usec/1000000.0));
    }

    struct rusage main_usage;
    getrusage(RUSAGE_THREAD, &main_usage); // to avoid child threads
    printf("\033[0;32mmain thread - user: %.4f s, system: %.4f s \033[0m\n",
    (main_usage.ru_utime.tv_sec + main_usage.ru_utime.tv_usec/1000000.0),
    (main_usage.ru_stime.tv_sec + main_usage.ru_stime.tv_usec/1000000.0));

    struct rusage whole_usage;
    getrusage(RUSAGE_SELF, &main_usage); // to avoid child threads
    printf("\033[0;32mWhole process - user: %.4f s, system: %.4f s \033[0m\n",
    (main_usage.ru_utime.tv_sec + main_usage.ru_utime.tv_usec/1000000.0),
    (main_usage.ru_stime.tv_sec + main_usage.ru_stime.tv_usec/1000000.0));
}

// ----------------------------------------------------------------------------
// entry function for multi-threading matrix multiplication
// @note: YOU CAN NOT MODIFY this FUNCTION SIGNATURE!!!
void mat_vec_mul(float* out, QuantizedTensor *vec, QuantizedTensor *mat, int col, int row) {
    // for each row
    // @note parallel this loop
    int start_row = 0;
    int end_row; //up to row-1   !!! It's not row, but row-1!!
    int r_per_thr = (row + pool->num_threads - 1) / pool->num_threads;

    for (int i = 0; i < pool->num_threads; i++)
    {
        // printf("Main thread: about to sigal thread %d\n", i);

        pthread_mutex_lock(&pool->threads[i].mutex);
        pool->threads[i].is_busy = 1;
        pool->threads[i].task = mat_vec_mul_task_func;
        start_row = i * r_per_thr; //calculate the starting row to work on
        end_row = (i+1) * r_per_thr - 1;  //calculate the last row to work on by the thread.
        if (start_row > row - 1) { //is the starting row exceeding the max row?
            //if yes, just leave the rest of the threads asleep
            pool->threads[i].is_busy = 0;
            pool->threads[i].task = NULL;
            pthread_mutex_unlock(&pool->threads[i].mutex);
            break;
        }
        else if (end_row > row - 1) // is end row exceeding the last row?
            end_row = row - 1; //if yes, adjust the end_row to the last row

        // Allocate memory to ThreadInfo's args first before assigning mat_mul_arg
        mat_mul_arg arg = (mat_mul_arg) {
            .out = out,
            .vec = vec,
            .mat = mat,
            .col = col,
            .start_row = start_row,
            .end_row = end_row
        };
        pool->threads[i].args = malloc(sizeof(mat_mul_arg));
        *(mat_mul_arg *)pool->threads[i].args = arg;

        // printf("Main thread: set is busy to 1 thread %d\n", i);

        pthread_cond_signal(&pool->threads[i].work_cond);
        // printf("Main thread: sent signal thread %d\n", i);

        pthread_mutex_unlock(&pool->threads[i].mutex);
        // printf("Main thread: unlocked mutex thread %d\n", i);
    }

    for(int i=0; i < pool->num_threads; i++) 
    {
        pthread_mutex_lock(&pool->threads[i].mutex);
        while (pool->threads[i].is_busy != 0)
        {
            pthread_cond_wait(&pool->threads[i].done_cond, &pool->threads[i].mutex);
        }
        // printf("Main thread: confirmed is_busy set back to 0 thread %d\n", i);

        /* reset */
        pool->threads[i].task = NULL;

        free(pool->threads[i].args); //free the resources, prevent memory leak
        pool->threads[i].args = NULL;
        // if (!pool->threads[i].args)
        //     printf("Arguments back to null, thread %d\n", i);
        
        pthread_mutex_unlock(&pool->threads[i].mutex);
    }
}


// ----------------------------------------------------------------------------
// entry function for multi-threading multi-head-attention
// @note: YOU CAN NOT MODIFY FUNCTION SIGNATURE!!!
void multi_head_attn(
    float* out,         // output tensor [head, head_size]
    float* q,           // query tensor  [head, head_size]
    float* key_cache,   // cache of history key tensor   [kv_head, seq_len, head_size]
    float* value_cache, // cache of history value tensor [kv_head, seq_len, head_size]
    float* att,         // buffer for attention score [head, seq_len]
    int seq_len,        // current sequence length
    int n_heads,        // number of heades
    int head_size,      // size of each head
    int kv_dim,
    int kv_mul) {
    
    int start_head = 0;
    int end_head; //up to row-1   !!! It's not row, but row-1!!
    int r_per_thr = (n_heads + pool->num_threads - 1) / pool->num_threads;

    /* The following works exactly the same as mat_vec_mul. 
    See the comments there for explanation
    */
    for (int i = 0; i < pool->num_threads; i++)
    {
        // printf("Main thread: about to sigal thread %d\n", i);

        pthread_mutex_lock(&pool->threads[i].mutex);
        pool->threads[i].is_busy = 1;
        pool->threads[i].task = multi_head_attn_task_func;
        start_head = i * r_per_thr;
        end_head = (i+1) * r_per_thr - 1;
        if (start_head > n_heads - 1) {
            //leave
            pool->threads[i].is_busy = 0;
            pool->threads[i].task = NULL;
            pthread_mutex_unlock(&pool->threads[i].mutex);
            break;
        }
        else if (end_head > n_heads - 1) 
            end_head = n_heads - 1;

        head_mul_arg arg = (head_mul_arg) {
            .out = out,
            .q = q,
            .key_cache = key_cache,
            .value_cache = value_cache,
            .att = att,
            .seq_len = seq_len,
            .n_heads = n_heads,
            .head_size = head_size,
            .kv_dim = kv_dim,
            .kv_mul = kv_mul,
            .start_head = start_head,
            .end_head = end_head
        };
        pool->threads[i].args = malloc(sizeof(head_mul_arg));
        *(head_mul_arg *)pool->threads[i].args = arg;

        // printf("Main thread: set is busy to 1 thread %d\n", i);

        pthread_cond_signal(&pool->threads[i].work_cond);
        // printf("Main thread: sent signal thread %d\n", i);

        pthread_mutex_unlock(&pool->threads[i].mutex);
        // printf("Main thread: unlocked mutex thread %d\n", i);
    }

    for(int i=0; i < pool->num_threads; i++) 
    {
        pthread_mutex_lock(&pool->threads[i].mutex);
        while (pool->threads[i].is_busy != 0)
        {
            pthread_cond_wait(&pool->threads[i].done_cond, &pool->threads[i].mutex);
        }
        // printf("Main thread: confirmed is_busy set back to 0 thread %d\n", i);

        /* reset */
        pool->threads[i].task = NULL;

        free(pool->threads[i].args);
        pool->threads[i].args = NULL;
        // if (!pool->threads[i].args)
        //     printf("Arguments back to null, thread %d\n", i);
        
        pthread_mutex_unlock(&pool->threads[i].mutex);
    }
}

// YOUR CODE ENDS HERE OWARI

// ----------------------------------------------------------------------------
// forward Transformer, you're not allowed to modify this part
float* forward(Transformer* transformer, int token, int pos) {

    // a few convenience variables
    Config* p = &transformer->config;
    TransformerWeights* w = &transformer->weights;
    RunState* s = &transformer->state;
    float *x = s->x;
    int dim = p->dim;
    int kv_dim = (p->dim * p->n_kv_heads) / p->n_heads;
    int kv_mul = p->n_heads / p->n_kv_heads; // integer multiplier of the kv sharing in multiquery
    int hidden_dim =  p->hidden_dim;
    int head_size = dim / p->n_heads;

    // copy the token embedding into x
    memcpy(x, w->token_embedding_table + token*dim, dim * sizeof(float));

    // forward all the layers
    for(int l = 0; l < p->n_layers; l++) {

        // attention rmsnorm
        rmsnorm(s->xb, x, w->rms_att_weight + l*dim, dim);

        // qkv matmuls for this position
        quantize(&s->xq, s->xb, dim);
        mat_vec_mul(s->q, &s->xq, w->wq + l, dim, dim);
        mat_vec_mul(s->k, &s->xq, w->wk + l, dim, kv_dim);
        mat_vec_mul(s->v, &s->xq, w->wv + l, dim, kv_dim);

        // RoPE relative positional encoding: complex-valued rotate q and k in each head
        for (int i = 0; i < dim; i+=2) {
            int head_dim = i % head_size;
            float freq = 1.0f / powf(10000.0f, head_dim / (float)head_size);
            float val = pos * freq;
            float fcr = cosf(val);
            float fci = sinf(val);
            int rotn = i < kv_dim ? 2 : 1; // how many vectors? 2 = q & k, 1 = q only
            for (int v = 0; v < rotn; v++) {
                float* vec = v == 0 ? s->q : s->k; // the vector to rotate (query or key)
                float v0 = vec[i];
                float v1 = vec[i+1];
                vec[i]   = v0 * fcr - v1 * fci;
                vec[i+1] = v0 * fci + v1 * fcr;
            }
        }

        // save key,value at this time step (pos) to our kv cache
        int loff = l * p->seq_len * kv_dim; // kv cache layer offset for convenience
        float* key_cache_row = s->key_cache + loff + pos * kv_dim;
        float* value_cache_row = s->value_cache + loff + pos * kv_dim;
        memcpy(key_cache_row, s->k, kv_dim * sizeof(*key_cache_row));
        memcpy(value_cache_row, s->v, kv_dim * sizeof(*value_cache_row));

        multi_head_attn(s->xb, s->q, s->key_cache + loff, s->value_cache + loff, s->att, p->seq_len, p->n_heads, head_size, kv_dim, kv_mul);

        // final matmul to get the output of the attention
        quantize(&s->xq, s->xb, dim);
        mat_vec_mul(s->xb2, &s->xq, w->wo + l, dim, dim);

        // residual connection back into x
        for (int i = 0; i < dim; i++) {
            x[i] += s->xb2[i];
        }

        // ffn rmsnorm
        rmsnorm(s->xb, x, w->rms_ffn_weight + l*dim, dim);

        // Now for FFN in PyTorch we have: self.w2(F.silu(self.w1(x)) * self.w3(x))
        // first calculate self.w1(x) and self.w3(x)
        quantize(&s->xq, s->xb, dim);
        mat_vec_mul(s->hb, &s->xq, w->w1 + l, dim, hidden_dim);
        mat_vec_mul(s->hb2, &s->xq, w->w3 + l, dim, hidden_dim);

        // SwiGLU non-linearity
        for (int i = 0; i < hidden_dim; i++) {
            float val = s->hb[i];
            // silu(x)=x*σ(x), where σ(x) is the logistic sigmoid
            val *= (1.0f / (1.0f + expf(-val)));
            // elementwise multiply with w3(x)
            val *= s->hb2[i];
            s->hb[i] = val;
        }

        // final matmul to get the output of the ffn
        quantize(&s->hq, s->hb, hidden_dim);
        mat_vec_mul(s->xb, &s->hq, w->w2 + l, hidden_dim, dim);

        // residual connection
        for (int i = 0; i < dim; i++) {
            x[i] += s->xb[i];
        }
    }

    // final rmsnorm
    rmsnorm(x, x, w->rms_final_weight, dim);

    // classifier into logits
    quantize(&s->xq, x, dim);
    mat_vec_mul(s->logits, &s->xq, w->wcls, dim, p->vocab_size);
    return s->logits;
}

// ----------------------------------------------------------------------------
// generation loop, you're not allowed to modify this part
void generate(char *prompt) {
    // encode the (string) prompt into tokens sequence
    int num_prompt_tokens = 0;
    int* prompt_tokens = (int*)malloc((strlen(prompt)+6) * sizeof(int)); // +6 reserved for prompt template
    encode(&tokenizer, prompt, prompt_tokens, &num_prompt_tokens);
    if (num_prompt_tokens < 1) {
        fprintf(stderr, "something is wrong, expected at least 1 prompt token\n");
        exit(EXIT_FAILURE);
    }

    // start the main loop
    int next;        // place holder for next token
    int token = prompt_tokens[0]; // place holder of prev token, kickoff as prompt_tokens[0]
    int end_pos = pos + MAX_NEW_TOKENS + num_prompt_tokens;
    int start_pos = pos;
    long start_time = 0; // to be lazy iniialzied
    while (pos < end_pos) {

        // forward the transformer to get logits for the next token
        float* logits = forward(&transformer, token, pos);

        if (pos < start_pos + num_prompt_tokens - 1) {
            // if we are still processing the input prompt, force the next prompt token
            next = prompt_tokens[pos - start_pos + 1];
        } else if (pos == end_pos - 2) {
            // reaching the end, force it to close by <|im_end|>
            next = 2; // := <|im_end|>
        } else {
            // otherwise sample the next token from the logits
            next = sample(&sampler, logits);
        }

        pos++;

        // print the token as string, decode it with the Tokenizer object
        char* piece = decode(&tokenizer, token, next);
        if (pos >= num_prompt_tokens) {
            safe_printf(piece); // same as printf("%s", piece), but skips "unsafe" bytes
            fflush(stdout);
        }

        token = next;

        // init the timer here because the first iteration can be slower
        if (start_time == 0) { start_time = time_in_ms(); }
    }
    printf("\n");

    long end_time = time_in_ms();
    // \033[0;32m set color to green and \033[0m reset to default, they won't be generate by LLM
    fprintf(stdout, "\033[0;32mlength: %d, speed (tok/s): %.4f \033[0m\n", 
        pos, (pos - start_pos) / (float) (end_time - start_time) * 1000);
    
    free(prompt_tokens);
}

int main(int argc, char *argv[]) {

    // default parameters
    char *model_path     = "model.bin";  // e.g. out/model.bin
    char *tokenizer_path = "tokenizer.bin";
    float temperature    = 0.6f;  // 0.0 = greedy deterministic. 1.0 = original. don't set higher
    float topp           = 0.9f;  // top-p in nucleus sampling. 1.0 = off. 0.9 works well, but slower
    char *prompt         = NULL;  // prompt strings
    int num_prompt       = 0; // number of prompts
    uint64_t rng_seed    = 0; // seed rng with time by default
    int num_thr          = 0;

    if (argc == 4) {
        num_thr  = atoi(argv[1]);
        rng_seed = atoi(argv[2]);
        prompt   = argv[3];
    } else {
        fprintf(stderr, "Usage:   ./seq <num_thr> <seed> <prompt>\n");
        fprintf(stderr, "Example: ./seq 4 42 \"What is Fibonacci Number?\"\n");
        fprintf(stderr, "Note:    <prompt> must be quoted with \"\", only one prompt supported\n");
        exit(1);
    }

    // parameter validation/overrides
    if (num_thr <= 0 || num_thr > 16) {
        fprintf(stderr, "num_thr must between 1 and 16 \n");
        exit(EXIT_FAILURE);
    }
    if (rng_seed <= 0) rng_seed = (unsigned int)time(NULL);

    // build the Transformer via the model .bin file
    build_transformer(&transformer, model_path);
    // build the Tokenizer via the tokenizer .bin file
    build_tokenizer(&tokenizer, tokenizer_path, transformer.config.vocab_size);
    // build the Sampler
    build_sampler(&sampler, transformer.config.vocab_size, temperature, topp, rng_seed);

    // initialize thread pool
    init_thr_pool(num_thr);

    printf("user: %s \n", prompt);
    // perform multi-threading generation
    generate(prompt);

    // close thread pool
    close_thr_pool();

    // memory and file handles cleanup
    free_sampler(&sampler);
    free_tokenizer(&tokenizer);
    free_transformer(&transformer);
    return 0;
}