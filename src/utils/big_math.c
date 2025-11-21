#include <stdint.h>
#include <string.h>

int big_cmp(const uint8_t *a, const uint8_t *b, size_t n)
{
	for (size_t i = n; i-- > 0;) {
		if (a[i] < b[i])
			return -1;
		if (a[i] > b[i])
			return 1;
	}
	return 0;
}

void big_sub(uint8_t *out, const uint8_t *a, const uint8_t *b, size_t n)
{
	// out = a - b (assumes a >= b)
	int borrow = 0;
	for (size_t i = 0; i < n; ++i) {
		int v = (int)a[i] - (int)b[i] - borrow;
		if (v < 0) {
			v += 256;
			borrow = 1;
		} else
			borrow = 0;
		out[i] = (uint8_t)v;
	}
}

void big_add(uint8_t *out, const uint8_t *a, const uint8_t *b, size_t n)
{
	int carry = 0;
	for (size_t i = 0; i < n; ++i) {
		int v = (int)a[i] + (int)b[i] + carry;
		out[i] = (uint8_t)(v & 0xFF);
		carry = v >> 8;
	}
}

// add a and b into out, then if out >= mod reduce out -= mod
void big_add_mod(uint8_t *out, const uint8_t *a, const uint8_t *b, const uint8_t *mod, size_t n)
{
	big_add(out, a, b, n);
	if (big_cmp(out, mod, n) > 0) {
		big_sub(out, out, mod, n);
	}
}

// left-shift (multiply by 2) modulo mod: out = (in * 2) mod mod
void big_double_mod(uint8_t *out, const uint8_t *in, const uint8_t *mod, size_t n)
{
	uint16_t carry = 0;
	for (size_t i = 0; i < n; ++i) {
		uint16_t v = ((uint16_t)in[i] << 1) | carry;
		out[i] = (uint8_t)(v & 0xFF);
		carry = (v >> 8) & 0xFF;
	}
	if (big_cmp(out, mod, n) > 0) {
		big_sub(out, out, mod, n);
	}
}

// schoolbook multiplication: a * b -> mulrez (2*n bytes)
void big_mul(uint8_t *mulrez, const uint8_t *a, const uint8_t *b, size_t n)
{
	memset(mulrez, 0, 2 * n);
	for (size_t i = 0; i < n; ++i) {
		uint16_t carry = 0;
		for (size_t j = 0; j < n; ++j) {
			size_t pos = i + j;
			uint32_t prod = (uint32_t)a[i] * (uint32_t)b[j];
			uint32_t sum = (uint32_t)mulrez[pos] + (prod & 0xFF) + carry;
			mulrez[pos] = (uint8_t)(sum & 0xFF);
			carry = (uint16_t)((prod >> 8) + (sum >> 8));
		}
		// propagate carry
		size_t pos = i + n;
		while (carry) {
			uint32_t sum = (uint32_t)mulrez[pos] + carry;
			mulrez[pos] = (uint8_t)(sum & 0xFF);
			carry = (uint16_t)(sum >> 8);
			pos++;
		}
	}
}