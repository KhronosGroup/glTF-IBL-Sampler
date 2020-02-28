#version 450
#extension GL_ARB_separate_shader_objects : enable

#define UX3D_MATH_PI 3.1415926535897932384626433832795
#define UX3D_MATH_INV_PI (1.0 / UX3D_MATH_PI)

#define UX3D_MATH_E 2.718281828459

#define UX3D_GAMMA 2.2
#define UX3D_INV_GAMMA (1.0 / UX3D_GAMMA)

// Air
#define UX3D_N0	1.0
// Dielectric										
#define UX3D_R1	0.04									
// Research by "infinity ward"
#define UX3D_K	0.5										
// Max
#define UX3D_N3	4.0
// derived from r1 = ((n1 - n0) / (n1 + n0))^2
#define UX3D_N1	1.5
// derived from k = r1 / r2
#define UX3D_R2	0.08
// derived from r2 = ((n2 - n0) / (n2 + n0))^2
#define UX3D_N2	((10.0 * sqrt(2.0) + 27.0) / 23.0)

#define UX3D_IRIDESCENCE_OFFSET		0.0
#define UX3D_IRIDESCENCE_SCALE		1200.0


layout(set = 0, binding = 0) uniform sampler2D uPanorama;
layout(set = 0, binding = 1) uniform samplerCube uCubeMap;

// enum
const uint cLambertian = 0;
const uint cGGX = 1;
const uint cCharlie = 2;
const uint cThinfilm = 3;

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

layout(location = 6) out vec3 outLUT;

float sqr(float a)
{
	return a*a;
}

vec3 LINEARtoSRGB(vec3 color)
{
    return pow(color, vec3(UX3D_INV_GAMMA));
}

vec3 SRGBtoLINEAR(vec3 srgbIn)
{
    return vec3(pow(srgbIn.xyz, vec3(UX3D_GAMMA)));
}

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
	
	// Eliminates singularities.
	float NdotX = dot(normal, vec3(1.0, 0.0, 0.0));
	float NdotY = dot(normal, vec3(0.0, 1.0, 0.0));
	float NdotZ = dot(normal, vec3(0.0, 0.0, 1.0));
	if (abs(NdotY) > abs(NdotX) && abs(NdotY) > abs(NdotZ))
	{
		// Sampling +Y or -Y, so we need a more robust bitangent.
		if (NdotY > 0.0)
		{
			bitangent = vec3(0.0, 0.0, 1.0);
		}
		else
		{
			bitangent = vec3(0.0, 0.0, -1.0);
		}
	}

    vec3 tangent = cross(bitangent, normal);
    bitangent = cross(normal, tangent);
    
	return normalize(tangent * H.x + bitangent * H.y + normal * H.z);
}

float F_Schlick(float f0, float cosTheta)
{
    return f0 + (1.0 - f0) * pow(1.0 - cosTheta, 5.0);
}

// https://github.com/google/filament/blob/master/shaders/src/brdf.fs#L136
float V_Ashikhmin(float NdotL, float NdotV)
{
    return clamp(1.0 / (4.0 * (NdotL + NdotV - NdotL * NdotV)), 0.0, 1.0);
}

// NDF
float D_GGX(float NdotH, float roughness)
{
    float alpha = roughness * roughness;
    
    float alpha2 = alpha * alpha;
    
    float divisor = NdotH * NdotH * (alpha2 - 1.0) + 1.0;
        
    return alpha2 / (UX3D_MATH_PI * divisor * divisor); 
}

