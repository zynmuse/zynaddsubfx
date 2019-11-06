/*
  ZynAddSubFX - a software synthesizer

  Matrix.h - Basic matrix template class
  Copyright (C) 2019-2019 Johannes Lorenz
  Author: Johannes Lorenz

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.
*/

#ifndef MATRIX_H
#define MATRIX_H

#include <array>
#include <cassert>
#include <cstddef>
#include <iostream>

namespace zyn {

template<class T, std::size_t r, std::size_t c>
class Matrix
{
public:
    using size_t = std::size_t;
    Matrix(const Matrix<T, r, c> &m) = default;
    Matrix(Matrix<T, r, c> &m) = default;
    Matrix(std::array<T, r*c> dat)
            :data(dat)
        {}
    Matrix()
    {
        std::fill(data.begin(), data.end(), 0.f);
    }
    Matrix& operator=(const Matrix<T, r, c> &m) = default;
    Matrix& operator=(Matrix<T, r, c>&& m) = default;

    T &operator()(size_t _r, size_t _c)
    {
        assert(_r < r);
        assert(_c < c);
        assert(_r >= 0);
        assert(_c >= 0);
        return data[_r*c+_c];
    }
    const T &operator()(size_t _r, size_t _c) const
    {
        assert(_r < r);
        assert(_c < c);
        assert(_r >= 0);
        assert(_c >= 0);
        return data[_r*c+_c];
    }

    template<size_t rhsC>
    Matrix<T, r, rhsC> operator*(const Matrix<T, c, rhsC> &b) const
    {
        const Matrix &a = *this;
        Matrix<T, r, rhsC> next;
        for(size_t i=0; i<r; ++i) {
            for(size_t j=0; j<rhsC; ++j) {
                for(size_t k=0; k<r; ++k) {
                    next(i,j) += a(i,k)*b(k,j);
                }
            }
        }
        return next;
    }

    Matrix operator*(T b)
    {
        Matrix &a = *this;
        Matrix next;
        for(size_t i=0; i<r; ++i) {
            for(size_t j=0; j<c; ++j) {
                next(i,j) = a(i,j)*b;
            }
        }
        return next;
    }

    Matrix &operator+=(const Matrix &b)
    {
        Matrix &a = *this;
        for(size_t i=0; i<r; ++i)
            for(size_t j=0; j<c; ++j)
                a(i,j) += b(i,j);

        return a;
    }

    Matrix operator+(const Matrix &b) const
    {
        const Matrix &a = *this;
        Matrix next;
        for(size_t i=0; i<r; ++i)
            for(size_t j=0; j<c; ++j)
                next(i,j) = a(i,j) + b(i,j);

        return next;
    }

    Matrix apply(T (*fptr)(T)) const
    {
        Matrix out;
        for(size_t i=0; i<r*c; ++i)
            out.data[i] = (*fptr)(data[i]);
        return out;
    }

    bool operator==(const Matrix& rhs) const
    {
        return data == rhs.data;
    }

    //! Check if every value in other differs at most @p delta
    bool equals(const Matrix& other, const T& delta)
    {
        const T absDelta = fabs(delta);
        bool equal = true;
        for(size_t i=0; equal && i<r*c; ++i)
            equal = fabs(data[i] - other.data[i]) < absDelta;
        return equal;
    }

private:
    std::array<T, r*c> data;
};

template<class T, std::size_t r, std::size_t c>
std::ostream& operator<<(std::ostream& stream, const Matrix<T,r,c>& m)
{
    for(size_t i=0; i<r; ++i)
    {
        for(size_t j=0; j<c; ++j)
        {
            stream << m(i, j);
            if(j != c-1)
                stream << "  ";
        }
        stream << std::endl;
    }
    return stream;
}

}

#endif // MATRIX_H
