#include <cassert>
#include <cstdio>
#include <vector>
#include <cmath>

#include "MoogFilter.h"


// ``A matrix approach to the Moog ladder filter'',
// Raph Levien 2013
//
// http://levien.com/ladder.pdf

struct Matrix
{
    using size_t = std::size_t;
    Matrix(const Matrix &m)
        :data(m.data), r(m.r), c(m.c)
    {}
    Matrix(std::vector<float> dat, size_t _r, size_t _c)
        :data(dat), r(_r), c(_c)
    {}
    Matrix(size_t _r, size_t _c)
        :data(_r*_c, 0), r(_r), c(_c)
    {}
    float &operator()(size_t _r, size_t _c)
    {
        assert(_r < r);
        assert(_c < c);
        assert(_r >= 0);
        assert(_c >= 0);
        return data[_r*c+_c];
    }
    const float &operator()(size_t _r, size_t _c) const
    {
        assert(_r < r);
        assert(_c < c);
        assert(_r >= 0);
        assert(_c >= 0);
        return data[_r*c+_c];
    }
    Matrix operator*(const Matrix &b)
    {
        Matrix &a = *this;
        assert(a.c == b.r);
        Matrix next(a.r,b.c);
        for(size_t i=0; i<next.r; ++i) {
            for(size_t j=0; j<next.c; ++j) {
                for(size_t k=0; k<r; ++k) {
                    next(i,j) += a(i,k)*b(k,j);
                }
            }
        }
        return next;
    }
    Matrix operator*(float b)
    {
        Matrix &a = *this;
        Matrix next(r,c);
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
        assert(a.c == b.c);
        assert(a.r == b.r);
        for(size_t i=0; i<r; ++i)
            for(size_t j=0; j<c; ++j)
                a(i,j) += b(i,j);

        return a;
    }

    Matrix operator+(const Matrix &b)
    {
        Matrix &a = *this;
        assert(a.c == b.c);
        assert(a.r == b.r);
        Matrix next(r,c);
        for(size_t i=0; i<r; ++i)
            for(size_t j=0; j<c; ++j)
                next(i,j) = a(i,j) + b(i,j);

        return next;
    }

    std::vector<float> data;
    size_t r;
    size_t c;
};

Matrix nle(const Matrix &m)
{
    Matrix out(m.r, m.c);
    for(Matrix::size_t i=0; i<m.r*m.c; ++i)
        out.data[i] = tanhf(m.data[i]);
    return out;
}

struct moog_filter
{
    Matrix B;
    Matrix C;
    Matrix y;
    float k;
};

float step(moog_filter &mf, float x)
{
    mf.y += mf.B*tanhf(x-mf.k*mf.y(3,0)) + mf.C*nle(mf.y);
    return mf.y(3,0);
}

moog_filter make_filter(float alpha, float k, int N)
{
    float a = alpha / (1<<N);
    float b = 1-a;

    std::vector<float> init = {
        1, 0, 0, 0, 0,
        a, b, 0, 0, -k*a,
        0, a, b, 0, 0,
        0, 0, a, b, 0,
        0, 0, 0, a, b
    };

    Matrix m(init, 5, 5);
    for(int i=0; i<N; ++i)
        m = m*m;

    Matrix B(4,1), C(4,4);

    for(Matrix::size_t i=0; i<4; ++i)
        B(i,0) = m(1+i,0);
    
    for(Matrix::size_t i=0; i<4; ++i)
        for(Matrix::size_t j=0; j<4; ++j)
            C(i,j) = m(1+i,1+j);

    for(Matrix::size_t i=0; i<4; ++i)
        C(i,i) -= 1;
    
    for(Matrix::size_t i=0; i<4; ++i)
        C(i,i) += k*B(i,0);

    return moog_filter{B, C, Matrix(4,1), k};
}

std::vector<float> impulse_response(float alpha, float k)
{
    moog_filter filter = make_filter(alpha, k, 10);

    int ir_len = 10000;

    std::vector<float> output;
    output.push_back(step(filter, 1.0f));
    for(int i=0; i<ir_len-1; ++i)
        output.push_back(step(filter, 0.0f));


    moog_filter filter2 = make_filter(2*alpha, k, 10);
    filter2.y = filter.y;
    for(int i=0; i<ir_len-1; ++i)
        output.push_back(step(filter2, 0.0f));

    return output;
}

//int main(int argc, const char **argv)
//{
//    std::vector<float> ir = impulse_response(0.01f, 0.9f);
//    for(int i=0; i<(int)ir.size(); ++i)
//        printf("%d %f\n", i, ir[i]);
//    return 0;
//}

namespace zyn{
MoogFilter::MoogFilter(float Ffreq, float Fq,
        unsigned char non_linear_element,
        unsigned int srate, int bufsize)
    :Filter(srate, bufsize), sr(srate), gain(1.0f)
{
    moog_filter *filter = new moog_filter{Matrix(4,1),Matrix(4,4),Matrix(4,1),0.0f};
    *filter = make_filter(Ffreq/srate, Fq, 10);
    data = filter;

}

MoogFilter::~MoogFilter(void)
{
    moog_filter *filter = (moog_filter*)data;
    delete filter;
}
void MoogFilter::filterout(float *smp)
{
    moog_filter *filter = (moog_filter*)data;
    for(int i=0; i<buffersize; ++i)
        smp[i] = ::step(*filter, gain*smp[i]);
}
void MoogFilter::setfreq(float frequency)
{
    //Dummy
}

void MoogFilter::setfreq_and_q(float frequency, float q_)
{
    moog_filter *old_filter = (moog_filter*)data;
    moog_filter *new_filter = new moog_filter{Matrix(4,1),Matrix(4,4),Matrix(4,1),0.0f};
    *new_filter = make_filter(frequency*3.0f/sr, q_/4.0f, 10);
    new_filter->y = old_filter->y;
    delete old_filter;
    data = new_filter;
}

void MoogFilter::setq(float q_)
{
}

void MoogFilter::setgain(float dBgain)
{
    gain = dB2rap(dBgain);
}
};
