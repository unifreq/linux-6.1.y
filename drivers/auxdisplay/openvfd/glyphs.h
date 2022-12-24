#ifndef __GLYPHS__
#define __GLYPHS__

typedef unsigned char	u_int8;
typedef unsigned short	u_int16;
typedef unsigned long	u_int32;

/** Character conversion of digital tube display code*/
typedef struct _led_bitmap {
	u_int8 character;
	u_int8 bitmap;
}led_bitmap;

/** Character conversion of digital tube display code array*/

#define a1 0x01
#define b1 0x02
#define c1 0x04
#define d1 0x08
#define e1 0x10
#define f1 0x20
#define g1 0x40
#define p1 0x80

static const led_bitmap LED_decode_tab1[] = {
/*
 * Most displays have 5 sections, 1 - 4 are the digits,
 * the 5th is mapped to indicators (5A-5G).
 * The 7 segment sequences are shown below.
 *
 *  dp
 *       +--d/08--+
 *       |        |
 *  c/04 |        | e/10
 *       +--g/40--+
 *       |        |
 *  b/02 |        | f/20
 *       +--a/01--+
 *
 */

	{0,   a1|b1|c1|d1|e1|f1   },	{1,   e1|f1               },
	{2,   a1|b1|d1|e1|g1      },	{3,   a1|d1|e1|f1|g1      },
	{4,   c1|e1|f1|g1         },	{5,   a1|c1|d1|f1|g1      },
	{6,   a1|b1|c1|d1|f1|g1   },	{7,   d1|e1|f1            },
	{8,   a1|b1|c1|d1|e1|f1|g1},	{9,   a1|c1|d1|e1|f1|g1   },

	{'0', a1|b1|c1|d1|e1|f1   },	{'1', e1|f1               },
	{'2', a1|b1|d1|e1|g1      },	{'3', a1|d1|e1|f1|g1      },
	{'4', c1|e1|f1|g1         },	{'5', a1|c1|d1|f1|g1      },
	{'6', a1|b1|c1|d1|f1|g1   },	{'7', d1|e1|f1            },
	{'8', a1|b1|c1|d1|e1|f1|g1},	{'9', a1|c1|d1|e1|f1|g1   },

	{'a', b1|c1|d1|e1|f1|g1   },	{'A', b1|c1|d1|e1|f1|g1   },
	{'b', a1|b1|c1|f1|g1      },	{'B', a1|b1|c1|f1|g1      },
	{'c', a1|b1|c1|d1         },	{'C', a1|b1|c1|d1         },
	{'d', a1|b1|e1|f1|g1      },	{'D', a1|b1|e1|f1|g1      },
	{'e', a1|b1|c1|d1|g1      },	{'E', a1|b1|c1|d1|g1      },
	{'f', b1|c1|d1|g1         },	{'F', b1|c1|d1|g1         },
	{'g', a1|b1|c1|d1|f1      },	{'G', a1|b1|c1|d1|f1      },
	{'h', b1|c1|f1|g1         },	{'H', b1|c1|f1|g1         },
	{'i', b1|c1               },	{'I', b1|c1               },
	{'j', a1|b1|e1|f1         },	{'J', a1|b1|e1|f1         },
	{'k', b1|c1|d1|f1|g1      },	{'K', b1|c1|d1|f1|g1      },
	{'l', a1|b1|c1            },	{'L', a1|b1|c1            },
	{'m', b1|d1|f1            },	{'M', b1|d1|f1            },
	{'n', b1|c1|d1|e1|f1      },	{'N', b1|c1|d1|e1|f1      },
	{'o', a1|b1|f1|g1         },	{'O', a1|b1|f1|g1         },
	{'p', b1|c1|d1|e1|g1      },	{'P', b1|c1|d1|e1|g1      },
	{'q', c1|d1|e1|f1|g1      },	{'Q', c1|d1|e1|f1|g1      },
	{'r', b1|g1               },	{'R', b1|g1               },
	{'s', a1|c1|d1|f1|g1      },	{'S', a1|c1|d1|f1|g1      },
	{'t', a1|b1|c1|g1         },	{'T', a1|b1|c1|g1         },
	{'u', a1|b1|f1            },	{'U', a1|b1|f1            },
	{'v', a1|b1|c1|e1|f1      },	{'V', a1|b1|c1|e1|f1      },
	{'w', a1|c1|e1            },	{'W', a1|c1|e1            },
	{'x', b1|c1|e1|f1|g1      },	{'X', b1|c1|e1|f1|g1      },
	{'y', a1|c1|e1|f1|g1      },	{'Y', a1|c1|e1|f1|g1      },
	{'z', a1|b1|d1|e1|g1      },	{'Z', a1|b1|d1|e1|g1      },
	{'_', a1}, {'-', g1}, {' ', 0}, { 0xB0, c1|d1|e1|g1 }
};

