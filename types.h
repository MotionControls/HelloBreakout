#ifndef TYPES_H
#define TYPES_H

typedef struct{
	int x,y;
}Vec2i;

typedef struct{
	int x,y,w,h;
}Vec4i;

// These already have equivalents in libgte,
// but for the sake of "clean" I'm making my own here.
typedef struct{	// Ideal for normalized vectors.
	short x,y;	// 64 >> 6 = 1
				// 32 >> 6 = 0.5
}Vec2sfx16;

typedef struct{			// Ideal for positions.
	unsigned long x,y;	// 4096 >> 12 = 1
						// 2048 >> 12 = 0.5
}Vec2ufx32;

#endif