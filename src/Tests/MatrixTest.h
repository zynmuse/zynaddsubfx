/*
  ZynAddSubFX - a software synthesizer

  MatrixTest.h - CxxTest for Matrix
  Copyright (C) 2019-2019 Johannes Lorenz
  Authors: Johannes Lorenz

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.
*/
#include <cxxtest/TestSuite.h>

#include "../DSP/MoogFilter.cpp"

class MatrixTest:public CxxTest::TestSuite
{
    public:
        void setUp() {}
        void tearDown() {}

        void testMult()
        {
            Matrix<3,3> mobile(aMobile);
            Matrix<3,3> id(aId3);
            array_t<9> aExp = {
                30, 36, 42,
                66, 81, 96,
                102, 126, 150
            };
            Matrix<3,3> exp(aExp);

            TS_ASSERT_EQUALS(mobile * id, mobile)
            TS_ASSERT_EQUALS(id * mobile, mobile)
            TS_ASSERT_EQUALS(mobile * mobile, exp)
        }

        void testApply()
        {
            auto func = [](base_t x) -> base_t { return 10 - x; };
            Matrix<3,3> mobile(aMobile);
            array_t<9> aExp = {
                9, 8, 7,
                6, 5, 4,
                3, 2, 1
            };
            Matrix<3,3> exp(aExp);

            TS_ASSERT_EQUALS(mobile.apply(func), exp)
        }

    private:
        using base_t = long long int;
        template<std::size_t N>
        using array_t = std::array<base_t, N>;
        template<std::size_t r, std::size_t c>
        using Matrix = zyn::Matrix<base_t, r, c>;

        const array_t<9> aMobile = {
          1, 2, 3,
          4, 5, 6,
          7, 8, 9 };

        const array_t<9> aId3 = {
          1, 0, 0,
          0, 1, 0,
          0, 0, 1 };

};

