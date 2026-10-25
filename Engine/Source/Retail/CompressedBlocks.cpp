#include "Engine/Source/Retail/CompressedBlocks.h"
#include "Engine/Source/Retail/Checksum.h"
#include <algorithm>
#include <atomic>
#include <cstring>
#include <thread>
#include <windows.h>
#include <compressapi.h>

namespace Retail {

// 1 MB blocks decode in about 2 ms each, so a 37 MB model spreads over the threads in 13 ms instead of 75.
static constexpr size_t BLOCK_SIZE = 1 << 20;
static constexpr size_t TABLE_ENTRY_SIZE = 12;
static constexpr unsigned MAX_THREADS = 8;
static constexpr DWORD ALGORITHM = COMPRESS_ALGORITHM_XPRESS_HUFF | COMPRESS_RAW;

// Runs worker on this thread and up to MAX_THREADS - 1 more, one per two hardware threads; each worker claims blocks
// from next until none are left.
template <typename Worker> static void OnThreads(size_t blocks, Worker worker) {
    unsigned threads = std::max(1u, std::min(MAX_THREADS, std::thread::hardware_concurrency() / 2));
    threads = static_cast<unsigned>(std::min<size_t>(threads, blocks));

    std::vector<std::thread> pool;
    for (unsigned i = 1; i < threads; i++) {
        pool.emplace_back(worker);
    }
    worker();
    for (std::thread& thread : pool) {
        thread.join();
    }
}

static size_t BlockCount(size_t size) {
    return (size + BLOCK_SIZE - 1) / BLOCK_SIZE;
}

std::vector<uint8_t> CompressBlocks(const uint8_t* data, size_t size) {
    size_t blocks = BlockCount(size);
    std::vector<std::vector<uint8_t>> packed(blocks);
    std::atomic<size_t> next { 0 };

    OnThreads(blocks, [&] {
        COMPRESSOR_HANDLE compressor = nullptr;
        bool open = CreateCompressor(ALGORITHM, nullptr, &compressor) != 0;
        for (size_t i; (i = next++) < blocks;) {
            const uint8_t* block = data + i * BLOCK_SIZE;
            size_t blockSize = std::min(BLOCK_SIZE, size - i * BLOCK_SIZE);
            std::vector<uint8_t>& out = packed[i];
            out.resize(blockSize);
            SIZE_T written = 0;
            if (!open || !Compress(compressor, block, blockSize, out.data(), out.size(), &written) || written >= blockSize) {
                out.assign(block, block + blockSize);
            } else {
                out.resize(written);
            }
        }
        if (open) {
            CloseCompressor(compressor);
        }
    });

    std::vector<uint8_t> out(4 + blocks * TABLE_ENTRY_SIZE);
    uint32_t count = static_cast<uint32_t>(blocks);
    std::memcpy(out.data(), &count, 4);
    for (size_t i = 0; i < blocks; i++) {
        uint32_t packedSize = static_cast<uint32_t>(packed[i].size());
        uint64_t checksum = WordChecksum(packed[i].data(), packed[i].size());
        std::memcpy(out.data() + 4 + i * TABLE_ENTRY_SIZE, &packedSize, 4);
        std::memcpy(out.data() + 8 + i * TABLE_ENTRY_SIZE, &checksum, 8);
        out.insert(out.end(), packed[i].begin(), packed[i].end());
    }
    return out;
}

namespace {

struct Block {
    const uint8_t* packed;
    uint32_t packedSize;
    uint64_t checksum;
};

}

// The blocks the table lists, each no larger than its decoded size and all of them filling packedSize exactly.
static bool ReadTable(const uint8_t* packed, size_t packedSize, size_t size, std::vector<Block>& blocks) {
    uint32_t count = 0;
    if (packedSize >= 4) {
        std::memcpy(&count, packed, 4);
    }
    if (packedSize < 4 || count != BlockCount(size) || packedSize - 4 < size_t(count) * TABLE_ENTRY_SIZE) {
        return false;
    }

    const uint8_t* at = packed + 4 + size_t(count) * TABLE_ENTRY_SIZE;
    size_t left = packedSize - 4 - size_t(count) * TABLE_ENTRY_SIZE;
    blocks.resize(count);
    for (size_t i = 0; i < count; i++) {
        Block& block = blocks[i];
        std::memcpy(&block.packedSize, packed + 4 + i * TABLE_ENTRY_SIZE, 4);
        std::memcpy(&block.checksum, packed + 8 + i * TABLE_ENTRY_SIZE, 8);
        if (block.packedSize > left || block.packedSize > std::min(BLOCK_SIZE, size - i * BLOCK_SIZE)) {
            return false;
        }
        block.packed = at;
        at += block.packedSize;
        left -= block.packedSize;
    }
    return left == 0;
}

bool DecompressBlocks(const uint8_t* packed, size_t packedSize, uint8_t* out, size_t size) {
    std::vector<Block> blocks;
    if (!ReadTable(packed, packedSize, size, blocks)) {
        return false;
    }

    std::atomic<size_t> next { 0 };
    std::atomic<bool> failed { false };
    OnThreads(blocks.size(), [&] {
        DECOMPRESSOR_HANDLE decompressor = nullptr;
        bool open = CreateDecompressor(ALGORITHM, nullptr, &decompressor) != 0;
        for (size_t i; !failed && (i = next++) < blocks.size();) {
            const Block& block = blocks[i];
            uint8_t* target = out + i * BLOCK_SIZE;
            size_t blockSize = std::min(BLOCK_SIZE, size - i * BLOCK_SIZE);
            SIZE_T written = 0;
            bool ok = WordChecksum(block.packed, block.packedSize) == block.checksum;
            if (ok && block.packedSize == blockSize) {
                std::memcpy(target, block.packed, blockSize);
            } else if (ok) {
                ok = open && Decompress(decompressor, block.packed, block.packedSize, target, blockSize, &written) && written == blockSize;
            }
            failed = failed || !ok;
        }
        if (open) {
            CloseDecompressor(decompressor);
        }
    });
    return !failed;
}

}
