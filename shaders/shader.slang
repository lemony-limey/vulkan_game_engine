struct VertexShaderInput
{
    float2 in_position;
    float3 in_colour;
};

// SV_position refers to screen space coordinates.
struct VertexShaderOutput
{
    float4 sv_position : SV_Position;
    float3 colour;
};

// vid = vertex_id

[shader("vertex")]
VertexShaderOutput vertMain(VertexShaderInput input)
{
    VertexShaderOutput output;
    output.sv_position = float4(input.in_position, 0.0, 1.0);
    output.colour = input.in_colour;
    return output;
}

// inVert = input vertex

[shader("fragment")]
float4 fragMain(VertexShaderOutput in_vert) : SV_Target
{
    // Add the alpha value of 1.0 to set colour to opaque.
    return float4(in_vert.colour, 1.0);
}