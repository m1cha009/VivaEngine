#version 450

// Draws grid lines onto the square from Grid.vert. Every pixel works out how close it is to the
// nearest line, measured in pixels, so lines stay about one pixel wide however far away they are,
// and fade out where they would crowd together.

layout(location = 0) in vec3 inWorldPosition;
layout(location = 1) in vec3 inCameraPosition;

layout(location = 0) out vec4 outColor;

// How strongly the lines `spacing` units apart cover this pixel: 1 on a line, 0 between them.
float Lines(vec2 position, float spacing)
{
    const vec2 cells = position / spacing;
    // fwidth: how much `cells` changes from this pixel to the next one across or down (the GPU
    // shades pixels in 2x2 groups and compares neighbours). It's the size of one pixel, measured
    // in cells.
    const vec2 pixel = fwidth(cells);
    // fract(cells - 0.5) - 0.5 runs from -0.5 to 0.5 across a cell, through 0 on its lines: the
    // distance to the nearest line in cells. Divided by the pixel size, it's in pixels.
    const vec2 pixelsToLine = abs(fract(cells - 0.5) - 0.5) / pixel;
    const float line = 1.0 - min(min(pixelsToLine.x, pixelsToLine.y), 1.0);
    // Where a cell is only a few pixels across, the lines would merge into flickering patterns
    // (moire): they fade out before that happens, and the next coarser grid takes over.
    const float crowding = max(pixel.x, pixel.y);
    return line * (1.0 - smoothstep(0.1, 0.3, crowding));
}

void main()
{
    const vec2 position = inWorldPosition.xz;
    const float minor = Lines(position, 1.0);
    const float major = Lines(position, 10.0);
    vec3 color = vec3(0.5);
    float alpha = max(minor * 0.25, major * 0.5);

    // The world's axes, as in Unity: the X axis (where z = 0) red, the Z axis (where x = 0) blue.
    const vec2 pixel = fwidth(position);
    if (abs(position.y) < pixel.y) {
        color = vec3(0.8, 0.15, 0.1);
        alpha = 0.8;
    }
    if (abs(position.x) < pixel.x) {
        color = vec3(0.1, 0.3, 0.9);
        alpha = 0.8;
    }

    // Fades out with distance from the camera, further the higher it is, so the grid has no edge.
    const float fadeEnd = max(80.0, abs(inCameraPosition.y) * 8.0);
    alpha *= 1.0 - smoothstep(fadeEnd * 0.3, fadeEnd, distance(position, inCameraPosition.xz));
    // Between the lines alpha is 0, so blending leaves what's behind as it was. (Skipping such
    // pixels with "discard" would save a little blending, but glslc turns discard into an
    // instruction that needs a device feature this engine doesn't turn on.)
    outColor = vec4(color, alpha);
}
