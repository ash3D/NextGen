/**
\author		Alexey Shaydurov aka ASH
\date		23.06.2018 (c)Korotkov Andrey

This file is a part of DGLE project and is distributed
under the terms of the GNU Lesser General Public License.
See "DGLE.h" for more details.
*/

#include "stdafx.h"
#include "sun.h"

using namespace std;
using namespace Renderer;
using namespace Sun::HLSL;

// from http://www.cs.utah.edu/~shirley/papers/sunsky/sunsky.pdf, [380-780] nm range, 10 nm step, last 3 samples (missed in original table) are extrapolated (is this correct?)
constexpr double spectralRadianceRawTable[] =
{
	1655.9,
	1623.37,
	2112.75,
	2588.82,
	2582.91,
	2423.23,
	2676.05,
	2965.83,
	3054.54,
	3005.75,
	3066.37,
	2883.04,
	2871.21,
	2782.5,
	2710.06,
	2723.36,
	2636.13,
	2550.38,
	2506.02,
	2531.16,
	2535.59,
	2513.42,
	2463.15,
	2417.32,
	2368.53,
	2321.21,
	2282.77,
	2233.98,
	2197.02,
	2152.67,
	2109.79,
	2072.83,
	2024.04,
	1987.08,
	1942.72,
	1907.24,
	1862.89,
	1825.92,
	1788.95,
	1751.98,
	1715.01
};

// replace with C++20 template lambda
template<size_t ...idx>
static constexpr array<double, sizeof...(idx)> MakeSpectralRadianceTable(index_sequence<idx...>)
{
	const auto ResampleRadiance = [](size_t i) constexpr
	{
		const auto RemapRadiance = [](double r) constexpr
		{
			return r * 1e10;	// cm->m x cm->m x μm->m (2+2+6)
		};
		double result = RemapRadiance(spectralRadianceRawTable[i >> 1]);
		if (i & 1)
		{
			result += RemapRadiance(spectralRadianceRawTable[(i >> 1) + 1]);
			result *= .5;
		}
		return result;
	};
	return { ResampleRadiance(idx)... };
}

constexpr auto spectralRadianceTable = MakeSpectralRadianceTable(make_index_sequence<extent_v<decltype(spectralRadianceRawTable)> * 2 - 1>());

