#ifndef TONEMAP_PARAMS_INCLUDED
#define TONEMAP_PARAMS_INCLUDED

struct TonemapParams
{
    float exposure, whitePoint, whitePointFactor /*1/whitePoint^2*/;
};

#endif	// TONEMAP_PARAMS_INCLUDED