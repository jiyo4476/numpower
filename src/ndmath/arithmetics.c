#include <php.h>
#include "Zend/zend_alloc.h"
#include "Zend/zend_API.h"
#include <string.h>
#include "arithmetics.h"
#include "../../config.h"
#include "../initializers.h"
#include "../iterators.h"
#include "../types.h"
#include "../manipulation.h"
#include "double_math.h"

#ifdef HAVE_CUBLAS
#include <cuda_runtime.h>
#include <cublas_v2.h>
#include "cuda/cuda_math.h"
#include "../gpu_alloc.h"

#endif

#ifdef HAVE_CBLAS
#include <cblas.h>
#endif

#ifdef HAVE_AVX2
#include <immintrin.h>
#endif

/**
 * Product of array element-wise
 *
 * @param a
 * @param b
 * @return
 */
float
NDArray_Float_Prod(NDArray* a) {
    float value = 1;
    if (NDArray_DEVICE(a) == NDARRAY_DEVICE_GPU) {
#ifdef HAVE_CUBLAS
        cuda_prod_float(NDArray_NUMELEMENTS(a), NDArray_FDATA(a), &value, NDArray_NUMELEMENTS(a));
#endif
    } else {
        for (int i = 0; i < NDArray_NUMELEMENTS(a); i++) {
            value *= NDArray_FDATA(a)[i];
        }
    }
    return value;
}

/**
 * Add elements of a element-wise
 *
 * @param a
 * @param b
 * @return
 */
float
NDArray_Sum_Float(NDArray* a) {
    float value = 0;
    if (NDArray_DEVICE(a) == NDARRAY_DEVICE_GPU) {
#ifdef HAVE_CUBLAS
        cuda_sum_float(NDArray_NUMELEMENTS(a), NDArray_FDATA(a), &value, NDArray_NUMELEMENTS(a));
#endif
    } else {
        for (int i = 0; i < NDArray_NUMELEMENTS(a); i++) {
            value += NDArray_FDATA(a)[i];
        }
    }
    return value;
}

/**
 * Add elements of a element-wise
 *
 * @param a
 * @param b
 * @return
 */
float
NDArray_Mean_Float(NDArray* a) {
    NDArray_Print(a, 0);
    float value = 0;
    if (NDArray_DEVICE(a) == NDARRAY_DEVICE_GPU) {
#ifdef HAVE_CUBLAS
        cuda_sum_float(NDArray_NUMELEMENTS(a), NDArray_FDATA(a), &value, NDArray_NUMELEMENTS(a));
        value = value / NDArray_NUMELEMENTS(a);
#endif
    } else {

#ifdef HAVE_CBLAS
        value = cblas_sasum(NDArray_NUMELEMENTS(a), NDArray_FDATA(a), 1);
        value = value / NDArray_NUMELEMENTS(a);
#else
        for (int i = 0; i < NDArray_NUMELEMENTS(a); i++) {
            value += NDArray_FDATA(a)[i];
        }
        value = value / NDArray_NUMELEMENTS(a);
#endif
    }
    return value;
}

// Comparison function for sorting
int compare(const void* a, const void* b) {
    float fa = *((const float*)a);
    float fb = *((const float*)b);
    return (fa > fb) - (fa < fb);
}

float calculate_median(float* matrix, int size) {
    // Copy matrix elements to a separate array
    float* temp = malloc(size * sizeof(float));
    if (temp == NULL) {
        // Handle memory allocation error
        fprintf(stderr, "Memory allocation failed.\n");
        exit(1);
    }
    memcpy(temp, matrix, size * sizeof(float));

    // Sort the array in ascending order
    qsort(temp, size, sizeof(float), compare);

    // Calculate the median value
    float median;
    if (size % 2 == 0) {
        // If the number of elements is even, average the two middle values
        median = (temp[size / 2 - 1] + temp[size / 2]) / 2.0f;
    } else {
        // If the number of elements is odd, take the middle value
        median = temp[size / 2];
    }

    // Free the temporary array
    free(temp);

    return median;
}

/**
 * Add elements of a element-wise
 *
 * @todo Implement GPU support
 * @param a
 * @param b
 * @return
 */
float
NDArray_Median_Float(NDArray* a) {
    if (NDArray_DEVICE(a) == NDARRAY_DEVICE_GPU) {
#ifdef HAVE_CUBLAS
        zend_throw_error(NULL, "Median not available for GPU.");
        return -1;
#endif
    } else {
        return calculate_median(NDArray_FDATA(a), NDArray_NUMELEMENTS(a));
    }
}

