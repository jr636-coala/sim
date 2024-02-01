#include <SDL2/SDL.h>
#include <algorithm>
#include <functional>
#include "types.hpp"

#define PIXEL(r,g,b) (r|g<<8|b<<16|255<<24)

inline float rem(float x, float y) {
  const i32 quot = x/y;
  return x - y * quot;
}

struct Pixel {
  Pixel() {}
  Pixel(u8 r, u8 g, u8 b, u8 a = 255) : r(r), g(g), b(b), a(a) {}
  Pixel(u32 data) : data(data) {}
  operator u32() { return data; }
  union {
    u32 data;
    struct { u8 r,g,b,a; };
  };

  static Pixel HSL(f32 hue, f32 sat, f32 light) {
    const auto f = [&](auto n){
      const auto k = rem(n + hue / 30.0f, 12.0f);
      const auto a = sat * fmin(light, 1 - light);
      return (light - a * fmax(-1.0f, fmin(fmin(k - 3, 9 - k), 1.0f))) * 255;
    };
    return Pixel(f(0), f(8), f(4));
  }

  static const u32 White = PIXEL(255,255,255);
  static const u32 Magenta = PIXEL(255,0,255);
  static const u32 Red = PIXEL(255,0,0);
  static const u32 Green = PIXEL(0,255,0);
  static const u32 Blue = PIXEL(0,0,255);
  static const u32 Black = PIXEL(0,0,0);
};

struct GridRenderer {
  GridRenderer(int width, int height, int size = -1) : width(width), height(height), size(size) {
    SDL_Init(SDL_INIT_EVERYTHING);
    if (size == -1) {
      SDL_DisplayMode dm;
      SDL_GetCurrentDisplayMode(0, &dm);
      size = std::max(dm.h, dm.w) / std::max(width, height);
    }
    windowWidth = size * width;
    windowHeight = size * height;
    window = SDL_CreateWindow("", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, windowWidth, windowHeight, SDL_WINDOW_SHOWN);
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, width, height);
    pixels = new Pixel[width * height];
  }

  void draw() {
    SDL_UpdateTexture(texture, nullptr, pixels, sizeof(Pixel)*width);
    SDL_RenderCopy(renderer, texture, nullptr, nullptr);
    SDL_RenderPresent(renderer);
  }

  void pollEvents() {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      switch (e.type) {
        case SDL_QUIT: finished = true; break;
        case SDL_MOUSEBUTTONDOWN:
          if (onclick) {
            const auto x = (e.button.x*width)/windowWidth;
            const auto y = (e.button.y*height)/windowHeight;
            onclick(x, y);
          }
          break;
        case SDL_MOUSEBUTTONUP:
          if (onmouseup) {
            const auto x = (e.button.x*width)/windowWidth;
            const auto y = (e.button.y*height)/windowHeight;
            onmouseup(x, y);
          }
          break;
        case SDL_MOUSEMOTION:
          if (onmousemove) {
            const auto x = (e.motion.x*width)/windowWidth;
            const auto y = (e.motion.y*height)/windowHeight;
            const auto xrel = (e.motion.xrel*width)/windowWidth;
            const auto yrel = (e.motion.yrel*height)/windowHeight;
            onmousemove(x, y, xrel, yrel);
          }
          break;
        case SDL_KEYDOWN:
          if (onkeydown) {
            const auto key = e.key.keysym.sym;
            onkeydown(key);
          }
        }
      }
  }

  Pixel* operator[](int y) {
    return &pixels[y * width];
  }

  void clear(Pixel color) {
    forPixels([&](Pixel& pixel, auto, auto) {
      pixel = color;
    });
  }

  template <typename F>
    requires std::invocable<F, Pixel&, int, int>
  void forPixels(F func) {
    for (auto y = 0; y < height; ++y) {
      for (auto x = 0; x < width; ++x) {
        func((*this)[y][x], x,y);
      }
    }
  }

  template <std::invocable F>
  void loop(F func) {
    u64 tick = SDL_GetTicks64();
    while (!finished) {
      pollEvents();
      func();
      draw();
      const u64 curr = SDL_GetTicks64();
      tickdiff[frame % STAT_INTERVAL] = curr - tick;
      tick = curr;
      if (!(frame % STAT_INTERVAL)) {
        u32 sum = 0;
        for (auto i = 0; i < STAT_INTERVAL; ++i) sum += tickdiff[i];
        printf("fps: %f\n", 1000/(sum/static_cast<f32>(STAT_INTERVAL)));
      }
      ++frame;
    }
  }

  static const auto STAT_INTERVAL = 100;
  u32 tickdiff[STAT_INTERVAL] {0};
  u32 frame {1};
  SDL_Window* window;
  SDL_Renderer* renderer;
  SDL_Texture* texture;
  Pixel* pixels;
  int windowWidth;
  int windowHeight;
  bool finished {false};
  // Grid options
  int width;
  int height;
  int size;
  std::function<void(i32,i32)> onclick;
  std::function<void(i32,i32)> onmouseup;
  std::function<void(i32,i32,i32,i32)> onmousemove;
  std::function<void(i32)> onkeydown;
};
