import numpy as np
import matplotlib.pyplot as plt
import matplotlib.animation as animation
from matplotlib.widgets import Button

# --- CONFIGURATION CONFIGS ---
MAP_SIZE = 64  # The generated map is MAP_SIZE x MAP_SIZE
GRID_RES = 4
np.random.seed(1466142110)

# --- 1. TORUS MAP GENERATION (Cubic Interpolation) ---
def cubic_interp(p0, p1, p2, p3, t):
    """Standard Catmull-Rom/Cubic spline for smooth transitions."""
    return p1 + 0.5 * t * (p2 - p0 + t * (2.0 * p0 - 5.0 * p1 + 4.0 * p2 - p3 + t * (3.0 * p1 - p0 - 3.0 * p2 + p3)))

def generate_torus_map(size=32, grid_res=4):
    """Generates a perfectly tileable 2D torus map using cubic curves."""
    # Create low-res anchor grid that tiles perfectly
    anchors = np.random.randint(128, 240, size=(grid_res, grid_res))

    torus_map = np.zeros((size, size), dtype=np.uint8)
    scale = size // grid_res

    for y in range(size):
        for x in range(size):
            # Calculate grid coordinates and fractional offsets
            gy, gx = y // scale, x // scale
            ty, tx = (y % scale) / scale, (x % scale) / scale

            # Fetch 4x4 surrounding anchor points wrapping toroidally
            row_nodes = []
            for dy in [-1, 0, 1, 2]:
                col_nodes = []
                for dx in [-1, 0, 1, 2]:
                    col_nodes.append(anchors[(gy + dy) % grid_res, (gx + dx) % grid_res])
                # Interpolate horizontally
                row_nodes.append(cubic_interp(col_nodes[0], col_nodes[1], col_nodes[2], col_nodes[3], tx))
            # Interpolate vertically
            val = cubic_interp(row_nodes[0], row_nodes[1], row_nodes[2], row_nodes[3], ty)
            torus_map[y, x] = np.clip(val, 0, 255).astype(np.uint8)

    return torus_map

# --- 2. FIXED-POINT & XORSHIFT PHYSICS SIMULATION ---
class ToroidalWalker:
    def __init__(self, size=32):
        self.mask_val = size - 1
        self.rng_state = 123456789
        self.x_fp = 0  # 16-bit Fixed-Point (Upper 8: Index, Lower 8: Fraction)
        self.y_fp = 0
        self.dx = 16   # Initial speeds
        self.dy = 8
        self.frame_counter = 0

    def xorshift32(self):
        x = self.rng_state
        x ^= (x << 13) & 0xFFFFFFFF
        x ^= (x >> 17) & 0xFFFFFFFF
        x ^= (x << 5) & 0xFFFFFFFF
        self.rng_state = x
        return x

    def step(self):
        self.x_fp = (self.x_fp + self.dx) & 0xFFFF
        self.y_fp = (self.y_fp + self.dy) & 0xFFFF

        map_x = (self.x_fp >> 8) & self.mask_val
        map_y = (self.y_fp >> 8) & self.mask_val

        self.frame_counter += 1
        if self.frame_counter >= 40:  # Redirect path every ~2 seconds
            self.frame_counter = 0
            rng = self.xorshift32()
            # Random speed vectors
            self.dx = (int(rng & 0x0F) - 8) * 4
            self.dy = (int((rng >> 4) & 0x0F) - 8) * 4
            if self.dx == 0 and self.dy == 0:
                self.dx, self.dy = 16, 8

        return map_x, map_y

# Init variables
current_map = generate_torus_map(size=MAP_SIZE)
walker = ToroidalWalker(MAP_SIZE)

# Render Framework Plots Layouts
fig, (ax_map, ax_preview) = plt.subplots(1, 2, figsize=(10, 5.5))
fig.canvas.manager.set_window_title('Velaflame Low-Power Toroidal Map Simulator')
plt.subplots_adjust(bottom=0.2)

map_img = ax_map.imshow(current_map, cmap='magma', vmin=0, vmax=255, origin='upper')
marker, = ax_map.plot([], [], 'go', markersize=10, markeredgecolor='white', label='Walker position')
ax_map.set_title(f"{MAP_SIZE}x{MAP_SIZE} Toroidal Map")
ax_map.legend()

preview_box = np.zeros((10, 10))
preview_img = ax_preview.imshow(preview_box, cmap='gray', vmin=0, vmax=255)
ax_preview.set_title("Physical LED Output Preview")
ax_preview.axis('off')
preview_text = ax_preview.text(
    4.5, 4.5, "",
    color="orange", weight="bold", fontsize=16,
    ha="center", va="center"
)

def update_frame(frame):
    mx, my = walker.step()
    val = current_map[my, mx]
    marker.set_data([mx], [my])
    preview_img.set_array(np.full((10, 10), val))

    preview_text.set_text(str(int(val)))
    preview_text.set_color("black" if val > 128 else "white")

    return marker, preview_img, preview_text, map_img

ani = animation.FuncAnimation(fig, update_frame, frames=200, interval=50, blit=True)

# --- BUTTON INTERACTION TRIGGERS ---
def handle_rerandomize(event):
    global current_map, current_active_seed

    # 1. Generate a brand new random integer seed
    current_active_seed = np.random.randint(0, 2**31 - 1)

    # 2. Seed the generator explicitly with it
    np.random.seed(current_active_seed)

    # 3. rerandomize the map
    current_map = generate_torus_map(size=MAP_SIZE)
    map_img.set_array(current_map)
    print(f"Topography map surface reshuffled smoothly! {current_active_seed=}")

def handle_export(event):
    filename = "../esphome/algorithms/torus_map.h"
    with open(filename, "w") as f:
        f.write("// SPDX-License-Identifier: GPL-3.0-or-later")
        f.write("// SPDX-FileCopyrightText: 2026 Brian Alvarez and Korte Maki")
        f.write("//")
        f.write(f"// Auto-generated {MAP_SIZE}x{MAP_SIZE} Toroidal Array for Velaflame Low Power Mode.")
        f.write("// Regenerate with scripts/generate_torus.py (do not hand-edit).")
        f.write("#pragma once\n#include <pgmspace.h>\n\n")
        f.write(f"static const uint8_t TORUS_MAP[{MAP_SIZE}][{MAP_SIZE}] PROGMEM = {{\n")
        for row in current_map:
            f.write("    {" + ", ".join(map(str, row)) + "},\n")
        f.write("};\n")
    print(f"File exported successfully: output saved to '{filename}' path location.")

# Render UI Buttons
ax_rand = plt.axes([0.2, 0.05, 0.25, 0.075])
ax_save = plt.axes([0.55, 0.05, 0.25, 0.075])
btn_rand = Button(ax_rand, 'New Torus Map', color='lightblue', hovercolor='skyblue')
btn_save = Button(ax_save, 'Export torus_map.h', color='lightgreen', hovercolor='palegreen')

btn_rand.on_clicked(handle_rerandomize)
btn_save.on_clicked(handle_export)

plt.show()
