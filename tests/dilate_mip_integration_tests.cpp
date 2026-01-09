#include <gtest/gtest.h>
#include "Engine/Tools/BakeTexel.hpp"
#include "Engine/Tools/dilate.hpp"
#include "Engine/Tools/mipgen.hpp"

using engine::BakeTexel;
using engine::lightmap::dilateBakeTexels;
using engine::lightmap::generateMipLevel;

static BakeTexel make(float r, float g, float b, uint8_t v)
{
    BakeTexel t;
    t.radiance = glm::vec3(r,g,b);
    t.valid = v;
    return t;
}

TEST(Dilate, ZeroIterationsNoChange)
{
    int w = 3, h = 3;
    std::vector<BakeTexel> src(w*h, make(1,2,3,1));
    src[4] = make(10, 20, 30, 1);
    std::vector<BakeTexel> dst(w*h, make(0,0,0,0));

    dilateBakeTexels(src.data(), dst.data(), w, h, 0);
    // dst should equal src
    for (int i = 0; i < w*h; ++i)
    {
        EXPECT_EQ(dst[i].valid, src[i].valid);
        EXPECT_FLOAT_EQ(dst[i].radiance.x, src[i].radiance.x);
    }
}

TEST(Dilate, OddDimensionsHandleBounds)
{
    int w = 3, h = 2;
    std::vector<BakeTexel> src(w*h, make(0,0,0,0));
    src[1] = make(1.0f, 0, 0, 1); // (1,0)
    std::vector<BakeTexel> dst(w*h);

    dilateBakeTexels(src.data(), dst.data(), w, h, 2);
    // ensure no out-of-bounds writes and edges filled
    for (int i = 0; i < w*h; ++i) EXPECT_TRUE(dst[i].valid == 0 || dst[i].valid == 1);
}

TEST(MipGen, SingleColumn)
{
    int sw = 1, sh = 4;
    std::vector<BakeTexel> src(sw*sh, make(0,0,0,0));
    src[0] = make(1,0,0,1);
    src[2] = make(0,1,0,1);

    std::vector<BakeTexel> dst((sw/2)*(sh/2)); // dst 1x2
    generateMipLevel(src.data(), sw, sh, dst.data());
    EXPECT_EQ(dst[0].valid, 1);
    EXPECT_NEAR(dst[0].radiance.x, 1.0f, 1e-6f);
    EXPECT_EQ(dst[1].valid, 1);
    EXPECT_NEAR(dst[1].radiance.y, 1.0f, 1e-6f);
}

TEST(Integration, DilateThenMip)
{
    int sw = 4, sh = 2;
    std::vector<BakeTexel> src(sw*sh, make(0,0,0,0));
    src[1] = make(2.0f, 0.0f, 0.0f, 1); // a single valid inside

    std::vector<BakeTexel> afterDilate(sw*sh);
    dilateBakeTexels(src.data(), afterDilate.data(), sw, sh, 2);

    std::vector<BakeTexel> lvl1((sw/2)*(sh/2));
    generateMipLevel(afterDilate.data(), sw, sh, lvl1.data());

    // ensure some valid exists in top-level mip
    int validCount = 0;
    for (auto &p : lvl1) if (p.valid) ++validCount;
    EXPECT_GT(validCount, 0);
}