NDArray*
NDArray_Add_Float(NDArray* a, NDArray* b) {
    NDArray *a_temp = NULL, *b_temp = NULL;
    if (NDArray_DEVICE(a) != NDArray_DEVICE(b) && NDArray_NDIM(a) != 0 && NDArray_NDIM(b) != 0) {
        zend_throw_error(NULL, "Device mismatch, both NDArray MUST be in the same device.");
        return NULL;
    }

    // If a or b are scalars, reshape
    if (NDArray_NDIM(a) == 0 && NDArray_NDIM(b) > 0) {
        a_temp = a;
        int *n_shape = emalloc(sizeof(int) * NDArray_NDIM(b));
        copy(NDArray_SHAPE(b), n_shape, NDArray_NDIM(b));
        a = NDArray_Zeros(n_shape, NDArray_NDIM(b), NDArray_TYPE(b), NDArray_DEVICE(b));
        a = NDArray_Fill(a, NDArray_FDATA(a_temp)[0]);
    } else if (NDArray_NDIM(b) == 0 && NDArray_NDIM(a) > 0) {
        b_temp = b;
        int *n_shape = emalloc(sizeof(int) * NDArray_NDIM(a));
        copy(NDArray_SHAPE(a), n_shape, NDArray_NDIM(a));
        b = NDArray_Zeros(n_shape, NDArray_NDIM(a), NDArray_TYPE(a), NDArray_DEVICE(a));
        b = NDArray_Fill(b, NDArray_FDATA(b_temp)[0]);
    }

    NDArray *broadcasted = NULL;
    NDArray *a_broad = NULL, *b_broad = NULL;

    if (NDArray_NUMELEMENTS(a) < NDArray_NUMELEMENTS(b)) {
        broadcasted = NDArray_Broadcast(a, b);
        a_broad = broadcasted;
        b_broad = b;
    } else if (NDArray_NUMELEMENTS(b) < NDArray_NUMELEMENTS(a)) {
        broadcasted = NDArray_Broadcast(b, a);
        b_broad = broadcasted;
        a_broad = a;
    } else {
        b_broad = b;
        a_broad = a;
    }

    if (b_broad == NULL || a_broad == NULL) {
        zend_throw_error(NULL, "Can't broadcast arrays.");
        return NULL;
    }

    // Check if the element size of the input arrays match
    if (a->descriptor->elsize != sizeof(float) || b->descriptor->elsize != sizeof(float)) {
        // Element size mismatch, return an error or handle it accordingly
        return NULL;
    }

    // Create a new NDArray to store the result
    NDArray *result = (NDArray *) emalloc(sizeof(NDArray));
    result->strides = (int *) emalloc(a_broad->ndim * sizeof(int));
    result->dimensions = (int *) emalloc(a_broad->ndim * sizeof(int));
    result->ndim = a_broad->ndim;
    if (NDArray_DEVICE(a_broad) == NDARRAY_DEVICE_GPU) {
#if HAVE_CUBLAS
        vmalloc((void **) &result->data, NDArray_NUMELEMENTS(a_broad) * sizeof(float));
        cudaDeviceSynchronize();
        result->device = NDARRAY_DEVICE_GPU;
#endif
    } else {
        result->data = (char *) emalloc(a_broad->descriptor->numElements * sizeof(float));
    }
    result->base = NULL;
    result->flags = 0;  // Set appropriate flags
    result->descriptor = (NDArrayDescriptor *) emalloc(sizeof(NDArrayDescriptor));
    result->descriptor->type = NDARRAY_TYPE_FLOAT32;
    result->descriptor->elsize = sizeof(float);
    result->descriptor->numElements = a_broad->descriptor->numElements;
    result->refcount = 1;
    result->device = NDArray_DEVICE(a_broad);

    // Perform element-wise subtraction
    result->strides = memcpy(result->strides, a_broad->strides, a_broad->ndim * sizeof(int));
    result->dimensions = memcpy(result->dimensions, a_broad->dimensions, a_broad->ndim * sizeof(int));
    float *resultData = (float *) result->data;
    float *aData = (float *) a_broad->data;
    float *bData = (float *) b_broad->data;
    int numElements = a_broad->descriptor->numElements;
    NDArrayIterator_INIT(result);
    if (NDArray_DEVICE(a_broad) == NDARRAY_DEVICE_GPU && NDArray_DEVICE(b_broad) == NDARRAY_DEVICE_GPU) {
#if HAVE_CUBLAS
        cuda_add_float(NDArray_NUMELEMENTS(a_broad), NDArray_FDATA(a_broad), NDArray_FDATA(b_broad), NDArray_FDATA(result),
                            NDArray_NUMELEMENTS(a_broad));
#endif
    } else {
#ifdef HAVE_AVX2
        int i;
        __m256 vec1, vec2, sub;

        for (i = 0; i < NDArray_NUMELEMENTS(a) - 7; i += 8) {
            vec1 = _mm256_loadu_ps(&aData[i]);
            vec2 = _mm256_loadu_ps(&bData[i]);
            sub = _mm256_add_ps(vec1, vec2);
            _mm256_storeu_ps(&resultData[i], sub);
        }

        // Handle remaining elements if the length is not a multiple of 4
        for (; i < numElements; i++) {
            resultData[i] = aData[i] + bData[i];
        }
#else
        for (int i = 0; i < numElements; i++) {
            resultData[i] = aData[i] + bData[i];
        }
#endif
    }
    if (a_temp != NULL) {
        NDArray_FREE(a);
    }
    if (b_temp != NULL) {
        NDArray_FREE(b);
    }
    if (broadcasted != NULL) {
        NDArray_FREE(broadcasted);
    }
    return result;
}

__m256 fix_negative_zero(__m256 vec) {
    __m256 zero = _mm256_set1_ps(-0.0f);
    __m256 mask = _mm256_cmp_ps(vec, zero, _CMP_EQ_OQ);
    return _mm256_blendv_ps(vec, zero, mask);
}

/**
 * Multiply elements of a and b element-wise
 *
 * @param a
 * @param b
 * @return
 */
