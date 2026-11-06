go straight to [usage](#usage)

# Treestep

Treestep is a white-box symbolic chess engine written in C++17 with a simple interactive html/js visual inspector 

instead of using black-box neural networks (like NNUE or AlphaZero) or massive hardcoded evaluation tables, Treestep evaluates board positions using a **mixture of symbolic byte-AST specialists**; compact binary decision trees that inspect 8-bit rank/file bitboard projections alongside a Pointwise Mutual Information (PMI) spatial markov matrix (yes)

every single evaluation and move decision can be traced step by step from the root predicate down to the leaf formula

## background

this project grew out of the discrete mathematical architecture originally built for **Arboreal Symphony** (a symbolic music generation engine based on relative interval markov chains, boolean decision trees, parsimony-penalized genetic algorithms, and phased specialist orchestration for a uni project about discrete mathematics, its repo is private as i plan to improve it drastically and also upload it, it'll be linked here)

while music and chess are completely different domains, Treestep adapts the same core discrete framework to adversarial search:

1. **8-bit rank & file byte projections:** a chessboard is 8x8, meaning every rank and every file projection fits into a single `uint8_t` byte. Treestep projects 64-bit bitboards into 8-bit registers (pawn files, passed files, connected phalanxes, king shields, center ranks) and evaluates them using bitwise shifts

2. **piecewise-linear byte-AST leaves:** to avoid boolean saturation (where a `true/false` predicate stops rewarding progress once triggered), internal nodes route the board state through binary questions while leaf nodes apply linear scaling multipliers (`base_weight + scale_weight * register_count`)

3. **6 phased specialists (tapered maestro):** evaluation is split across 3 middlegame specialists (`Lado A`) and 3 endgame specialists (`Lado B`), blended smoothly by the remaining material phase ($\phi \in [0, 24]$) to prevent evaluation cliffs when trading pieces

4. **spatial PMI markov prior:** evaluates local pawn-chain geometry and king-shield structure using laplacian-smoothed log-odds ($\log_2(P \cdot V)$)

5. **alpha-beta negamax + quiescence search:** wraps the symbolic evaluator in a fast bitboard search with capture/check quiescence extensions and zobrist repetition detection

## project structure

`bitboard.hpp` - 64-bit bitboard representation, zobrist hashing, 8-bit rank/file projections, and legal move generator (including castling, en passant, and promotions)

`bytetree.hpp` - 8-bit spatial register extractor, bitwise predicates, compiler-style dead-branch elimination (`compact()`), and PMI markov evaluator

`maestro.hpp` - 6-specialist tapered orchestrator, alpha-beta negamax + quiescence search, genetic operators, and json serializer

`uci.hpp` - universal Chess Interface (UCI) protocol loop for plugging `treestep` into chess guis like Lucas Chess, Arena, En Croissant, or `lichess-bot`

`main.cpp` - openMP multi-threaded self-play evolutionary trainer and cli entrypoint

`chess.html` - self-contained interactive web interface with pgn export, heavy match telemetry auditing (`.txt` and `.json`), horizon-blunder detection, and a live svg decision-tree visualizer

## building

requires a C++17 compiler with openMP support (`g++` via Linux, macOS, or MSYS2 MinGW-w64 on Windows)

**general compilation command (should autodetect your cpu's architecture):**

```bash
g++ -O3 -DNDEBUG -fopenmp -static -march=native -mtune=native -flto=auto -fwhole-program -fno-math-errno -fno-trapping-math -funroll-loops -fomit-frame-pointer -std=c++17 main.cpp -o treestep
```

**Intel Tiger Lake (i7-1165G7) specific build (my laptop lol):**

```powershell
g++ -O3 -DNDEBUG -fopenmp -static -march=tigerlake -mtune=tigerlake -mprefer-vector-width=256 -flto=8 -fwhole-program -fno-math-errno -fno-trapping-math -funroll-loops -fomit-frame-pointer -std=c++17 main.cpp -o treestep.exe
```

## usage

### 1. training the trees (evolutionary self-play)

to evolve the 6 specialist trees from self-play matches across an opening book:

```powershell
./treestep.exe --train --pop=32 --gens=500 --depth=2 --plies=60 --out=treestep_model.json
```

whenever a challenger defeats both the reigning champion and the baseline anchor in a 12-game symmetric title gauntlet, the new champion is automatically exported to `treestep_model.json`

### 2. playing in the visualizer (`chess.html`)

open `chess.html` in any browser:
* **manual mode:** play as white against Treestep while watching the live svg decision tree light up the active root-to-leaf path for all 6 specialists

* **autoplay mode:** watch Treestep play against itself

* **load custom models:** upload your evolved `treestep_model.json` file
  - there are currently 3 models pre-trained included in this repo
    1. [v0.90](treestep_model_200g_v0.90.json) - 200 generations
    2. [v0.92](treestep_model_50g_v0.92.json) - 50 generations (updated evaluator)
    3. [v1.0](treestep_model_500g_v1.0.json) - 500 generations (completed training for initial release)
  - [treestep_model.json](treestep_model.json) is just a copy of [v1.0](treestep_model_500g_v1.0.json), so when cloning you can just load up chess.html and it'll work with the latest trained model (you must select it first though)

* **match analysis/telemetry:** export the game as a standard `.pgn` (for lichess/chess.com analysis), a full human-readable post-mortem (`audit .txt`), or structured per-ply telemetry (`log .json`). extremely useful for developing algorithms based on these white box AIs

### 3. running in Lucas Chess / UCI guis

When launched without `--train`, `treestep.exe` runs in standard **UCI mode** and automatically loads `treestep_model.json` from its directory.

To add it to **Lucas Chess**:
1. go to `Engines` -> `External Engines` -> `New`
2. select `treestep.exe`
3. set the seed ELO to `1550` and click `Accept`

btw, when passing `--out=treestep_model_200g_v0.90.json` without `--train` actually tells the uci loop to load that specific json file

---

Developed by **EPBH (me)**

README by **Gemini 4 Argon**, tweaked by me

