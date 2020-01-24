#version 450
#extension GL_ARB_separate_shader_objects : enable

#define UX3D_MATH_PI 3.1415926535897932384626433832795
#define UX3D_MATH_INV_PI (1.0 / UX3D_MATH_PI)

layout(set = 0, binding = 0) uniform sampler2D uPanorama;
layout(set = 0, binding = 1) uniform samplerCube uCubeMap;

// enum
const uint cLambertian = 0;
const uint cGGX = 1;
const uint cCharlie = 2;

layout(push_constant) uniform FilterParameters {
  float roughness;
  uint sampleCount;
  uint currentMipLevel;
  uint width;
  float lodBias;
  uint distribution; // enum
} pFilterParameters;

layout (location = 0) in vec2 inUV;

// output cubemap faces
layout(location = 0) out vec4 outFace0;
layout(location = 1) out vec4 outFace1;
layout(location = 2) out vec4 outFace2;
layout(location = 3) out vec4 outFace3;
layout(location = 4) out vec4 outFace4;
layout(location = 5) out vec4 outFace5;

layout(location = 6) out vec2 outLUT;

void writeFace(int face, vec3 colorIn)
{
	vec4 color = vec4(colorIn.rgb, 1.0f);
	
	if(face == 0)
		outFace0 = color;
	else if(face == 1)
		outFace1 = color;
	else if(face == 2)
		outFace2 = color;
	else if(face == 3)
		outFace3 = color;
	else if(face == 4)
		outFace4 = color;
	else //if(face == 5)
		outFace5 = color;
}

vec3 uvToXYZ(int face, vec2 uv)
{
	if(face == 0)
		return vec3(     1.f,   uv.y,    -uv.x);
		
	else if(face == 1)
		return vec3(    -1.f,   uv.y,     uv.x);
		
	else if(face == 2)
		return vec3(   +uv.x,   -1.f,    +uv.y);		
	
	else if(face == 3)
		return vec3(   +uv.x,    1.f,    -uv.y);
		
	else if(face == 4)
		return vec3(   +uv.x,   uv.y,      1.f);
		
	else //if(face == 5)
		return vec3(    -uv.x,  +uv.y,     -1.f);
}

vec2 dirToUV(vec3 dir)
{
	return vec2(
		0.5f + 0.5f * atan(dir.z, dir.x) / UX3D_MATH_PI,
		1.f - acos(dir.y) / UX3D_MATH_PI);
}

float saturate(float v)
{
	return clamp(v, 0.0f, 1.0f);
}

float Hammersley(uint i)
{
    return bitfieldReverse(i) * 2.3283064365386963e-10;
}

vec3 getImportanceSampleDirection(vec3 normal, float sinTheta, float cosTheta, float phi)
{
	vec3 H = normalize(vec3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta));
    
    vec3 bitangent = vec3(0.0, 1.0, 0.0);

    float NdotB = dot(normal, bitangent);

    if (NdotB == 1.0)
    {
        bitangent = vec3(0.0, 0.0, -1.0);
    }
    else if (NdotB == -1.0)
    {
        bitangent = vec3(0.0, 0.0, 1.0);
    }

    vec3 tangent = cross(bitangent, normal);
    bitangent = cross(normal, tangent);
    
	return normalize(tangent * H.x + bitangent * H.y + normal * H.z);
}

// https://github.com/google/filament/blob/master/shaders/src/brdf.fs#L136
float V_Neubelt(float NdotL, float NdotV)
{
    return clamp(1.0 / (4.0 * (NdotL + NdotV - NdotL * NdotV)),0.0,1.0);
}

// NDF: Trowbridge-Reitz GGX
float D_GGX(float NdotH, float roughness)
{
    float alpha = roughness * roughness;
    
    float alpha2 = alpha * alpha;
    
    float divisor = NdotH * NdotH * (alpha2 - 1.0) + 1.0;
        
    return alpha2 / (UX3D_MATH_PI * divisor * divisor); 
}

// NDF
float D_Charlie(float sheenRoughness, float NdotH)
{
    sheenRoughness = max(sheenRoughness, 0.000001); //clamp (0,1]
    float alphaG = sheenRoughness * sheenRoughness;
    float invR = 1.0 / alphaG;
    float cos2h = NdotH * NdotH;
    float sin2h = 1.0 - cos2h;
    return (2.0 + invR) * pow(sin2h, invR * 0.5) / (2.0 * UX3D_MATH_PI);
}

vec3 getSampleVector(uint sampleIndex, vec3 N, float roughness)
{
	float X = float(sampleIndex) / float(pFilterParameters.sampleCount);
	float Y = Hammersley(sampleIndex);

	float phi = 2.0 * UX3D_MATH_PI * X;
    float cosTheta = 0.f;
	float sinTheta = 0.f;

	if(pFilterParameters.distribution == cLambertian)
	{
		cosTheta = 1.0 - Y;
		sinTheta = sqrt(1.0 - cosTheta*cosTheta);	
	}
	else if(pFilterParameters.distribution == cGGX)
	{
		float alpha = roughness * roughness;
		cosTheta = sqrt((1.0 - Y) / (1.0 + (alpha*alpha - 1.0) * Y));
		sinTheta = sqrt(1.0 - cosTheta*cosTheta);		
	}
	else if(pFilterParameters.distribution == cCharlie)
	{
		float alpha = roughness * roughness; // sqared ?
		sinTheta = pow(Y, alpha / (2.0*alpha + 1.0));
		cosTheta = sqrt(1.0 - sinTheta * sinTheta);
	}	
	
	return getImportanceSampleDirection(N, sinTheta, cosTheta, phi);
}