// NDF
float D_Ashikhmin(float NdotH, float roughness)
{
	float alpha = roughness * roughness;
    // Ashikhmin 2007, "Distribution-based BRDFs"
	float a2 = alpha * alpha;
	float cos2h = NdotH * NdotH;
	float sin2h = 1.0 - cos2h;
	float sin4h = sin2h * sin2h;
	float cot2 = -cos2h / (a2 * sin2h);
	return 1.0 / (UX3D_MATH_PI * (4.0 * a2 + 1.0) * sin4h) * (4.0 * exp(cot2) + sin4h);
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
		float alpha = roughness * roughness;
		sinTheta = pow(Y, alpha / (2.0*alpha + 1.0));
		cosTheta = sqrt(1.0 - sinTheta * sinTheta);
	}	
	else if(pFilterParameters.distribution == cThinfilm)
	{
		// Any value can go here.
		sinTheta = UX3D_MATH_PI * 0.5;
		cosTheta = sqrt(1.0 - sinTheta * sinTheta);
	}	
	
	return getImportanceSampleDirection(N, sinTheta, cosTheta, phi);
}

float PDF(vec3 V, vec3 H, vec3 N, vec3 L, float roughness)
{
	if(pFilterParameters.distribution == cLambertian)
	{
		float NdotL = dot(N, L);
		return max(NdotL * UX3D_MATH_INV_PI, 0.0);
	}
	else if(pFilterParameters.distribution == cGGX)
	{
		float VdotH = dot(V, H);
		float NdotH = dot(N, H);
	
		float D = D_GGX(NdotH, roughness);
		return max(D * NdotH / (4.0 * VdotH), 0.0);
	}
	else if(pFilterParameters.distribution == cCharlie)
	{
		float VdotH = dot(V, H);
		float NdotH = dot(N, H);
		
		float D = D_Charlie(roughness, NdotH);
		return max(D * NdotH / abs(4.0 * VdotH), 0.0);
	}
	else if(pFilterParameters.distribution == cThinfilm)
	{
		return 0.0;
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

// Compute LUT for GGX distribution.
// See https://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_notes_v2.pdf
vec3 LUT(float NdotV, float roughness)
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
	float C = 0;

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
				C += 0;
			}

			if (pFilterParameters.distribution == cCharlie)
			{
				// LUT for Charlie distribution.
				
				float sheenDistribution = D_Charlie(roughness, NdotH);
				float sheenVisibility = V_Ashikhmin(NdotL, NdotV);

				A += 0;
				B += 0;
				C += sheenVisibility * sheenDistribution * NdotL * VdotH;
			}
		}
	}

	// The PDF is simply pdf(v, h) -> NDF * <nh>.
	// To parametrize the PDF over l, use the Jacobian transform, yielding to: pdf(v, l) -> NDF * <nh> / 4<vh>
	// Since the BRDF divide through the PDF to be normalized, the 4 can be pulled out of the integral.
	return vec3(4.0 * A, 4.0 * B, 4.0 * 2.0 * UX3D_MATH_PI * C) / pFilterParameters.sampleCount;
}

// see https://en.wikipedia.org/wiki/Fresnel_equations
float fresnelRs(float n1, float n2, float cosThetaIncidence)
{
	float sinThetaIncidence = sqrt(1.0 - cosThetaIncidence * cosThetaIncidence);

	float sinThetaTransmittance = n1 * (sinThetaIncidence / n2);
	float cosThetaTransmittance = sqrt(1.0 - sinThetaTransmittance * sinThetaTransmittance);

	float n2CosThetaTransmittance = n2 * cosThetaTransmittance;
	float n1CosThetaIncidence = n1 * cosThetaIncidence;

	float sPolarizedSqrt = (n1CosThetaIncidence - n2CosThetaTransmittance) / (n1CosThetaIncidence + n2CosThetaTransmittance);

	return sPolarizedSqrt * sPolarizedSqrt;
}

float fresnelRp(float n1, float n2, float cosThetaIncidence)
{
	float sinThetaIncidence = sqrt(1.0 - cosThetaIncidence * cosThetaIncidence);

	float sinThetaTransmittance = n1 * (sinThetaIncidence / n2);
	float cosThetaTransmittance = sqrt(1.0 - sinThetaTransmittance * sinThetaTransmittance);

	float n1CosThetaTransmittance = n1 * cosThetaTransmittance;
	float n2CosThetaIncidence = n2 * cosThetaIncidence;

	float pPolarizedSqrt = (n2CosThetaIncidence - n1CosThetaTransmittance) / (n2CosThetaIncidence + n1CosThetaTransmittance);

	return pPolarizedSqrt * pPolarizedSqrt;
}