NDArray*
NDArray_Multiply_Float(NDArray* a, NDArray* b) {
    NDArray *broadcasted = NULL;
    NDArray *a_temp = NULL, *b_temp = NULL;
    if (NDArray_DEVICE(a) != NDArray_DEVICE(b) && NDArray_NDIM(a) != 0 && NDArray_NDIM(b) != 0) {
        zend_throw_error(NULL, "Device mismatch, both NDArray MUST be in the same device.");
        return NULL;
    }

    if (NDArray_NDIM(a) == 0 && NDArray_NDIM(b) == 0) {
        if (NDArray_DEVICE(a) == NDARRAY_DEVICE_GPU) {
#ifdef HAVE_CUBLAS
            int *shape = ecalloc(1, sizeof(int));
            NDArray *rtn = NDArray_Empty(shape, 0, NDARRAY_TYPE_FLOAT32, NDARRAY_DEVICE_GPU);
            cuda_multiply_float(1, NDArray_FDATA(a), NDArray_FDATA(b), NDArray_FDATA(rtn), 1);
            return rtn;
#endif
        } else {
            int *shape = ecalloc(1, sizeof(int));
            NDArray *rtn = NDArray_Empty(shape, 0, NDARRAY_TYPE_FLOAT32, NDARRAY_DEVICE_CPU);
            NDArray_FDATA(rtn)[0] = NDArray_FDATA(a)[0] * NDArray_FDATA(b)[0];
            return rtn;
        }
    }

    // If a or b are scalars, reshape
    if (NDArray_NDIM(a) == 0 && NDArray_NDIM(b) > 0) {
        a_temp = a;
        int *n_shape = emalloc(sizeof(int) * NDArray_NDIM(b));
        copy(NDArray_SHAPE(b), n_shape, NDArray_NDIM(b));
        a = NDArray_Zeros(n_shape, NDArray_NDIM(b), NDArray_TYPE(b), NDArray_DEVICE(b));
        a = NDArray_Fill(a, NDArray_FDATA(a_temp)[0]);
    } else if (NDArray_NDIM(b) == 0 && NDArray_NDIM(a) > 0) {
        b_temp = b;
        int *n_shape = emalloc(sizeof(int) * NDArray_NDIM(a));
        copy(NDArray_SHAPE(a), n_shape, NDArray_NDIM(a));
        b = NDArray_Zeros(n_shape, NDArray_NDIM(a), NDArray_TYPE(a), NDArray_DEVICE(a));
        b = NDArray_Fill(b, NDArray_FDATA(b_temp)[0]);
    }

    NDArray *a_broad = NULL, *b_broad = NULL;

    if (NDArray_NUMELEMENTS(a) < NDArray_NUMELEMENTS(b)) {
        broadcasted = NDArray_Broadcast(a, b);
        a_broad = broadcasted;
        b_broad = b;
    } else if (NDArray_NUMELEMENTS(b) < NDArray_NUMELEMENTS(a)) {
        broadcasted = NDArray_Broadcast(b, a);
        b_broad = broadcasted;
        a_broad = a;
    } else {
        b_broad = b;
        a_broad = a;
    }
    if (b_broad == NULL || a_broad == NULL) {
        zend_throw_error(NULL, "Can't broadcast arrays.");
        return NULL;
    }

    // Check if the element size of the input arrays match
    if (a->descriptor->elsize != sizeof(float) || b->descriptor->elsize != sizeof(float)) {
        // Element size mismatch, return an error or handle it accordingly
        return NULL;
    }

    // Create a new NDArray to store the result
    NDArray *result = (NDArray *) emalloc(sizeof(NDArray));
    result->strides = (int *) emalloc(a_broad->ndim * sizeof(int));
    result->dimensions = (int *) emalloc(a_broad->ndim * sizeof(int));
    result->ndim = a->ndim;
    result->device = NDArray_DEVICE(a_broad);
    if (NDArray_DEVICE(a_broad) == NDARRAY_DEVICE_GPU) {
#if HAVE_CUBLAS
        vmalloc((void **) &result->data, NDArray_NUMELEMENTS(a_broad) * sizeof(float));
        result->device = NDARRAY_DEVICE_GPU;
#endif
    } else {
        result->data = (char *) emalloc(a_broad->descriptor->numElements * sizeof(float));
    }
    result->base = NULL;
    result->flags = 0;  // Set appropriate flags
    result->descriptor = (NDArrayDescriptor *) emalloc(sizeof(NDArrayDescriptor));
    result->descriptor->type = NDARRAY_TYPE_FLOAT32;
    result->descriptor->elsize = sizeof(float);
    result->descriptor->numElements = a_broad->descriptor->numElements;
    result->refcount = 1;

    // Perform element-wise product
    result->strides = memcpy(result->strides, a_broad->strides, a_broad->ndim * sizeof(int));
    result->dimensions = memcpy(result->dimensions, a_broad->dimensions, a_broad->ndim * sizeof(int));
    float *resultData = (float *) result->data;
    float *aData = (float *) a_broad->data;
    float *bData = (float *) b_broad->data;
    int numElements = a_broad->descriptor->numElements;
    NDArrayIterator_INIT(result);
    if (NDArray_DEVICE(a_broad) == NDARRAY_DEVICE_GPU && NDArray_DEVICE(b_broad) == NDARRAY_DEVICE_GPU) {
#if HAVE_CUBLAS
        cuda_multiply_float(NDArray_NUMELEMENTS(a_broad), NDArray_FDATA(a_broad), NDArray_FDATA(b_broad), NDArray_FDATA(result),
                            NDArray_NUMELEMENTS(a_broad));
        result->device = NDARRAY_DEVICE_GPU;
#endif
    } else {
#ifdef HAVE_AVX2
        int i = 0;
        __m256 vec1, vec2, mul;

        for (; i < NDArray_NUMELEMENTS(a) - 7; i += 8) {
            vec1 = _mm256_loadu_ps(&aData[i]);
            vec2 = _mm256_loadu_ps(&bData[i]);
            mul = _mm256_mul_ps(vec1, vec2);
            mul = fix_negative_zero(mul); // Fix any -0.0 results
            _mm256_storeu_ps(&resultData[i], mul);
        }

        // Handle remaining elements if the length is not a multiple of 4
        for (; i < numElements; i++) {
            resultData[i] = aData[i] * bData[i];
            if (resultData[i] == 0.0f && signbit(resultData[i])) {
                resultData[i] = 0.0f;
            }
        }
#else
        for (int i = 0; i < numElements; i++) {
            resultData[i] = aData[i] * bData[i];
        }
#endif
    }
    if (a_temp != NULL) {
        NDArray_FREE(a);
    }
    if (b_temp != NULL) {
        NDArray_FREE(b);
    }
    if (broadcasted != NULL) {
        NDArray_FREE(broadcasted);
    }
    return result;
}