float PDF(vec3 V, vec3 H, vec3 N, vec3 L, float roughness)
{
	if(pFilterParameters.distribution == cLambertian)
	{
		float NdotL = dot(N, L); // NdotH ?
		return max(NdotL * UX3D_MATH_INV_PI, 0.0);
	}
	else if(pFilterParameters.distribution == cGGX)
	{
		float VdotH = dot(V, H); // if V = N then VdotH = NdotH and D * NdotH / (4.0 * VdotH) = D / 4.0 (which seems weird)
		float NdotH = dot(N, H);
	
		float D = D_GGX(NdotH, roughness);
		return max(D * NdotH / (4.0 * VdotH), 0.0); // todo: try to derive '... / 4 VoH'
	}
	else if(pFilterParameters.distribution == cCharlie)
	{
		// a = pFilterParameters.roughness * pFilterParameters.roughness
		// D = ((1/a + 2) * sin(t)^(1/a)) / 2pi
		float NdotH = dot(N, H);
		return max(D_Charlie(roughness, NdotH) * NdotH, 0.0);
	}
	
	return 0.f;
}

vec3 filterColor(vec3 N)
{
	vec4 color = vec4(0.f);
	const uint NumSamples = pFilterParameters.sampleCount;
	const float solidAngleTexel = 4.0 * UX3D_MATH_PI / (6.0 * pFilterParameters.width * pFilterParameters.width);	
	
	for(uint i = 0; i < NumSamples; ++i)
	{
		vec3 H = getSampleVector(i, N, pFilterParameters.roughness);

		// Note: reflect takes incident vector.
		// Note: N = V
		vec3 V = N;
    
		vec3 L = normalize(reflect(-V, H));
    
		float NdotL = dot(N, L);

		if (NdotL > 0.0)
		{
			float lod = 0.0;
		
			if (pFilterParameters.roughness > 0.0 || pFilterParameters.distribution == cLambertian)
			{		
				// Mipmap Filtered Samples 
				// see https://github.com/derkreature/IBLBaker
				// see https://developer.nvidia.com/gpugems/GPUGems3/gpugems3_ch20.html
				float pdf = PDF(V, H, N, L, pFilterParameters.roughness );
				
				float solidAngleSample = 1.0 / (NumSamples * pdf);
				
				lod = 0.5 * log2(solidAngleSample / solidAngleTexel);
				lod += pFilterParameters.lodBias;
			}
						
			if(pFilterParameters.distribution == cLambertian)
			{
				color += vec4(texture(uCubeMap, H, lod).rgb, 1.0);						
			}
			else
			{				
				color += vec4(textureLod(uCubeMap, L, lod).rgb * NdotL, NdotL);		
			}			
		}
	}

	if(color.w == 0.f)
	{
		return color.rgb;
	}

	return color.rgb / color.w;
}

// From the filament docs. Geometric Shadowing function
// https://google.github.io/filament/Filament.html#toc4.4.2
float V_SmithGGXCorrelated(float NoV, float NoL, float roughness) {
	float a2 = pow(roughness, 4.0);
	float GGXV = NoL * sqrt(NoV * NoV * (1.0 - a2) + a2);
	float GGXL = NoV * sqrt(NoL * NoL * (1.0 - a2) + a2);
	return 0.5 / (GGXV + GGXL);
}

vec2 LUT_uniform_sampling(float NdotV, float roughness)
{
	vec3 V = vec3(sqrt(1.0 - NdotV * NdotV), 0.0, NdotV);
	vec3 N = vec3(0.0, 0.0, 1.0);

	float A = 0;
	float B = 0;

	const uint Nk = 100; // todo: use push constant
	vec3 hamAcc = vec3(0, 0, 0);

	for (int k = 0; k < Nk; k++) {
		float X = float(k) / float(Nk);
		float Y = Hammersley(k);

		float phi = 2.0 * UX3D_MATH_PI * X;
		float cosTheta = 1.0 - Y;
		float sinTheta = sqrt(1 - cosTheta * cosTheta);

//		float alpha = roughness * roughness;
//		cosTheta = sqrt((1.0 - Y) / (1.0 + (alpha*alpha - 1.0) * Y));
//		sinTheta = sqrt(1.0 - cosTheta*cosTheta);

		vec3 d = normalize(vec3(
			cosTheta * cos(phi),
			sinTheta * sin(phi),
			sinTheta
			));
		hamAcc += d;

		vec3 H = d;
		vec3 L = normalize(reflect(-V, H));

		float NdotL = saturate(L.z);
		float NdotH = saturate(H.z);
		float VdotH = saturate(dot(V, H));

		if (NdotL > 0.0) // why not let H interpolate over arc and compute L = norm(V + L)?
		{
			float D = D_GGX(NdotH, roughness);
			float V = V_SmithGGXCorrelated(NdotV, NdotL, roughness) * NdotL;
			float Fc = pow(1.0 - VdotH, 5.0);
			A += (1.0 - Fc) * V * D;
			B += Fc * V * D;
		}
	}

	//return vec2(hamAcc.z / float(Nk), 0);
	return 2 * UX3D_MATH_PI * vec2(A, B) / float(Nk);
}

