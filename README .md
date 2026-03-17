# Helium Atom Simulator

A real-time simulation of the Helium atom, featuring both a 2D Bohr model and a 3D quantum orbital visualizer. Built with C++ and OpenGL, inspired by [kavan010's Hydrogen Atom Simulator](https://github.com/kavan010/Atoms).

---

## Demos

### 2D Bohr Model
![Helium 2D](images/helium_2d.png)

### 3D Quantum Orbital
![Helium 3D](images/helium_3d.png)

---

## What does it simulate?

### 2D Mode
- **Helium nucleus** with 2 protons (shown as two red circles)
- **2 electrons** orbiting the nucleus — cyan and orange
- **Photon absorption**: incoming cyan photons excite electrons to higher energy levels (orbit expands)
- **Photon emission**: electrons drop back down and emit yellow photons
- **Electron-electron repulsion**: electrons push each other apart
- **Z_eff = 1.6875**: effective nuclear charge accounting for electron shielding (variational method)

### 3D Mode
- **Quantum probability cloud**: each dot represents a possible position of an electron
- **2 electrons** shown separately — fire colormap (orange/red) and cool colormap (blue/cyan)
- Positions are sampled from the Schrödinger equation using the 1s orbital wave function
- Brighter areas = higher probability of finding the electron there
- Drag to rotate, scroll to zoom

---

## Physics

Helium has 2 electrons. Unlike Hydrogen, the Schrödinger equation cannot be solved exactly because of electron-electron repulsion. We use the **variational method**:

- Each electron sees an effective nuclear charge of **Z_eff = 27/16 ≈ 1.6875**
- This accounts for the fact that each electron partially shields the nucleus from the other
- Energy levels are scaled accordingly: **E = -13.6 × Z_eff² / n²** eV

---

## Controls

### 2D
| Input | Action |
|-------|--------|
| Left Click | Shoot photons |

### 3D
| Key | Action |
|-----|--------|
| Drag | Rotate camera |
| Scroll | Zoom in/out |
| T | More particles |
| G | Fewer particles |
| R | Resample positions |

---

## Building

### Requirements
- C++17 compiler
- [CMake](https://cmake.org/)
- [Vcpkg](https://vcpkg.io/)
- [Git](https://git-scm.com/)

### Steps

1. Clone the repository:
```
git clone https://github.com/SirPicklee/Helium.git
cd Helium
```

2. Install dependencies:
```
vcpkg install glew glfw3 glm
```

3. Build:
```
mkdir build
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake
cmake --build build
```

4. Run:
```
.\build\Debug\helium_2d.exe
.\build\Debug\helium_3d.exe
```

---

## Compared to Hydrogen Simulator

| Feature | Hydrogen (kavan010) | Helium (this project) |
|---------|--------------------|-----------------------|
| Electrons | 1 | 2 |
| Nucleus | 1 proton | 2 protons |
| Photon absorption/emission | ✅ | ✅ |
| Electron-electron repulsion | ❌ | ✅ |
| Z_eff physics | ❌ | ✅ |
| 3D orbital cloud | ✅ | ✅ |
| Two-color electrons | ❌ | ✅ |

---

## Credits

Inspired by [kavan010/Atoms](https://github.com/kavan010/Atoms)
