#ifndef ENGINE_SOURCE_RETAIL_COMPRESSEDBLOCKS_H
#define ENGINE_SOURCE_RETAIL_COMPRESSEDBLOCKS_H

#include <cstddef>
#include <cstdint>
#include <vector>

namespace Retail {

// XPRESS Huffman from the Windows Compression API, in independent 1 MB blocks so several threads decode one payload.
// Each block carries its checksum; one that would not shrink is stored as it is.
std::vector<uint8_t> CompressBlocks(const uint8_t* data, size_t size);

// Exactly size bytes into out. False on any damage: a block table that disagrees with size or packedSize, a checksum
// that fails, a block that does not decode to its size.
bool DecompressBlocks(const uint8_t* packed, size_t packedSize, uint8_t* out, size_t size);

}

#endif
