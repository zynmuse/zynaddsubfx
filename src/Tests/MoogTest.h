/*
  ZynAddSubFX - a software synthesizer

  MoogTest.h - CxxTest for Moog Filter
  Copyright (C) 2019-2019 Johannes Lorenz
  Authors: Johannes Lorenz

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.
*/
#include <cxxtest/TestSuite.h>

#include <cmath>

#include "../DSP/MoogFilter.cpp"

class MoogTest:public CxxTest::TestSuite
{
    public:
        void setUp() {}
        void tearDown() {}

        void testMakeFilter()
        {
            float frequency = 1000.0f;
            float q = 1.0f;
            float sr = 44100;
            zyn::moog_filter moog =
                zyn::make_filter(frequency*3.0f/sr, q/4.0f, 10);
#if 0
            std::cout << moog.B << std::endl;
            std::cout << moog.C << std::endl;
            std::cout << moog.y << std::endl;
            std::cout << moog.k << std::endl;
#endif

            std::array<float, 4> aExpB = {
                .066f,
                .002f,
                .000f,
                .000f
            };
            zyn::Matrix<float, 4, 1> expB(aExpB);

            std::array<float, 16> aExpC = {
                -.049f,  .000f, -.001f, -.016f,
                 .064f, -.065f,  .000f, -.001f,
                 .002f,  .064f, -.066f,  .000f,
                 .000f,  .002f,  .064f, -.066f
            };
            zyn::Matrix<float, 4, 4> expC(aExpC);

            std::array<float, 4> aExpY = {
                0.f,
                0.f,
                0.f,
                0.f
            };
            zyn::Matrix<float, 4, 1> expY(aExpY);

            // these test are just to detect unwanted filter changes
            // if the filter changes are voluntary, feel free to change the
            // expectations below
            TS_ASSERT(moog.B.equals(expB, .0005f))
            TS_ASSERT(moog.C.equals(expC, .0005f))
            TS_ASSERT(moog.y.equals(expY, .0005f))
            TS_ASSERT_DELTA(moog.k, .25f, .0005f)
        }
};