/**
 * Subtract elements of a and b element-wise
 *
 * @param a
 * @param b
 * @return
 */
NDArray*
NDArray_Subtract_Float(NDArray* a, NDArray* b) {
    NDArray *a_temp = NULL, *b_temp = NULL;
    if (NDArray_DEVICE(a) != NDArray_DEVICE(b) && NDArray_NDIM(a) != 0 && NDArray_NDIM(b) != 0) {
        zend_throw_error(NULL, "Device mismatch, both NDArray MUST be in the same device.");
        return NULL;
    }

    // If a or b are scalars, reshape
    if (NDArray_NDIM(a) == 0 && NDArray_NDIM(b) > 0) {
        a_temp = a;
        int *n_shape = emalloc(sizeof(int) * NDArray_NDIM(b));
        copy(NDArray_SHAPE(b), n_shape, NDArray_NDIM(b));
        a = NDArray_Zeros(n_shape, NDArray_NDIM(b), NDArray_TYPE(b), NDArray_DEVICE(b));
        a = NDArray_Fill(a, NDArray_FDATA(a_temp)[0]);
    } else if (NDArray_NDIM(b) == 0 && NDArray_NDIM(a) > 0) {
        b_temp = b;
        int *n_shape = emalloc(sizeof(int) * NDArray_NDIM(a));
        copy(NDArray_SHAPE(a), n_shape, NDArray_NDIM(a));
        b = NDArray_Zeros(n_shape, NDArray_NDIM(a), NDArray_TYPE(a), NDArray_DEVICE(a));
        b = NDArray_Fill(b, NDArray_FDATA(b_temp)[0]);
    }

    NDArray *broadcasted = NULL;
    NDArray *a_broad = NULL, *b_broad = NULL;

    if (NDArray_NUMELEMENTS(a) < NDArray_NUMELEMENTS(b)) {
        broadcasted = NDArray_Broadcast(a, b);
        a_broad = broadcasted;
        b_broad = b;
    } else if (NDArray_NUMELEMENTS(b) < NDArray_NUMELEMENTS(a)) {
        broadcasted = NDArray_Broadcast(b, a);
        b_broad = broadcasted;
        a_broad = a;
    } else {
        b_broad = b;
        a_broad = a;
    }

    if (b_broad == NULL || a_broad == NULL) {
        zend_throw_error(NULL, "Can't broadcast arrays.");
        return NULL;
    }

    // Check if the element size of the input arrays match
    if (a->descriptor->elsize != sizeof(float) || b->descriptor->elsize != sizeof(float)) {
        // Element size mismatch, return an error or handle it accordingly
        return NULL;
    }

    // Create a new NDArray to store the result
    NDArray *result = (NDArray *) emalloc(sizeof(NDArray));
    result->strides = (int *) emalloc(a_broad->ndim * sizeof(int));
    result->dimensions = (int *) emalloc(a_broad->ndim * sizeof(int));
    result->ndim = a_broad->ndim;
    if (NDArray_DEVICE(a_broad) == NDARRAY_DEVICE_GPU) {
#if HAVE_CUBLAS
        vmalloc((void **) &result->data, NDArray_NUMELEMENTS(a_broad) * sizeof(float));
        cudaDeviceSynchronize();
        result->device = NDARRAY_DEVICE_GPU;
#endif
    } else {
        result->data = (char *) emalloc(a_broad->descriptor->numElements * sizeof(float));
    }
    result->base = NULL;
    result->flags = 0;  // Set appropriate flags
    result->descriptor = (NDArrayDescriptor *) emalloc(sizeof(NDArrayDescriptor));
    result->descriptor->type = NDARRAY_TYPE_FLOAT32;
    result->descriptor->elsize = sizeof(float);
    result->descriptor->numElements = a_broad->descriptor->numElements;
    result->refcount = 1;
    result->device = NDArray_DEVICE(a_broad);

    // Perform element-wise subtraction
    result->strides = memcpy(result->strides, a_broad->strides, a_broad->ndim * sizeof(int));
    result->dimensions = memcpy(result->dimensions, a_broad->dimensions, a_broad->ndim * sizeof(int));
    float *resultData = (float *) result->data;
    float *aData = (float *) a_broad->data;
    float *bData = (float *) b_broad->data;
    int numElements = a_broad->descriptor->numElements;
    NDArrayIterator_INIT(result);
    if (NDArray_DEVICE(a_broad) == NDARRAY_DEVICE_GPU && NDArray_DEVICE(b_broad) == NDARRAY_DEVICE_GPU) {
#if HAVE_CUBLAS
        cuda_subtract_float(NDArray_NUMELEMENTS(a_broad), NDArray_FDATA(a_broad), NDArray_FDATA(b_broad), NDArray_FDATA(result),
                            NDArray_NUMELEMENTS(a_broad));
#endif
    } else {
#ifdef HAVE_AVX2
        int i;
        __m256 vec1, vec2, sub;

        for (i = 0; i < NDArray_NUMELEMENTS(a) - 7; i += 8) {
            vec1 = _mm256_loadu_ps(&aData[i]);
            vec2 = _mm256_loadu_ps(&bData[i]);
            sub = _mm256_sub_ps(vec1, vec2);
            _mm256_storeu_ps(&resultData[i], sub);
        }

        // Handle remaining elements if the length is not a multiple of 4
        for (; i < numElements; i++) {
            resultData[i] = aData[i] - bData[i];
        }
#else
        for (int i = 0; i < numElements; i++) {
            resultData[i] = aData[i] - bData[i];
        }
#endif
    }
    if (a_temp != NULL) {
        NDArray_FREE(a);
    }
    if (b_temp != NULL) {
        NDArray_FREE(b);
    }
    if (broadcasted != NULL) {
        NDArray_FREE(broadcasted);
    }
    return result;
}

