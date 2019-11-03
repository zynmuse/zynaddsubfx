/*
  ZynAddSubFX - a software synthesizer

  Moog Filter.h - Several analog filters (lowpass, highpass...)
  Copyright (C) 2018-2018 Mark McCurry
  Author: Mark McCurry

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.
*/

#pragma once
#include "Filter.h"

namespace zyn {

class MoogFilter:public Filter
{
    public:
        MoogFilter(float Ffreq, float Fq,
                unsigned char non_linear_element,
                unsigned int srate, int bufsize);
        ~MoogFilter() override;
        void filterout(float *smp) override;
        void setfreq(float /*frequency*/) override;
        void setfreq_and_q(float frequency, float q_) override;
        void setq(float /*q_*/) override;

        void setgain(float dBgain) override;
    private:
        struct moog_filter *data;
        struct moog_filter *data_old;
        unsigned sr;
        float gain;
};

}
