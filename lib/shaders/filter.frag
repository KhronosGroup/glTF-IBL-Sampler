#version 450
#extension GL_ARB_separate_shader_objects : enable

#define UX3D_MATH_PI 3.1415926535897932384626433832795
#define UX3D_MATH_INV_PI (1.0 / UX3D_MATH_PI)

layout(set = 0, binding = 0) uniform sampler2D uPanorama;
layout(set = 0, binding = 1) uniform samplerCube uCubeMap;

layout(push_constant) uniform FilterParameters {
  float roughness;
  uint sampleCount;
  uint currentMipLevel;
  uint width;
} pFilterParameters;

layout (location = 0) in vec2 inUV;

// output cubemap faces
layout(location = 0) out vec4 outFace0;
layout(location = 1) out vec4 outFace1;
layout(location = 2) out vec4 outFace2;
layout(location = 3) out vec4 outFace3;
layout(location = 4) out vec4 outFace4;
layout(location = 5) out vec4 outFace5;

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

vec2 Hammersley(uint i, uint n)
{
    return vec2(float(i) / float(n), bitfieldReverse(i) * 2.3283064365386963e-10);
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

vec3 lambertWeightedVector(vec2 e, vec3 normal)
{
    float phi = 2.0 * UX3D_MATH_PI * e.y;
    float cosTheta = 1.0 - e.x;
    float sinTheta = sqrt(1.0 - cosTheta*cosTheta);
    
    return getImportanceSampleDirection(normal, sinTheta, cosTheta, phi);
}

vec3 ggxWeightedVector(vec2 e, vec3 normal, float roughness)
{
    float alpha = roughness * roughness;

    float phi = 2.0 * UX3D_MATH_PI * e.y;
    float cosTheta = sqrt((1.0 - e.x) / (1.0 + (alpha*alpha - 1.0) * e.x));
    float sinTheta = sqrt(1.0 - cosTheta*cosTheta);
    
    return getImportanceSampleDirection(normal, sinTheta, cosTheta, phi);
}

float ndfTrowbridgeReitzGGX(float NdotH, float roughness)
{
    float alpha = roughness * roughness;
    
    float alpha2 = alpha * alpha;
    
    float divisor = NdotH * NdotH * (alpha2 - 1.0) + 1.0;
        
    return alpha2 / (UX3D_MATH_PI * divisor * divisor); 
}

vec3 filterSpecular(float roughness, vec3 N)
{
	vec3 specular = vec3(0.f);
	const uint NumSamples = pFilterParameters.sampleCount;
	const float solidAngleTexel = 4.0 * UX3D_MATH_PI / (6.0 * pFilterParameters.width * pFilterParameters.width);
	
	float weight = 0.f;
	
	for(uint i = 0; i < NumSamples; ++i)
	{
		vec3 H = ggxWeightedVector(Hammersley(i, NumSamples), N, roughness);

		// Note: reflect takes incident vector.
		// Note: N = V
		vec3 V = N;
    
		vec3 L = normalize(reflect(-V, H));
    
		float NdotL = dot(N, L);

		if (NdotL > 0.0)
		{
			float lod = 0.0;
		
			if (roughness > 0.0)
			{    
				float VdotH = dot(V, H);
				float NdotH = dot(N, H);
			
				float D = ndfTrowbridgeReitzGGX(NdotH, roughness);
				float pdf = max(D * NdotH / (4.0 * VdotH), 0.0);
				
				float solidAngleSample = 1.0 / (NumSamples * pdf);
				
				lod = 0.5 * log2(solidAngleSample / solidAngleTexel);
			}
			
			specular += textureLod(uCubeMap, L, lod).rgb * NdotL;
						
			weight += NdotL;
		}
	}

	if(weight == 0.f)
	{
		weight = 1.f;		
	}

	return specular / weight;
}

vec3 filterDiffuse(vec3 N)
{
	vec3 diffuse = vec3(0.f);
	uint weight = 0;
	
	const uint NumSamples = pFilterParameters.sampleCount;
	const float solidAngleTexel = 4.0 * UX3D_MATH_PI / (6.0 * pFilterParameters.width * pFilterParameters.width);
	
	for(uint i = 0; i < NumSamples; i++ )
	{		
		vec3 H = lambertWeightedVector(Hammersley(i, NumSamples), N);
    
		// Note: reflect takes incident vector.
		// Note: N = V
		vec3 V = N;
		vec3 L = normalize(reflect(-V, H));
		float NdotL = dot(N, L);
		
		if (NdotL > 0.0)
		{   
			// Mipmap Filtered Samples 
			// see https://github.com/derkreature/IBLBaker
			// see https://developer.nvidia.com/gpugems/GPUGems3/gpugems3_ch20.html
			
			float pdf = max(NdotL * UX3D_MATH_INV_PI, 0.0);				
			
			float solidAngleSample = 1.0 / (float(NumSamples) * pdf);
				
			float lod = 0.5 * log2(solidAngleSample / solidAngleTexel);
			
			diffuse += texture(uCubeMap, H, lod).rgb;
			++weight;
		}		
	}
	
	weight = max(weight, 1u);
	return diffuse / float(weight);
}

// entry point
void panoramaToCubeMap() 
{
	for(int face = 0; face < 6; ++face)
	{		
		vec3 scan = uvToXYZ(face, inUV*2.0-1.0);		
			
		vec3 direction = normalize(scan);		
	
		vec2 src = dirToUV(direction);		
			
		writeFace(face,  texture(uPanorama, src).rgb);
	}
}

// entry point
void filterCubeMapSpecular() 
{
	vec2 newUV = inUV * float(1 << (pFilterParameters.currentMipLevel));
	 
	newUV = newUV*2.0-1.0;
	
	for(int face = 0; face < 6; ++face)
	{
		vec3 scan = uvToXYZ(face, newUV);		
			
		vec3 direction = normalize(scan);	
		direction.y = -direction.y;

		writeFace(face, filterSpecular(pFilterParameters.roughness, direction));
		
		//Debug output:
		//writeFace(face,  texture(uCubeMap, direction).rgb);
		//writeFace(face,   direction);
	}
}

// entry point
void filterCubeMapDiffuse() 
{	
	for(int face = 0; face < 6; ++face)
	{
		vec3 scan = uvToXYZ(face, inUV*2.0-1.0);		
			
		vec3 direction = normalize(scan);	
		direction.y = -direction.y;

		writeFace(face, filterDiffuse(direction));
	}
}
