cbuffer CBScene : register(b0)
{
    float4x4 Mvp;
};

struct VSIn
{
    float3 pos : POSITION;
    float4 col : COLOR;
};

struct VSOut
{
    float4 pos : SV_Position;
    float4 col : COLOR;
};

VSOut VSMain(VSIn i)
{
    VSOut o;
    o.pos = mul(float4(i.pos, 1.0f), Mvp);
    o.col = i.col;
    return o;
}

float4 PSMain(VSOut i) : SV_Target
{
    return i.col;
}