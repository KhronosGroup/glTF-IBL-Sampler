R""(
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

layout(location = 6) out vec3 outLUT;

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

// hammersley2d describes a sequence of points in the 2d unit square [0,1)^2
// that can be used for quasi Monte Carlo integration
vec2 hammersley2d(uint i, uint N) {
    return vec2(float(i)/float(N), bitfieldReverse(i) * 2.3283064365386963e-10);
}

// Hemisphere Sample

// TBN generates a tangent bitangent normal coordinate frame from the normal
// (the normal must be normalized)
mat3 generateTBN(vec3 normal)
{ 
    vec3 bitangent = vec3(0.0, 1.0, 0.0);
	
    float NdotUp = dot(normal, vec3(0.0, 1.0, 0.0));
    float epsilon = 0.0000001;
    if (1.0 - abs(NdotUp) <= epsilon)
    {
        // Sampling +Y or -Y, so we need a more robust bitangent.
        if (NdotUp > 0.0)
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
	return mat3(tangent, bitangent, normal);
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

float PDF(vec3 H, vec3 N, float roughness)
{
    float NdotH = dot(N, H);
    if(pFilterParameters.distribution == cLambertian)
    {
        return max(NdotH * (1.0 / UX3D_MATH_PI), 0.0);
    }
    else if(pFilterParameters.distribution == cGGX)
    {
        float D = D_GGX(NdotH, roughness);
        return max(D / 4.0, 0.0);
    }
    else if(pFilterParameters.distribution == cCharlie)
    {
        float D = D_Charlie(roughness, NdotH);
        return max(D / 4.0, 0.0);
    }

    return 0.f;
}

vec4 getImportanceSample(uint sampleIndex, vec3 N, float roughness)
{
    // generate a quasi monte carlo point in the unit square [0.1)^2
    vec2 hammersleyPoint = hammersley2d(sampleIndex, pFilterParameters.sampleCount);
    float u = hammersleyPoint.x;
    float v = hammersleyPoint.y;

    // declare importance sample parameters
    float phi = 0.0; // theoretically there could be a distribution that defines phi differently
    float cosTheta = 0.f;
	float sinTheta = 0.f;
    float pdf = 0.0;

    // generate the points on the hemisphere with a fitting mapping for
    // the distribution (e.g. lambertian uses a cosine importance)
    if(pFilterParameters.distribution == cLambertian)
    {
        // Cosine weighted hemisphere sampling
        // http://www.pbr-book.org/3ed-2018/Monte_Carlo_Integration/2D_Sampling_with_Multidimensional_Transformations.html#Cosine-WeightedHemisphereSampling
        cosTheta = sqrt(1.0 - u);
        sinTheta = sqrt(u); // equivalent to `sqrt(1.0 - cosTheta*cosTheta)`;
        phi = 2.0 * UX3D_MATH_PI * v;

        pdf = cosTheta / UX3D_MATH_PI; // evaluation for solid angle, therefore drop the sinTheta
    }
    else if(pFilterParameters.distribution == cGGX)
    {
        // specular mapping
        float alpha = roughness * roughness;
        cosTheta = sqrt((1.0 - u) / (1.0 + (alpha*alpha - 1.0) * u));
        sinTheta = sqrt(1.0 - cosTheta*cosTheta);
        phi = 2.0 * UX3D_MATH_PI * v;
    }
    else if(pFilterParameters.distribution == cCharlie)
    {
        // sheen mapping
        float alpha = roughness * roughness;
        sinTheta = pow(u, alpha / (2.0*alpha + 1.0));
        cosTheta = sqrt(1.0 - sinTheta * sinTheta);
        phi = 2.0 * UX3D_MATH_PI * v;
    }

    // transform the hemisphere sample to the normal coordinate frame
    // i.e. rotate the hemisphere to the normal direction
    vec3 localSpaceDirection = normalize(vec3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta));
    mat3 TBN = generateTBN(N);
    vec3 direction = TBN * localSpaceDirection;

    if(pFilterParameters.distribution == cGGX || pFilterParameters.distribution == cCharlie)
    {
        pdf = PDF(direction, N, roughness);
    }

    return vec4(direction, pdf);
}

// Mipmap Filtered Samples (GPU Gems 3, 20.4)
// https://developer.nvidia.com/gpugems/gpugems3/part-iii-rendering/chapter-20-gpu-based-importance-sampling
float computeLod(float pdf)
{
    // IBL Baker (Matt Davidson)
    // https://github.com/derkreature/IBLBaker/blob/65d244546d2e79dd8df18a28efdabcf1f2eb7717/data/shadersD3D11/IblImportanceSamplingDiffuse.fx#L215
    float solidAngleTexel = 4.0 * UX3D_MATH_PI / (6.0 * float(pFilterParameters.width) * float(pFilterParameters.sampleCount));
    float solidAngleSample = 1.0 / (float(pFilterParameters.sampleCount) * pdf);
    float lod = 0.5 * log2(solidAngleSample / solidAngleTexel);

    return lod;
}

vec3 filterColor(vec3 N)
{
	  vec4 color = vec4(0.f);

    for(int i = 0; i < pFilterParameters.sampleCount; ++i)
    {
        vec4 importanceSample = getImportanceSample(i, N, pFilterParameters.roughness);

        vec3 H = vec3(importanceSample.xyz);
        float pdf = importanceSample.w;

        // mipmap filtered samples (GPU Gems 3, 20.4)
        float lod = computeLod(pdf);

        // apply the bias to the lod
        lod += pFilterParameters.lodBias;

        if(pFilterParameters.distribution == cLambertian)
        {
            float NdotH = clamp(dot(N, H), 0.0, 1.0);

            // sample lambertian at a lower resolution to avoid fireflies
            vec3 lambertian = textureLod(uCubeMap, H, lod).rgb;

            //// the below operations cancel each other out
            // lambertian *= NdotH; // lamberts law
            // lambertian /= pdf; // invert bias from importance sampling
            // lambertian /= MATH_PI; // convert irradiance to radiance https://seblagarde.wordpress.com/2012/01/08/pi-or-not-to-pi-in-game-lighting-equation/

            color += vec4(lambertian, 1.0);
        }
        else if(pFilterParameters.distribution == cGGX || pFilterParameters.distribution == cCharlie)
        {
            // Note: reflect takes incident vector.
            vec3 V = N;
            vec3 L = normalize(reflect(-V, H));
            float NdotL = dot(N, L);

            if (NdotL > 0.0)
            {
                if(pFilterParameters.roughness == 0.0)
                {
                    // without this the roughness=0 lod is too high (taken from original implementation)
                    lod = pFilterParameters.lodBias;
                }

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
    float A = 0.0;
    float B = 0.0;
    float C = 0.0;

    for(int i = 0; i < pFilterParameters.sampleCount; ++i)
    {
        // Importance sampling, depending on the distribution.
        vec3 H = getImportanceSample(i, N, roughness).xyz;
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
                C += 0.0;
            }

            if (pFilterParameters.distribution == cCharlie)
            {
                // LUT for Charlie distribution.

                float sheenDistribution = D_Charlie(roughness, NdotH);
                float sheenVisibility = V_Ashikhmin(NdotL, NdotV);

                A += 0.0;
                B += 0.0;
                C += sheenVisibility * sheenDistribution * NdotL * VdotH;
            }
        }
    }

    // The PDF is simply pdf(v, h) -> NDF * <nh>.
    // To parametrize the PDF over l, use the Jacobian transform, yielding to: pdf(v, l) -> NDF * <nh> / 4<vh>
    // Since the BRDF divide through the PDF to be normalized, the 4 can be pulled out of the integral.
    return vec3(4.0 * A, 4.0 * B, 4.0 * 2.0 * UX3D_MATH_PI * C) / float(pFilterParameters.sampleCount);
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
		
		outLUT = LUT(inUV.x, inUV.y);
	
	}
}
)""