#include "PngLoader.h"
#include "PlatformFileSystem.h"
#include <algorithm>
#include <stdexcept>
#include <cstring>
#include <iostream>
#include <zlib.h>

namespace CC
{
    // Constructor
    PngLoader::PngLoader() : pngData{}
    {
        pngData.hasPalette = false;
    }

    // Helper function for byte swapping
    uint32_t PngLoader::SwapEndian(uint32_t value)
    {
        return ((value & 0xFF) << 24) |
            ((value & 0xFF00) << 8) |
            ((value & 0xFF0000) >> 8) |
            ((value & 0xFF000000) >> 24);
    }

    // Read a 32-bit unsigned integer from the byte reader
    uint32_t PngLoader::ReadUint32(ByteReader& reader)
    {
        uint32_t value = 0;
        reader.Read(&value, 4);
        return SwapEndian(value);
    }

    // Calculate Paeth predictor
    int PngLoader::PaethPredictor(int a, int b, int c)
    {
        int p = a + b - c;
        int pa = std::abs(p - a);
        int pb = std::abs(p - b);
        int pc = std::abs(p - c);

        if (pa <= pb && pa <= pc)
        {
            return a;
        }
        if (pb <= pc)
        {
            return b;
        }
        return c;
    }

    // Check PNG signature
    bool PngLoader::CheckSignature(ByteReader& reader)
    {
        uint8_t signature[8];
        reader.Read(signature, 8);
        return (memcmp(signature, PNG_SIGNATURE, 8) == 0);
    }

    // Read a chunk from the byte reader
    Chunk PngLoader::ReadChunk(ByteReader& reader)
    {
        Chunk chunk;
        chunk.length = ReadUint32(reader);
        reader.Read(&chunk.type, 4);
        chunk.type = SwapEndian(chunk.type);

        chunk.data.resize(chunk.length);
        if (chunk.length > 0)
        {
            reader.Read(chunk.data.data(), chunk.length);
        }

        reader.Read(&chunk.crc, 4);
        chunk.crc = SwapEndian(chunk.crc);

        return chunk;
    }

