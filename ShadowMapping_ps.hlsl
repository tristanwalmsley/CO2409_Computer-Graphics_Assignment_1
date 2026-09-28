//--------------------------------------------------------------------------------------
// Per-Pixel Lighting Pixel Shader
//--------------------------------------------------------------------------------------
// Pixel shader receives position and normal from the vertex shader and uses them to calculate
// lighting per pixel. Also samples a samples a diffuse + specular texture map and combines with light colour.

#include "Common.hlsli" // Shaders can also use include files - note the extension


//--------------------------------------------------------------------------------------
// Textures (texture maps)
//--------------------------------------------------------------------------------------

// Here we allow the shader access to a texture that has been loaded from the C++ side and stored in GPU memory.
// Note that textures are often called maps (because texture mapping describes wrapping a texture round a mesh).
// Get used to people using the word "texture" and "map" interchangably.
Texture2D DiffuseSpecularMap : register(t0); // Textures here can contain a diffuse map (main colour) in their rgb channels and a specular map (shininess) in the a channel
SamplerState TexSampler      : register(s0); // A sampler is a filter for a texture like bilinear, trilinear or anisotropic - this is the sampler used for the texture above

Texture2D ShadowMapLight1 : register(t3); // Texture holding the view of the scene from a light
Texture2D ShadowMapLight2 : register(t4); // Texture holding the view of the scene from a light
SamplerState PointClamp   : register(s1); // No filtering for shadow maps (you might think you could use trilinear or similar, but it will filter light depths not the shadows cast...)


//--------------------------------------------------------------------------------------
// Shader code
//--------------------------------------------------------------------------------------

