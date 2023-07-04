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
#include <stdbool.h>

#include "ngscope/hdr/dciLib/ngscope_util.h"

/************   Quick Sorting of int ************/
int cmpfunc_uint64(const void * a, const void * b) {
   return ( *(uint64_t*)a - *(uint64_t*)b );
}

void quick_sort_uint64(uint64_t* array, int array_size){
	qsort(array, array_size, sizeof(uint64_t), cmpfunc_uint64);
	return;
}

int cmpfunc_uint16(const void * a, const void * b) {
   return ( *(uint16_t*)a - *(uint16_t*)b );
}

void quick_sort_uint16(uint16_t* array, int array_size){
	qsort(array, array_size, sizeof(uint16_t), cmpfunc_uint16);
	return;
}

/*********** Average ***************/
uint16_t sum_array_uint16(uint16_t* array, int array_size){
	uint16_t sum = 0;
	for(int i=0; i<array_size; i++){
		sum += array[i];
	}
	return sum;
}
uint64_t sum_array_uint64(uint64_t* array, int array_size){
	uint64_t sum = 0;
	for(int i=0; i<array_size; i++){
		sum += array[i];
	}
	return sum;
}

uint16_t mean_array_uint16(uint16_t* array, int array_size){
	return sum_array_uint16(array, array_size) / array_size;
}

uint64_t mean_array_uint64(uint64_t* array, int array_size){
	return sum_array_uint64(array, array_size) / array_size;
}

/*********** Max Min in Array  ***************/
uint64_t min_in_array_uint64_v(uint64_t* array, int array_size){
	uint64_t min_v = array[0];
	for(int i=0; i<array_size; i++){
		if(array[i] < min_v){
			min_v = array[i];
		}
	}
	return min_v;
}

uint64_t max_in_array_uint64_v(uint64_t* array, int array_size){
	uint64_t max_v = array[0];
	for(int i=0; i<array_size; i++){
		if(array[i] > max_v){
			max_v = array[i];
		}
	}
	return max_v;
}

uint16_t min_in_array_uint16_v(uint16_t* array, int array_size){
	uint16_t min_v = array[0];
	for(int i=0; i<array_size; i++){
		if(array[i] < min_v){
			min_v = array[i];
		}
	}
	return min_v;
}

uint16_t max_in_array_uint16_v(uint16_t* array, int array_size){
	uint16_t max_v = array[0];
	for(int i=0; i<array_size; i++){
		if(array[i] > max_v){
			max_v = array[i];
		}
	}
	return max_v;
}


void min_in_array_uint64_vi(uint64_t* array, int array_size, uint64_t* val, int* idx){
	uint64_t min_v = array[0];
	int min_i = 0;
	for(int i=0; i<array_size; i++){
		if(array[i] < min_v){
			min_v = array[i];
			min_i = i;
		}
	}
	*val = min_v;
	*idx = min_i;
	return;
}

void max_in_array_uint64_vi(uint64_t* array, int array_size, uint64_t* val, int* idx){
	uint64_t max_v = array[0];
	int max_i = 0;
	for(int i=0; i<array_size; i++){
		if(array[i] > max_v){
			max_v = array[i];
			max_i = i;
		}
	}
	*val = max_v;
	*idx = max_i;
	return;
}
void min_in_array_uint16_vi(uint16_t* array, int array_size, uint16_t* val, int* idx){
	uint16_t min_v = array[0];
	int min_i = 0;
	for(int i=0; i<array_size; i++){
		if(array[i] < min_v){
			min_v = array[i];
			min_i = i;
		}
	}
	*val = min_v;
	*idx = min_i;
	return;
}

void max_in_array_uint16_vi(uint16_t* array, int array_size, uint16_t* val, int* idx){
	uint16_t max_v = array[0];
	int max_i = 0;
	for(int i=0; i<array_size; i++){
		if(array[i] > max_v){
			max_v = array[i];
			max_i = i;
		}
	}
	*val = max_v;
	*idx = max_i;
	return;
}

int find_element_in_array_uint16(uint16_t* array, int array_size, uint16_t ele){
	for(int i=0; i<array_size; i++){
		if(ele == array[i]){
			return i;
		}
	}
	return -1;
}