/**
 * Divide elements of a and b element-wise
 *
 * @param a
 * @param b
 * @return
 */
NDArray*
NDArray_Divide_Float(NDArray* a, NDArray* b) {
    NDArray *a_temp = NULL, *b_temp = NULL;

    if (NDArray_DEVICE(a) != NDArray_DEVICE(b) && NDArray_NDIM(a) != 0 && NDArray_NDIM(b) != 0) {
        zend_throw_error(NULL, "Device mismatch, both NDArray MUST be in the same device.");
        return NULL;
    }

    if (NDArray_NDIM(a) == 0 && NDArray_NDIM(b) == 0) {
        int *shape = ecalloc(1, sizeof(int));
        NDArray *rtn = NDArray_Zeros(shape, 0, NDARRAY_TYPE_FLOAT32, NDArray_DEVICE(a));
        NDArray_FDATA(rtn)[0] = NDArray_FDATA(a)[0] / NDArray_FDATA(b)[0];
        return rtn;
    }

    // If a or b are scalars, reshape
    if (NDArray_NDIM(a) == 0 && NDArray_NDIM(b) > 0) {
        a_temp = a;
        int *n_shape = emalloc(sizeof(int) * NDArray_NDIM(b));
        copy(NDArray_SHAPE(b), n_shape, NDArray_NDIM(b));
        a = NDArray_Zeros(n_shape, NDArray_NDIM(b), NDArray_TYPE(b), NDArray_DEVICE(b));
        a = NDArray_Fill(a, NDArray_FDATA(a_temp)[0]);
    } else if (NDArray_NDIM(b) == 0 && NDArray_NDIM(a) > 0) {
        b_temp = b;
        int *n_shape = emalloc(sizeof(int) * NDArray_NDIM(a));
        copy(NDArray_SHAPE(a), n_shape, NDArray_NDIM(a));
        b = NDArray_Zeros(n_shape, NDArray_NDIM(a), NDArray_TYPE(a), NDArray_DEVICE(a));
        b = NDArray_Fill(b, NDArray_FDATA(b_temp)[0]);
    }

    NDArray *broadcasted = NULL;
    NDArray *a_broad = NULL, *b_broad = NULL;

    if (NDArray_NUMELEMENTS(a) < NDArray_NUMELEMENTS(b)) {
        broadcasted = NDArray_Broadcast(a, b);
        a_broad = broadcasted;
        b_broad = b;
    } else if (NDArray_NUMELEMENTS(b) < NDArray_NUMELEMENTS(a)) {
        broadcasted = NDArray_Broadcast(b, a);
        b_broad = broadcasted;
        a_broad = a;
    } else {
        b_broad = b;
        a_broad = a;
    }

    if (b_broad == NULL || a_broad == NULL) {
        zend_throw_error(NULL, "Can't broadcast arrays.");
        return NULL;
    }

    // Check if the element size of the input arrays match
    if (a->descriptor->elsize != sizeof(float) || b->descriptor->elsize != sizeof(float)) {
        // Element size mismatch, return an error or handle it accordingly
        return NULL;
    }

    // Create a new NDArray to store the result
    NDArray *result = (NDArray *) emalloc(sizeof(NDArray));
    result->strides = (int *) emalloc(a_broad->ndim * sizeof(int));
    result->dimensions = (int *) emalloc(a_broad->ndim * sizeof(int));
    result->device = NDArray_DEVICE(a_broad);
    result->ndim = a_broad->ndim;
    if (NDArray_DEVICE(a_broad) == NDARRAY_DEVICE_GPU) {
#if HAVE_CUBLAS
        vmalloc((void **) &result->data, NDArray_NUMELEMENTS(a_broad) * sizeof(float));
        cudaDeviceSynchronize();
        result->device = NDARRAY_DEVICE_GPU;
#endif
    } else {
        result->data = (char *) emalloc(a_broad->descriptor->numElements * sizeof(float));
    }
    result->base = NULL;
    result->flags = 0;  // Set appropriate flags
    result->descriptor = (NDArrayDescriptor *) emalloc(sizeof(NDArrayDescriptor));
    result->descriptor->type = NDARRAY_TYPE_FLOAT32;
    result->descriptor->elsize = sizeof(float);
    result->device = NDArray_DEVICE(a_broad);
    result->descriptor->numElements = a_broad->descriptor->numElements;
    result->refcount = 1;

    // Perform element-wise division
    result->strides = memcpy(result->strides, a_broad->strides, a_broad->ndim * sizeof(int));
    result->dimensions = memcpy(result->dimensions, a_broad->dimensions, a_broad->ndim * sizeof(int));
    float *resultData = (float *) result->data;
    float *aData = (float *) a_broad->data;
    float *bData = (float *) b_broad->data;
    int numElements = a_broad->descriptor->numElements;
    NDArrayIterator_INIT(result);
    if (NDArray_DEVICE(a_broad) == NDARRAY_DEVICE_GPU && NDArray_DEVICE(b_broad) == NDARRAY_DEVICE_GPU) {
#if HAVE_CUBLAS
        cuda_divide_float(NDArray_NUMELEMENTS(a_broad), NDArray_FDATA(a_broad), NDArray_FDATA(b_broad), NDArray_FDATA(result),
                          NDArray_NUMELEMENTS(a_broad));
#endif
    } else {
#ifdef HAVE_AVX2
        int i;
        __m256 vec1, vec2, sub;

        for (i = 0; i < NDArray_NUMELEMENTS(a) - 7; i += 8) {
            vec1 = _mm256_loadu_ps(&aData[i]);
            vec2 = _mm256_loadu_ps(&bData[i]);
            sub = _mm256_div_ps(vec1, vec2);
            _mm256_storeu_ps(&resultData[i], sub);
        }

        // Handle remaining elements if the length is not a multiple of 4
        for (; i < numElements; i++) {
            resultData[i] = aData[i] / bData[i];
        }
#else
        for (int i = 0; i < numElements; i++) {
            resultData[i] = aData[i] / bData[i];
        }
#endif
    }
    if (a_temp != NULL) {
        NDArray_FREE(a);
    }
    if (b_temp != NULL) {
        NDArray_FREE(b);
    }
    if (broadcasted != NULL) {
        NDArray_FREE(broadcasted);
    }
    return result;
}

