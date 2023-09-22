#ifndef PHPSCI_NDARRAY_ITERATORS_H
#define PHPSCI_NDARRAY_ITERATORS_H

#include "ndarray.h"

typedef struct NDArrayIter {
    int          nd_m1;            /* number of dimensions - 1 */
    int          index, size;
    int          coordinates[NDARRAY_MAX_DIMS];/* N-dimensional loop */
    int          dims_m1[NDARRAY_MAX_DIMS];    /* ao->dimensions - 1 */
    int          strides[NDARRAY_MAX_DIMS];    /* ao->strides or fake */
    int          backstrides[NDARRAY_MAX_DIMS];/* how far to jump back */
    int          factors[NDARRAY_MAX_DIMS];     /* shape factors */
    NDArray      *ao;
    char         *dataptr;        /* pointer to current item*/
    bool         contiguous;
    int          bounds[NDARRAY_MAX_DIMS][2];
    int          limits[NDARRAY_MAX_DIMS][2];
    int          limits_sizes[NDARRAY_MAX_DIMS];
} NDArrayIter;

NDArray* NDArrayIterator_GET(NDArray* array);
void NDArrayIterator_INIT(NDArray* array);
void NDArrayIterator_REWIND(NDArray* array);
int NDArrayIterator_ISDONE(NDArray* array);
void NDArrayIterator_NEXT(NDArray* array);
void NDArrayIterator_FREE(NDArray* array);

NDArray* NDArrayIteratorPHP_GET(NDArray* array);
void NDArrayIteratorPHP_REWIND(NDArray* array);
int NDArrayIteratorPHP_ISDONE(NDArray* array);
void NDArrayIteratorPHP_NEXT(NDArray* array);

NDArrayIter* NDArray_NewElementWiseIter(NDArray *target);

#define _NDArray_ITER_NEXT1(it) do { \
        (it)->dataptr += (it)->strides[0]; \
        (it)->coordinates[0]++; \
} while (0)

#define _NDArray_ITER_NEXT2(it) do { \
        if ((it)->coordinates[1] < (it)->dims_m1[1]) { \
                (it)->coordinates[1]++; \
                (it)->dataptr += (it)->strides[1]; \
        } \
        else { \
                (it)->coordinates[1] = 0; \
                (it)->coordinates[0]++; \
                (it)->dataptr += (it)->strides[0] - \
                        (it)->backstrides[1]; \
        } \
} while (0)

#define NDArray_ITER_RESET(it) do { \
        (it)->index = 0; \
        (it)->dataptr = NDArray_DATA((it)->ao); \
        memset((it)->coordinates, 0, \
               ((it)->nd_m1+1)*sizeof(int)); \
} while (0)

#define NDArray_ITER_NEXT(it) do { \
        (it)->index++; \
        if ((it)->nd_m1 == 0) { \
                _NDArray_ITER_NEXT1((it)); \
        } \
        else if ((it)->contiguous) \
                (it)->dataptr += NDArray_DESCRIPTOR((it)->ao)->elsize; \
        else if ((it)->nd_m1 == 1) { \
                _NDArray_ITER_NEXT2(it); \
        } \
        else { \
                int __nd_i; \
                for (__nd_i=(it)->nd_m1; __nd_i >= 0; __nd_i--) { \
                        if ((it)->coordinates[__nd_i] < \
                            (it)->dims_m1[__nd_i]) { \
                                (it)->coordinates[__nd_i]++; \
                                (it)->dataptr += \
                                        (it)->strides[__nd_i]; \
                                break; \
                        } \
                        else { \
                                (it)->coordinates[__nd_i] = 0; \
                                (it)->dataptr -= \
                                        (it)->backstrides[__nd_i]; \
                        } \
                } \
        } \
} while (0)
#define NDArray_ITER_DATA(it) ((void *)((it)->dataptr))
#endif //PHPSCI_NDARRAY_ITERATORS_H
