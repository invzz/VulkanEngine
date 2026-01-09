#include <gtest/gtest.h>
#include "Engine/Tools/BakeTexel.hpp"
#include "Engine/Tools/dilate.hpp"
#include "Engine/Tools/mipgen.hpp"

using engine::BakeTexel;
using engine::lightmap::dilateBakeTexels;
using engine::lightmap::generateMipLevel;

TEST(Guards, DilateHandlesNullSrcOrZeroSize)
{
    // Should not crash
    dilateBakeTexels(nullptr, nullptr, 0, 0, 1);

    // Valid src but null dst should return safely
    std::vector<BakeTexel> src(4);
    src[0].valid = 1;
    dilateBakeTexels(src.data(), nullptr, 2, 2, 1);
}

TEST(Guards, MipgenHandlesNullDstOrZeroSize)
{
    // Should not crash
    generateMipLevel(nullptr, 0, 0, nullptr);

    std::vector<BakeTexel> src(4);
    src[0].valid = 1;
    generateMipLevel(src.data(), 2, 2, nullptr);
}