/**
 * @param a
 * @param b
 * @return
 */
NDArray*
NDArray_Mod_Float(NDArray* a, NDArray* b) {
    NDArray *a_temp = NULL, *b_temp = NULL;
    if (NDArray_DEVICE(a) != NDArray_DEVICE(b) && NDArray_NDIM(a) != 0 && NDArray_NDIM(b) != 0) {
        zend_throw_error(NULL, "Device mismatch, both NDArray MUST be in the same device.");
        return NULL;
    }

    // If a or b are scalars, reshape
    if (NDArray_NDIM(a) == 0 && NDArray_NDIM(b) > 0) {
        a_temp = a;
        int *n_shape = emalloc(sizeof(int) * NDArray_NDIM(b));
        copy(NDArray_SHAPE(b), n_shape, NDArray_NDIM(b));
        a = NDArray_Zeros(n_shape, NDArray_NDIM(b), NDArray_TYPE(b), NDArray_DEVICE(b));
        a = NDArray_Fill(a, NDArray_FDATA(a_temp)[0]);
    } else if (NDArray_NDIM(b) == 0 && NDArray_NDIM(a) > 0) {
        b_temp = b;
        int *n_shape = emalloc(sizeof(int) * NDArray_NDIM(a));
        copy(NDArray_SHAPE(a), n_shape, NDArray_NDIM(a));
        b = NDArray_Zeros(n_shape, NDArray_NDIM(a), NDArray_TYPE(a), NDArray_DEVICE(a));
        b = NDArray_Fill(b, NDArray_FDATA(b_temp)[0]);
    }

    NDArray *broadcasted = NULL;
    NDArray *a_broad = NULL, *b_broad = NULL;

    if (NDArray_NUMELEMENTS(a) < NDArray_NUMELEMENTS(b)) {
        broadcasted = NDArray_Broadcast(a, b);
        a_broad = broadcasted;
        b_broad = b;
    } else if (NDArray_NUMELEMENTS(b) < NDArray_NUMELEMENTS(a)) {
        broadcasted = NDArray_Broadcast(b, a);
        b_broad = broadcasted;
        a_broad = a;
    } else {
        b_broad = b;
        a_broad = a;
    }

    if (b_broad == NULL || a_broad == NULL) {
        zend_throw_error(NULL, "Can't broadcast arrays.");
        return NULL;
    }

    // Check if the element size of the input arrays match
    if (a->descriptor->elsize != sizeof(float) || b->descriptor->elsize != sizeof(float)) {
        // Element size mismatch, return an error or handle it accordingly
        return NULL;
    }

    // Create a new NDArray to store the result
    NDArray *result = (NDArray *) emalloc(sizeof(NDArray));
    result->strides = (int *) emalloc(a_broad->ndim * sizeof(int));
    result->dimensions = (int *) emalloc(a_broad->ndim * sizeof(int));
    result->ndim = a_broad->ndim;
    if (NDArray_DEVICE(a_broad) == NDARRAY_DEVICE_GPU) {
#if HAVE_CUBLAS
        vmalloc((void **) &result->data, NDArray_NUMELEMENTS(a_broad) * sizeof(float));
        cudaDeviceSynchronize();
        result->device = NDARRAY_DEVICE_GPU;
#endif
    } else {
        result->data = (char *) emalloc(a_broad->descriptor->numElements * sizeof(float));
    }
    result->base = NULL;
    result->flags = 0;  // Set appropriate flags
    result->descriptor = (NDArrayDescriptor *) emalloc(sizeof(NDArrayDescriptor));
    result->descriptor->type = NDARRAY_TYPE_FLOAT32;
    result->descriptor->elsize = sizeof(float);
    result->descriptor->numElements = a_broad->descriptor->numElements;
    result->refcount = 1;
    result->device = NDArray_DEVICE(a_broad);

    // Perform element-wise subtraction
    result->strides = memcpy(result->strides, a_broad->strides, a_broad->ndim * sizeof(int));
    result->dimensions = memcpy(result->dimensions, a_broad->dimensions, a_broad->ndim * sizeof(int));
    float *resultData = (float *) result->data;
    float *aData = (float *) a_broad->data;
    float *bData = (float *) b_broad->data;
    int numElements = a_broad->descriptor->numElements;
    NDArrayIterator_INIT(result);
    if (NDArray_DEVICE(a_broad) == NDARRAY_DEVICE_GPU && NDArray_DEVICE(b_broad) == NDARRAY_DEVICE_GPU) {
#if HAVE_CUBLAS
        cuda_mod_float(NDArray_NUMELEMENTS(a_broad), NDArray_FDATA(a_broad), NDArray_FDATA(b_broad), NDArray_FDATA(result),
                       NDArray_NUMELEMENTS(a_broad));
#endif
    } else {
#ifdef HAVE_AVX2
        int i;
        __m256 vec1, vec2, vout;

        for (i = 0; i < NDArray_NUMELEMENTS(a) - 7; i += 8) {
            vec1 = _mm256_loadu_ps(&aData[i]);
            vec2 = _mm256_loadu_ps(&bData[i]);
            vout = _mm256_sub_ps(vec1, _mm256_mul_ps(_mm256_floor_ps(_mm256_div_ps(vec1, vec2)), vec2));
            _mm256_storeu_ps(&resultData[i], vout);
        }

        // Handle remaining elements if the length is not a multiple of 4
        for (; i < numElements; i++) {
            resultData[i] = fmodf(aData[i], bData[i]);
        }
#else
        for (int i = 0; i < numElements; i++) {
            resultData[i] = fmodf(aData[i], bData[i]);
        }
#endif
    }
    if (a_temp != NULL) {
        NDArray_FREE(a);
    }
    if (b_temp != NULL) {
        NDArray_FREE(b);
    }
    if (broadcasted != NULL) {
        NDArray_FREE(broadcasted);
    }
    return result;
}