#define a2 0x08
#define b2 0x10
#define c2 0x20
#define d2 0x01
#define e2 0x02
#define f2 0x04
#define g2 0x40
#define p2 0x80

static const led_bitmap LED_decode_tab2[] = {
/*
 *
 *  dp
 *       +--d/01--+
 *       |        |
 *  c/20 |        | e/02
 *       +--g/40--+
 *       |        |
 *  b/10 |        | f/04
 *       +--a/08--+
 *
 */

	{0,   a2|b2|c2|d2|e2|f2   },	{1,   e2|f2               },
	{2,   a2|b2|d2|e2|g2      },	{3,   a2|d2|e2|f2|g2      },
	{4,   c2|e2|f2|g2         },	{5,   a2|c2|d2|f2|g2      },
	{6,   a2|b2|c2|d2|f2|g2   },	{7,   d2|e2|f2            },
	{8,   a2|b2|c2|d2|e2|f2|g2},	{9,   a2|c2|d2|e2|f2|g2   },

	{'0', a2|b2|c2|d2|e2|f2   },	{'1', e2|f2               },
	{'2', a2|b2|d2|e2|g2      },	{'3', a2|d2|e2|f2|g2      },
	{'4', c2|e2|f2|g2         },	{'5', a2|c2|d2|f2|g2      },
	{'6', a2|b2|c2|d2|f2|g2   },	{'7', d2|e2|f2            },
	{'8', a2|b2|c2|d2|e2|f2|g2},	{'9', a2|c2|d2|e2|f2|g2   },

	{'a', b2|c2|d2|e2|f2|g2   },	{'A', b2|c2|d2|e2|f2|g2   },
	{'b', a2|b2|c2|f2|g2      },	{'B', a2|b2|c2|f2|g2      },
	{'c', a2|b2|c2|d2         },	{'C', a2|b2|c2|d2         },
	{'d', a2|b2|e2|f2|g2      },	{'D', a2|b2|e2|f2|g2      },
	{'e', a2|b2|c2|d2|g2      },	{'E', a2|b2|c2|d2|g2      },
	{'f', b2|c2|d2|g2         },	{'F', b2|c2|d2|g2         },
	{'g', a2|b2|c2|d2|f2      },	{'G', a2|b2|c2|d2|f2      },
	{'h', b2|c2|f2|g2         },	{'H', b2|c2|f2|g2         },
	{'i', b2|c2               },	{'I', b2|c2               },
	{'j', a2|b2|e2|f2         },	{'J', a2|b2|e2|f2         },
	{'k', b2|c2|d2|f2|g2      },	{'K', b2|c2|d2|f2|g2      },
	{'l', a2|b2|c2            },	{'L', a2|b2|c2            },
	{'m', b2|d2|f2            },	{'M', b2|d2|f2            },
	{'n', b2|c2|d2|e2|f2      },	{'N', b2|c2|d2|e2|f2      },
	{'o', a2|b2|f2|g2         },	{'O', a2|b2|f2|g2         },
	{'p', b2|c2|d2|e2|g2      },	{'P', b2|c2|d2|e2|g2      },
	{'q', c2|d2|e2|f2|g2      },	{'Q', c2|d2|e2|f2|g2      },
	{'r', b2|g2               },	{'R', b2|g2               },
	{'s', a2|c2|d2|f2|g2      },	{'S', a2|c2|d2|f2|g2      },
	{'t', a2|b2|c2|g2         },	{'T', a2|b2|c2|g2         },
	{'u', a2|b2|f2            },	{'U', a2|b2|f2            },
	{'v', a2|b2|c2|e2|f2      },	{'V', a2|b2|c2|e2|f2      },
	{'w', a2|c2|e2            },	{'W', a2|c2|e2            },
	{'x', b2|c2|e2|f2|g2      },	{'X', b2|c2|e2|f2|g2      },
	{'y', a2|c2|e2|f2|g2      },	{'Y', a2|c2|e2|f2|g2      },
	{'z', a2|b2|d2|e2|g2      },	{'Z', a2|b2|d2|e2|g2      },
	{'_', a2}, {'-', g2}, {' ', 0}, { 0xB0, c2|d2|e2|g2 }
};

