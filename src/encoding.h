enum Encoding {
	UTF-8,
	ISO-8859-1,
	ISO-8859-2
};


struct EncodingHandler {
	char *name;
	size_t (* encoder)(const uint32_t mchar, char *buf, size_t bufsize);
	uint32_t (* decoder)(const char c);
	size_t (* charwidth)();
};
