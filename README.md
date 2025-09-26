## Game Engine Project Structure

```
.
├── CMakeLists.txt
├── CMakePresets.json
├── README.md
├── src/
│   └── main.cpp
├── include/
├── tests/
└── build/
```

### How to Build

```bash
cmake --preset=default
cmake --build --preset=default
```

### How to Run

```bash
cd build
./main
```

### Where to Put Your Code
- Place engine source files in `src/`
- Place headers in `include/`
- Place tests in `tests/`
Building...