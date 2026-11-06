#pragma once
#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "maestro.hpp"

inline ChessBoard make_starting_board() {
  ChessBoard b;
  b.bb[W_PAWN] = 0x000000000000FF00ULL;
  b.bb[W_KNIGHT] = 0x0000000000000042ULL;
  b.bb[W_BISHOP] = 0x0000000000000024ULL;
  b.bb[W_ROOK] = 0x0000000000000081ULL;
  b.bb[W_QUEEN] = 0x0000000000000008ULL;
  b.bb[W_KING] = 0x0000000000000010ULL;

  b.bb[B_PAWN] = 0x00FF000000000000ULL;
  b.bb[B_KNIGHT] = 0x4200000000000000ULL;
  b.bb[B_BISHOP] = 0x2400000000000000ULL;
  b.bb[B_ROOK] = 0x8100000000000000ULL;
  b.bb[B_QUEEN] = 0x0800000000000000ULL;
  b.bb[B_KING] = 0x1000000000000000ULL;
  b.white_to_move = true;
  b.castling = 0xF;
  b.ep_sq = -1;
  return b;
}

inline ChessBoard parse_fen(const std::string& fen) {
  ChessBoard b;
  for (int i = 0; i < 12; ++i) b.bb[i] = 0ULL;
  b.castling = 0;
  b.ep_sq = -1;

  std::istringstream iss(fen);
  std::string board_part, turn_part, castle_part, ep_part;
  iss >> board_part >> turn_part >> castle_part >> ep_part;

  int r = 7, c = 0;
  for (char ch : board_part) {
    if (ch == '/') {
      r--;
      c = 0;
    } else if (ch >= '1' && ch <= '8') {
      c += (ch - '0');
    } else {
      int sq = r * 8 + c;
      int p = NO_PIECE;
      if (ch == 'P')
        p = W_PAWN;
      else if (ch == 'N')
        p = W_KNIGHT;
      else if (ch == 'B')
        p = W_BISHOP;
      else if (ch == 'R')
        p = W_ROOK;
      else if (ch == 'Q')
        p = W_QUEEN;
      else if (ch == 'K')
        p = W_KING;
      else if (ch == 'p')
        p = B_PAWN;
      else if (ch == 'n')
        p = B_KNIGHT;
      else if (ch == 'b')
        p = B_BISHOP;
      else if (ch == 'r')
        p = B_ROOK;
      else if (ch == 'q')
        p = B_QUEEN;
      else if (ch == 'k')
        p = B_KING;
      if (p != NO_PIECE) b.bb[p] |= (1ULL << sq);
      c++;
    }
  }
  b.white_to_move = (turn_part != "b");
  for (char ch : castle_part) {
    if (ch == 'K')
      b.castling |= 1;
    else if (ch == 'Q')
      b.castling |= 2;
    else if (ch == 'k')
      b.castling |= 4;
    else if (ch == 'q')
      b.castling |= 8;
  }
  if (ep_part.length() == 2 && ep_part[0] >= 'a' && ep_part[0] <= 'h' &&
      ep_part[1] >= '1' && ep_part[1] <= '8') {
    b.ep_sq = (ep_part[1] - '1') * 8 + (ep_part[0] - 'a');
  }
  return b;
}

inline std::string move_to_uci_str(const Move& m) {
  std::string s;
  s += static_cast<char>('a' + (m.from % 8));
  s += static_cast<char>('1' + (m.from / 8));
  s += static_cast<char>('a' + (m.to % 8));
  s += static_cast<char>('1' + (m.to / 8));
  if (m.promoted_piece != NO_PIECE) {
    int pt = m.promoted_piece % 6;
    if (pt == 1)
      s += 'n';
    else if (pt == 2)
      s += 'b';
    else if (pt == 3)
      s += 'r';
    else
      s += 'q';
  }
  return s;
}

