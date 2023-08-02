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

#ifndef ARCPROXY_H
#define ARCPROXY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>


typedef struct arcproxy_t arcproxy_t;
typedef uint32_t arcref_t;

extern arcproxy_t *arcproxy_create(uint32_t size);
extern void arcproxy_destroy(arcproxy_t *proxy);

extern arcref_t arcproxy_ref_acquire(arcproxy_t *proxy);
extern void arcproxy_ref_release(arcproxy_t *proxy, arcref_t ref);

extern bool arcproxy_retire(arcproxy_t *proxy, void *obj, void (*dealloc)(void *));

#ifdef __cplusplus
}
#endif

#endif  // ARCPROXY_H