    // Parse IHDR chunk
    void PngLoader::ParseIHDR(const Chunk& chunk)
    {
        if (chunk.length != 13)
        {
            throw std::runtime_error("Invalid IHDR chunk size");
        }

        const uint8_t* data = chunk.data.data();
        pngData.header.width = (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
        pngData.header.height = (data[4] << 24) | (data[5] << 16) | (data[6] << 8) | data[7];
        pngData.header.bitDepth = data[8];
        pngData.header.colorType = data[9];
        pngData.header.compressionMethod = data[10];
        pngData.header.filterMethod = data[11];
        pngData.header.interlaceMethod = data[12];

        // Validate some restrictions
        if (pngData.header.compressionMethod != 0)
        {
            throw std::runtime_error("Unsupported compression method");
        }

        if (pngData.header.filterMethod != 0)
        {
            throw std::runtime_error("Unsupported filter method");
        }

        if (pngData.header.interlaceMethod != 0)
        {
            throw std::runtime_error("Interlaced images not supported");
        }
    }

    // Parse PLTE chunk
    void PngLoader::ParsePLTE(const Chunk& chunk)
    {
        if (chunk.length % 3 != 0)
        {
            throw std::runtime_error("Invalid PLTE chunk size");
        }

        const uint8_t* data = chunk.data.data();
        uint32_t entries = chunk.length / 3;
        pngData.palette.resize(entries);

        for (uint32_t i = 0; i < entries; i++)
        {
            uint8_t r = data[i * 3];
            uint8_t g = data[i * 3 + 1];
            uint8_t b = data[i * 3 + 2];
            pngData.palette[i] = (0xFF << 24) | (b << 16) | (g << 8) | r; // RGBA in memory
        }

        pngData.hasPalette = true;
    }

    // Process IDAT chunks
    void PngLoader::ProcessIDATChunks(const std::vector<Chunk>& idatChunks)
    {
        // Calculate total compressed data size
        size_t totalSize = 0;
        for (const auto& chunk : idatChunks)
        {
            totalSize += chunk.length;
        }

        // Concatenate all IDAT chunks
        std::vector<uint8_t> compressedData;
        compressedData.reserve(totalSize);

        for (const auto& chunk : idatChunks)
        {
            compressedData.insert(compressedData.end(), chunk.data.begin(), chunk.data.end());
        }

        // Decompress data using zlib
        z_stream infstream;
        memset(&infstream, 0, sizeof(infstream));

        infstream.avail_in = static_cast<uInt>(compressedData.size());
        infstream.next_in = compressedData.data();

        if (inflateInit(&infstream) != Z_OK)
        {
            throw std::runtime_error("Failed to initialize zlib inflation");
        }

        // Calculate expected uncompressed size
        size_t bytesPerPixel = 0;
        switch (pngData.header.colorType)
        {
        case GRAYSCALE:
            bytesPerPixel = pngData.header.bitDepth / 8;
            if (bytesPerPixel == 0)
            {
                bytesPerPixel = 1;
            }
            break;
        case RGB:
            bytesPerPixel = 3 * pngData.header.bitDepth / 8;
            break;
        case PALETTE:
            bytesPerPixel = 1;
            break;
        case GRAYSCALE_ALPHA:
            bytesPerPixel = 2 * pngData.header.bitDepth / 8;
            break;
        case RGBA:
            bytesPerPixel = 4 * pngData.header.bitDepth / 8;
            break;
        default:
            throw std::runtime_error("Unsupported color type");
        }

        size_t scanlineSize = pngData.header.width * bytesPerPixel;
        size_t expectedSize = (scanlineSize + 1) * pngData.header.height; // +1 for filter type byte

        std::vector<uint8_t> decompressedData(expectedSize);
        infstream.avail_out = static_cast<uInt>(decompressedData.size());
        infstream.next_out = decompressedData.data();

        int ret = inflate(&infstream, Z_FINISH);
        if (ret != Z_STREAM_END && ret != Z_OK)
        {
            inflateEnd(&infstream);
            throw std::runtime_error("Failed to decompress image data");
        }

        size_t decompressedSize = decompressedData.size() - infstream.avail_out;
        decompressedData.resize(decompressedSize);

        inflateEnd(&infstream);

        // Process scanlines and apply filters
        std::vector<uint8_t> unfilteredData;
        unfilteredData.reserve(scanlineSize * pngData.header.height);

        for (uint32_t y = 0; y < pngData.header.height; y++)
        {
            size_t scanlineStart = y * (scanlineSize + 1);
            uint8_t filterType = decompressedData[scanlineStart];

            std::vector<uint8_t> currentScanline(scanlineSize);
            std::vector<uint8_t> previousScanline(scanlineSize, 0);

            if (y > 0)
            {
                std::copy(unfilteredData.end() - scanlineSize, unfilteredData.end(), previousScanline.begin());
            }

            // Apply reverse filter
            for (size_t x = 0; x < scanlineSize; x++)
            {
                uint8_t filterByte = decompressedData[scanlineStart + 1 + x];
                uint8_t left = x >= bytesPerPixel ? currentScanline[x - bytesPerPixel] : 0;
                uint8_t above = previousScanline[x];
                uint8_t aboveLeft = x >= bytesPerPixel ? previousScanline[x - bytesPerPixel] : 0;

                switch (filterType)
                {
                case NONE:
                    currentScanline[x] = filterByte;
                    break;
                case SUB:
                    currentScanline[x] = filterByte + left;
                    break;
                case UP:
                    currentScanline[x] = filterByte + above;
                    break;
                case AVERAGE:
                    currentScanline[x] = filterByte + ((left + above) / 2);
                    break;
                case PAETH:
                    currentScanline[x] = filterByte + PaethPredictor(left, above, aboveLeft);
                    break;
                default:
                    throw std::runtime_error("Unknown filter type");
                }
            }

            unfilteredData.insert(unfilteredData.end(), currentScanline.begin(), currentScanline.end());
        }

        // Convert to RGBA format for OpenGL
        pngData.imageData.resize(pngData.header.width * pngData.header.height * 4);

        for (uint32_t y = 0; y < pngData.header.height; y++)
        {
            for (uint32_t x = 0; x < pngData.header.width; x++)
            {
                size_t srcPos = (y * pngData.header.width + x) * bytesPerPixel;
                size_t dstPos = ((pngData.header.height - 1 - y) * pngData.header.width + x) * 4; // Flip Y for OpenGL

                switch (pngData.header.colorType)
                {
                case GRAYSCALE:
                {
                    uint8_t gray = unfilteredData[srcPos];
                    pngData.imageData[dstPos] = gray;
                    pngData.imageData[dstPos + 1] = gray;
                    pngData.imageData[dstPos + 2] = gray;
                    pngData.imageData[dstPos + 3] = 255;
                    break;
                }
                case RGB:
                {
                    pngData.imageData[dstPos] = unfilteredData[srcPos];
                    pngData.imageData[dstPos + 1] = unfilteredData[srcPos + 1];
                    pngData.imageData[dstPos + 2] = unfilteredData[srcPos + 2];
                    pngData.imageData[dstPos + 3] = 255;
                    break;
                }
                case PALETTE:
                {
                    if (!pngData.hasPalette)
                    {
                        throw std::runtime_error("Palette color type but no palette found");
                    }
                    uint8_t index = unfilteredData[srcPos];
                    if (index >= pngData.palette.size())
                    {
                        throw std::runtime_error("Palette index out of bounds");
                    }
                    uint32_t color = pngData.palette[index];
                    pngData.imageData[dstPos] = color & 0xFF;
                    pngData.imageData[dstPos + 1] = (color >> 8) & 0xFF;
                    pngData.imageData[dstPos + 2] = (color >> 16) & 0xFF;
                    pngData.imageData[dstPos + 3] = (color >> 24) & 0xFF;
                    break;
                }
                case GRAYSCALE_ALPHA:
                {
                    uint8_t gray = unfilteredData[srcPos];
                    uint8_t alpha = unfilteredData[srcPos + 1];
                    pngData.imageData[dstPos] = gray;
                    pngData.imageData[dstPos + 1] = gray;
                    pngData.imageData[dstPos + 2] = gray;
                    pngData.imageData[dstPos + 3] = alpha;
                    break;
                }
                case RGBA:
                {
                    pngData.imageData[dstPos] = unfilteredData[srcPos];
                    pngData.imageData[dstPos + 1] = unfilteredData[srcPos + 1];
                    pngData.imageData[dstPos + 2] = unfilteredData[srcPos + 2];
                    pngData.imageData[dstPos + 3] = unfilteredData[srcPos + 3];
                    break;
                }
                }
            }
        }
    }

    // Decode PNG file
    PngData PngLoader::Load(const std::string& filename)
    {
        std::vector<uint8_t> fileBytes;
        if (!PlatformFileSystem::Get()->ReadFileBinary(filename.c_str(), fileBytes))
        {
            throw std::runtime_error("Failed to open file: " + filename);
        }

        ByteReader reader(fileBytes.data(), fileBytes.size());

        if (!CheckSignature(reader))
        {
            throw std::runtime_error("Invalid PNG signature");
        }

        std::vector<Chunk> idatChunks;
        bool foundIHDR = false;
        bool foundIEND = false;

        while (!foundIEND && reader.Good())
        {
            Chunk chunk = ReadChunk(reader);

            switch (chunk.type)
            {
            case IHDR:
                if (foundIHDR)
                {
                    throw std::runtime_error("Multiple IHDR chunks found");
                }
                ParseIHDR(chunk);
                foundIHDR = true;
                break;

            case PLTE:
                ParsePLTE(chunk);
                break;

            case IDAT:
                idatChunks.push_back(chunk);
                break;

            case IEND:
                foundIEND = true;
                break;

            default:
                // Skip other chunk types
                break;
            }
        }

        if (!foundIHDR || idatChunks.empty() || !foundIEND)
        {
            throw std::runtime_error("Missing required PNG chunks");
        }

        ProcessIDATChunks(idatChunks);

        return pngData;
    }
}
