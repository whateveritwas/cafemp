file(READ "${INPUT}" data HEX)

string(REGEX REPLACE "(..)" "0x\\1, " data "${data}")

file(SIZE "${INPUT}" size)

set(out
"#pragma once
#include <stdint.h>

static const uint8_t ${SYMBOL}[] = {
${data}
};

static const unsigned int ${SYMBOL}_len = ${size};
")

file(WRITE "${OUTPUT}" "${out}")