int find_element_in_array_uint64(uint64_t* array, int array_size, uint64_t ele){
	for(int i=0; i<array_size; i++){
		if(ele == array[i]){
			return i;
		}
	}
	return -1;
}



/****************** Un-wrap the TTI  ******************/
uint16_t wrap_tti(uint16_t tti){
	return tti % MAX_TTI;
}

void unwrap_tti_array(uint16_t* array, int array_size){
	if(array_size == 1){
		return;
	}
	uint16_t max_tti = max_in_array_uint16_v(array, array_size);
	for(int i=0; i<array_size; i++){
		if( abs(max_tti -array[i]) >  abs(max_tti -(array[i] +  MAX_TTI))){
			array[i] +=  MAX_TTI;
		}
	}

}
uint16_t max_tti_in_array(uint16_t* array, int array_size){
	if(array_size == 1){
		return array[0];
	}else if(array_size <=0){
		printf("WRONG ARRAY SIZE!\n");
		return 0;
	}

	unwrap_tti_array(array, array_size);

	// Sort the TTI array 
	quick_sort_uint16(array, array_size);

	// Max tti in the array
	uint16_t max_tti = max_in_array_uint16_v(array, array_size);

	return wrap_tti(max_tti);
}
/********************* TTI related operation ******************/
int tti_distance(uint16_t start, uint16_t end){
	if(start < end){
		return (end - start + 1);
	}else{
		return (end + MAX_TTI - start + 1);
	}
}

int tti_difference(uint16_t a, uint16_t b){
	uint16_t array[] = {a, b};		
	unwrap_tti_array(array, 2);
	int diff = abs((int) array[0] - (int)array[1]);
	return diff;
}
// tti compare
bool tti_a_l_b(uint16_t a, uint16_t b){
	uint16_t array[] = {a, b};
	unwrap_tti_array(array, 2);
	return (array[0] > array[1]) ? true:false;
}
bool tti_a_le_b(uint16_t a, uint16_t b){
	uint16_t array[] = {a, b};
	unwrap_tti_array(array, 2);
	return (array[0] >= array[1]) ? true:false;
}
bool tti_a_s_b(uint16_t a, uint16_t b){
	uint16_t array[] = {a, b};
	unwrap_tti_array(array, 2);
	return (array[0] < array[1]) ? true:false;
}

bool tti_a_se_b(uint16_t a, uint16_t b){
	uint16_t array[] = {a, b};
	unwrap_tti_array(array, 2);
	return (array[0] <= array[1]) ? true:false;
}

// find the nearest neighbour inside an array
uint64_t nearest_neighbour(uint64_t* array, int array_size, uint64_t num){
	long dist_min;
	int  idx =0;
	for(int i=0; i< array_size; i++){
		long dist_tmp = abs( (long)array[i] - (long)num);
		if(i==0){
			dist_min = dist_tmp;
		}else{
			if(dist_tmp < dist_min){
				dist_min = dist_tmp;
				idx 	= i;
			}
		}
	}
	return array[idx];
}

int nearest_neighbour_index(uint64_t* array, int array_size, uint64_t num){
	long dist_min;
	int  idx =0;
	for(int i=0; i< array_size; i++){
		long dist_tmp = abs( (long)array[i] - (long)num);
		if(i==0){
			dist_min = dist_tmp;
		}else{
			if(dist_tmp < dist_min){
				dist_min = dist_tmp;
				idx 	= i;
			}
		}
	}
	return idx;
}
// the same function but with offset
uint64_t nearest_neighbour_w_offset(uint64_t* array, int array_size, uint64_t num, int offset){
	long dist_min;
	int  idx =0;
	for(int i=0; i< array_size; i++){
		long dist_tmp = abs( ((long)array[i] + offset) - (long)num);
		if(i==0){
			dist_min = dist_tmp;
		}else{
			if(dist_tmp < dist_min){
				dist_min = dist_tmp;
				idx 	= i;
			}
		}
	}
	return array[idx];
}

