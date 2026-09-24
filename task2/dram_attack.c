#include "mem_utils.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>


#define SAMPLES 1000        // number of attacks 
#define BUF_SIZE 4096		//number of bytes to flush/reload

int main(void){

	uint8_t *buf = malloc(BUF_SIZE);  //malloc gives 4096 bytes of memory and return where they start
	if (buf == NULL){
		perror("malloc");	//print error if memory is not allocated to buf
		return 1;
	}

	for(int i = 0; i< BUF_SIZE; i++){
		buf[i]=1;
	}

	uint8_t *target = buf;

	uint32_t dram_latency[SAMPLES];

	for(int i=0; i<SAMPLES ; i++){
		maccess(target);
		flush(target);
		asm volatile("mfence");

		dram_latency[i] = memaccesstime(target);

		
	}
	
	printf("dram\n");
	for(int i = 0;i < SAMPLES; i++){
		printf("%u\n",dram_latency[i]);
	}

	free(buf);
	return 0;

}





