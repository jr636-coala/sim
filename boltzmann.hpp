#pragma once
#include <atomic>
#include <cstring>
#include <thread>
#include "types.hpp"

struct BoltzmannLattice {
  static constexpr f32 SITE_WEIGHT[] = {1/36.0,1/9.0,1/36.0,1/9.0,4/9.0,1/9.0,1/36.0,1/9.0,1/36.0};
  static constexpr i32 SITE_DIRX[] = {-1,0,1,-1,0,1,-1,0,1};
  static constexpr i32 SITE_DIRY[] = {-1,-1,-1,0,0,0,1,1,1};
  static constexpr u32 DIR = 9;
  static constexpr f32 DT = .7; 
  static constexpr u32 MAX = 1;

  void worker(u32 tid, u32 n) {
    while (!finished.load(std::memory_order_relaxed)) {
      while (!tstart.load(std::memory_order_acquire)) if (finished.load(std::memory_order_relaxed)) return;
      tworking[tid].store(1);
      while (tstart.load(std::memory_order_acquire)) if (finished.load(std::memory_order_relaxed)) return;
      for (auto y = 0; y < height; ++y) {
        for (auto x = 0; x < width; ++x) {
          const auto index = this->index(0, x, y);
          const auto stride = this->index(1, 0, 0);
          const auto i0 = index+stride*n;
          const auto i1 = i0+stride;
          const auto i2 = i1+stride;
          const auto old0 = grid[i0];
          const auto old1 = grid[i1];
          const auto old2 = grid[i2];
          ngrid[i0] = old0 + DT * (equib(n, x, y) - old0);
          ngrid[i1] = old1 + DT * (equib(n+1, x, y) - old1);
          ngrid[i2] = old2 + DT * (equib(n+2, x, y) - old2);
        }
      }
      tworking[tid].store(0);
    }
  }

  BoltzmannLattice(u32 width, u32 height)
    : width(width), height(height) {
    tcount = DIR/3;
    threads = new std::thread[tcount];
    tworking = new std::atomic_int[tcount];
    tstart.store(0);
    for (auto i = 0; i < tcount; ++i) {
      tworking[i].store(0);
      threads[i] = std::thread(&BoltzmannLattice::worker, this, i, i*3);
    }
    // Setup grids
    grid = new f32[width * height * DIR];
    barrier = new bool[width * height];
    memset(barrier, 0, sizeof(bool) * width * height);
    ngrid = new f32[width * height * DIR];
    for (auto i = 0; i < width * height * DIR; ++i) grid[i] = MAX;
    stabilise();
  }

  void setBarrier(const u32 x, const u32 y, const i32 size) {
    for (auto i = -size; i < size; ++i) {
      for (auto j = -size; j < size; ++j) {
        if (i*i+j*j>size*size) continue;
        barrier[index(0, (x+j+width)%width, (y+i+height)%height)] = true;
      }
    }
  }

  void stabilise() {
    for (auto n = 0; n < DIR; ++n) {
      for (auto y = 0; y < height; ++y) {
        for (auto x = 0; x < width; ++x) {
          grid[index(n, x, y)] = nequib(n, x, y, 0, 0, 1);
        }
      }
    }
  }

  ~BoltzmannLattice() {
    finished.store(1);
    for (auto i = 0; i < tcount; ++i) threads[i].join();
  }

  inline u32 index(auto n, auto x, auto y) const { return n*width*height+y*width+x; }
  inline void swap() { std::swap(grid, ngrid); }
  inline f32 density(auto x, auto y) const {
    f32 vx, vy, den; denvel(x,y,den,vx,vy); return den;
  }

  std::pair<f32, f32> velocity(auto x, auto y) const {
    f32 vx, vy, den; denvel(x, y, den, vx, vy); return { vx, vy };
  }

  void denvel(auto x, auto y, f32& den, f32& vx, f32& vy) const {
    vx = 0.0f, vy = 0.0f, den = 0.0f;
    for (auto n = 0; n < DIR; ++n) {
      const auto g = grid[index(n, x, y)];
      den += g;
      vx += SITE_DIRX[n]*g;
      vy += SITE_DIRY[n]*g;
    }
    vx /= den;
    vy /= den;
  }