// the same function but with offset
long nearest_neighbour_dist_w_offset(uint64_t* array, int array_size, uint64_t num, int offset){
	long dist_min = 0;
	//int  idx =0;
	for(int i=0; i< array_size; i++){
		long dist_tmp = abs( ((long)array[i] + offset) - (long)num);
		if(i==0){
			dist_min = dist_tmp;
		}else{
			if(dist_tmp < dist_min){
				dist_min = dist_tmp;
				//idx 	= i;
			}
		}
	}
	return dist_min;
}
// correlate two vectors:
// For each element inside array 1, we find its nearest neighbour in array2  and calculate 
// the distance between the elemement in array1 and its nearest neighbour in array2
long correlate_2_vec(uint64_t* array1, uint64_t* array2, int array_size1, int array_size2){
	long dist = 0; 
	for(int i=0; i<array_size1; i++){
		//find the nearest neighbour in the 2nd array
		uint64_t element = nearest_neighbour(array2, array_size2, array1[i]);
		dist += abs( (long)element - (long)array1[i]);
	}
	return dist;
}

long correlate_2_vec_w_offset(uint64_t* array1, uint64_t* array2, int array_size1, int array_size2, int offset){
	long dist = 0; 
	for(int i=0; i<array_size1; i++){
		//find the nearest neighbour in the 2nd array
		//uint64_t element = nearest_neighbour_w_offset(array2, array_size2, array1[i], offset);
		dist += nearest_neighbour_dist_w_offset(array2, array_size2, array1[i], offset);
		//printf("%ld|%ld " , element, dist);
	}
	//printf("\n");
	return dist;
}

// Shift the elements inside the array by the value of offset
uint64_t* shift_vec_w_offset(uint64_t* array, int array_size, int offset){
	uint64_t* new_array = (uint64_t *)calloc(array_size, sizeof(uint64_t));
	for(int i=0; i<array_size; i++){
		new_array[i] = array[i] + offset;
	}
	return new_array;
}

void shift_array_uint16_right(uint16_t* array, int size, int index){
	if(index >= size){
		printf("ERROR: index is larger than size!\n");
		return;
	}

	uint16_t tmp= array[index], tmp1;

	if(index == size-1){
		return;
	}
	for(int i=index; i<size-1; i++){
		tmp1 			= array[i+1];
		array[i+1] 		= tmp;
		tmp 			= tmp1;
	}
	return;
}

void shift_array_uint16_left(uint16_t* array, int size, int index){
	if(index >= size){
		printf("ERROR: index is larger than size!\n");
		return;
	}

	if(index == size-1){
		return;
	}
	for(int i=index; i<size-1; i++){
		array[i] 		= array[i+1];
	}
	return;
}


void shift_array_uint32_right(uint32_t* array, int size, int index){
	if(index >= size){
		printf("ERROR: index is larger than size!\n");
		return;
	}

	uint32_t tmp= array[index], tmp1;
	if(index == size-1){
		return;
	}
	for(int i=index; i<size-1; i++){
		tmp1 			= array[i+1];
		array[i+1] 		= tmp;
		tmp 			= tmp1;
	}
	return;
}

void shift_array_uint32_left(uint32_t* array, int size, int index){
	if(index >= size){
		printf("ERROR: index is larger than size!\n");
		return;
	}


	if(index == size-1){
		return;
	}
	for(int i=index; i<size-1; i++){
		array[i] 		= array[i+1];
	}
	return;
}


void swap_array_uint16(uint16_t* array, int size, int idx1, int idx2){
	if(idx1 >= size || idx2 >= size){
		printf("ERROR SWAP ARRAY: size out of bound!\n");
		return;
	}

	uint16_t tmp;
	tmp 		= array[idx2];
	array[idx2] = array[idx1];
	array[idx1] = tmp;
	return;
}
void swap_array_uint32(uint32_t* array, int size, int idx1, int idx2){
	if(idx1 >= size || idx2 >= size){
		printf("ERROR SWAP ARRAY: size out of bound!\n");
		return;
	}

	uint32_t tmp;
	tmp 		= array[idx2];
	array[idx2] = array[idx1];
	array[idx1] = tmp;
	return;
}
