#include "Common.hlsli"

Texture2D gDiffuseMap : register(t1); // add the sphere's own texture
TextureCube gCubeMap : register(t0);
SamplerState gSampler : register(s0);

float4 main(NormalMappingPixelShaderInput input) : SV_Target
{
    float3 worldNormal = normalize(input.modelNormal);
    float3 viewDir = normalize(gCameraPosition - input.worldPosition);
    float3 reflectDir = reflect(-viewDir, worldNormal);
    reflectDir.y = -reflectDir.y; // flip Y to correct upside down cube map


    float4 reflection = gCubeMap.Sample(gSampler, reflectDir);

    // Tint it slightly rather than mixing in a broken texture
    float3 tint = float3(0.8f, 0.9f, 1.0f); // slight cool chrome tint
    return float4(reflection.rgb * tint, 1.0f);
}
