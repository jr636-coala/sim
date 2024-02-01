#include <cmath>
#include "gridrenderer.hpp"
#include "boltzmann.hpp"

int main(int argc, char** argv) {
  GridRenderer gr(1500, 900);
  BoltzmannLattice bl(gr.width, gr.height);

  const auto w2 = bl.width/2;
  const auto h2 = bl.height/2;
  const std::function<f32(f32)> f[] = {
    [&](f32 x) { return (x-w2)*(x-w2); },
    [&](f32 x) { return w2*2-(x-w2)*(x-w2); },
    [&](f32 x) { return sin(x/10.0f)*w2-w2/2; },
    [&](f32 x) { return cos(x/10.0f)*w2-w2/2; }
  };

  // Plot
  const auto scale = 10000.0f;
  for (auto n = 0; n < 4; ++n) {
    for (f32 x = 0; x < bl.width; x += 1/scale) {
      const auto y = f[n](x);
      if (y >= bl.height || y < 0) continue;
      bl.setBarrier(x, y, 1); 
    }
  }

  // Doing the stuff
  bool drag = false;
  f32 highestVelocity = 0;
  gr.onmousemove = [&](auto x, auto y, auto xrel, auto yrel) {
    const auto size = 3;
    if (drag) bl.setBarrier(x, y, size);
    else {
      const auto density = bl.density(x, y);
      const f32 ux = 10*xrel/static_cast<f32>(bl.width);
      const f32 uy = 10*yrel/static_cast<f32>(bl.height);
      for (auto n = 0; n < bl.DIR; ++n)
        bl.grid[bl.index(n,x,y)] = bl.nequib(n, x, y, ux*bl.SITE_DIRX[n], uy*bl.SITE_DIRY[n], density);
    }
  };
  gr.onclick = [&](auto x, auto y) {
    drag = true;
    gr.onmousemove(x, y, 0, 0);
  };
  gr.onmouseup = [&](auto,auto) { drag = false; };
  gr.onkeydown = [&](auto key) {
    if (key == 's') {
      bl.stabilise();
      highestVelocity = 0;
    }
    else if (key == 'e') {
      // Emitter
      bl.grid[bl.index(1,w2/2,h2+h2/2)] = bl.nequib(1, w2/2, h2+h2/2, .1, .1, 1);
    }
    else if (key == 'q') gr.finished = true;
  };

  gr.loop([&]() {
      bl.update();
      gr.forPixels([&](Pixel& pixel, auto x, auto y) {
        if (bl.barrier[bl.index(0,x,y)]) {
          pixel = Pixel::Black;
          return;
        }
        const auto [ux, uy] = bl.velocity(x, y);
        const auto v = sqrt(ux*ux+uy*uy);
        highestVelocity = fmax(v, highestVelocity);
        const auto grey = 100000*v/highestVelocity;
        pixel = Pixel::HSL(grey,.5,.5);
      });
  });
  return 0;
}