float thinFilm(float lambda, float d, float cosThetaIncidence, float n1, float n2)
{
	float n0 = 1.0;	// Air

	float s01 = fresnelRs(n0, n1, cosThetaIncidence);
	float s12 = fresnelRs(n1, n2, cosThetaIncidence);

	float p01 = fresnelRp(n0, n1, cosThetaIncidence);
	float p12 = fresnelRp(n1, n2, cosThetaIncidence);

	//

	float sinThetaIncidence = sqrt(1.0 - cosThetaIncidence * cosThetaIncidence);

	float sinThetaTransmittance = n1 * (sinThetaIncidence / n2);
	float cosThetaTransmittance = sqrt(1.0 - sinThetaTransmittance * sinThetaTransmittance);

	//

	float cosDelta = cos(4.0 * UX3D_MATH_PI * cosThetaTransmittance * n1 * d / lambda);

	//

	float Fs = (sqr(s01) + sqr(s12) + 2.0 * s01 * s12 * cosDelta) / (1.0 + sqr(s01 * s12) + 2.0 * s01 * s12 * cosDelta);
	float Fp = (sqr(p01) + sqr(p12) + 2.0 * p01 * p12 * cosDelta) / (1.0 + sqr(p01 * p12) + 2.0 * p01 * p12 * cosDelta);

	return (Fs + Fp) * 0.5;
}

vec3 colorMatchingFunctionXYZ(float lambda)
{
	if (lambda < 380.0 || lambda > 780.0)
	{
		return vec3(0.0, 0.0, 0.0);
	}

	//

	float cie_colour_match[81][3] = {
	        {0.0014,0.0000,0.0065}, {0.0022,0.0001,0.0105}, {0.0042,0.0001,0.0201},
	        {0.0076,0.0002,0.0362}, {0.0143,0.0004,0.0679}, {0.0232,0.0006,0.1102},
	        {0.0435,0.0012,0.2074}, {0.0776,0.0022,0.3713}, {0.1344,0.0040,0.6456},
	        {0.2148,0.0073,1.0391}, {0.2839,0.0116,1.3856}, {0.3285,0.0168,1.6230},
	        {0.3483,0.0230,1.7471}, {0.3481,0.0298,1.7826}, {0.3362,0.0380,1.7721},
	        {0.3187,0.0480,1.7441}, {0.2908,0.0600,1.6692}, {0.2511,0.0739,1.5281},
	        {0.1954,0.0910,1.2876}, {0.1421,0.1126,1.0419}, {0.0956,0.1390,0.8130},
	        {0.0580,0.1693,0.6162}, {0.0320,0.2080,0.4652}, {0.0147,0.2586,0.3533},
	        {0.0049,0.3230,0.2720}, {0.0024,0.4073,0.2123}, {0.0093,0.5030,0.1582},
	        {0.0291,0.6082,0.1117}, {0.0633,0.7100,0.0782}, {0.1096,0.7932,0.0573},
	        {0.1655,0.8620,0.0422}, {0.2257,0.9149,0.0298}, {0.2904,0.9540,0.0203},
	        {0.3597,0.9803,0.0134}, {0.4334,0.9950,0.0087}, {0.5121,1.0000,0.0057},
	        {0.5945,0.9950,0.0039}, {0.6784,0.9786,0.0027}, {0.7621,0.9520,0.0021},
	        {0.8425,0.9154,0.0018}, {0.9163,0.8700,0.0017}, {0.9786,0.8163,0.0014},
	        {1.0263,0.7570,0.0011}, {1.0567,0.6949,0.0010}, {1.0622,0.6310,0.0008},
	        {1.0456,0.5668,0.0006}, {1.0026,0.5030,0.0003}, {0.9384,0.4412,0.0002},
	        {0.8544,0.3810,0.0002}, {0.7514,0.3210,0.0001}, {0.6424,0.2650,0.0000},
	        {0.5419,0.2170,0.0000}, {0.4479,0.1750,0.0000}, {0.3608,0.1382,0.0000},
	        {0.2835,0.1070,0.0000}, {0.2187,0.0816,0.0000}, {0.1649,0.0610,0.0000},
	        {0.1212,0.0446,0.0000}, {0.0874,0.0320,0.0000}, {0.0636,0.0232,0.0000},
	        {0.0468,0.0170,0.0000}, {0.0329,0.0119,0.0000}, {0.0227,0.0082,0.0000},
	        {0.0158,0.0057,0.0000}, {0.0114,0.0041,0.0000}, {0.0081,0.0029,0.0000},
	        {0.0058,0.0021,0.0000}, {0.0041,0.0015,0.0000}, {0.0029,0.0010,0.0000},
	        {0.0020,0.0007,0.0000}, {0.0014,0.0005,0.0000}, {0.0010,0.0004,0.0000},
	        {0.0007,0.0002,0.0000}, {0.0005,0.0002,0.0000}, {0.0003,0.0001,0.0000},
	        {0.0002,0.0001,0.0000}, {0.0002,0.0001,0.0000}, {0.0001,0.0000,0.0000},
			{0.0001,0.0000,0.0000}, {0.0001,0.0000,0.0000}, {0.0000,0.0000,0.0000}
	};

	uint index = uint((lambda - 380.0) / 5.0);

	return vec3(cie_colour_match[index][0], cie_colour_match[index][1], cie_colour_match[index][2]);
}

