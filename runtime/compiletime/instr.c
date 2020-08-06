#include <assert.h>
#include <math.h>
#include <types.h>

#define CHAR_BIT 8

// TODO: Throughout here we use `assert` for error conditions, which isn't optimal
// Instead we should use `unlikely` branches to a single trapping function (which should optimize better)
// The below functions are for implementing WASM instructions

// ROTL and ROTR helper functions
INLINE uint32_t
rotl_u32(uint32_t n, uint32_t c_u32)
{
	// WASM requires a modulus here (usually a single bitwise op, but it means we need no assert)
	unsigned int       c    = c_u32 % (CHAR_BIT * sizeof(n));
	const unsigned int mask = (CHAR_BIT * sizeof(n) - 1); // assumes width is a power of 2.

	c &= mask;
	return (n << c) | (n >> ((-c) & mask));
}

INLINE uint32_t
rotr_u32(uint32_t n, uint32_t c_u32)
{
	// WASM requires a modulus here (usually a single bitwise op, but it means we need no assert)
	unsigned int       c    = c_u32 % (CHAR_BIT * sizeof(n));
	const unsigned int mask = (CHAR_BIT * sizeof(n) - 1);

	c &= mask;
	return (n >> c) | (n << ((-c) & mask));
}

INLINE uint64_t
rotl_u64(uint64_t n, uint64_t c_u64)
{
	// WASM requires a modulus here (usually a single bitwise op, but it means we need no assert)
	unsigned int       c    = c_u64 % (CHAR_BIT * sizeof(n));
	const unsigned int mask = (CHAR_BIT * sizeof(n) - 1); // assumes width is a power of 2.

	c &= mask;
	return (n << c) | (n >> ((-c) & mask));
}

INLINE uint64_t
rotr_u64(uint64_t n, uint64_t c_u64)
{
	// WASM requires a modulus here (usually a single bitwise op, but it means we need no assert)
	unsigned int       c    = c_u64 % (CHAR_BIT * sizeof(n));
	const unsigned int mask = (CHAR_BIT * sizeof(n) - 1);

	c &= mask;
	return (n >> c) | (n << ((-c) & mask));
}

// Now safe division and remainder
INLINE uint32_t
u32_div(uint32_t a, uint32_t b)
{
	assert(b);
	return a / b;
}

INLINE uint32_t
u32_rem(uint32_t a, uint32_t b)
{
	assert(b);
	return a % b;
}

INLINE int32_t
i32_div(int32_t a, int32_t b)
{
	assert(b && (a != INT32_MIN || b != -1));
	return a / b;
}

INLINE int32_t
i32_rem(int32_t a, int32_t b)
{
	assert(b && (a != INT32_MIN || b != -1));
	return a % b;
}

INLINE uint64_t
u64_div(uint64_t a, uint64_t b)
{
	assert(b);
	return a / b;
}

INLINE uint64_t
u64_rem(uint64_t a, uint64_t b)
{
	assert(b);
	return a % b;
}

INLINE int64_t
i64_div(int64_t a, int64_t b)
{
	assert(b && (a != INT64_MIN || b != -1));
	return a / b;
}

INLINE int64_t
i64_rem(int64_t a, int64_t b)
{
	assert(b && (a != INT64_MIN || b != -1));
	return a % b;
}

// float to integer conversion methods
// In C, float => int conversions always truncate
// If a int2float(int::min_value) <= float <= int2float(int::max_value), it must always be safe to truncate it
uint32_t
u32_trunc_f32(float f)
{
	assert(0 <= f && f <= UINT32_MAX);
	return (uint32_t)f;
}

int32_t
i32_trunc_f32(float f)
{
	assert(INT32_MIN <= f && f <= INT32_MAX);
	return (int32_t)f;
}

uint32_t
u32_trunc_f64(double f)
{
	assert(0 <= f && f <= UINT32_MAX);
	return (uint32_t)f;
}

int32_t
i32_trunc_f64(double f)
{
	assert(INT32_MIN <= f && f <= INT32_MAX);
	return (int32_t)f;
}

uint64_t
u64_trunc_f32(float f)
{
	assert(0 <= f && f <= UINT64_MAX);
	return (uint64_t)f;
}

int64_t
i64_trunc_f32(float f)
{
	assert(INT64_MIN <= f && f <= INT64_MAX);
	return (int64_t)f;
}

uint64_t
u64_trunc_f64(double f)
{
	assert(0 <= f && f <= UINT64_MAX);
	return (uint64_t)f;
}

int64_t
i64_trunc_f64(double f)
{
	assert(INT64_MIN <= f && f <= INT64_MAX);
	return (int64_t)f;
}

// Float => Float truncation functions
INLINE float
f32_trunc_f32(float f)
{
	return trunc(f);
}

INLINE float
f32_min(float a, float b)
{
	return a < b ? a : b;
}

INLINE float
f32_max(float a, float b)
{
	return a > b ? a : b;
}

INLINE float
f32_floor(float a)
{
	return floor(a);
}

INLINE double
f64_min(double a, double b)
{
	return a < b ? a : b;
}

INLINE double
f64_max(double a, double b)
{
	return a > b ? a : b;
}

INLINE double
f64_floor(double a)
{
	return floor(a);
}