#define a3 0x01
#define b3 0x08
#define c3 0x20
#define d3 0x02
#define e3 0x10
#define f3 0x04
#define g3 0x40
#define p3 0x80

static const led_bitmap LED_decode_tab3[] = {
/*
 *
 *  dp
 *       +--d/02--+
 *       |        |
 *  c/20 |        | e/10
 *       +--g/40--+
 *       |        |
 *  b/08 |        | f/04
 *       +--a/01--+
 *
 */

	{0,   a3|b3|c3|d3|e3|f3   },	{1,   e3|f3               },
	{2,   a3|b3|d3|e3|g3      },	{3,   a3|d3|e3|f3|g3      },
	{4,   c3|e3|f3|g3         },	{5,   a3|c3|d3|f3|g3      },
	{6,   a3|b3|c3|d3|f3|g3   },	{7,   d3|e3|f3            },
	{8,   a3|b3|c3|d3|e3|f3|g3},	{9,   a3|c3|d3|e3|f3|g3   },

	{'0', a3|b3|c3|d3|e3|f3   },	{'1', e3|f3               },
	{'2', a3|b3|d3|e3|g3      },	{'3', a3|d3|e3|f3|g3      },
	{'4', c3|e3|f3|g3         },	{'5', a3|c3|d3|f3|g3      },
	{'6', a3|b3|c3|d3|f3|g3   },	{'7', d3|e3|f3            },
	{'8', a3|b3|c3|d3|e3|f3|g3},	{'9', a3|c3|d3|e3|f3|g3   },

	{'a', b3|c3|d3|e3|f3|g3   },	{'A', b3|c3|d3|e3|f3|g3   },
	{'b', a3|b3|c3|f3|g3      },	{'B', a3|b3|c3|f3|g3      },
	{'c', a3|b3|c3|d3         },	{'C', a3|b3|c3|d3         },
	{'d', a3|b3|e3|f3|g3      },	{'D', a3|b3|e3|f3|g3      },
	{'e', a3|b3|c3|d3|g3      },	{'E', a3|b3|c3|d3|g3      },
	{'f', b3|c3|d3|g3         },	{'F', b3|c3|d3|g3         },
	{'g', a3|b3|c3|d3|f3      },	{'G', a3|b3|c3|d3|f3      },
	{'h', b3|c3|f3|g3         },	{'H', b3|c3|f3|g3         },
	{'i', b3|c3               },	{'I', b3|c3               },
	{'j', a3|b3|e3|f3         },	{'J', a3|b3|e3|f3         },
	{'k', b3|c3|d3|f3|g3      },	{'K', b3|c3|d3|f3|g3      },
	{'l', a3|b3|c3            },	{'L', a3|b3|c3            },
	{'m', b3|d3|f3            },	{'M', b3|d3|f3            },
	{'n', b3|c3|d3|e3|f3      },	{'N', b3|c3|d3|e3|f3      },
	{'o', a3|b3|f3|g3         },	{'O', a3|b3|f3|g3         },
	{'p', b3|c3|d3|e3|g3      },	{'P', b3|c3|d3|e3|g3      },
	{'q', c3|d3|e3|f3|g3      },	{'Q', c3|d3|e3|f3|g3      },
	{'r', b3|g3               },	{'R', b3|g3               },
	{'s', a3|c3|d3|f3|g3      },	{'S', a3|c3|d3|f3|g3      },
	{'t', a3|b3|c3|g3         },	{'T', a3|b3|c3|g3         },
	{'u', a3|b3|f3            },	{'U', a3|b3|f3            },
	{'v', a3|b3|c3|e3|f3      },	{'V', a3|b3|c3|e3|f3      },
	{'w', a3|c3|e3            },	{'W', a3|c3|e3            },
	{'x', b3|c3|e3|f3|g3      },	{'X', b3|c3|e3|f3|g3      },
	{'y', a3|c3|e3|f3|g3      },	{'Y', a3|c3|e3|f3|g3      },
	{'z', a3|b3|d3|e3|g3      },	{'Z', a3|b3|d3|e3|g3      },
	{'_', a3}, {'-', g3}, {' ', 0}, { 0xB0, c3|d3|e3|g3 }
};