  inline f32 equib(auto n, auto x, auto y) const {
    f32 ux, uy, den;
    denvel(x, y, den, ux, uy);
    return nequib(n, x, y, ux, uy, den);
  }

  inline f32 nequib(auto n, auto x, auto y, auto ux, auto uy, auto density) const {
    const f32 duxy=SITE_DIRX[n]*ux+SITE_DIRY[n]*uy;
    const f32 dot0 = duxy*3;
    const f32 dot1 = 4.5f*duxy*duxy;
    const f32 dot2 = 1.5f*(ux*ux+uy*uy);
    return density * SITE_WEIGHT[n] * (1 + dot0 + dot1 - dot2);
  }

  void streaming() {
    for (auto y = 0; y < height - 1; ++y) {
      for (auto x = 0; x < width - 1; ++x) grid[index(0,x,y)] = grid[index(0,x+1,y+1)];
      for (auto x = 0; x < width; ++x) grid[index(1,x,y)] = grid[index(1,x,y+1)];
      for (auto x = width; --x > 0;) grid[index(2,x,y)] = grid[index(2,x-1,y+1)];
    }

    for (auto y = 0; y < height; ++y) {
      for (auto x = 0; x < width - 1; ++x) grid[index(3,x,y)] = grid[index(3,x+1,y)];
      for (auto x = width; --x > 0;) grid[index(5,x,y)] = grid[index(5,x-1,y)];
    }

    for (auto y = height; --y > 0;) {
      for (auto x = 0; x < width - 1; ++x) grid[index(6,x,y)] = grid[index(6,x+1,y-1)];
      for (auto x = 0; x < width; ++x) grid[index(7,x,y)] = grid[index(7,x,y-1)];
      for (auto x = width; --x > 0;) grid[index(8,x,y)] = grid[index(8,x-1,y-1)];
    }

    barrierBounce();

    for (auto n = 0; n < DIR; ++n) {
      for (auto y = 0; y < height; ++y) {
        grid[index(n,0,y)] = nequib(n,0,y,0,0,1);
        grid[index(n,width-1,y)] = nequib(n,width-1,y,0,0,1);
      }
      for (auto x = 0; x < width; ++x) {
        grid[index(n,x,0)] = nequib(n,x,0,0,0,1);
        grid[index(n,x,height-1)] = nequib(n,x,height-1,0,0,1);
      }
    }
  }

  void barrierBounce() {
    for (auto y = 1; y < height - 1; ++y) {
      for (auto x = 1; x < width - 1; ++x) {
        const auto index = this->index(0,x,y);
        const auto gs = this->index(1,0,0);
        const auto pitch = this->index(0,0,1);
        const auto it = [&](auto n, auto dx, auto dy) {
          grid[gs*(8-n)+index+pitch*dy+dx] = grid[index+gs*n];
        };
        if (barrier[index]) {
          it(0, 1,  1); it(1, 0,  1); it(2, -1,  1);
          it(3, 1,  0);               it(5, -1,  0);
          it(6, 1, -1); it(7, 0, -1); it(8, -1, -1);

          grid[index+gs*0] = 0; grid[index+gs*1] = 0; grid[index+gs*2] = 0;
          grid[index+gs*3] = 0;                       grid[index+gs*5] = 0;
          grid[index+gs*6] = 0; grid[index+gs*7] = 0; grid[index+gs*8] = 0;
        }
      }
    }
  }

  void startCollision() {
    tstart.store(1, std::memory_order_release);
    for (auto i = 0; i < tcount; ++i) while (!tworking[i].load());
    tstart.store(0, std::memory_order_release);
  }

  void endCollision() {
    for (auto i = 0; i < tcount; ++i) while (tworking[i].load());
  }

  void update() {
    startCollision();
    endCollision();
    swap();
    streaming();
  }

  void set(auto x, auto y) {
    grid[index(1, x, y)] = MAX;
    grid[index(3, x, y)] = MAX;
    grid[index(5, x, y)] = MAX;
    grid[index(7, x, y)] = MAX;
  }

  int width;
  int height;
  f32* grid;
  f32* ngrid;
  bool* barrier;

  u32 tcount;
  std::atomic_int finished { false };
  std::atomic_int tstart;
  std::atomic_int* tworking;
  std::thread* threads;
};
