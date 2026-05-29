struct VSInput
{
    float2 in_position;
    float3 in_colour;
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
    float3 colour;
};

// vid = vertex_id

[shader("vertex")]
VSOutput vertMain(VSInput input)
{
    VSOutput output;
    output.sv_position = mul(ubo.projection, mul(ubo.view, mul(ubo.model, float4(input.in_position, 0.0, 1.0))));
    output.colour = input.in_colour;
    return output;
}

// inVert = input vertex

[shader("fragment")]
float4 fragMain(VSOutput in_vert) : SV_Target
{
    // Add the alpha value of 1.0 to set colour to opaque.
    return float4(in_vert.colour, 1.0);
}