// Pixel shader entry point - each shader has a "main" function
// This shader just samples a diffuse texture map
float4 main(LightingPixelShaderInput input) : SV_Target
{
	// Slight adjustment to calculated depth of pixels so they don't shadow themselves
	const float DepthAdjust = 0.0005f;

    // Normal might have been scaled by model scaling or interpolation so renormalise
    input.worldNormal = normalize(input.worldNormal);
    
    // Direction from pixel to camera
    float3 cameraDirection = normalize(gCameraPosition - input.worldPosition);
	
	//************************
	// Calculate Lighting
	//************************

	//------------------------
	// LIGHT 1 - point light
	//------------------------
	
    float3 diffuseLight1 = 0;
	float3 specularLight1 = 0;

	// Direction from pixel to light
	float3 light1Direction = normalize(gLight1Position - input.worldPosition);
	float  light1Distance  = length(gLight1Position - input.worldPosition);
	float  attenuation     = 1.0f / (1.0f + 0.01f * light1Distance + 0.001f * light1Distance * light1Distance);
	
    diffuseLight1 = gLight1Colour * max(dot(input.worldNormal, light1Direction), 0) * attenuation;
    float3 halfway = normalize(light1Direction + cameraDirection);
    specularLight1 = diffuseLight1 * pow(max(dot(input.worldNormal, halfway), 0), gSpecularPower);
	
	//------------------------
	// LIGHT 2 - point light cycling colour
	//------------------------
	
    float3 diffuseLight2 = 0;
    float3 specularLight2 = 0;
	
    float3 light2Direction = normalize(gLight2Position - input.worldPosition);
	float  light2Distance = length(gLight2Position - input.worldPosition);
    attenuation = 1.0f / (1.0f + 0.01f * light2Distance + 0.001f * light2Distance * light2Distance);
	
    diffuseLight2 = gLight2Colour * max(dot(input.worldNormal, light2Direction), 0) * attenuation;
    halfway = normalize(light2Direction + cameraDirection);
    specularLight2 = diffuseLight2 * pow(max(dot(input.worldNormal, halfway), 0), gSpecularPower);
	
	//------------------------
	// LIGHT 3 - directional light
	//------------------------
	
    float3 diffuseLight3 = 0;
    float3 specularLight3 = 0;
	
    float3 light3Direction = normalize(-gLight3Direction); // Negated as it is the direction towards the light
    diffuseLight3 = gLight3Colour * max(dot(input.worldNormal, light3Direction), 0);
    halfway = normalize(light3Direction + cameraDirection);
    specularLight3 = diffuseLight3 * pow(max(dot(input.worldNormal, halfway), 0), gSpecularPower);
	
	//------------------------
	// LIGHT 4 - spotlight, incl. shadow mapping
	//------------------------
	
    float3 diffuseLight4 = 0;
    float3 specularLight4 = 0;
	
    float3 light4Direction = normalize(gLight4Position - input.worldPosition);
	
	// Check if the pixel falls within the light cone
    if (dot(light4Direction, -gLight4Facing) > gLight4CosHalfAngle)
    {
	    // Using the world position of the current pixel and the matrices of the light (as a camera), find the 2D position of the
	    // pixel *as seen from the light*. Will use this to find which part of the shadow map to look at.
	    // These are the same as the view / projection matrix multiplies in a vertex shader (can improve performance by putting these lines in vertex shader)
        float4 light4ViewPosition = mul(gLight4ViewMatrix, float4(input.worldPosition, 1.0f));
        float4 light4Projection   = mul(gLight4ProjectionMatrix, light4ViewPosition);
		
		// Convert 2D pixel position as viewed from light into texture coordinates for shadow map - an advanced topic related to the projection step
		// Detail: 2D position x & y get perspective divide, then converted from range -1->1 to UV range 0->1. Also flip V axis
        float2 shadowMapUV = 0.5f * light4Projection.xy / light4Projection.w + float2(0.5f, 0.5f);
        shadowMapUV.y = 1.0f - shadowMapUV.y; // Check if pixel is within light cone

		// Get depth of this pixel if it were visible from the light (another advanced projection step)
        float depthFromLight = light4Projection.z / light4Projection.w - DepthAdjust;
		
		// Compare pixel depth from light with depth held in shadow map of the light. If shadow map depth is less than something is nearer
		// to the light than this pixel - so the pixel gets no effect from this light
        if (depthFromLight < ShadowMapLight1.Sample(PointClamp, shadowMapUV).r)
        {
            float light4Distance = length(gLight4Position - input.worldPosition);
            diffuseLight4 = gLight4Colour * max(dot(input.worldNormal, light4Direction), 0) / light4Distance; // Equations from lighting lecture
            float3 halfway = normalize(light4Direction + cameraDirection);
            specularLight4 = diffuseLight4 * pow(max(dot(input.worldNormal, halfway), 0), gSpecularPower); // Multiplying by diffuseLight instead of light colour - my own personal preference
        }
    }
	
	//------------------------
	// Combine textures and all the lighting
	//------------------------
	
    // Sample diffuse material and specular material colour for this pixel from a texture using a given sampler that you set up in the C++ code
    float4 textureColour = DiffuseSpecularMap.Sample(TexSampler, input.uv);
    float3 diffuseMaterialColour = textureColour.rgb; // Diffuse material colour in texture RGB (base colour of model)
    float specularMaterialColour = textureColour.a; // Specular material colour in texture A (shininess of the surface)
	
	// Sum the lights
    float3 diffuseLight  = gAmbientColour + diffuseLight1 + diffuseLight2 + diffuseLight3 + diffuseLight4;
    float3 specularLight = specularLight1 + specularLight2 + specularLight3 + specularLight4;

    // Combine lighting with texture colours
    float3 finalColour = diffuseLight * diffuseMaterialColour + specularLight * specularMaterialColour;

    return float4(finalColour, 1.0f); // Always use 1.0f for output alpha - no alpha blending in this lab
	
	/*
	
	// Check if pixel is within light cone
    if (dot(gLight1Position, -gLight1Facing) > gLight1CosHalfAngle) //**** TODO: This condition needs to be written as the first exercise to get spotlights working
           //           As well as the variables above, you also will need values from the constant buffers in "common.hlsli"
	{
	    // Using the world position of the current pixel and the matrices of the light (as a camera), find the 2D position of the
	    // pixel *as seen from the light*. Will use this to find which part of the shadow map to look at.
	    // These are the same as the view / projection matrix multiplies in a vertex shader (can improve performance by putting these lines in vertex shader)
	    float4 light1ViewPosition = mul(gLight1ViewMatrix,       float4(input.worldPosition, 1.0f)); 
	    float4 light1Projection   = mul(gLight1ProjectionMatrix, light1ViewPosition );

		// Convert 2D pixel position as viewed from light into texture coordinates for shadow map - an advanced topic related to the projection step
		// Detail: 2D position x & y get perspective divide, then converted from range -1->1 to UV range 0->1. Also flip V axis
		float2 shadowMapUV = 0.5f * light1Projection.xy / light1Projection.w + float2(0.5f, 0.5f);
		shadowMapUV.y = 1.0f - shadowMapUV.y;	// Check if pixel is within light cone

		// Get depth of this pixel if it were visible from the light (another advanced projection step)
		float depthFromLight = light1Projection.z / light1Projection.w; //- DepthAdjust; //*** Adjustment so polygons don't shadow themselves
		
		// Compare pixel depth from light with depth held in shadow map of the light. If shadow map depth is less than something is nearer
		// to the light than this pixel - so the pixel gets no effect from this light
		if (depthFromLight < ShadowMapLight1.Sample(PointClamp, shadowMapUV).r)
		{
            float3 light1Dist = length(gLight1Position - input.worldPosition);
            diffuseLight1 = gLight1Colour * max(dot(input.worldNormal, light1Direction), 0) / light1Dist; // Equations from lighting lecture
            float3 halfway = normalize(light1Direction + cameraDirection);
            specularLight1 =  diffuseLight1 * pow(max(dot(input.worldNormal, halfway), 0), gSpecularPower); // Multiplying by diffuseLight instead of light colour - my own personal preference
        }
    }
	
	//----------
	// LIGHT 2

    float3 diffuseLight2 = 0; // Initialy assume no contribution from this light
    float3 specularLight2 = 0;

	// Direction from pixel to light
    float3 light2Direction = normalize(gLight2Position - input.worldPosition);

	// Check if pixel is within light cone
    if (dot(gLight2Position, -gLight2Facing) > gLight2CosHalfAngle) //**** TODO: This condition needs to be written as the first exercise to get spotlights working
           //           As well as the variables above, you also will need values from the constant buffers in "common.hlsli"
    {
	    // Using the world position of the current pixel and the matrices of the light (as a camera), find the 2D position of the
	    // pixel *as seen from the light*. Will use this to find which part of the shadow map to look at.
	    // These are the same as the view / projection matrix multiplies in a vertex shader (can improve performance by putting these lines in vertex shader)
        float4 light2ViewPosition = mul(gLight2ViewMatrix, float4(input.worldPosition, 1.0f));
        float4 light2Projection = mul(gLight2ProjectionMatrix, light2ViewPosition);

		// Convert 2D pixel position as viewed from light into texture coordinates for shadow map - an advanced topic related to the projection step
		// Detail: 2D position x & y get perspective divide, then converted from range -1->1 to UV range 0->1. Also flip V axis
        float2 shadowMapUV = 0.5f * light2Projection.xy / light2Projection.w + float2(0.5f, 0.5f);
        shadowMapUV.y = 1.0f - shadowMapUV.y; // Check if pixel is within light cone

		// Get depth of this pixel if it were visible from the light (another advanced projection step)
        float depthFromLight = light2Projection.z / light2Projection.w; //- DepthAdjust; //*** Adjustment so polygons don't shadow themselves
		
		// Compare pixel depth from light with depth held in shadow map of the light. If shadow map depth is less than something is nearer
		// to the light than this pixel - so the pixel gets no effect from this light
        if (depthFromLight < ShadowMapLight2.Sample(PointClamp, shadowMapUV).r)
        {
            float3 light2Dist = length(gLight2Position - input.worldPosition);
            diffuseLight2 = gLight2Colour * max(dot(input.worldNormal, light2Direction), 0) / light2Dist; // Equations from lighting lecture
            float3 halfway = normalize(light2Direction + cameraDirection);
            specularLight2 = diffuseLight2 * pow(max(dot(input.worldNormal, halfway), 0), gSpecularPower); // Multiplying by diffuseLight instead of light colour - my own personal preference
        }
    }
	
	// Sum the effect of the lights - add the ambient at this stage rather than for each light (or we will get too much ambient)
	float3 diffuseLight = gAmbientColour + diffuseLight1 + diffuseLight2;
	float3 specularLight = specularLight1 + specularLight2;
	*/

}