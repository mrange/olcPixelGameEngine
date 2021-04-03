#include "pch.hpp"

#define OLC_CUSTOM_FRAGMENT_SHADER my_fragment_shader;
#define OLC_PGE_APPLICATION
#define OLC_GFX_OPENGL33

namespace {
  char const * const my_fragment_shader = R"SHADER(
#version 330 core

#extension GL_EXT_gpu_shader4 : enable

out vec4 pixel;
in vec2 oTex;
in vec4 oCol;

uniform sampler2D sprTex;

// COPY TO HERE -->
#define DOWNSCALE   0.5
#define PI          3.141592654
#define TAU         (2.0*PI)
#define RESOLUTION  SpriteSize()

vec2 SpriteSize() {
  return textureSize2D(sprTex, 0);
}

// HSV to RGB conversion
//  From: https://stackoverflow.com/questions/15095909/from-rgb-to-hsv-in-opengl-glsl
//  sam hocevar claims he is the author of these in particular
vec3 hsv2rgb(vec3 c) {
    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

// RGB to HSV conversion
//  From: https://stackoverflow.com/questions/15095909/from-rgb-to-hsv-in-opengl-glsl
//  sam hocevar claims he is the author of these in particular
vec3 rgb2hsv(vec3 c) {
    vec4 K = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
    vec4 p = mix(vec4(c.bg, K.wz), vec4(c.gb, K.xy), step(c.b, c.g));
    vec4 q = mix(vec4(p.xyw, c.r), vec4(c.r, p.yzx), step(p.x, c.r));

    float d = q.x - min(q.w, q.y);
    float e = 1.0e-10;
    return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
}

// Ray sphere intersection
//  From: https://iquilezles.org/www/articles/spherefunctions/spherefunctions.htm
float raySphere(vec3 ro, vec3 rd, vec4 sph) {
  vec3 oc = ro - sph.xyz;
  float b = dot(oc, rd);
  float c = dot(oc, oc) - sph.w*sph.w;
  float h = b*b - c;
  if (h<0.0) return -1.0;
  h = sqrt(h);
  return -b - h;
}

float psin(float a) {
  return 0.5+0.5*sin(a);
}

vec3 sampleHSV(vec2 p) {
  vec2 cp = abs(p - 0.5);
  vec4 s = texture(sprTex, p);
  vec3 col = mix(oCol.xyz, s.xyz, s.w);
  return rgb2hsv(col)*step(cp.x, 0.5)*step(cp.y, 0.5);
}

vec3 screen(vec2 reso, vec2 p, float diff, float spe) {
  float sr = reso.y/reso.x;
  float res=reso.y/DOWNSCALE;

  // Lots of experimentation lead to these rows

  vec2 ap = p;
  ap.x *= sr;

  // tanh is such a great function!

  // Viginetting
  vec2 vp = ap + 0.5;
  float vig = tanh(pow(max(100.0*vp.x*vp.y*(1.0-vp.x)*(1.0-vp.y), 0.0), 0.35));

  ap *= 1.025;

  // Screen at coord
  vec2 sp = ap;
  sp += 0.5;
  vec3 shsv = sampleHSV(sp);

  // Scan line brightness
  float scanbri = mix(0.25, 2.0, psin(PI*res*p.y));

  shsv.z *= scanbri;
  shsv.z = tanh(1.5*shsv.z);
  shsv.z *= vig;

  // Simulate bad CRT screen
  float dist = (p.x+p.y)*0.05;
  shsv.x += dist;


  vec3 col = vec3(0.0, 0.0, 0.0);
  col += hsv2rgb(shsv);
  col += (0.35*spe+0.25*diff)*vig;

  return col;
}

// Computes the color given the ray origin and texture coord p [-1, 1]
vec3 color(vec2 reso, vec3 ro, vec2 p) {
  // Quick n dirty way to get ray direction
  vec3 rd = normalize(vec3(p, 2.0));

  // The screen is imagined to be projected on a large sphere to give it a curve
  const float radius = 20.0;
  const vec4 center = vec4(0.0, 0.0, radius, radius-1.0);
  vec3 lightPos = 0.95*vec3(-1.0, -1.0, 0.0);

  // Find the ray sphere intersection, basically a single ray tracing step
  float sd = raySphere(ro, rd, center);

  if (sd > 0.0) {
    // sp is the point on sphere where the ray intersected
    vec3 sp = ro + sd*rd;
    // Normal of the sphere allows to compute lighting
    vec3 nor = normalize(sp - center.xyz);
    vec3 ld = normalize(lightPos - sp);

    // Diffuse lighting
    float diff = max(dot(ld, nor), 0.0);
    // Specular lighting
    float spe = pow(max(dot(reflect(rd, nor), ld), 0.0),30.0);

    // Due to how the scene is setup up we cheat and use sp.xy as the screen coord
    return screen(reso, sp.xy, diff, spe);
  } else {
    return vec3(0.0, 0.0, 0.0);
  }
}

void main(void) {
  vec2 reso = RESOLUTION;
  vec2 q = oTex;
  vec2 p = -1. + 2. * q;
  p.x *= reso.x/reso.y;

  vec3 ro = vec3(0.0, 0.0, 0.0);
  vec3 col = color(reso, ro, p);

  pixel = vec4(col, 1.0);
}

/*
void main(void) {
  pixel = 0.5*texture(sprTex, oTex) * oCol;
} */
)SHADER";
}

#include "../../../olcPixelGameEngine.h"

struct Example : olc::PixelGameEngine {
  Example() {
    sAppName = "Example";
  }

  bool OnUserCreate() override {
    // Called once at the start, so create things here
    return true;
  }

  bool OnUserUpdate(float fElapsedTime) override {
    this->FillRect(0, 0, 256, 240, olc::DARK_BLUE);
    this->FillCircle(100, 100, 50, olc::CYAN);
    this->FillCircle(200, 150, 50, olc::MAGENTA);
    return true;
  }
};


int main() {
  Example demo;

  if (demo.Construct(256, 240, 4, 4))
    demo.Start();

  return 0;
}
