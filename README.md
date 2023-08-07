# arcproxy
Proxy collector using atomic reference counting for lock-free read access to shared data.

This is based on

1991-05-16 patent 5,295,262 Read-only access without blocking via access vectorsThis is differential reference counting, aka split reference counting, for readers.

2002-11-07 Atomic refcounted smart pointerusing compare and exchange with double word single target.
https://groups.google.com/g/comp.programming.threads/c/Sev_8xKh3RU/m/wEkEqnOhs_oJ

2003-03-09 Atomic refcounted proxy collector
https://groups.google.com/g/comp.programming.threads/c/zV_fGijahA4/m/BAcsYEOBU0UJ

## Example
In main thread

```
arcproxy_t *proxy = arcproxy_create(200);

arcproxy_destroy(proxy);
```

In reader threads
```
arcref_t ref = arcproxy_ref_acquire(proxy); // prior to every read access to data

arcproxy_ref_release(proxy, ref); // after every read access data
```

In writer thread
```
 // update shared data
arcproxy_retire(proxy, pdata, &free);   // free data when safe to do so
```

## Build
In main directory
```
cmake .
make install
```

Header file, arcproxy.h, in project include directory.
Library file, libarcproxy.a, in project lib directory.

## Usage restrictions

A 16 bit monotonic counter is used for ephemeral references.  It can wrap without a prolmen
provided there are less than 2**16 concurrent references via arcproxy_ref_acquire.
