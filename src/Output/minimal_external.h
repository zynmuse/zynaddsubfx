/*
  ZynAddSubFX - a software synthesizer

  minimal.cpp - Audio functions for the minimal sequencer
  Copyright (C) 2002 Nasca Octavian Paul
  Author: Nasca Octavian Paul

  This program is free software; you can redistribute it and/or modify
  it under the terms of version 2 of the GNU General Public License
  as published by the Free Software Foundation.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License (version 2 or later) for more details.

  You should have received a copy of the GNU General Public License (version 2)
  along with this program; if not, write to the Free Software Foundation,
  Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA

*/

#ifndef MINIMAL_EXTERNAL_H
#define MINIMAL_EXTERNAL_H

#ifdef __cplusplus
extern "C" {
#endif

// TODO: not needed?
// TODO: not sure whether we need to return void*,
// however, this seems to work
mini::instrument_t *instantiate(unsigned long sample_rate);

#ifdef __cplusplus
}
#endif

#endif // MINIMAL_EXTERNAL_H
