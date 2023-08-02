/*
   Copyright 20203 Joseph W. Seigh
   
   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/

/*
* ARCProxy usage example
*/
#include <stdlib.h>
#include <stdio.h>
#include <stdatomic.h>
#include <threads.h>
#include <string.h>

#include "../src/arcproxy.h"



#define LIVE    "live"
#define STALE   "stale"
#define INVALID "invalid"

typedef struct {
    arcproxy_t *proxy;
    char *pdata;
} env_t;


static int reader(env_t *env)
{

    for (int ndx = 0; ndx < 4; ndx++)
    {
        arcref_t ref = arcproxy_ref_acquire(env->proxy); // prior to every read of data

        char *pdata = atomic_load_explicit(&env->pdata, memory_order_acquire);

        fprintf(stdout, "%d) data=%s (before yield)\n", ndx, pdata);
        thrd_yield();
        fprintf(stdout, "%d) data=%s (after yield)\n", ndx, pdata);

        arcproxy_ref_release(env->proxy, ref); // after every read of data
    }

    return 0;
}

static void freedata(void *data) {
    int *pdata = data;
    strcpy(data, INVALID);
    thrd_yield();
    free(data);
}

static int writer(env_t *env)
{
    char *pdata = malloc(16);
    strcpy(pdata, LIVE);
    pdata = atomic_exchange(&env->pdata, pdata);
    strcpy(pdata, STALE);   // indicate old data is now stale

    arcproxy_retire(env->proxy, pdata, &freedata);
    return 0;
}

int main(int argc, char **argv)
{
    env_t env;

    env.proxy = arcproxy_create(200);      // default config
    env.pdata = malloc(16);
    strcpy(env.pdata, LIVE);

    thrd_t reader_tid;
    thrd_create(&reader_tid, (thrd_start_t) &reader, &env);
    thrd_yield();

    thrd_t writer_tid;
    thrd_create(&writer_tid, (thrd_start_t) &writer, &env);

    thrd_join(reader_tid, NULL);
    thrd_join(writer_tid, NULL);


    arcproxy_destroy(env.proxy);
    return 0;
}