// CIE 1964 supplementary standard colorimetric observer\
[380-780] nm range, 5 nm step\
!: consider constexpr ctors (and other stuff) for vector math
const double3 CIE_colorMatchingTable[] =
{
	{0.000160, 0.000017, 0.000705},
	{0.000662, 0.000072, 0.002928},
	{0.002362, 0.000253, 0.010482},
	{0.007242, 0.000769, 0.032344},
	{0.019110, 0.002004, 0.086011},
	{0.043400, 0.004509, 0.197120},
	{0.084736, 0.008756, 0.389366},
	{0.140638, 0.014456, 0.656760},
	{0.204492, 0.021391, 0.972542},
	{0.264737, 0.029497, 1.282500},
	{0.314679, 0.038676, 1.553480},
	{0.357719, 0.049602, 1.798500},
	{0.383734, 0.062077, 1.967280},
	{0.386726, 0.074704, 2.027300},
	{0.370702, 0.089456, 1.994800},
	{0.342957, 0.106256, 1.900700},
	{0.302273, 0.128201, 1.745370},
	{0.254085, 0.152761, 1.554900},
	{0.195618, 0.185190, 1.317560},
	{0.132349, 0.219940, 1.030200},
	{0.080507, 0.253589, 0.772125},
	{0.041072, 0.297665, 0.570060},
	{0.016172, 0.339133, 0.415254},
	{0.005132, 0.395379, 0.302356},
	{0.003816, 0.460777, 0.218502},
	{0.015444, 0.531360, 0.159249},
	{0.037465, 0.606741, 0.112044},
	{0.071358, 0.685660, 0.082248},
	{0.117749, 0.761757, 0.060709},
	{0.172953, 0.823330, 0.043050},
	{0.236491, 0.875211, 0.030451},
	{0.304213, 0.923810, 0.020584},
	{0.376772, 0.961988, 0.013676},
	{0.451584, 0.982200, 0.007918},
	{0.529826, 0.991761, 0.003988},
	{0.616053, 0.999110, 0.001091},
	{0.705224, 0.997340, 0.000000},
	{0.793832, 0.982380, 0.000000},
	{0.878655, 0.955552, 0.000000},
	{0.951162, 0.915175, 0.000000},
	{1.014160, 0.868934, 0.000000},
	{1.074300, 0.825623, 0.000000},
	{1.118520, 0.777405, 0.000000},
	{1.134300, 0.720353, 0.000000},
	{1.123990, 0.658341, 0.000000},
	{1.089100, 0.593878, 0.000000},
	{1.030480, 0.527963, 0.000000},
	{0.950740, 0.461834, 0.000000},
	{0.856297, 0.398057, 0.000000},
	{0.754930, 0.339554, 0.000000},
	{0.647467, 0.283493, 0.000000},
	{0.535110, 0.228254, 0.000000},
	{0.431567, 0.179828, 0.000000},
	{0.343690, 0.140211, 0.000000},
	{0.268329, 0.107633, 0.000000},
	{0.204300, 0.081187, 0.000000},
	{0.152568, 0.060281, 0.000000},
	{0.112210, 0.044096, 0.000000},
	{0.081261, 0.031800, 0.000000},
	{0.057930, 0.022602, 0.000000},
	{0.040851, 0.015905, 0.000000},
	{0.028623, 0.011130, 0.000000},
	{0.019941, 0.007749, 0.000000},
	{0.013842, 0.005375, 0.000000},
	{0.009577, 0.003718, 0.000000},
	{0.006605, 0.002565, 0.000000},
	{0.004553, 0.001768, 0.000000},
	{0.003145, 0.001222, 0.000000},
	{0.002175, 0.000846, 0.000000},
	{0.001506, 0.000586, 0.000000},
	{0.001045, 0.000407, 0.000000},
	{0.000727, 0.000284, 0.000000},
	{0.000508, 0.000199, 0.000000},
	{0.000356, 0.000140, 0.000000},
	{0.000251, 0.000098, 0.000000},
	{0.000178, 0.000070, 0.000000},
	{0.000126, 0.000050, 0.000000},
	{0.000090, 0.000036, 0.000000},
	{0.000065, 0.000025, 0.000000},
	{0.000046, 0.000018, 0.000000},
	{0.000033, 0.000013, 0.000000}
};

// XYZ -> sRGB D65 transformation matrix from http://www.brucelindbloom.com/index.html?Eqn_RGB_XYZ_Matrix.html
const double3x3 XYZ_2_RGB
(
	+3.2404542, -1.5371385, -0.4985314,
	-0.9692660, +1.8760108, +0.0415560,
	+0.0556434, -0.2040259, +1.0572252
);

const double3 sunExtraterrestrialRadianceRGB = []
{
	static_assert(tuple_size_v<decltype(spectralRadianceTable)> == extent_v<decltype(CIE_colorMatchingTable)>);
	const double3 XYZ = inner_product(begin(spectralRadianceTable), end(spectralRadianceTable), begin(CIE_colorMatchingTable), double3()) * 5e-9/*dλ (5nm step in m)*/;
	return mul(XYZ_2_RGB, XYZ);
}();

float3 Sun::Dir(float zenith, float azimuth)
{
	float3 dir(cos(azimuth), sin(azimuth), cos(zenith));
	dir.xy *= sin(zenith);
	return dir;
}

float3 Sun::Radiance(float zenith, float cosZenith)
{
	// calculate rcp of relative optical mass
	const double m_rcp = cosZenith + .15 * pow(93.885 - 180 * M_1_PI * fabs(zenith), -1.253);

	// from http://www.babelcolor.com/index_htm_files/A%20review%20of%20RGB%20color%20spaces.pdf
	static const double3 RGB_primaries(.610, .530, .450);	// in μm

	// Rayleigh scattering
	using namespace std::placeholders;
	static const double3 RayleighFactor = -.008735 * RGB_primaries.apply(bind(powl/*to eliminate ambiguity of pow*/, _1, -4.08));
	return sunExtraterrestrialRadianceRGB * (RayleighFactor / m_rcp).apply(exp);
}