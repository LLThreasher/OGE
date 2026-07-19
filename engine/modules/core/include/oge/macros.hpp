#define NO_COPY(Name)           \
    Name(const Name&) = delete; \
    Name& operator=(const Name&) = delete;