// Compute LUT for GGX distribution.
// See https://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_notes_v2.pdf
vec2 LUT(float NdotV, float roughness)
{
	// Compute spherical view vector: (sin(phi), 0, cos(phi))
	vec3 V = vec3(sqrt(1.0 - NdotV * NdotV), 0.0, NdotV);

	// The macro surface normal just points up.
	vec3 N = vec3(0.0, 0.0, 1.0);

	// To make the LUT independant from the material's F0, which is part of the Fresnel term
	// when substituted by Schlick's approximation, we factor it out of the integral,
	// yielding to the form: F0 * I1 + I2
	// I1 and I2 are slighlty different in the Fresnel term, but both only depend on
	// NoL and roughness, so they are both numerically integrated and written into two channels.
	float A = 0;
	float B = 0;

	for(uint i = 0; i < pFilterParameters.sampleCount; ++i)
	{
		// Importance sampling, depending on the distribution.
		vec3 H = getSampleVector(i, N, roughness);
		vec3 L = normalize(reflect(-V, H));

		float NdotL = saturate(L.z);
		float NdotH = saturate(H.z);
		float VdotH = saturate(dot(V, H));
		if (NdotL > 0.0)
		{
			if (pFilterParameters.distribution == cGGX)
			{
				// LUT for GGX distribution.

				// Taken from: https://bruop.github.io/ibl
				// Shadertoy: https://www.shadertoy.com/view/3lXXDB
				// Terms besides V are from the GGX PDF we're dividing by.
				float V_pdf = V_SmithGGXCorrelated(NdotV, NdotL, roughness) * VdotH * NdotL / NdotH;
				float Fc = pow(1.0 - VdotH, 5.0);
				A += (1.0 - Fc) * V_pdf;
				B += Fc * V_pdf;
			}

			if (pFilterParameters.distribution == cCharlie)
			{
				// LUT for Charlie distribution.

				//TODO Begin

				float sheenDistribution = D_Charlie(roughness, NdotH);
				float sheenVisibility = V_Neubelt(NdotL, NdotV);
				float sheenBRDF = sheenDistribution * sheenVisibility;

				sheenBRDF=1.0/(4*(VdotH));

				float sheenPDF = PDF(V, H, N, L, roughness);
				sheenPDF=1.0/(2*UX3D_MATH_PI);

				float weightedBRDF = sheenBRDF/sheenPDF;

				float Fc =0;
				//acc.x += (1.0 - Fc) * weightedBRDF;
				//acc.y += Fc * weightedBRDF;

				A += weightedBRDF;
				B += 0;
				//TODO End
			}
		}
	}

	// TODO: Doc, why scale by 4?

	// The PDF is simply pdf(v, h) -> NDF * <nh>.
	// To parametrize the PDF over l, use the Jacobian transform, yielding to: pdf(v, l) -> NDF * <nh> / 4<vh>
	// Since the BRDF divide through the PDF to be normalized, the 4 can be pulled out of the integral.
	return 4 * vec2(A, B) / pFilterParameters.sampleCount;
}

// entry point
void panoramaToCubeMap() 
{
	for(int face = 0; face < 6; ++face)
	{		
		vec3 scan = uvToXYZ(face, inUV*2.0-1.0);		
			
		vec3 direction = normalize(scan);		
	
		vec2 src = dirToUV(direction);		
			
		writeFace(face, texture(uPanorama, src).rgb);
	}
}

// entry point
void filterCubeMap() 
{
	vec2 newUV = inUV * float(1 << (pFilterParameters.currentMipLevel));
	 
	newUV = newUV*2.0-1.0;
	
	for(int face = 0; face < 6; ++face)
	{
		vec3 scan = uvToXYZ(face, newUV);		
			
		vec3 direction = normalize(scan);	
		direction.y = -direction.y;

		writeFace(face, filterColor(direction));
		
		//Debug output:
		//writeFace(face,  texture(uCubeMap, direction).rgb);
		//writeFace(face,   direction);
	}

	// Write LUT:
	// x-coordinate: NdotV
	// y-coordinate: roughness
	if (pFilterParameters.currentMipLevel == 0)
	{
//		outLUT = LUT(inUV.x, inUV.y) * vec2(1, 0);
		outLUT = LUT_uniform_sampling(inUV.x, inUV.y) * vec2(0, 1);
	}
}
