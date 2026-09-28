#include "Common.hlsli"

Texture2D DiffuseSpecularMap : register(t0);
Texture2D NormalMap : register(t1);
SamplerState TexSampler : register(s0);

float4 main(NormalMappingPixelShaderInput input) : SV_Target
{
    // Build TBN matrix for normal mapping
    float3 modelNormal = normalize(input.modelNormal);
    float3 modelTangent = normalize(input.modelTangent);
    float3 modelBiTangent = cross(modelNormal, modelTangent);
    float3x3 invTangentMatrix = float3x3(modelTangent, modelBiTangent, modelNormal);

    // Sample and unpack normal map
    float3 textureNormal = 2.0f * NormalMap.Sample(TexSampler, input.uv).rgb - 1.0f;
    textureNormal.xy *= 2.0f; // Amplify bump effect
    textureNormal = normalize(textureNormal);

    // Transform normal to world space
    float3 worldNormal = normalize(mul((float3x3) gWorldMatrix, mul(textureNormal, invTangentMatrix)));

    // Sample base texture
    float4 textureColour = DiffuseSpecularMap.Sample(TexSampler, input.uv);

    // Colour shift over time
    float3 colourShift = float3(
        sin(gFrameTime * 2.0f) * 0.5f + 0.5f,
        sin(gFrameTime * 2.0f + 2.09f) * 0.5f + 0.5f,
        sin(gFrameTime * 2.0f + 4.18f) * 0.5f + 0.5f
    );

    // Simple lighting using world normal so bumps catch light
    float3 cameraDirection = normalize(gCameraPosition - input.worldPosition);
    float3 light1Dir = normalize(gLight1Position - input.worldPosition);
    float3 light2Dir = normalize(gLight2Position - input.worldPosition);
    float3 light3Dir = normalize(-gLight3Direction);

    float diffuse = max(dot(worldNormal, light1Dir), 0) +
                    max(dot(worldNormal, light2Dir), 0) +
                    max(dot(worldNormal, light3Dir), 0) * 0.5f;

    // Combine texture, colour shift and lighting
    float3 finalColour = textureColour.rgb * colourShift * (gAmbientColour + diffuse);
    
    float pulse = sin(gFrameTime * 2.0f) * 0.2f + 0.6f; // pulses between 0.4 and 0.8
    return float4(finalColour * pulse, 1.0f);
}