/**
 * @param a
 * @param b
 * @return
 */
NDArray*
NDArray_Pow_Float(NDArray* a, NDArray* b) {
    NDArray *a_temp = NULL, *b_temp = NULL;
    if (NDArray_DEVICE(a) != NDArray_DEVICE(b) && NDArray_NDIM(a) != 0 && NDArray_NDIM(b) != 0) {
        zend_throw_error(NULL, "Device mismatch, both NDArray MUST be in the same device.");
        return NULL;
    }

    // If a or b are scalars, reshape
    if (NDArray_NDIM(a) == 0 && NDArray_NDIM(b) > 0) {
        a_temp = a;
        int *n_shape = emalloc(sizeof(int) * NDArray_NDIM(b));
        copy(NDArray_SHAPE(b), n_shape, NDArray_NDIM(b));
        a = NDArray_Zeros(n_shape, NDArray_NDIM(b), NDArray_TYPE(b), NDArray_DEVICE(b));
        a = NDArray_Fill(a, NDArray_FDATA(a_temp)[0]);
    } else if (NDArray_NDIM(b) == 0 && NDArray_NDIM(a) > 0) {
        b_temp = b;
        int *n_shape = emalloc(sizeof(int) * NDArray_NDIM(a));
        copy(NDArray_SHAPE(a), n_shape, NDArray_NDIM(a));
        b = NDArray_Zeros(n_shape, NDArray_NDIM(a), NDArray_TYPE(a), NDArray_DEVICE(a));
        b = NDArray_Fill(b, NDArray_FDATA(b_temp)[0]);
    }

    NDArray *broadcasted = NULL;
    NDArray *a_broad = NULL, *b_broad = NULL;

    if (NDArray_NUMELEMENTS(a) < NDArray_NUMELEMENTS(b)) {
        broadcasted = NDArray_Broadcast(a, b);
        a_broad = broadcasted;
        b_broad = b;
    } else if (NDArray_NUMELEMENTS(b) < NDArray_NUMELEMENTS(a)) {
        broadcasted = NDArray_Broadcast(b, a);
        b_broad = broadcasted;
        a_broad = a;
    } else {
        b_broad = b;
        a_broad = a;
    }

    if (b_broad == NULL || a_broad == NULL) {
        zend_throw_error(NULL, "Can't broadcast arrays.");
        return NULL;
    }

    // Check if the element size of the input arrays match
    if (a->descriptor->elsize != sizeof(float) || b->descriptor->elsize != sizeof(float)) {
        // Element size mismatch, return an error or handle it accordingly
        return NULL;
    }

    // Create a new NDArray to store the result
    NDArray *result = (NDArray *) emalloc(sizeof(NDArray));
    result->strides = (int *) emalloc(a_broad->ndim * sizeof(int));
    result->dimensions = (int *) emalloc(a_broad->ndim * sizeof(int));
    result->ndim = a_broad->ndim;
    if (NDArray_DEVICE(a_broad) == NDARRAY_DEVICE_GPU) {
#if HAVE_CUBLAS
        vmalloc((void **) &result->data, NDArray_NUMELEMENTS(a_broad) * sizeof(float));
        cudaDeviceSynchronize();
        result->device = NDARRAY_DEVICE_GPU;
#endif
    } else {
        result->data = (char *) emalloc(a_broad->descriptor->numElements * sizeof(float));
    }
    result->base = NULL;
    result->flags = 0;  // Set appropriate flags
    result->descriptor = (NDArrayDescriptor *) emalloc(sizeof(NDArrayDescriptor));
    result->descriptor->type = NDARRAY_TYPE_FLOAT32;
    result->descriptor->elsize = sizeof(float);
    result->descriptor->numElements = a_broad->descriptor->numElements;
    result->refcount = 1;
    result->device = NDArray_DEVICE(a_broad);

    // Perform element-wise subtraction
    result->strides = memcpy(result->strides, a_broad->strides, a_broad->ndim * sizeof(int));
    result->dimensions = memcpy(result->dimensions, a_broad->dimensions, a_broad->ndim * sizeof(int));
    float *resultData = (float *) result->data;
    float *aData = (float *) a_broad->data;
    float *bData = (float *) b_broad->data;
    int numElements = a_broad->descriptor->numElements;
    NDArrayIterator_INIT(result);
    if (NDArray_DEVICE(a_broad) == NDARRAY_DEVICE_GPU && NDArray_DEVICE(b_broad) == NDARRAY_DEVICE_GPU) {
#if HAVE_CUBLAS
        cuda_pow_float(NDArray_NUMELEMENTS(a_broad), NDArray_FDATA(a_broad), NDArray_FDATA(b_broad), NDArray_FDATA(result),
                       NDArray_NUMELEMENTS(a_broad));
#endif
    } else {
        for (int i = 0; i < numElements; i++) {
            resultData[i] = powf(aData[i], bData[i]);
        }
    }
    if (a_temp != NULL) {
        NDArray_FREE(a);
    }
    if (b_temp != NULL) {
        NDArray_FREE(b);
    }
    if (broadcasted != NULL) {
        NDArray_FREE(broadcasted);
    }
    return result;
}

