
/*
 *
 */

/* DecodeChar(enc, c) - decode chars to ucs_4 so it can be stored in mchar struct 
 * Encoding enc - input encoding
 * char c - one byte of input
 *
 */

uint32_t DecodeChar(const Encoding enc, const char c) {

	return (uint32_t)c;
}

/* EncodeChar(enc, mchar, buf, bufsize) - encode ucs_4 in target encoding */
size_t EncodeChar(const Encoding enc, const uint32_t mchar, char *buf, size_t bufsize) {
}

/* get character width */
size_t CharWidth(uint32_t mchar) {

	return 1;
}
