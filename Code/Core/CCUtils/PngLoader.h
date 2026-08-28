#ifndef PNGLOADER_H
#define PNGLOADER_H

#include <string>
#include <vector>
#include <cstdint>
#include <cstring>

namespace CC
{
    // PNG Header Signature
    const uint8_t PNG_SIGNATURE[] = { 137, 80, 78, 71, 13, 10, 26, 10 };

    // PNG Chunk Types
    enum ChunkType
    {
        IHDR = 0x49484452,
        IDAT = 0x49444154,
        IEND = 0x49454E44,
        PLTE = 0x504C5445
    };

    // PNG Color Types
    enum ColorType
    {
        GRAYSCALE = 0,
        RGB = 2,
        PALETTE = 3,
        GRAYSCALE_ALPHA = 4,
        RGBA = 6
    };

    // PNG Filter Types
    enum FilterType
    {
        NONE = 0,
        SUB = 1,
        UP = 2,
        AVERAGE = 3,
        PAETH = 4
    };

    // PNG IHDR Chunk
    struct PngHeader
    {
        uint32_t width;
        uint32_t height;
        uint8_t bitDepth;
        uint8_t colorType;
        uint8_t compressionMethod;
        uint8_t filterMethod;
        uint8_t interlaceMethod;
    };

    // PNG Chunk structure
    struct Chunk
    {
        uint32_t length;
        uint32_t type;
        std::vector<uint8_t> data;
        uint32_t crc;
    };

    // PNG Image Data
    struct PngData
    {
        PngHeader header;
        std::vector<uint8_t> imageData;
        std::vector<uint32_t> palette;
        bool hasPalette;
    };

    // Minimal in-memory byte cursor. PNG decoding used to stream from
    // std::ifstream; since PlatformFileSystem delivers whole files as a
    // byte vector (Android / WebGL don't stream), the cursor replaces
    // the subset of ifstream the PNG code relied on.
    class ByteReader
    {
    public:
        ByteReader(const uint8_t* data, size_t size)
            : data(data), size(size), pos(0), failed(false)
        {}

        void Read(void* out, size_t count)
        {
            if (pos + count > size)
            {
                failed = true;
                return;
            }
            memcpy(out, data + pos, count);
            pos += count;
        }

        bool Good() const { return !failed && pos < size; }

    private:
        const uint8_t* data;
        size_t         size;
        size_t         pos;
        bool           failed;
    };

    // PNG Decoder
    class PngLoader
    {
    private:
        PngData pngData;

        // Check PNG signature
        bool CheckSignature(ByteReader& reader);

        // Read a chunk from the byte reader
        Chunk ReadChunk(ByteReader& reader);

        // Parse IHDR chunk
        void ParseIHDR(const Chunk& chunk);

        // Parse PLTE chunk
        void ParsePLTE(const Chunk& chunk);

        // Process IDAT chunks
        void ProcessIDATChunks(const std::vector<Chunk>& idatChunks);

        // Helper functions for byte swapping
        uint32_t SwapEndian(uint32_t value);

        // Calculate Paeth predictor
        int PaethPredictor(int a, int b, int c);

        // Read a 32-bit unsigned integer from the byte reader
        uint32_t ReadUint32(ByteReader& reader);

    public:
        PngLoader();

        // Decode PNG file and return image data
        PngData Load(const std::string& filename);
    };
}

#endif // PNGLOADER_H
