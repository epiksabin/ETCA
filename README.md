# Spectre Tiles Compression Algorithm

## The Novel Project -------

### Overview

- **The Einstein Tiles Compression Algorithm** is a novel approach to image encoding based on the mathematical discovery of the "Hat" and "Spectre" tiles in **March/May 2023**. These are the world's first **aperiodic monotiles**, a single shape that can tile an infinite plane but *never* repeats a pattern.
- Inspired by aperiodic monotiles (Einstein tiles), specifically the Spectre tile discovered in 2023.

### Folders

* `DOCS/`: Documentation of Spectre Tiles theory and its applications.
* `RESEARCH/`: Research papers, notes, and studies related to Spectre Tiles.
* `SRC/`: Source code implementing the Spectre Tiles Compression Algorithm.

---

## The Math & Theory

### The "Spectre" Tile
Unlike squares or hexagons, the Spectre tile does not snap to a standard Cartesian $(x,y)$ grid. Instead, it follows a **Hierarchical Substitution System**:

1.  **Inflation:** A single Spectre tile can be mathematically subdivided into specific smaller "child" Spectres.
2.  **Aperiodicity:** Because the pattern never repeats, it lacks a dominant frequency. In signal processing terms, it reduces structured aliasing. This prevents **Moiré patterns** (aliasing) common in grid-based sampling.

## Architecture

### 1. The Spectre-Tree (vs. Quadtree)
Traditional compression uses a Quadtree (dividing a square into 4 smaller squares). We replace this with a **Spectre-Tree**.

*   **Root:** We start with one giant Spectre tile covering the image.
*   **Variance Check:** We calculate the color variance (detail) within that tile.
*   **Subdivision:** If variance > threshold, the tile is inflated (subdivided) into its mathematical children.
*   **Recursion:** This repeats recursively until the tile can be approximated within an error bound.

### 2. The Addressing System
Since we cannot use $(x,y)$ coordinates, we utilize a hierarchical addressing system based on inflation rules.
*   **Example Address:** `1.4.2.0`

---

## Compression pros and cons, what to expect and what are its trade-offs?

### Problem #1: Overhead of Hierarchical Addressing

- **Issue**: Unlike simple $(x,y)$ coordinates, the Spectre-Tree requires storing hierarchical addresses for each tile.
- **Impact**: Slightly higher metadata overhead, especially for small tiles or images with lots of detail.

### Problem #2: Computational Complexity

- **Issue**: Inflating tiles and recursively checking variance is more CPU-intensive than a traditional quadtree.
- **Impact**: Slower compression, especially for high-resolution images.

### Problem #3: Entropy Coding Challenges

- **Issue**: Aperiodicity reduces simple redundancy patterns.
- **Impact**: Standard run-length or block-based compression might be less efficient; custom entropy coding may be needed.

### Benefit #1: Reduced Structured Aliasing

- **Explanation**: Aperiodic tiling avoids grid-aligned Moiré patterns, which improves visual quality for textures and high-frequency patterns.

### Benefit #2: Adaptive Detail Representation

- **Explanation**: Variance-driven subdivision naturally allocates more tiles to detailed regions and fewer to smooth regions, improving perceptual compression efficiency.
