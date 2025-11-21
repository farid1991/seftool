#ifndef big_math_h
#define big_math_h

#define BIGNUM_SIZE 64
#define MULREZ_SIZE (BIGNUM_SIZE * 2)
#define D41C31D 0x1FF // 511

int big_cmp(const uint8_t *a, const uint8_t *b, size_t n);
void big_sub(uint8_t *out, const uint8_t *a, const uint8_t *b, size_t n);
void big_add(uint8_t *out, const uint8_t *a, const uint8_t *b, size_t n);
void big_add_mod(uint8_t *out, const uint8_t *a, const uint8_t *b, const uint8_t *mod, size_t n);
void big_double_mod(uint8_t *out, const uint8_t *in, const uint8_t *mod, size_t n);
void big_mul(uint8_t *mulrez, const uint8_t *a, const uint8_t *b, size_t n);

#endif // big_math_h