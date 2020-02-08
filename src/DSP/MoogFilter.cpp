#include <cstdio>
#include <vector>
#include <cmath>

#include "../Misc/Matrix.h"
#include "MoogFilter.h"

namespace zyn{

// ``A matrix approach to the Moog ladder filter'',
// Raph Levien 2013
//
// http://levien.com/ladder.pdf

template<class T, std::size_t r, std::size_t c>
Matrix<T, r, c> nle(const Matrix<T, r, c> &m)
{
    // encapsulate tanhf for the case it's a macro
    return m.apply([](float x) -> float { return tanhf(x); });
}

struct moog_filter
{
    Matrix<float, 4, 1> B;
    Matrix<float, 4, 4> C;
    Matrix<float, 4, 1> y;
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

    std::array<float, 25> init = {
        1, 0, 0, 0, 0,
        a, b, 0, 0, -k*a,
        0, a, b, 0, 0,
        0, 0, a, b, 0,
        0, 0, 0, a, b
    };

    Matrix<float, 5,5> m(init);
    for(int i=0; i<N; ++i)
        m = m*m;

    Matrix<float, 4,1> B;
    Matrix<float, 4,4> C;

    for(std::size_t i=0; i<4; ++i)
        B(i,0) = m(1+i,0);
    
    for(std::size_t i=0; i<4; ++i)
        for(std::size_t j=0; j<4; ++j)
            C(i,j) = m(1+i,1+j);

    for(std::size_t i=0; i<4; ++i)
        C(i,i) -= 1;

    for(std::size_t i=0; i<4; ++i)
        C(i,i) += k*B(i,0);

    return moog_filter{B, C, Matrix<float, 4,1>(), k};
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

// revert parameters from "baseq" calculation (FilterParams.cpp)
// output shall be linear to the Pq parameter from FilterParams
static float mapQ(float input)
{
    // "* 2.0": Adding more distortion than 1.0 makes interesting
    //          effects and just sounds good
    return powf(logf(input + .9f) / logf(1000.f), 1.f/2.f) * 2.0f;
}

MoogFilter::MoogFilter(float Ffreq, float Fq,
        unsigned char non_linear_element,
        unsigned int srate, int bufsize)
    :Filter(srate, bufsize), sr(srate), gain(1.0f)
{
    (void) non_linear_element;
    moog_filter *filter = new moog_filter{Matrix<float,4,1>(),Matrix<float,4,4>(),Matrix<float,4,1>(),0.0f};
    *filter = make_filter(Ffreq/srate, mapQ(Fq), 10);
    data = filter;
}

MoogFilter::~MoogFilter(void)
{
    delete data;
}
void MoogFilter::filterout(float *smp)
{
    moog_filter *filter = data;
    for(int i=0; i<buffersize; ++i)
        smp[i] = zyn::step(*filter, gain*smp[i]);
}
void MoogFilter::setfreq(float /*frequency*/)
{
    //Dummy
}

void MoogFilter::setfreq_and_q(float frequency, float q_)
{
    // TODO: avoid allocation?
    moog_filter *old_filter = data;
    moog_filter *new_filter = new moog_filter{Matrix<float,4,1>(),Matrix<float,4,4>(),Matrix<float,4,1>(),0.0f};
    *new_filter = make_filter(frequency*3.0f/sr, mapQ(q_), 10);
    new_filter->y = old_filter->y;
    delete old_filter;
    data = new_filter;
}

void MoogFilter::setq(float /*q_*/)
{
}

void MoogFilter::setgain(float dBgain)
{
    gain = dB2rap(dBgain);
}
};