// see https://en.wikipedia.org/wiki/Standard_illuminant Illuminant A
// see https://www.fourmilab.ch/documents/specrend/
float spectralRadiance(float lambda, float T)
{
	if (lambda < 380.0 || lambda > 780.0)
	{
		return 0.0;
	}

	//

	float wavelength = lambda * 0.000000001; // To meters

	float c1 = 3.74183e-16;
	float c2 = 0.014388;

	return (c1 * pow(wavelength, -5.0)) / (pow(UX3D_MATH_E, c2 / (wavelength * T)) - 1.0);
}

// see http://www.brucelindbloom.com/index.html?Eqn_RGB_XYZ_Matrix.html
vec3 xyzToRGB(vec3 xyzC)
{
	// CIE RGB
	mat3 matrix = mat3(2.3706743, -0.5138850, 0.0052982, -0.9000405, 1.4253036, -0.0146949, -0.4706338, 0.0885814, 1.0093968);

	return matrix * xyzC;
}

vec3 thinFilmRGB(float d, float cosTheta, float n1, float n2)
{
	vec3 xyz = vec3(0.0, 0.0, 0.0);

	// Spectral range 

	for (float lambda = 380.0; lambda <= 780.0; lambda += 5.0)
	{
		xyz += spectralRadiance(lambda, 5500.0) * colorMatchingFunctionXYZ(lambda) * thinFilm(lambda, d, cosTheta, n1, n2);
	}

	xyz /= (xyz.r + xyz.g + xyz.b);

	return LINEARtoSRGB(xyzToRGB(xyz));
}

vec3 LUTthinfilm(float x, float y)
{
	// LUT for Thinfilm distribution.
	float d = x;
	float cosTheta = y;
	d = d * UX3D_IRIDESCENCE_SCALE + UX3D_IRIDESCENCE_OFFSET;

	return thinFilmRGB(d, cosTheta, UX3D_N1, UX3D_N3);// - vec3(F_Schlick(UX3D_K, cosTheta));
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
		if (pFilterParameters.distribution != cThinfilm)
		{
			outLUT = LUT(inUV.x, inUV.y);
		}
		else
		{
			outLUT = LUTthinfilm(inUV.x, inUV.y);
		}
	}
}