inline void run_uci_loop(const char* argv0, const std::string& model_file) {
  std::filesystem::path exe_dir = std::filesystem::path(argv0).parent_path();
  std::string full_model_path = (exe_dir / model_file).string();

  ChessMaestro engine;
  for (int s = 0; s < 3; ++s) {
    engine.lado_A[s] = create_seeded_byte_specialist(s, false);
    engine.lado_B[s] = create_seeded_byte_specialist(s, true);
  }
  if (!engine.load_json(full_model_path)) {
    engine.load_json(model_file);
  }

  ChessBoard board = make_starting_board();
  std::vector<uint64_t> history = {board.compute_hash()};

  std::string line;
  while (std::getline(std::cin, line)) {
    if (line.empty()) continue;
    std::istringstream iss(line);
    std::string token;
    iss >> token;

    if (token == "uci") {
      std::cout << "id name Treestep 1.0\n";
      std::cout << "id author EPBH\n";
      std::cout << "uciok\n" << std::flush;
    } else if (token == "isready") {
      if (!engine.load_json(full_model_path)) {
        engine.load_json(model_file);
      }
      std::cout << "readyok\n" << std::flush;
    } else if (token == "ucinewgame") {
      board = make_starting_board();
      history = {board.compute_hash()};
    } else if (token == "position") {
      std::string sub;
      iss >> sub;
      if (sub == "startpos") {
        board = make_starting_board();
        iss >> sub;
      } else if (sub == "fen") {
        std::string fen_str, part;
        for (int i = 0; i < 6 && (iss >> part); ++i) {
          if (part == "moves") {
            sub = "moves";
            break;
          }
          fen_str += (i ? " " : "") + part;
        }
        board = parse_fen(fen_str);
        if (sub != "moves") iss >> sub;
      }
      history = {board.compute_hash()};
      if (sub == "moves") {
        std::string mv_str;
        while (iss >> mv_str) {
          if (mv_str.length() < 4) continue;
          int f_col = mv_str[0] - 'a', f_row = mv_str[1] - '1';
          int t_col = mv_str[2] - 'a', t_row = mv_str[3] - '1';
          int from_sq = f_row * 8 + f_col;
          int to_sq = t_row * 8 + t_col;

          std::vector<Move> legal;
          generate_legal_moves(board, legal, false);
          bool matched = false;
          for (auto m : legal) {
            if (m.from == from_sq && m.to == to_sq) {
              if (mv_str.length() >= 5) {
                int off = board.white_to_move ? 0 : 6;
                char pr = mv_str[4];
                if (pr == 'n')
                  m.promoted_piece = off + 1;
                else if (pr == 'b')
                  m.promoted_piece = off + 2;
                else if (pr == 'r')
                  m.promoted_piece = off + 3;
                else
                  m.promoted_piece = off + 4;
              }
              board = apply_move(board, m);
              history.push_back(board.compute_hash());
              matched = true;
              break;
            }
          }
          if (!matched) {
            int mp = board.piece_at(from_sq);
            int cp = board.piece_at(to_sq);
            if (mp != NO_PIECE) {
              Move fallback = {
                  (uint8_t)from_sq, (uint8_t)to_sq, (uint8_t)mp, (uint8_t)cp,
                  NO_PIECE,         false,          false};
              board = apply_move(board, fallback);
              history.push_back(board.compute_hash());
            }
          }
        }
      }
    } else if (token == "go") {
      int depth = 4;
      int wtime = 60000, btime = 60000;
      std::string sub;
      while (iss >> sub) {
        if (sub == "depth") {
          iss >> depth;
          depth = std::clamp(depth, 1, 5);
        } else if (sub == "movetime") {
          int mt = 1000;
          iss >> mt;
          depth = (mt < 250) ? 3 : 4;
        } else if (sub == "wtime") {
          iss >> wtime;
        } else if (sub == "btime") {
          iss >> btime;
        }
      }
      int my_time = board.white_to_move ? wtime : btime;
      if (my_time < 2000)
        depth = 2;
      else if (my_time < 8000)
        depth = std::min(depth, 3);

      Move best = engine.select_best_move(board, depth, history);
      std::cout << "bestmove " << move_to_uci_str(best) << "\n" << std::flush;
    } else if (token == "quit") {
      break;
    }
  }
}