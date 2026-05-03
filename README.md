# zring

[![test](https://github.com/mah0x211/zring/actions/workflows/test.yml/badge.svg)](https://github.com/mah0x211/zring/actions/workflows/test.yml)
[![codecov](https://codecov.io/gh/mah0x211/zring/branch/master/graph/badge.svg)](https://codecov.io/gh/mah0x211/zring)

A single-header, header-only C99 ring buffer (circular buffer) library backed
by externally-provided memory.  No allocation, no dependencies beyond
`<stddef.h>`.

## Features

- **Zero-copy** peek-and-consume interface for both read and write sides
- **Zero-allocation** — memory is caller-supplied; no internal `malloc`/`free`
- **Contiguous-segment API** — each call returns a single, contiguous region;
  callers loop to handle wrap-around
- **Strict bounds checking** — `zring_commit` / `zring_consume` return `-1`
  on overflow rather than corrupting state
- **Header-only** — drop `zring.h` into your project and `#include` it

## API

```c
// Initialise a ring buffer over an existing memory region.
void   zring_init(zring_t *rb, void *mem, size_t cap);

// Size of the next contiguous writable / readable segment.
size_t zring_space_size(zring_t *rb);
size_t zring_data_size(zring_t *rb);

// Pointer to the next contiguous writable / readable segment.
void  *zring_space(zring_t *rb, size_t *len);
void  *zring_data(zring_t *rb,  size_t *len);

// Advance write / read position by n bytes.
// Returns 0 on success, -1 if n exceeds the contiguous segment.
int    zring_commit(zring_t *rb,  size_t n);
int    zring_consume(zring_t *rb, size_t n);
```

## Usage

```c
#include "zring.h"

char backing[4096];
zring_t rb;
zring_init(&rb, backing, sizeof(backing));

// Write
size_t len;
char *buf = zring_space(&rb, &len);
ssize_t n = read(fd, buf, len);
if (n > 0) zring_commit(&rb, (size_t)n);

// Read
char *data = zring_data(&rb, &len);
ssize_t sent = write(fd, data, len);
if (sent > 0) zring_consume(&rb, (size_t)sent);
```

## Building / Testing

```sh
make            # build and run tests with AddressSanitizer + UBSan
make coverage   # build with gcov, run tests, generate HTML report
make clean
```

## License

MIT — see [LICENSE](LICENSE).
