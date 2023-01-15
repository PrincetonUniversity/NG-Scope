#ifndef NGSCOPE_UTIL_HH
#define NGSCOPE_UTIL_HH
#include <assert.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <poll.h>
#include <time.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif


#define MAX_TTI 10240

void quick_sort_uint64(uint64_t* array, int array_size);
void quick_sort_uint16(uint16_t* array, int array_size);

//Mean sum of array
uint16_t sum_array_uint16(uint16_t* array, int array_size);
uint64_t sum_array_uint64(uint64_t* array, int array_size);
uint16_t mean_array_uint16(uint16_t* array, int array_size);
uint64_t mean_array_uint64(uint64_t* array, int array_size);

// Find the min/max value in an array 
// --> return the max/min value
uint64_t min_in_array_uint64_v(uint64_t* array, int array_size);
uint64_t max_in_array_uint64_v(uint64_t* array, int array_size);
uint16_t min_in_array_uint16_v(uint16_t* array, int array_size);
uint16_t max_in_array_uint16_v(uint16_t* array, int array_size);

// Find the min/max value in an array
// --> return both the max/min value and their index
void min_in_array_uint64_vi(uint64_t* array, int array_size, uint64_t* val, int* idx);
void max_in_array_uint64_vi(uint64_t* array, int array_size, uint64_t* val, int* idx);
void min_in_array_uint16_vi(uint16_t* array, int array_size, uint16_t* val, int* idx);
void max_in_array_uint16_vi(uint16_t* array, int array_size, uint16_t* val, int* idx);

int find_element_in_array_uint16(uint16_t* array, int array_size, uint16_t ele);
int find_element_in_array_uint64(uint64_t* array, int array_size, uint64_t ele);
	
// TTI related operation 
uint16_t wrap_tti(uint16_t tti);
void unwrap_tti_array(uint16_t* array, int array_size);
uint16_t max_tti_in_array(uint16_t* array, int array_size);

int tti_distance(uint16_t start, uint16_t end);
int tti_difference(uint16_t a, uint16_t b);

bool tti_a_l_b(uint16_t a, uint16_t b);
bool tti_a_le_b(uint16_t a, uint16_t b);
bool tti_a_s_b(uint16_t a, uint16_t b);
bool tti_a_se_b(uint16_t a, uint16_t b);
	
// Array related operation
uint64_t nearest_neighbour(uint64_t* array, int array_size, uint64_t num);
int nearest_neighbour_index(uint64_t* array, int array_size, uint64_t num);
uint64_t nearest_neighbour_w_offset(uint64_t* array, int array_size, uint64_t num, int offset);
long nearest_neighbour_dist_w_offset(uint64_t* array, int array_size, uint64_t num, int offset);

long correlate_2_vec(uint64_t* array1, uint64_t* array2, int array_size1, int array_size2);
long correlate_2_vec_w_offset(uint64_t* array1, uint64_t* array2, int array_size1, int array_size2, int offset);
uint64_t* shift_vec_w_offset(uint64_t* array, int array_size, int offset);


// shift the array from index 
void shift_array_uint16_right(uint16_t* array, int size, int index);
void shift_array_uint16_left(uint16_t* array, int size, int index);
void shift_array_uint32_right(uint32_t* array, int size, int index);
void shift_array_uint32_left(uint32_t* array, int size, int index);

void swap_array_uint16(uint16_t* array, int size, int idx1, int idx2);
void swap_array_uint32(uint32_t* array, int size, int idx1, int idx2);

#ifdef __cplusplus
}
#endif


#endif
