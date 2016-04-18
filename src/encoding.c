
/*
 *
 */
Encoding Encodings[] = {
	{"UTF-8", encode_unicode, decode_unicode, unicode_width},
	{"ISO-8859-1", encode_iso8850_1, decode_iso8859_1, width_1},
	{"ISO-8859-2", encode_iso8850_2, decode_iso8859_2, width_1},
}

/* DecodeChar(enc, c) - decode chars to ucs_4 so it can be stored in mchar struct
 * Encoding enc - input encoding
 * char c - one byte of input
 *
 * reentrant, because we may need to parse few bytes of input to get our final character
 */

uint32_t DecodeChar(const Encoding *enc, const char c) {
	uint32_t ucs;

	ucs = enc->decoder(c);

	return ucs;
}

/* EncodeChar(enc, mchar, buf, bufsize) - encode ucs_4 in target encoding */
size_t EncodeChar(const Encoding *enc, const uint32_t mchar, char *buf, size_t bufsize) {

	enc->encoder(mchar, buf, bufsize);

}

/* get character width */
size_t CharWidth(const Encoding *enc, const uint32_t mchar) {

	return enc->charwidth(mchar);
}
