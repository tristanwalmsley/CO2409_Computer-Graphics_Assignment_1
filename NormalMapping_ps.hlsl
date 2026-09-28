//--------------------------------------------------------------------------------------
// Texture Pixel Shader
//--------------------------------------------------------------------------------------
// Pixel shader simply samples a diffuse texture map and tints with colours from vertex shadeer

#include "Common.hlsli" // Shaders can also use include files - note the extension


//--------------------------------------------------------------------------------------
// Textures (texture maps)
//--------------------------------------------------------------------------------------

// Here we allow the shader access to a texture that has been loaded from the C++ side and stored in GPU memory.
// Note that textures are often called maps (because texture mapping describes wrapping a texture round a mesh).
// Get used to people using the word "texture" and "map" interchangably.
Texture2D    DiffuseSpecularMap : register(t0); // Diffuse map (main colour) in rgb and specular map (shininess level) in alpha - C++ must load this into slot 0
Texture2D    NormalMap          : register(t1); // Normal map in rgb - C++ must load this into slot 1
Texture2D    ShadowMapLight1    : register(t2); // Shadow map for light 1 - C++ must load this into slot 2
SamplerState TexSampler         : register(s0); // A sampler is a filter for a texture like bilinear, trilinear or anisotropic
SamplerState PointClamp         : register(s1); // No filtering for shadow maps (you might think you could use trilinear or similar, but it will filter light depths not the shadows cast...)



//--------------------------------------------------------------------------------------
// Shader code
//--------------------------------------------------------------------------------------

