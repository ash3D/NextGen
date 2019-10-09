/*
	performs shading supersampling
	should be injected in shader code - can't implement this as a function because HLSL's 'EvaluateAttributeSnapped' requires immediate shader input
	inputs: 
	- const float2 shadeAALevel
	- shadeResult (not inited)
	- ShadeRegular callback
	- ShadeAA(sampleOffet[, lodBias]) callback
	- SHADE_AA_ENABLE_LOD_BIAS if needed
	- SHADE_AA_POW2_SAMPLE_ALLOCATION is experimental and not working properly yet
*/

// input validation
#ifndef ShadeRegular
#	error ShadeRegular callback is not provided
#endif
#ifndef ShadeAA
#	error ShadeAA callback is not provided
#endif

{
	// SSAA config
	static const uint maxSampleCount = 16, maxAxisSampleCount = min(16, maxSampleCount);
	static const float AAThreshold = 1.5f, skinWidth = /*rcp(max(16, maxSampleCount))*/1.f / max(16, maxSampleCount)/*'max' is probably not mandatory*/;

	// use 'WaveActiveMax' to boost sample count "for free" - dilate max sample count over warp/wavefront (but is it free for texture fetches?)
#ifdef SHADE_AA_ENABLE_LOD_BIAS
	// ensure quad uniform for proper texture fetches, alternative is to use gradients but it is slower
	const float2 fixedAALevel = max(shadeAALevel, max(QuadReadAcrossDiagonal(shadeAALevel), max(QuadReadAcrossX(shadeAALevel), QuadReadAcrossY(shadeAALevel))));
#else
	const float2 fixedAALevel = shadeAALevel;
#endif
	const float2 thickAALevel = clamp(fixedAALevel, 1, maxAxisSampleCount);
	const float targetFlatAALevel = min(WaveActiveMax(thickAALevel.x * thickAALevel.y), maxSampleCount);

	// consider smooth AA <-> regular transition
	[branch]
	if (targetFlatAALevel > AAThreshold)
	{
		const float2 thinAALevel = clamp(fixedAALevel, skinWidth, maxAxisSampleCount);
		const float rescale = sqrt(targetFlatAALevel / (thinAALevel.x * thinAALevel.y));
#if SHADE_AA_POW2_SAMPLE_ALLOCATION
		const float2 AAPow = log2(clamp(thinAALevel * rescale, 1, min(maxAxisSampleCount, targetFlatAALevel))), snappedAAPow = ceil(AAPow), oddWeights = snappedAAPow - AAPow;
		const uint2 sampleGridPow = snappedAAPow, sampleGrid = 1 << sampleGridPow, stride = 1 << 4 - sampleGridPow;
#else
		const uint2 sampleGrid = clamp(uint2(thinAALevel * rescale), 1, min(maxAxisSampleCount, uint(targetFlatAALevel)));
		const float2 stride = 16.f / sampleGrid;
#endif
		const uint sampleCount = sampleGrid.x * sampleGrid.y;
#ifdef SHADE_AA_ENABLE_LOD_BIAS
		// oversampling if < 1
#if 1
		const float2 undersamplingAmount = max(shadeAALevel, 1) / sampleGrid;
#else
		const float2 undersamplingAmount = shadeAALevel / sampleGrid;
#endif
		/*
		too noisy normal variation, pow smooths things out which leads to better look although it limits lodBias effect
		to get rid of pow hack better quality normal variation estimation needed
		*/
		const float lodBias = log2(pow(max(undersamplingAmount.x, undersamplingAmount.y), .1f));
#endif
		shadeResult = 0;

		// use flat loop (not nested 2D) in order to reduce branching divergence for differently shaped sampling grids having same overall sample count
		for (uint i = 0; i < sampleCount; i++)
		{
#if SHADE_AA_POW2_SAMPLE_ALLOCATION
			const uint2 idx2D = { i & sampleGridPow.x, i >> sampleGridPow.x };
			const float2 weights = lerp(1, sampleGridPow != 1 ^ idx2D & 1 ? 2 : 0, oddWeights);
			const float weight = weights.x * weights.y;
			const int2 sampleOffset = idx2D * stride - (sampleGridPow ? 8 : 0);
#ifdef SHADE_AA_ENABLE_LOD_BIAS
			shadeResult += ShadeAA(sampleOffset, lodBias) * weight;
#else
			shadeResult += ShadeAA(sampleOffset) * weight;
#endif
#else
			const int2 sampleOffset = uint2(i % sampleGrid.x, i / sampleGrid.x) * stride + stride * .5f - 8.5f/*'-.5' compensates 'stride * .5f' and snaps to 16x16 grid for 'sampleCount = 16' case*/;
#ifdef SHADE_AA_ENABLE_LOD_BIAS
			shadeResult += ShadeAA(sampleOffset, lodBias);
#else
			shadeResult += ShadeAA(sampleOffset);
#endif
#endif
		}

		// weighting can also be done in loop for free (mad instead of add) but provided shading routine can potentially perform mul as last op thus making weighting not free anymore
		shadeResult /= sampleCount;
	}
	else
		shadeResult = ShadeRegular;
}

// cleanup
#undef ShadeRegular
#undef ShadeAA
#undef SHADE_AA_ENABLE_LOD_BIAS
#undef SHADE_AA_POW2_SAMPLE_ALLOCATION