#define a4 0x04
#define b4 0x02
#define c4 0x80
#define d4 0x01
#define e4 0x40
#define f4 0x10
#define g4 0x20
#define p4 0x08

static const led_bitmap LED_decode_tab4[] = {
/*
 *
 *  dp
 *       +--d/01--+
 *       |        |
 *  c/80 |        | e/40
 *       +--g/20--+
 *       |        |
 *  b/02 |        | f/10
 *       +--a/04--+.p/08
 *
 */

	{0,   a4|b4|c4|d4|e4|f4   },	{1,   e4|f4               },
	{2,   a4|b4|d4|e4|g4      },	{3,   a4|d4|e4|f4|g4      },
	{4,   c4|e4|f4|g4         },	{5,   a4|c4|d4|f4|g4      },
	{6,   a4|b4|c4|d4|f4|g4   },	{7,   d4|e4|f4            },
	{8,   a4|b4|c4|d4|e4|f4|g4},	{9,   a4|c4|d4|e4|f4|g4   },

	{'0', a4|b4|c4|d4|e4|f4   },	{'1', e4|f4               },
	{'2', a4|b4|d4|e4|g4      },	{'3', a4|d4|e4|f4|g4      },
	{'4', c4|e4|f4|g4         },	{'5', a4|c4|d4|f4|g4      },
	{'6', a4|b4|c4|d4|f4|g4   },	{'7', d4|e4|f4            },
	{'8', a4|b4|c4|d4|e4|f4|g4},	{'9', a4|c4|d4|e4|f4|g4   },

	{'a', b4|c4|d4|e4|f4|g4   },	{'A', b4|c4|d4|e4|f4|g4   },
	{'b', a4|b4|c4|f4|g4      },	{'B', a4|b4|c4|f4|g4      },
	{'c', a4|b4|c4|d4         },	{'C', a4|b4|c4|d4         },
	{'d', a4|b4|e4|f4|g4      },	{'D', a4|b4|e4|f4|g4      },
	{'e', a4|b4|c4|d4|g4      },	{'E', a4|b4|c4|d4|g4      },
	{'f', b4|c4|d4|g4         },	{'F', b4|c4|d4|g4         },
	{'g', a4|b4|c4|d4|f4      },	{'G', a4|b4|c4|d4|f4      },
	{'h', b4|c4|f4|g4         },	{'H', b4|c4|f4|g4         },
	{'i', b4|c4               },	{'I', b4|c4               },
	{'j', a4|b4|e4|f4         },	{'J', a4|b4|e4|f4         },
	{'k', b4|c4|d4|f4|g4      },	{'K', b4|c4|d4|f4|g4      },
	{'l', a4|b4|c4            },	{'L', a4|b4|c4            },
	{'m', b4|d4|f4            },	{'M', b4|d4|f4            },
	{'n', b4|c4|d4|e4|f4      },	{'N', b4|c4|d4|e4|f4      },
	{'o', a4|b4|f4|g4         },	{'O', a4|b4|f4|g4         },
	{'p', b4|c4|d4|e4|g4      },	{'P', b4|c4|d4|e4|g4      },
	{'q', c4|d4|e4|f4|g4      },	{'Q', c4|d4|e4|f4|g4      },
	{'r', b4|g4               },	{'R', b4|g4               },
	{'s', a4|c4|d4|f4|g4      },	{'S', a4|c4|d4|f4|g4      },
	{'t', a4|b4|c4|g4         },	{'T', a4|b4|c4|g4         },
	{'u', a4|b4|f4            },	{'U', a4|b4|f4            },
	{'v', a4|b4|c4|e4|f4      },	{'V', a4|b4|c4|e4|f4      },
	{'w', a4|c4|e4            },	{'W', a4|c4|e4            },
	{'x', b4|c4|e4|f4|g4      },	{'X', b4|c4|e4|f4|g4      },
	{'y', a4|c4|e4|f4|g4      },	{'Y', a4|c4|e4|f4|g4      },
	{'z', a4|b4|d4|e4|g4      },	{'Z', a4|b4|d4|e4|g4      },
	{'_', a4}, {'-', g4}, {' ', 0}, { 0xB0, c4|d4|e4|g4 }
};

