struct VSInput
{
    float2 in_position;
    float3 in_colour;
    float2 in_texture_coordinates;
};

struct UniformBuffer
{
    float4x4 model;
    float4x4 view;
    float4x4 projection;
}
ConstantBuffer<UniformBuffer> ubo;

// SV_position refers to screen space coordinates.
struct VSOutput
{
    float4 sv_position : SV_Position;
    float3 fragment_colour;
    float2 fragment_texture_coordinates;
};

// vid = vertex_id

[shader("vertex")]
VSOutput vertMain(VSInput input)
{
    VSOutput output;
    output.sv_position = mul(ubo.projection, mul(ubo.view, mul(ubo.model, float4(input.in_position, 0.0, 1.0))));
    output.fragment_colour = input.in_colour;
    output.fragment_texture_coordinates = input.in_texture_coordinates;
    return output;
}

// inVert = input vertex
Sampler2D texture;

[shader("fragment")]
float4 fragMain(VSOutput in_vert) : SV_Target
{
    // return float4(in_vert.fragment_colour * texture.Sample(in_vert.fragment_texture_coordinates).rgb, 1.0);
    return texture.Sample(in_vert.fragment_texture_coordinates);
}