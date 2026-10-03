#version 450

// Dear ImGui's fragment shader (the one built into imgui_impl_vulkan.cpp) with one change, for our
// sRGB swapchain. The renderer hands it to ImGui's Vulkan backend (Renderer/ImGuiRenderer.cpp).
//
// ImGui's colors are sRGB values, the way you'd pick them in a paint program. Our swapchain images
// are sRGB too, so the GPU encodes whatever a shader writes from linear to sRGB on the way out
// (see Swapchain.cpp). Written unchanged, ImGui's colors would be encoded a second time and look
// washed out: its dark grey windows would come out a light grey. Decoding them to linear first
// makes that encoding give back exactly the colors ImGui asked for.

// From ImGui's vertex shader: its inputs and outputs must match, so they're declared the same way.
layout(location = 0) in struct {
    vec4 Color;
    vec2 UV;
} In;

// The texture to draw with (usually ImGui's font atlas) and its sampler, in separate descriptor
// sets: the layout ImGui's backend creates.
layout(set = 0, binding = 0) uniform texture2D uTexture;
layout(set = 1, binding = 0) uniform sampler uSampler;

layout(location = 0) out vec4 outColor;

// The sRGB curve: a straight line near black, then a power curve with exponent 2.4.
vec3 SrgbToLinear(vec3 color)
{
    return mix(color / 12.92, pow((color + 0.055) / 1.055, vec3(2.4)), step(0.04045, color));
}

void main()
{
    // Only the vertex color is decoded. The font atlas is white (its letters are in alpha), and an
    // sRGB texture is already decoded to linear by the sampler when it's read.
    vec4 color = vec4(SrgbToLinear(In.Color.rgb), In.Color.a);
    outColor = color * texture(sampler2D(uTexture, uSampler), In.UV);
}