#define a5 0x40
#define b5 0x20
#define c5 0x10
#define d5 0x01
#define e5 0x02
#define f5 0x04
#define g5 0x08
#define p5 0x80

static const led_bitmap LED_decode_tab5[] = {
/*
 *
 *  dp
 *       +--d/01--+
 *       |        |
 *  c/10 |        | e/02
 *       +--g/08--+
 *       |        |
 *  b/20 |        | f/04
 *       +--a/40--+
 *
 */

	{0,   a5|b5|c5|d5|e5|f5   },	{1,   e5|f5               },
	{2,   a5|b5|d5|e5|g5      },	{3,   a5|d5|e5|f5|g5      },
	{4,   c5|e5|f5|g5         },	{5,   a5|c5|d5|f5|g5      },
	{6,   a5|b5|c5|d5|f5|g5   },	{7,   d5|e5|f5            },
	{8,   a5|b5|c5|d5|e5|f5|g5},	{9,   a5|c5|d5|e5|f5|g5   },

	{'0', a5|b5|c5|d5|e5|f5   },	{'1', e5|f5               },
	{'2', a5|b5|d5|e5|g5      },	{'3', a5|d5|e5|f5|g5      },
	{'4', c5|e5|f5|g5         },	{'5', a5|c5|d5|f5|g5      },
	{'6', a5|b5|c5|d5|f5|g5   },	{'7', d5|e5|f5            },
	{'8', a5|b5|c5|d5|e5|f5|g5},	{'9', a5|c5|d5|e5|f5|g5   },

	{'a', b5|c5|d5|e5|f5|g5   },	{'A', b5|c5|d5|e5|f5|g5   },
	{'b', a5|b5|c5|f5|g5      },	{'B', a5|b5|c5|f5|g5      },
	{'c', a5|b5|c5|d5         },	{'C', a5|b5|c5|d5         },
	{'d', a5|b5|e5|f5|g5      },	{'D', a5|b5|e5|f5|g5      },
	{'e', a5|b5|c5|d5|g5      },	{'E', a5|b5|c5|d5|g5      },
	{'f', b5|c5|d5|g5         },	{'F', b5|c5|d5|g5         },
	{'g', a5|b5|c5|d5|f5      },	{'G', a5|b5|c5|d5|f5      },
	{'h', b5|c5|f5|g5         },	{'H', b5|c5|f5|g5         },
	{'i', b5|c5               },	{'I', b5|c5               },
	{'j', a5|b5|e5|f5         },	{'J', a5|b5|e5|f5         },
	{'k', b5|c5|d5|f5|g5      },	{'K', b5|c5|d5|f5|g5      },
	{'l', a5|b5|c5            },	{'L', a5|b5|c5            },
	{'m', b5|d5|f5            },	{'M', b5|d5|f5            },
	{'n', b5|c5|d5|e5|f5      },	{'N', b5|c5|d5|e5|f5      },
	{'o', a5|b5|f5|g5         },	{'O', a5|b5|f5|g5         },
	{'p', b5|c5|d5|e5|g5      },	{'P', b5|c5|d5|e5|g5      },
	{'q', c5|d5|e5|f5|g5      },	{'Q', c5|d5|e5|f5|g5      },
	{'r', b5|g5               },	{'R', b5|g5               },
	{'s', a5|c5|d5|f5|g5      },	{'S', a5|c5|d5|f5|g5      },
	{'t', a5|b5|c5|g5         },	{'T', a5|b5|c5|g5         },
	{'u', a5|b5|f5            },	{'U', a5|b5|f5            },
	{'v', a5|b5|c5|e5|f5      },	{'V', a5|b5|c5|e5|f5      },
	{'w', a5|c5|e5            },	{'W', a5|c5|e5            },
	{'x', b5|c5|e5|f5|g5      },	{'X', b5|c5|e5|f5|g5      },
	{'y', a5|c5|e5|f5|g5      },	{'Y', a5|c5|e5|f5|g5      },
	{'z', a5|b5|d5|e5|g5      },	{'Z', a5|b5|d5|e5|g5      },
	{'_', a5}, {'-', g5}, {' ', 0}, { 0xB0, c5|d5|e5|g5 }
};

#endif
