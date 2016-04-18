typedef struct Encoding Encoding;
struct Encoding {
	const char *name;
	
	size_t (* encoder)(const uint32_t mchar, char *buf, size_t bufsize);
	uint32_t (* decoder)(const char c);
	size_t (* charwidth)(const uint32_t mchar);
};