/**
 * NDArray::abs
 *
 * @param nda
 * @return
 */
NDArray*
NDArray_Abs(NDArray *nda) {
    NDArray *rtn = NULL;
    if (NDArray_DEVICE(nda) == NDARRAY_DEVICE_CPU) {
        rtn = NDArray_Map(nda, float_abs);
    } else {
#ifdef HAVE_CUBLAS
        rtn = NDArrayMathGPU_ElementWise(nda, cuda_float_abs);
#else
        zend_throw_error(NULL, "GPU operations unavailable. CUBLAS not detected.");
#endif
    }
    return rtn;
}

NDArray*
NDArray_Cum_Flat(NDArray *a, ElementWiseFloatOperation1F op) {
    NDArray *rtn = NDArray_Copy(a, NDArray_DEVICE(a));
    int num_elements = NDArray_NUMELEMENTS(rtn);
    int *flat_shape = emalloc(sizeof(int) * 2);
    flat_shape[0] = 1;
    flat_shape[1] = num_elements;
    rtn = NDArray_Reshape(rtn, flat_shape, 2);
    for (int i = 1; i < num_elements; i++) {
        NDArray_FDATA(rtn)[i] = op(NDArray_FDATA(rtn)[i], NDArray_FDATA(rtn)[i - 1]);
    }
    return rtn;
}

NDArray*
NDArray_Cum_Axis(NDArray *a, int *axis, NDArray *(*operation)(NDArray *, NDArray *)) {
    if (*axis == 1) {
        a = NDArray_Transpose(a, NULL);
    }
    
    int rows = NDArray_SHAPE(a)[0];
    int cols = NDArray_SHAPE(a)[1];
    // setup indices for slicing
    int *indices_shape = emalloc(sizeof(int) * 2);
    indices_shape[0] = 2;
    indices_shape[1] = 1;
    NDArray** indices_axis = emalloc(sizeof(NDArray*) * 2);
    indices_axis[0] =  NDArray_Zeros(indices_shape, 1, NDARRAY_TYPE_FLOAT32, NDARRAY_DEVICE_CPU);
    indices_axis[1] =  NDArray_Zeros(indices_shape, 1, NDARRAY_TYPE_FLOAT32, NDARRAY_DEVICE_CPU);
    NDArray **row_array = emalloc(sizeof(NDArray*) * rows);
    // Set Column
    NDArray_FDATA(indices_axis[1])[0] = 0;
    NDArray_FDATA(indices_axis[1])[1] = cols;
    for (int i = 0; i < rows; i++) {
        // Set Row
        NDArray_FDATA(indices_axis[0])[0] = i;
        NDArray_FDATA(indices_axis[0])[1] = i + 1;
        if (i == 0) {
            row_array[i] = NDArray_Slice(a, indices_axis, 2);
        } else {
            NDArray *curr_row = NDArray_Slice(a, indices_axis, 2);
            row_array[i] = operation(row_array[i-1], curr_row);
            NDArray_FREE(curr_row);
        }
    }
    efree(indices_axis[0]);
    efree(indices_axis[1]);
    efree(indices_axis);
    efree(indices_shape);
    NDArray *combined = NDArray_Concatenate(row_array, rows, 0);
    for (int i = 0; i < rows; i++) {
        NDArray_FREE(row_array[i]);
    }
    efree(row_array);
    if (*axis == 1) {
        combined = NDArray_Transpose(combined, NULL);
    }
    return combined;
}

/**
 * NDArray::cumprod
 *
 * @param a
 * @param axis
 * @return
 */
NDArray*
NDArray_Cum_Prod(NDArray *a, int *axis) {
    NDArray *rtn = NULL;
    
    if (*axis == -1) {
        rtn = NDArray_Cum_Flat(a, float_product);
    } else {
        rtn = NDArray_Cum_Axis(a, axis, NDArray_Multiply_Float);
    }

    return rtn;
}

/**
 * NDArray::cumsum
 *
 * @param a
 * @param axis
 * @return
 */
NDArray*
NDArray_Cum_Sum(NDArray *a, int *axis) {
    NDArray *rtn = NULL;

    if (*axis == -1) {
        rtn = NDArray_Cum_Flat(a, float_sum);
    } else {
        rtn = NDArray_Cum_Axis(a, axis, NDArray_Add_Float);
    }

    return rtn;
}


