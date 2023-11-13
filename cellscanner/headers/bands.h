#ifndef __BANDS__
#define __BANDS__

// ALL BANDS
/*
SDL Bands: 29, 32
*/
int all_bands_length = 32;
int all_bands[] = {1, 2, 3, 4, 5, 7, 8, 11, 12, 13, 14, 17, 18, 19, 20, 21, 25, 26, 28, 30, 31, 38, 39, 40, 41, 42, 43, 46, 48, 65, 66, 71};

/*
TDD Bands: 41, 46, 48 
SDL Bands: 29  
*/
int usa_bands_length = 15;
int usa_bands[] = {2, 4, 5, 12, 13, 14, 17, 25, 26, 30, 41, 46, 48, 66, 71}; 

/*
TDD Bands: 38, 40, 46
SDL Bands: 32
*/
int eu_bands_length = 9;
int eu_bands[] = {1, 3, 7, 8, 20, 28, 38, 40, 46};


#endif