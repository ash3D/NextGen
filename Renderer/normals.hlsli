#pragma once

#define SMOOTH_PARALLAX 1
#define PRESERVE_PARALLAX_PLANE 1

namespace Normals
{
	static const float2 parallaxCurvatureParams = float2(.1f, 1.3f);

	// preserve 0
	float3 SafeNormalize(float3 vec)
	{
		vec = normalize(vec);
		return isfinite(vec) ? vec : 0;
	}

	/*
		makes n frontfacing so that it won't appear dark (with proper light direction)
		backface ideally should be never visible and advanced techniques such as POM can ensure it
			by picking n at view ray intersection point
		here is approximation which simulates parallax for micro normal (normal mapping) relative to macro normal
		assume single-sided/symmetric bump (blended via parallaxCurvatureParams.x)
	*/
	void FixNormal(in float3 N, inout float3 n, in float3 viewDir)
	{
		const float VdotN = dot(viewDir, n);	// recompute later to reduce GPR pressure?
		if (VdotN < 0)
		{
#if 1	// groove
			const float3 reflected = lerp(N, reflect(n, N), parallaxCurvatureParams.x);
#if SMOOTH_PARALLAX && PRESERVE_PARALLAX_PLANE
#define FP_DEGENERATE_POSSIBLE
			const float3 perp = cross(cross(N, n)/*could be 0 in degenerate case*/, viewDir);
#endif
#else	// cone
			// reflect off plane holding N and orthogonal to <N, viewDir> plane
#define FP_DEGENERATE_POSSIBLE
			const float3 plane = normalize(cross(cross(viewDir, N), N));	// could be 0 in degenerate case
			const float3 reflected = n - (1 + parallaxCurvatureParams.x) * dot(n, plane) * plane;
#if SMOOTH_PARALLAX && PRESERVE_PARALLAX_PLANE
			const float3 perp = cross(cross(reflected, n), viewDir);
#endif
#endif

#if SMOOTH_PARALLAX
#if !PRESERVE_PARALLAX_PLANE
			const float3 perp = n - dot(n, viewDir) * viewDir;	// do not normalize to prevent degenerate cases (+ perf boost as bonus)
#endif
			const float3 fixed = normalize(lerp(perp, reflected, saturate(VdotN * VdotN * parallaxCurvatureParams.y)));
#ifdef FP_DEGENERATE_POSSIBLE
#undef FP_DEGENERATE_POSSIBLE
			[flatten]
			if (all(isfinite(fixed)))	// extra check for degenerate case, should never happened according to math but possible due to floating point precision issues
#endif
				n = fixed;
#else
			n = reflected;
#endif
		}
	}

	// simulates parallax for surface macro normal (interpolated vertex normal), viewDir assumed to be unit length
	void FixNormal(inout float3 N, in float3 viewDir)
	{
#if 1
		N -= 2 * min(dot(viewDir, N), 0) * viewDir;
#else
		/*
		micro optimization, assumes N is unit length
		relies on following GPU behavior
			free source negation (that's why '-' applied to dot`s source vector, not scalar dot result)
			free result saturate
		*/
		N += 2 * saturate(dot(-viewDir, N)) * viewDir;
#endif
	}

	// simulates parallax for interpolated TBN frame, viewDir assumed to be unit length
	void FixTBN(inout float3x3 TBN, in float3 viewDir)
	{
		const float VdotN = dot(viewDir, TBN[2]) < 0;
		if (VdotN)
		{
			const float3 N = TBN[2];	// old normal
			TBN[2] -= 2 * VdotN * viewDir;

			/*
				rotation matrix from quaternion
				!:	consider alternative rot matrix construction from {axis, angle}, compare ALU stress / GPR pressure
					GPR pressure probably more important, particular considering that it will run for quite few pixels
			*/
			const float3 H = normalize(N + TBN[2]);
			float4 q, q2;
			q.xyz = cross(N, H);
			q2.xyz = q * q;
			q2.w = 1 - q2.xyz;
			q.w = sqrt(q2.w);
			const float3x3 rot =
			{
				q2.w + q2.x - q2.y - q2.z,		2 * (q.x * q.y - q.z * q.w),	2 * (q.x * q.z + q.y * q.w),
				2 * (q.x * q.y + q.z * q.w),	q2.w - q2.x + q2.y - q2.z,		2 * (q.y * q.z - q.x * q.w),
				2 * (q.x * q.z - q.y * q.w),	2 * (q.y * q.z + q.x * q.w),	q2.w - q2.x - q2.y + q2.z
			};

			// rotate T & B
			TBN[0] = mul(TBN[0], rot);
			TBN[1] = mul(TBN[1], rot);
		}
	}

