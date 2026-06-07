#include <kernel/unicode.h>
#include <errno.h>

int utf8_decode_char(const uint8_t *data, size_t count, int *codepoint) {
	if (count == 0) {
		if (codepoint) *codepoint = 0;
		return 0;
	}
	if (data[0] <= 0x7f) {
		if (codepoint) *codepoint = data[0];
		return 1;
	} else if (data[0] <= 0xc1) {
		return -EILSEQ;
	} else if (data[0] <= 0xdf) {
		if (count < 2) return -EILSEQ;
		if ((data[1] & 0xc0) != 0x80) return -EILSEQ;
		int decoded = ((data[0] & 0x1f) << 6);
		decoded |= (data[1] & 0x3f);
		if (decoded <= 0x7f) return -EILSEQ;
		if (codepoint) *codepoint = decoded;
		return 2;
	} else if (data[0] <= 0xef) {
		if (count < 3) return -EILSEQ;
		if ((data[1] & 0xc0) != 0x80) return -EILSEQ;
		if ((data[2] & 0xc0) != 0x80) return -EILSEQ;
		int decoded = ((data[0] & 0x0f) << 12);
		decoded |= (data[1] & 0x3f) << 6;
		decoded |= (data[2] & 0x3f);
		if (decoded <= 0x7ff) return -EILSEQ;
		if (decoded >= 0xd800 && decoded <= 0xdfff) {
			// these are used for utf16 surrogate
			return -EILSEQ;
		}
		if (codepoint) *codepoint = decoded;
		return 3;
	} else if (data[0] <= 0xf7) {
		if (count < 4) return -EILSEQ;
		if ((data[1] & 0xc0) != 0x80) return -EILSEQ;
		if ((data[2] & 0xc0) != 0x80) return -EILSEQ;
		if ((data[3] & 0xc0) != 0x80) return -EILSEQ;
		int decoded = ((data[0] & 0x07) << 18);
		decoded |= (data[1] & 0x3f) << 12;
		decoded |= (data[2] & 0x3f) << 6;
		decoded |= (data[3] & 0x3f);
		if (decoded <= 0xffff) return -EILSEQ;
		if (decoded > 0x10ffff) return -EILSEQ;
		if (codepoint) *codepoint = decoded;
		return 4;
	} else {
		return -EILSEQ;
	}
}

ssize_t utf8_decode_buf(const uint8_t *data, size_t count, int *codepoints) {
	ssize_t total = 0;
	while (count > 0) {
		int ret = utf8_decode_char(data, count, codepoints);
		if (ret < 0) return ret;
		if (codepoints) codepoints++;
		total++;
		data += ret;
		count -= ret;
	}
	return total;
}

int utf8_encode_char(int codepoint, uint8_t *data) {
	if (codepoint < 0) {
		return -EINVAL;
	} else if (codepoint <= 0x7f) {
		if (data) data[0] = codepoint;
		return 1;
	} else if (codepoint <= 0x7ff) {
		if (data) {
			data[0] = 0xc0 | ((codepoint >> 6) & 0x1f);
			data[1] = 0x80 | (codepoint & 0x3f);
		}
		return 2;
	} else if (codepoint <= 0xffff) {
		if (codepoint >= 0xd800 && codepoint <= 0xdfff) {
			// these are used for utf16 surrogate
			return -EINVAL;
		}
		if (data) {
			data[0] = 0xe0 | ((codepoint >> 12) & 0x0f);
			data[1] = 0x80 | ((codepoint >> 6) & 0x3f);
			data[2] = 0x80 | (codepoint & 0x3f);
		}
		return 3;
	} else if (codepoint <= 0x10ffff) {
		if (data) {
			data[0] = 0xf0 | ((codepoint >> 18) & 0x07);
			data[1] = 0x80 | ((codepoint >> 12) & 0x3f);
			data[2] = 0x80 | ((codepoint >> 6) & 0x3f);
			data[3] = 0x80 | (codepoint & 0x3f);
		}
		return 4;
	} else {
		return -EINVAL;
	}
}

ssize_t utf8_encode_buf(const int *codepoints, size_t count, uint8_t *data) {
	ssize_t total = 0;
	while (count > 0) {
		int ret = utf8_encode_char(*codepoints, data);
		if (ret < 0) return ret;
		codepoints++;
		total += ret;
		if (data) data += ret;
		count--;
	}
	return total;
}

int utf16_decode_char(const uint16_t *data, size_t count, int *codepoint) {
	if (count == 0) {
		if (codepoint) *codepoint = 0;
		return 0;
	}
	if (data[0] <= 0xd7ff) {
		if (codepoint) *codepoint = data[0];
		return 1;
	} else if (data[0] <= 0xdbff) {
		// first is high surrogate
		if (count < 2) return -EILSEQ;
		if ((data[1] & 0xfc00) != 0xdc00) return -EILSEQ;
		if (codepoint) *codepoint = (((data[0] & 0x3ff) << 10) | (data[1] & 0x3ff)) + 0x10000;
		return 2;
	} else if (data[0] <= 0xdfff) {
		// first is low surrogate
		// invalid
		return -EILSEQ;
	} else {
		if (codepoint) *codepoint = data[0];
		return 1;
	}
}

ssize_t utf16_decode_buf(const uint16_t *data, size_t count, int *codepoints) {
	ssize_t total = 0;
	while (count > 0) {
		int ret = utf16_decode_char(data, count, codepoints);
		if (ret < 0) return ret;
		if (codepoints) codepoints++;
		total++;
		data += ret;
		count -= ret;
	}
	return total;
}

int utf16_encode_char(int codepoint, uint16_t *data) {
	if (codepoint < 0) {
		return -EINVAL;
	} else if (codepoint <= 0xd7ff) {
		if (data) data[0] = codepoint;
		return 1;
	} else if (codepoint <= 0xdfff) {
		// these code points are used for indirection by utf16
		return -EINVAL;
	} else if (codepoint <= 0xffff) {
		if (data) data[0] = codepoint;
		return 1;
	} else if (codepoint <= 0x10ffff) {
		if (data) {
			codepoint -= 0x10000;
			data[1] = 0xd800 + ((codepoint >> 10) & 0x3ff);
			data[0] = 0xdc00 + (codepoint & 0x3ff);
		}
		return 2;
	} else {
		return -EINVAL;
	}
}

ssize_t utf16_encode_buf(const int *codepoints, size_t count, uint16_t *data) {
	ssize_t total = 0;
	while (count > 0) {
		int ret = utf16_encode_char(*codepoints, data);
		if (ret < 0) return ret;
		codepoints++;
		total += ret;
		if (data) data += ret;
		count--;
	}
	return total;
}

ssize_t utf16_to_utf8(const uint16_t *utf16, size_t count, uint8_t *utf8) {
	ssize_t total = 0;
	while (count > 0) {
		int codepoint;
		int ret = utf16_decode_char(utf16, count, &codepoint);
		if (ret < 0) return ret;
		utf16 += ret;
		count -= ret;

		ret = utf8_encode_char(codepoint, utf8);
		if (ret < 0) return ret;
		utf8 += ret;
		total += ret;
		if (codepoint == 0) break;
	}
	return total;
}
