#ifndef LEVELS_H
#define LEVELS_H

#include "types.h"

void InitLevels(OBJ_LEVEL* arr, int length){
	// Init all tile arrays.
	for(int i = 0; i < length; i++){
		for(int j = 0; j < LEVEL_WID*LEVEL_HGT; j++)
			arr[i].tiles[j] = 0;
	}
	
	arr[0].tiles[0] = 1;
}

#endif