//***| INFO |*********************************************************************************
// Normal mapping pixel shader function. The lighting part of the shader is the same as the
// per-pixel lighting shader - only the source of the surface normal is different
//
// An extra "Normal Map" texture is used - this contains normal (x,y,z) data in place of
// (r,g,b) data indicating the normal of the surface *per-texel*. This allows the lighting
// to take account of bumps on the texture surface. Using these normals is complex:
//    1. We must store a "tangent" vector as well as a normal for each vertex (the tangent
//       is basically the direction of the texture U axis in model space for each vertex)
//    2. Get the (interpolated) model normal and tangent at this pixel from the vertex
//       shader - these are the X and Z axes of "tangent space"
//    3. Use a "cross-product" to calculate the bi-tangent - the missing Y axis
//    4. Form the "tangent matrix" by combining these axes
//    5. Extract the normal from the normal map texture for this pixel
//    6. Use the tangent matrix to transform the texture normal into model space, then
//       use the world matrix to transform it into world space
//    7. This final world-space normal can be used in the usual lighting calculations, and
//       will show the "bumpiness" of the normal map
//
// Note that all this detail boils down to just five extra lines of code here
//********************************************************************************************
float4 main(NormalMappingPixelShaderInput input) : SV_Target
{
	// Slight adjustment to calculated depth of pixels so they don't shadow themselves
    const float DepthAdjust = 0.00005f;
    
	//************************
	// Normal Map Extraction
	//************************
    
	// Will use the model normal/tangent to calculate matrix for tangent space. The normals for each pixel are *interpolated* from the
	// vertex normals/tangents. This means they will not be length 1, so they need to be renormalised (same as per-pixel lighting issue)
    float3 modelNormal = normalize(input.modelNormal);
    float3 modelTangent = normalize(input.modelTangent);

	// Calculate bi-tangent to complete the three axes of tangent space - then create the *inverse* tangent matrix to convert *from*
	// tangent space into model space. This is just a matrix built from the three axes (very advanced note - by default shader matrices
	// are stored as columns rather than in rows as in the C++. This means that this matrix is created "transposed" from what we would
	// expect. However, for a 3x3 rotation matrix the transpose is equal to the inverse, which is just what we require)
    float3 modelBiTangent = cross(modelNormal, modelTangent);
    float3x3 invTangentMatrix = float3x3(modelTangent, modelBiTangent, modelNormal);
	
	// Get the texture normal from the normal map. The r,g,b pixel values actually store x,y,z components of a normal. However, r,g,b
	// values are stored in the range 0->1, whereas the x, y & z components should be in the range -1->1. So some scaling is needed
    float3 textureNormal = 2.0f * NormalMap.Sample(TexSampler, input.uv).rgb - 1.0f; // Scale from 0->1 to -1->1
    textureNormal.xy *= 2.0f; // Increase bumpiness
    textureNormal = normalize(textureNormal);
    
	// Now convert the texture normal into model space using the inverse tangent matrix, and then convert into world space using the world
	// matrix. Normalise, because of the effects of texture filtering and in case the world matrix contains scaling
    float3 worldNormal = normalize(mul((float3x3) gWorldMatrix, mul(textureNormal, invTangentMatrix)));
    
    // Camera direction
    float3 cameraDirection = normalize(gCameraPosition - input.worldPosition);
    
	//************************
	// Calculate Lighting
	//************************
    
	//------------------------
	// LIGHT 1 - point light
	//------------------------
    
    float3 light1Vector = gLight1Position - input.worldPosition;
    float  light1Distance = length(light1Vector);
    float3 light1Direction = light1Vector / light1Distance; // Quicker than normalising as we have length for attenuation
    float3 halfway1 = normalize(light1Direction + cameraDirection);
    
    // Attenuation is the light colour multiplied by the angle of the light and surface (dot product) multiplied by the distance from the light
    float  attenuation1 = 1.0f / (1.0f + 0.01f * light1Distance + 0.001f * light1Distance * light1Distance);
    float3 diffuseLight1 = gLight1Colour * max(dot(worldNormal, light1Direction), 0) * attenuation1;
    float3 specularLight1 = diffuseLight1 * pow(max(dot(worldNormal, halfway1), 0), gSpecularPower) * attenuation1;
    
	//------------------------
	// LIGHT 2 - point light cycling colour
	//------------------------
    
    float3 light2Vector = gLight2Position - input.worldPosition;
    float  light2Distance = length(light2Vector);
    float3 light2Direction = light2Vector / light2Distance; // Quicker than normalising as we have length for attenuation
    float3 halfway2 = normalize(light2Direction + cameraDirection);
    
    // Attenuation is the light colour multiplied by the angle of the light and surface (dot product) multiplied by the distance from the light
    float  attenuation2 = 1.0f / (1.0f + 0.01f * light2Distance + 0.001f * light2Distance * light2Distance);
    float3 diffuseLight2 = gLight2Colour * max(dot(worldNormal, light2Direction), 0) * attenuation2;
    float3 specularLight2 = diffuseLight2 * pow(max(dot(worldNormal, halfway2), 0), gSpecularPower) * attenuation2;
    
	//------------------------
	// LIGHT 3 - directional light
	//------------------------
    
    float3 light3Direction = normalize(-gLight3Direction);
    float3 halfway3 = normalize(light3Direction + cameraDirection);
    
    float3 diffuseLight3 = gLight3Colour * max(dot(worldNormal, light3Direction), 0);
    float3 specularLight3 = diffuseLight3 * pow(max(dot(worldNormal, halfway3), 0), gSpecularPower);
    
	//------------------------
	// LIGHT 4 - spotlight, incl. shadow mapping
	//------------------------
	
    float3 light4Vector = gLight4Position - input.worldPosition;
    float3 light4Direction = normalize(light4Vector);
    
    float3 diffuseLight4 = 0;
    float3 specularLight4 = 0;
    
	// Check if the pixel falls within the light cone
    if (dot(light4Direction, -gLight4Facing) > gLight4CosHalfAngle)
    {
	    // Using the world position of the current pixel and the matrices of the light (as a camera), find the 2D position of the
	    // pixel *as seen from the light*. Will use this to find which part of the shadow map to look at.
	    // These are the same as the view / projection matrix multiplies in a vertex shader (can improve performance by putting these lines in vertex shader)
        float4 light4ViewPosition = mul(gLight4ViewMatrix, float4(input.worldPosition, 1.0f));
        float4 light4Projection = mul(gLight4ProjectionMatrix, light4ViewPosition);
		
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
            float3 light4Distance = length(light4Vector);
            float3 halfway = normalize(light4Direction + cameraDirection);
            
            diffuseLight4 = gLight4Colour * max(dot(worldNormal, light4Direction), 0) / light4Distance; // Equations from lighting lecture
            specularLight4 = diffuseLight4 * pow(max(dot(worldNormal, halfway), 0), gSpecularPower); // Multiplying by diffuseLight instead of light colour - my own personal preference
        }
    }
    
    // Sample diffuse material colour for this pixel from a texture using a given sampler that you set up in the C++ code
    // Ignoring any alpha in the texture, just reading RGB
    float4 textureColour = DiffuseSpecularMap.Sample(TexSampler, input.uv);
    float3 diffuseMaterialColour = textureColour.rgb;
    float  specularMaterialColour = textureColour.a;

    float3 finalColour = (gAmbientColour + diffuseLight1 + diffuseLight2 + diffuseLight3 + diffuseLight4) * diffuseMaterialColour +
                         (specularLight1 + specularLight2 + specularLight3 + specularLight4) * specularMaterialColour;

    return float4(finalColour, 1.0f); // Always use 1.0f for alpha - no alpha blending in this lab
}