	float3 EvaluateSurfaceNormal(float3 N, float3 viewDir, bool frontFace)
	{
		N = normalize(frontFace ? +N : -N);	// handles two-sided materials
		FixNormal(N, viewDir);
		return N;
	}

	struct Nn
	{
		float3 N, n;
	};

	Nn EvaluateBump(float3 T, float3 B, float3 N, float3 viewDir, bool frontFace, float2 fetch, float2 normalScale)
	{
#if 0
		// validation error on current HLSL compiler, try with future version
		float3x3 TBN = { SafeNormalize(T), SafeNormalize(B), normalize(N) };
		if (!frontFace)	// handle two-sided materials
			TBN = -TBN;
#else
		float3x3 TBN = { SafeNormalize(frontFace ? +T : -T), SafeNormalize(frontFace ? +B : -B), normalize(frontFace ? +N : -N) };	// handles two-sided materials
#endif
		FixTBN(TBN, viewDir);
		float3 n;
		n.xy = fetch * normalScale;
		n.z = sqrt(saturate(1 - length(n.xy)));
		n = mul(n, TBN);
		FixNormal(TBN[2], n, viewDir);
		const Nn result = { TBN[2], n };
		return result;
	}

	Nn DoNormalMapping(float3 T, float3 B, float3 N, float3 viewDir, bool frontFace, uniform Texture2D normalMap, uniform SamplerState sampler, float2 uv, float2 normalScale)
	{
		return EvaluateBump(T, B, N, viewDir, frontFace, normalMap.Sample(sampler, uv), normalScale);
	}

	Nn DoNormalMapping(float3 T, float3 B, float3 N, float3 viewDir, bool frontFace, uniform Texture2D normalMap, uniform SamplerState sampler, float2 uv, float2 normalScale, float lodBias)
	{
		return EvaluateBump(T, B, N, viewDir, frontFace, normalMap.SampleBias(sampler, uv, lodBias), normalScale);
	}

	float3 EvaluateTerrainBump(float4x3 terrainWorldXform, float4x3 viewXform, float3 viewDir, float2 fetch)
	{
		/*
			TBN space for terrain in terrain space is
			{1, 0, 0}
			{0, 1, 0}
			{0, 0, -1}
			terrain 'up' inverted so -1 for N
		*/
		float3 n = { fetch, -sqrt(saturate(1 - length(fetch))) };

		/*
			xform normal 'terrain space' -> 'view space'
			similar note as for Flat shader applicable:
				both terrainWorldXform and viewXform assumed to be orthonormal, need inverse transpose otherwise
				mul with terrainWorldXform is suboptimal: get rid of terrain xform at all or merge in worldViewXform
		*/
		n = mul(mul(n, terrainWorldXform), viewXform);
		FixNormal(viewXform[2], n, viewDir);
		return n;
	}

	float3 DoTerrainNormalMapping(float4x3 terrainWorldXform, float4x3 viewXform, float3 viewDir, uniform Texture2D normalMap, uniform SamplerState sampler, float2 uv)
	{
		return EvaluateTerrainBump(terrainWorldXform, viewXform, viewDir, normalMap.Sample(sampler, uv));
	}

	float3 DoTerrainNormalMapping(float4x3 terrainWorldXform, float4x3 viewXform, float3 viewDir, uniform Texture2D normalMap, uniform SamplerState sampler, float2 uv, float lodBias)
	{
		return EvaluateTerrainBump(terrainWorldXform, viewXform, viewDir, normalMap.SampleBias(sampler, uv, lodBias));
	}
}